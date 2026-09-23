"""Source contracts for the picking guard; only a native hover proves the crash is gone."""
from pathlib import Path
import re
import unittest

from test_wide_indices_buffer_binding import SOURCE

WINDOW = (Path(__file__).resolve().parents[1]
          / "src/client/CM2Shared/VertexWindow.hpp").read_text(encoding="utf-8")

MATCH = "if (sec.indexStart != low || sec.indexCount != count) continue;"
GUARD = "if (window::CrossesWrap(sec.vertexStart, sec.vertexCount))"
REMAP = "const uint32_t wide = TriangleStart(sec, skin);"
ORIGINAL = "return g_origTriangleHitTest(scratch, edx, indexBegin, indexEnd, vertexBase,"


def body(name, text=SOURCE):
    # Unlike the diagnostics helper, this also finds the int-returning hook.
    match = re.search(r"\b(?:void|bool|int|uint32_t|WideSkinNote\s*\*)\s*(?:__fastcall\s+)?"
                      + name + r"\([^)]*\)\s*\{", text)
    if not match:
        raise AssertionError(f"Missing function {name}")
    start, depth = match.end(), 1
    for index in range(start, len(text)):
        depth += (text[index] == "{") - (text[index] == "}")
        if depth == 0:
            return text[start:index]
    raise AssertionError(f"Unclosed function {name}")


def block(text, opener):
    """The braces following opener, e.g. an if statement's body."""
    start = text.index("{", text.index(opener)) + 1
    depth = 1
    for index in range(start, len(text)):
        depth += (text[index] == "{") - (text[index] == "}")
        if depth == 0:
            return text[start:index]
    raise AssertionError(f"Unclosed block after {opener}")


def code(text):
    """Line comments dropped and whitespace collapsed, to compare a block's statements whole."""
    return re.sub(r"\s+", " ", re.sub(r"//[^\n]*", "", text)).strip()


def elide_call(text, name):
    """text with the arguments of its one name(...) call replaced by ...; literals may hold parentheses."""
    start = text.index(name + "(") + len(name) + 1
    depth, index, quoted = 1, start, False
    while depth:
        char = text[index]
        if char == '"' and text[index - 1] != "\\":
            quoted = not quoted
        elif not quoted:
            depth += (char == "(") - (char == ")")
        index += 1
    return text[:start] + "..." + text[index - 1:]


class PickingGuardTests(unittest.TestCase):
    def test_guard_sits_in_the_matched_section_before_the_remap(self):
        hook = body("hkSceneTriangleHitTest")
        loop = hook.index("for (uint32_t k = 0; k < skin.submeshCount; ++k)")
        self.assertLess(loop, hook.index(MATCH))
        self.assertLess(hook.index(MATCH), hook.index(GUARD))
        self.assertLess(hook.index(GUARD), hook.index(REMAP))
        self.assertLess(hook.index(REMAP), hook.index("if (wide != sec.indexStart)"))
        self.assertLess(hook.index("if (wide != sec.indexStart)"), hook.index(ORIGINAL))
        # Nothing but whitespace between the match and the guard: it tests the matched section.
        between = hook[hook.index(MATCH) + len(MATCH):hook.index(GUARD)]
        self.assertEqual(between.strip(), "")
        self.assertEqual(hook.count(GUARD), 1)

    def test_guard_uses_the_sections_own_vertex_range_not_the_triangle_start(self):
        # The stock test subtracts movzx sec.vertexStart from movzx index; the wrap depends only
        # on the 16-bit start and the count, so a crossing at 131072 is caught as well.
        self.assertIn("constexpr bool CrossesWrap(uint16_t firstLow, uint16_t count)", WINDOW)
        self.assertIn("return static_cast<uint32_t>(firstLow) + count > 0x10000u;", WINDOW)
        condition = GUARD[len("if ("):-1]
        self.assertNotIn("wide", condition.replace("CrossesWrap", ""))
        self.assertNotIn("indexStart", condition)

    def test_guard_warns_once_with_the_wide_start_and_returns_no_hit(self):
        guard = block(body("hkSceneTriangleHitTest"), GUARD)
        once = re.search(r"static bool (\w+) = false;\s*if \(!\1\)\s*\{\s*\1 = true;", guard)
        self.assertIsNotNone(once)
        warned = block(guard, f"if (!{once.group(1)})")
        self.assertIn("WideVertexStart(skin, k, wideStart)", warned)
        self.assertEqual(warned.count("WLOG_WARN("), 1)
        message = "".join(re.findall(r'"([^"]*)"', warned))  # adjacent literals concatenate
        for part in ("picking skips section %u", "model=%p", "skin=%p", "wide=%u", "vertexCount=%u",
                     "until the hit test handles wide skins"):
            self.assertIn(part, message)
        for argument in ("k, n.model, n.skin", "wideStart", "sec.vertexCount"):
            self.assertIn(argument, warned)
        # The wide start is computed for the message only, never on the per-frame path.
        self.assertEqual(guard.count("WideVertexStart("), warned.count("WideVertexStart("))
        # The whole guard, statement for statement: nothing before, between or after the once block
        # (an invented hit, a depth write, a second call) and nothing in it but the flag and the message.
        flag = once.group(1)
        self.assertEqual(code(guard.replace(warned, " ... ", 1)),
                         f"static bool {flag} = false; if (!{flag}) {{ ... }} return currentHit;")
        self.assertEqual(code(elide_call(warned, "WLOG_WARN")),
                         f"{flag} = true; uint32_t wideStart = 0; "
                         "const bool wideValid = WideVertexStart(skin, k, wideStart); WLOG_WARN(...);")

    def test_skip_path_never_calls_the_original_or_writes_depth(self):
        hook = body("hkSceneTriangleHitTest")
        guard = block(hook, GUARD)
        statements = code(guard)
        for name in ("g_origTriangleHitTest", "bestDepth", "indexBegin", "indexEnd", "candidate"):
            self.assertNotIn(name, statements)
        # currentHit leaves the guard as it came in: read once by the return, never assigned.
        self.assertIsNone(re.search(r"\bcurrentHit\s*(?:[-+*/%|&^]|<<|>>)?=(?!=)", statements))
        self.assertEqual(statements.count("currentHit"), 1)
        self.assertEqual(hook.count("return currentHit;"), 1)
        self.assertEqual(hook.count("g_origTriangleHitTest("), 1)
        self.assertEqual(hook.count("WLOG_WARN("), 1)

    def test_install_summary_names_the_skip(self):
        install = body("InstallWideIndices")
        self.assertIn("picking skips noted sections whose vertices cross a 16-bit wrap", install)
        self.assertIn("&hkSceneTriangleHitTest, &g_origTriangleHitTest", install)


if __name__ == "__main__":
    unittest.main()
