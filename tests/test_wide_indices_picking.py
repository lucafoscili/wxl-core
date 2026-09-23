"""Source contracts for the wide picking bridge, not execution of the client ABI."""
from pathlib import Path
import re
import unittest

from test_wide_indices_buffer_binding import SOURCE

ROOT = Path(__file__).resolve().parents[1]
WINDOW = (ROOT / "src/client/CM2Shared/VertexWindow.hpp").read_text(encoding="utf-8")
OFFSETS = (ROOT / "src/offsets/game/M2.hpp").read_text(encoding="utf-8")


def code(text):
    text = re.sub(r"/\*.*?\*/|//[^\n]*", "", text, flags=re.S)
    return re.sub(r"\s+", " ", text).strip()


def body(name, text=SOURCE):
    match = re.search(r"\b" + re.escape(name) + r"\([^)]*\)\s*\{", text)
    if not match:
        raise AssertionError(f"Missing function {name}")
    start, depth = match.end(), 1
    for index in range(start, len(text)):
        depth += (text[index] == "{") - (text[index] == "}")
        if not depth:
            return text[start:index]
    raise AssertionError(f"Unclosed function {name}")


class WidePickingTests(unittest.TestCase):
    def test_live_instance_is_the_only_route_to_source_pointers(self):
        current = body("CurrentPickingSource")
        self.assertIn("!n.convertedSharedIb", current)
        self.assertIn("*At<void*>(instance, off::kOffInstModel)", current)
        self.assertIn("*At<M2SkinProfile*>(model, off::kOffModelSkin)", current)
        self.assertIn("*At<M2Header*>(model, off::kOffModelHeader)", current)
        self.assertNotRegex(code(SOURCE[SOURCE.index("    // ---- picking:"):]),
                            r"(?:n\.skin|note->skin)\s*->|\*n\.skin")
        for field in ("indices", "indexCount", "vertexCount", "submeshes", "submeshCount",
                      "vertexLookup"):
            self.assertIn("skin->" + field + " ==", current)
        self.assertIn("header->vertices.offset ==", current)
        self.assertIn("header->vertices.count ==", current)
        self.assertIn("skin->vertexCount > 0x10000u", current)

    def test_certificate_published_only_after_completed_conversion(self):
        refill = body("RefillSharedIndices")
        commit = refill.index("CommitIndexBuffer(device, buffer);")
        self.assertLess(commit, refill.index("windowNote->pickingSource ="))
        self.assertLess(commit, refill.index("++windowNote->pickingGeneration"))
        self.assertNotIn("pickingSource =", body("hkSetModelIndices"))
        self.assertIn("++g_wideSkins[i].pickingGeneration", body("ClearSharedConversion"))
        self.assertIn("call.generation == call.note->pickingGeneration", body("CurrentPickingCall"))
        self.assertIn("n.pickingCall = active;", body("NoteWideSkin"))
        self.assertIn("n.pickingGeneration = generation;", body("NoteWideSkin"))

    def test_registry_remains_bounded_and_owns_added_state(self):
        self.assertIn("kMaxWideSkins = 64;", SOURCE)
        self.assertIn("WideSkinNote notes[kMaxWideSkins]", SOURCE)
        self.assertIn("if (g_wideSkinCount == kMaxWideSkins) return nullptr;", SOURCE)
        registry = SOURCE[SOURCE.index("    struct WideSkinRegistry"):SOURCE.index("    size_t g_wideSkinCount")]
        for field in ("geometryNext", "fillNext[3]", "pickingLogs"):
            self.assertIn(field, registry)
        self.assertNotIn("thread_local", code(SOURCE))
        picking = code(SOURCE[SOURCE.index("    // ---- picking:"):SOURCE.index("    uint32_t __fastcall hkSharedSetVertices")])
        self.assertNotRegex(picking, r"\b(?:new|malloc|calloc|realloc|_alloca|vector)\b")

    def test_geometry_passes_all_stock_arguments_and_retains_filters(self):
        hook = code(body("hkHitTestGeometry"))
        args = "scene, edx, instance, mode, projection, distance, point, candidate, bestDepth, currentHit"
        self.assertIn("if (!note) return g_wideSkins.geometryNext(" + args + ");", hook)
        run = code(body("RunPickingGeometry"))
        self.assertIn("return g_wideSkins.geometryNext(" + args + ");", run)
        self.assertIn("~Scope() { note.pickingCall = previous; }", run)
        self.assertIn("note.pickingCall = &call;", run)
        for forbidden in ("skin->batches", "materialLayer", "boneInfluences", "bestDepth ="):
            self.assertNotIn(forbidden, run)
        self.assertIn("__declspec(noinline) int RunPickingGeometry", SOURCE)

    def test_every_filler_continues_unchanged_before_the_repair(self):
        hook = code(body("hkFillPickingVertices"))
        forwarded = "g_wideSkins.fillNext[Filler](scene, edx, instance, skin, section, mode, projection, distance);"
        self.assertEqual(hook.count(forwarded), 1)
        self.assertLess(hook.index(forwarded), hook.index("PreparePickingSection("))
        self.assertIn("if (call) PreparePickingSection", hook)
        self.assertIn("if (call) call->pending = false;", hook)
        # No shape walk or palette read on the unmatched filler path.
        self.assertNotIn("WideVertexStart(", hook)
        self.assertNotIn("kOffInstBonePalette", hook)

    def test_exact_section_identity_replaces_first_match_guessing(self):
        prepare = code(body("PreparePickingSection"))
        self.assertIn("window::ArraySlot(reinterpret_cast<uintptr_t>(section)", prepare)
        self.assertIn("call.source.sectionCount, sizeof(M2SkinSection), index", prepare)
        self.assertIn("call.section = call.source.sections[index];", prepare)
        self.assertIn("WideVertexStart(*call.skin, index, call.first)", prepare)
        self.assertIn("(filler == 2) != (sec.boneInfluences == 1)", prepare)
        self.assertNotIn("sec.indexStart != low", SOURCE)
        self.assertNotIn("indexBegin - n.indices", SOURCE)
        self.assertNotIn("indexEnd > indexBegin", SOURCE)

    def test_preparation_checks_both_vertex_arrays_and_triangle_range(self):
        prepare = code(body("PreparePickingSection"))
        for total in ("call.skin->vertexCount", "call.source.vertexCount"):
            self.assertIn("window::Fits(call.first, sec.vertexStart, sec.vertexCount, " + total + ")", prepare)
        self.assertIn("window::TriangleRange(call.triangleStart, sec.indexCount, call.skin->indexCount)", prepare)
        self.assertIn("std::memcmp(&distance, &call.distance, sizeof distance)", prepare)
        self.assertIn("mode != call.mode || projection != call.projection", prepare)
        self.assertLess(prepare.index("CurrentPickingCall"), prepare.index("call.source.sections[index]"))
        self.assertLess(prepare.index("window::TriangleRange"), prepare.index("RefillPickingPositions"))

    def test_positions_use_dense_vertices_and_the_live_palette(self):
        refill = body("RefillPickingPositions")
        self.assertIn("window::NeedsPickingPositions(call.first, section.vertexCount)", refill)
        self.assertIn("source + size_t(call.first + k) * off::kModelVertexStride", refill)
        self.assertIn("*At<void*>(call.instance, off::kOffInstBonePalette)", refill)
        self.assertIn("off::kOffVertexBoneSlots", refill)
        self.assertIn("off::kOffVertexWeights", refill)
        self.assertNotIn("vertexLookup[", refill)
        self.assertNotIn("skin->bones", refill)
        self.assertNotIn("skin.bones", refill)
        self.assertIn("off::kOffSceneHitTestCapacity) < section.vertexCount", refill)
        self.assertIn("std::memcpy(dst + size_t(k) * 3, &projected, sizeof projected);", refill)

    def test_native_blend_paths_and_alignment_are_preserved(self):
        refill = code(body("RefillPickingPositions"))
        for token in ("alignas(16) float blended[16]", "uint32_t lastWeights = 0, lastBones = 0;",
                      "weights != lastWeights || bones != lastBones", "call.filler == 2",
                      "off::kBlendHitTestMatrixSse", "off::kBlendHitTestMatrixScalar",
                      "(palette, weights, bones, blended)", "window::PickingBonesFit(",
                      "reinterpret_cast<uintptr_t>(palette) & 15u", "off::kVec3Transform"):
            self.assertIn(token, refill)
        self.assertNotIn("D3FCEC", SOURCE)
        self.assertNotIn("255.0", refill)
        self.assertIn("call.mode, call.projection, call.distance", refill)

    def test_triangle_unmatched_and_empty_calls_forward_exact_arguments(self):
        hook = code(body("hkSceneTriangleHitTest"))
        forwarded = ("return g_origTriangleHitTest(scratch, edx, indexBegin, indexEnd, vertexBase, "
                     "point, mode, candidate, bestDepth, currentHit);")
        self.assertIn("if (!call) " + forwarded, hook)
        self.assertIn("if (!call->section.indexCount) " + forwarded, hook)
        self.assertNotIn("local[", hook)
        self.assertIn("__declspec(noinline) int TestPickingSection", SOURCE)

    def test_triangle_consumes_one_fill_and_checks_the_full_call(self):
        hook = code(body("hkSceneTriangleHitTest"))
        for token in ("const bool pending = call->pending;", "call->pending = false;",
                      "!pending || !call->ready || !CurrentPickingCall(*call)",
                      "vertexBase != int(call->section.vertexStart)",
                      "indexBegin != call->skin->indices + call->section.indexStart",
                      "indexEnd != indexBegin + call->section.indexCount"):
            self.assertIn(token, hook)
        match = code(body("PickingTriangleCall"))
        for token in ("call->scene == scene", "call->point == point", "call->mode == mode",
                      "call->candidate == candidate", "call->bestDepth == bestDepth"):
            self.assertIn(token, match)
        self.assertNotIn("candidate)", match)  # candidate is compared, never dereferenced.

    def test_entire_payload_is_validated_before_any_hit_can_change(self):
        test = body("TestPickingSection")
        check = test.index("window::LocalIndex(source[k], sec.vertexStart) >= sec.vertexCount")
        first_call = test.index("currentHit = g_origTriangleHitTest(")
        self.assertLess(check, first_call)
        self.assertIn("WarnPicking(call);", test[:first_call])
        self.assertIn("return currentHit;", test[:first_call])
        self.assertNotRegex(code(test), r"\*bestDepth\s*=")
        self.assertNotRegex(code(test), r"return\s+(?:0|candidate)\s*;")

    def test_chunks_are_local_bounded_ordered_and_keep_the_return_chain(self):
        test = code(body("TestPickingSection"))
        for token in ("uint16_t local[window::kPickingIndexChunk];",
                      "window::PickingChunk(sec.indexCount - done)",
                      "local[k] = window::LocalIndex(source[done + k], sec.vertexStart);",
                      "currentHit = g_origTriangleHitTest(call.scene, edx, local, local + count, 0, "
                      "point, mode, candidate, bestDepth, currentHit);", "done += count;",
                      "return currentHit;"):
            self.assertIn(token, test)
        self.assertIn("kPickingIndexChunk = 3072;", WINDOW)
        self.assertIn("count % 3 == 0", body("TriangleRange", WINDOW))
        self.assertIn("count <= total - first", body("TriangleRange", WINDOW))
        self.assertNotRegex(test, r"(?:source|skin->indices)\s*\[[^]]+\]\s*=")

    def test_logs_are_capped_and_not_claimed_as_hit_evidence(self):
        log = body("LogPicking")
        self.assertIn("kPickingLogLimit = 24;", SOURCE)
        self.assertIn("g_wideSkins.pickingLogs >= kPickingLogLimit", log)
        self.assertIn("call.note->pickingLogged & bit", log)
        self.assertEqual(log.count("++g_wideSkins.pickingLogs;"), 1)
        self.assertIn("if (chunks && CurrentPickingCall(call)) LogPicking(call, chunks);", body("TestPickingSection"))
        self.assertIn("static bool warned = false;", body("WarnPicking"))
        self.assertIn("if (!CurrentPickingCall(call))", body("TestPickingSection"))
        self.assertNotIn("hit=1", log)
        self.assertIn("wxl::log::Flush();", log)

    def test_all_named_addresses_and_typed_chains_are_installed(self):
        install = body("InstallWideIndices")
        for name, address in (("kHitTestGeometry", "0081DAF0"),
                              ("kFillHitTestVerticesSse", "0081D680"),
                              ("kFillHitTestVerticesScalar", "0081D830"),
                              ("kFillHitTestVerticesSingle", "0081D9C0"),
                              ("kBlendHitTestMatrixSse", "0081D2C0"),
                              ("kBlendHitTestMatrixScalar", "0081D3D0")):
            self.assertRegex(OFFSETS, name + r"\s*=\s*0x" + address)
            self.assertNotIn("0x" + address, SOURCE)
        for kind in range(3):
            self.assertIn(f"&hkFillPickingVertices<{kind}>, &g_wideSkins.fillNext[{kind}]", install)
        self.assertIn("&hkHitTestGeometry, &g_wideSkins.geometryNext", install)
        self.assertIn("&hkSceneTriangleHitTest, &g_origTriangleHitTest", install)
        self.assertIn("picking repairs validated wide positions", install)
        self.assertNotIn("picking skips noted sections", install)
        self.assertIn("M2_HitTestGeometryFn = int(__fastcall*)", OFFSETS)
        self.assertIn("M2_FillHitTestVerticesFn = void(__fastcall*)", OFFSETS)
        self.assertIn("M2_BlendHitTestMatrixFn = void(__cdecl*)", OFFSETS)


if __name__ == "__main__":
    unittest.main()
