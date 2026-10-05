"""Source contracts for the wide picking bridge, not execution of the client ABI."""
from pathlib import Path
import re
import unittest

from test_wide_indices_buffer_binding import SOURCE

ROOT = Path(__file__).resolve().parents[1]
WINDOW = (ROOT / "src/client/CM2Shared/VertexWindow.hpp").read_text(encoding="utf-8")
OFFSETS = (ROOT / "src/offsets/game/M2.hpp").read_text(encoding="utf-8")


FORWARDED = ("return g_origTriangleHitTest(scratch, edx, indexBegin, indexEnd, vertexBase, "
             "point, mode, candidate, bestDepth, currentHit);")

# One filler call arms exactly one triangle call. The fill hook clears the frame's pending flag
# before the chain runs, so a failed or foreign fill never leaves an older section armed, and
# only a completed certified upper fill or the record written after the chain can arm it.
FILL_HOOK = (
    "static_assert(Filler < 3); "
    "PickingCall* call = PickingFillCall(scene, instance, skin); "
    "if (call) call->pending = false; "
    "uint32_t index = 0, first = 0; "
    "if (call && !call->legacy && CurrentPickingCall(*call) "
    "&& window::ArraySlot(reinterpret_cast<uintptr_t>(section), "
    "reinterpret_cast<uintptr_t>(call->source.sections), call->source.sectionCount, "
    "sizeof(M2SkinSection), index) && WideVertexStart(*call->skin, index, first) "
    "&& window::NeedsPickingPositions(first, call->source.sections[index].vertexCount)) { "
    "PreparePickingSection(*call, section, Filler, mode, projection, distance); "
    "if (call->ready) return; call->pending = false; } "
    "g_wideSkins.fillNext[Filler](scene, edx, instance, skin, section, mode, projection, distance); "
    "if (!call) return; "
    "if (call->legacy) PrepareLegacySection(*call, section); "
    "else PreparePickingSection(*call, section, Filler, mode, projection, distance);")

# The legacy record reads the section from the frame's own skin, and only after CurrentLegacyCall
# shows that skin is the one the instance walks: the recorded section and the index array the
# triangle call plans against are one skin by construction, not through the fill lookup.
LEGACY_RECORD = (
    "call.pending = true; call.identified = false; uint32_t index = 0; "
    "if (!CurrentLegacyCall(call) || !window::ArraySlot(reinterpret_cast<uintptr_t>(section), "
    "reinterpret_cast<uintptr_t>(call.skin->submeshes), call.skin->submeshCount, "
    "sizeof(M2SkinSection), index)) return; "
    "call.sectionIndex = index; call.section = call.skin->submeshes[index]; call.identified = true;")

# The frame lookups. Every key is a conjunct: a weakened one would hand a frame to another
# instance's or skin's fill, or to a triangle call of another scene, point or candidate.
FILL_CALL = (
    "for (size_t i = 0; i < g_wideSkinCount; ++i) { PickingCall* call = g_wideSkins[i].pickingCall; "
    "if (call && call->scene == scene && call->instance == instance && call->skin == skin) return call; } "
    "return nullptr;")
TRIANGLE_CALL = (
    "for (size_t i = 0; i < g_wideSkinCount; ++i) { PickingCall* call = g_wideSkins[i].pickingCall; "
    "if (call && call->scene == scene && call->point == point && call->mode == mode "
    "&& call->candidate == candidate && call->bestDepth == bestDepth) return call; } "
    "return nullptr;")

# The legacy frame's identity: the live skin read through the instance, the note's address and
# size keys as one conjunction, and the currency gate every follow of the frame's skin sits behind.
LIVE_SKIN = (
    "void* model = instance ? *At<void*>(instance, off::kOffInstModel) : nullptr; "
    "return model ? *At<M2SkinProfile*>(model, off::kOffModelSkin) : nullptr;")
LEGACY_MATCH = (
    "return liveSkin && n.skin == liveSkin && n.indices == liveSkin->indices "
    "&& n.indexCount == liveSkin->indexCount && n.vertexCount == liveSkin->vertexCount;")
LEGACY_NOTE = (
    "liveSkin = LivePickingSkin(instance); "
    "if (!liveSkin || !liveSkin->indices || !liveSkin->submeshes) return nullptr; "
    "for (size_t i = 0; i < g_wideSkinCount; ++i) "
    "if (LegacyNoteMatches(g_wideSkins[i], liveSkin)) return &g_wideSkins[i]; "
    "return nullptr;")
CURRENT_LEGACY = (
    "const M2SkinProfile* live = LivePickingSkin(call.instance); "
    "return live == call.skin && LegacyNoteMatches(*call.note, live);")

# The geometry hook: admission first, the legacy lookup only when it fails, an unnoted instance
# forwarded with every stock argument. liveSkin is set only on the legacy path, so the wide-skin
# record is guarded by legacy before liveSkin is followed.
GEOMETRY_ARGS = "scene, edx, instance, mode, projection, distance, point, candidate, bestDepth, currentHit"
GEOMETRY_HOOK = (
    "WideSkinNote* note = PickingNote(instance); const bool legacy = !note; "
    "const M2SkinProfile* liveSkin = nullptr; "
    "if (legacy) note = LegacyPickingNote(instance, liveSkin); "
    "if (!note) return g_wideSkins.geometryNext(" + GEOMETRY_ARGS + "); "
    "if (legacy && liveSkin->vertexCount > 0x10000u) LogLegacyWide(*note, instance, *liveSkin); "
    "return RunPickingGeometry(*note, legacy, " + GEOMETRY_ARGS + ");")

# Warnings are once per process: the flag is set before the message. Records are once per note
# and class or reason (the bit is set before the record) under the shared 24-record cap.
WARN_PICKING = (
    "static bool warned = false; if (!warned) { warned = true; "
    "WLOG_WARN(\"m2native-indices: picking rejects an unsupported validated-wide call \" "
    "\"(scene=%p instance=%p skin=%p reason=%u); no hit or depth is invented; \" "
    "\"check source identity, section/filler pairing, bounds and bone palette; \" "
    "\"later rejects are only sampled as m2wide-beta picking-reject records\", "
    "call.scene, call.instance, call.skin, static_cast<unsigned>(reason)); }")
# The Beta log contract (HAIR-FLICKER.md) keys on this one-time line.
WARN_LEGACY_SKIP = (
    "static bool warnedCrossing = false; if (warnedCrossing) return; warnedCrossing = true; "
    "uint32_t wideStart = 0; "
    "const bool wideValid = WideVertexStart(*call.skin, call.sectionIndex, wideStart); "
    "WLOG_WARN(\"m2native-indices: picking skips section %u of model=%p skin=%p \" "
    "\"(vertexStart low=%u wide=%u wideValid=%u, vertexCount=%u) until the \" "
    "\"hit test handles wide skins: its 16-bit indices wrap below its 16-bit \" "
    "\"start; later skips are silent\", "
    "call.sectionIndex, *At<void*>(call.instance, off::kOffInstModel), call.skin, "
    "unsigned(call.section.vertexStart), wideStart, unsigned(wideValid), "
    "unsigned(call.section.vertexCount));")
LOG_CAP = "|| g_wideSkins.pickingLogs >= kPickingLogLimit) return; "
LOG_PICKING = (
    "const uint32_t bit = window::CrossesWrap(call.section.vertexStart, call.section.vertexCount) ? 2u "
    ": call.first >= 0x10000u ? 4u : 1u; "
    "if ((call.note->pickingLogged & bit) " + LOG_CAP +
    "call.note->pickingLogged |= bit; ++g_wideSkins.pickingLogs; "
    "const uint16_t* raw = call.skin->indices + call.triangleStart; "
    "WLOG_INFO(\"m2wide-beta: picking record=%u instance=%p skin=%p section=%u \" "
    "\"wideStart=%u vertexStartLow=%u vertices=%u triangleStart=%u indices=%u \" "
    "\"positionRepair=%u filler=%u chunks=%u raw=(%u,%u,%u) local=(%u,%u,%u)\", "
    "g_wideSkins.pickingLogs, call.instance, call.skin, call.sectionIndex, call.first, "
    "unsigned(call.section.vertexStart), unsigned(call.section.vertexCount), call.triangleStart, "
    "unsigned(call.section.indexCount), "
    "unsigned(window::NeedsPickingPositions(call.first, call.section.vertexCount)), "
    "call.filler, chunks, unsigned(raw[0]), unsigned(raw[1]), unsigned(raw[2]), "
    "unsigned(window::LocalIndex(raw[0], call.section.vertexStart)), "
    "unsigned(window::LocalIndex(raw[1], call.section.vertexStart)), "
    "unsigned(window::LocalIndex(raw[2], call.section.vertexStart))); "
    "wxl::log::Flush();")
LOG_PICKING_REJECT = (
    "const uint32_t bit = 0x8u << static_cast<uint32_t>(reason); "
    "if ((call.note->pickingLogged & bit) " + LOG_CAP +
    "call.note->pickingLogged |= bit; ++g_wideSkins.pickingLogs; "
    "WLOG_INFO(\"m2wide-beta: picking-reject record=%u instance=%p skin=%p reason=%u identified=%u \" "
    "\"section=%u vertexStartLow=%u vertices=%u indices=%u remapped=%u\", "
    "g_wideSkins.pickingLogs, call.instance, call.skin, static_cast<unsigned>(reason), "
    "unsigned(call.identified), call.sectionIndex, unsigned(call.section.vertexStart), "
    "unsigned(call.section.vertexCount), unsigned(call.section.indexCount), unsigned(remapped)); "
    "wxl::log::Flush();")
LOG_LEGACY_WIDE = (
    "if ((note.pickingLogged & kLegacyWideLogBit) " + LOG_CAP +
    "note.pickingLogged |= kLegacyWideLogBit; ++g_wideSkins.pickingLogs; "
    "WLOG_INFO(\"m2wide-beta: picking-legacy record=%u instance=%p skin=%p vertices=%u \" "
    "\"certificate=%u; no current shared-window certificate admits this wide skin, \" "
    "\"so its triangle starts are remapped and crossing sections skipped\", "
    "g_wideSkins.pickingLogs, instance, &skin, skin.vertexCount, "
    "unsigned(note.convertedSharedIb != nullptr)); "
    "wxl::log::Flush();")

ADMITTED_RECORD_START = (
    "call.pending = true; call.ready = false; call.identified = false; ++call.prepareEpoch; "
    "call.reject = PickingReject::Call; ")
ADMITTED_RECORD_IDENTITY = (
    "call.sectionIndex = index; call.section = call.source.sections[index]; call.filler = filler; "
    "call.identified = true; call.reject = PickingReject::Shape;")

# The triangle hook: frame lookup, one-fill consumption (read, then clear), legacy dispatch, the
# admitted exact-call match, and the admitted dispatch.
TRIANGLE_LOOKUP = ("PickingCall* call = PickingTriangleCall(scratch, point, mode, candidate, bestDepth); "
                   "if (!call) " + FORWARDED)
TRIANGLE_CONSUME = "const bool pending = call->pending; call->pending = false;"
TRIANGLE_LEGACY = ("if (call->legacy) return LegacyPickingTriangle(*call, pending, scratch, edx, "
                   "indexBegin, indexEnd, vertexBase, point, mode, candidate, bestDepth, currentHit);")
ADMITTED_MATCH = (
    "const M2SkinSection& sec = call->section; "
    "const bool matched = pending && call->identified && CurrentPickingCall(*call) "
    "&& window::StockTriangleCall(reinterpret_cast<uintptr_t>(call->skin->indices), Placement(sec), "
    "reinterpret_cast<uintptr_t>(indexBegin), reinterpret_cast<uintptr_t>(indexEnd), vertexBase);")
ADMITTED_EMPTY = "if (matched && (!sec.indexCount || !sec.vertexCount)) " + FORWARDED
ADMITTED_READY = ("if (matched && call->ready) return TestPickingSection(*call, edx, point, mode, "
                  "candidate, bestDepth, currentHit);")
ADMITTED_REJECT = ("return RejectPickingSection(*call, pending, matched, scratch, edx, vertexBase, "
                   "point, mode, candidate, bestDepth, currentHit);")
TRIANGLE_HOOK = (TRIANGLE_LOOKUP, TRIANGLE_CONSUME, TRIANGLE_LEGACY, ADMITTED_MATCH, ADMITTED_EMPTY,
                 ADMITTED_READY, ADMITTED_REJECT)

# The legacy triangle call: the frame's skin is followed only behind the pending, identified and
# current gate; the pure plan (VertexWindow.hpp, exercised by vertex_window_test.cpp) decides.
LEGACY_GATE = (
    "const M2SkinSection& sec = call.section; const M2SkinProfile* skin = call.skin; "
    "window::LegacyTriangle plan; "
    "if (pending && call.identified && CurrentLegacyCall(call)) "
    "plan = window::PlanLegacyTriangle(reinterpret_cast<uintptr_t>(skin->indices), skin->indexCount, "
    "Placement(sec), reinterpret_cast<uintptr_t>(indexBegin), reinterpret_cast<uintptr_t>(indexEnd), "
    "vertexBase);")
LEGACY_SKIP = "if (plan.action == window::LegacyAction::Skip) { WarnLegacySkip(call); return currentHit; }"
LEGACY_REMAP = ("if (plan.action == window::LegacyAction::Remap) { "
                "indexBegin = skin->indices + plan.triangleStart; indexEnd = indexBegin + sec.indexCount; }")
LEGACY_TRIANGLE = (LEGACY_GATE, LEGACY_SKIP, LEGACY_REMAP, FORWARDED)

REJECT_SECTION = (
    "const PickingReject reason = !pending ? PickingReject::Arguments "
    ": (!call.identified || matched) ? call.reject : PickingReject::Arguments;",
    "const M2SkinSection& sec = call.section; uint32_t first = 0;",
    "if (matched && WideVertexStart(*call.skin, call.sectionIndex, first) "
    "&& !window::NeedsPickingPositions(first, sec.vertexCount) "
    "&& !window::CrossesWrap(sec.vertexStart, sec.vertexCount)) {",
    "RejectPicking(call, reason, true); "
    "uint16_t* begin = call.skin->indices + TriangleStart(sec, *call.skin); "
    "return g_origTriangleHitTest(scratch, edx, begin, begin + sec.indexCount, vertexBase, point, mode, "
    "candidate, bestDepth, currentHit); }",
    "RejectPicking(call, reason, false); return currentHit;")


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
        # The whole body: admission is tried first; the live-skin legacy lookup only when it fails;
        # an unnoted instance still reaches the next geometry link with every original argument;
        # liveSkin is followed only on the legacy path that set it.
        self.assertEqual(code(body("hkHitTestGeometry")), GEOMETRY_HOOK)
        run = code(body("RunPickingGeometry"))
        self.assertIn("return g_wideSkins.geometryNext(" + GEOMETRY_ARGS + ");", run)
        self.assertIn("call.legacy = legacy;", run)
        self.assertIn("~Scope() { note.pickingCall = previous; }", run)
        self.assertIn("note.pickingCall = &call;", run)
        for forbidden in ("skin->batches", "materialLayer", "boneInfluences", "bestDepth ="):
            self.assertNotIn(forbidden, run)
        self.assertIn("__declspec(noinline) int RunPickingGeometry", SOURCE)

    def test_only_completed_certified_upper_fills_replace_native(self):
        # Whole body: exact upper section/current certificate, full preparation validates args
        # and scratch/palette. Failed/partial attempts disarm before the unchanged fallback.
        self.assertEqual(code(body("hkFillPickingVertices")), FILL_HOOK)
        # The fill lookup keys on scene, instance and skin together.
        self.assertEqual(code(body("PickingFillCall")), FILL_CALL)

    def test_exact_section_identity_replaces_first_match_guessing(self):
        prepare = code(body("PreparePickingSection"))
        # Every admitted fill re-arms, clears ready and identity, and advances the epoch first.
        self.assertTrue(prepare.startswith(ADMITTED_RECORD_START))
        self.assertIn("window::ArraySlot(reinterpret_cast<uintptr_t>(section)", prepare)
        self.assertIn("call.source.sectionCount, sizeof(M2SkinSection), index", prepare)
        self.assertIn(ADMITTED_RECORD_IDENTITY, prepare)
        self.assertIn("WideVertexStart(*call.skin, index, call.first)", prepare)
        self.assertIn("(filler == 2) != (sec.boneInfluences == 1)", prepare)
        self.assertEqual(prepare.count("call.pending"), 1)
        self.assertEqual(prepare.count("call.identified = true;"), 1)
        self.assertLess(prepare.index(ADMITTED_RECORD_IDENTITY),
                        prepare.index("(filler == 2) != (sec.boneInfluences == 1)"))
        # Neither frame kind brings back the stale first-low-tuple matcher of e9c68f6.
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
        self.assertIn("if (!call) " + FORWARDED, hook)
        self.assertIn(ADMITTED_EMPTY, hook)
        self.assertNotIn("local[", hook)
        self.assertIn("__declspec(noinline) int TestPickingSection", SOURCE)

    def test_unnoted_and_collision_triangle_calls_reach_the_next_link_untouched(self):
        hook = code(body("hkSceneTriangleHitTest"))
        # Nothing but the frame lookup runs before the untouched call: no skin, note or index scan.
        self.assertTrue(hook.startswith(
            "PickingCall* call = PickingTriangleCall(scratch, point, mode, candidate, bestDepth); "
            "if (!call) " + FORWARDED))
        # Frames exist only inside a geometry scope, so collision's own triangle call (no geometry
        # scope) and every unnoted skin find no frame.
        self.assertEqual(code(SOURCE).count("note.pickingCall = &call;"), 1)
        self.assertIn("note.pickingCall = &call;", code(body("RunPickingGeometry")))
        match = code(body("PickingTriangleCall"))
        self.assertNotIn("indices", match)
        self.assertNotIn("skin", match)

    def test_unadmitted_note_gets_a_live_skin_legacy_frame(self):
        # Whole bodies: the live skin, the one conjunction of address and size keys, the lookup
        # and the currency gate. The note's skin pointer is compared, never followed.
        self.assertEqual(code(body("LivePickingSkin")), LIVE_SKIN)
        matches = code(body("LegacyNoteMatches"))
        self.assertEqual(matches, LEGACY_MATCH)
        legacy = code(body("LegacyPickingNote"))
        self.assertEqual(legacy, LEGACY_NOTE)
        # Index-only notes carry no model, conversion or source record; none is required.
        for forbidden in ("convertedSharedIb", "n.model", "pickingSource", "0x10000"):
            self.assertNotIn(forbidden, matches + legacy)
        # The keys compared are exactly what both index fills record when they note a skin.
        self.assertIn("n = { skin, skin->indices, skin->indexCount, skin->vertexCount, nullptr, nullptr };",
                      code(body("NoteWideSkin")))
        self.assertEqual(code(body("CurrentLegacyCall")), CURRENT_LEGACY)

    def test_legacy_frame_records_the_exact_section_without_repair(self):
        # The whole body: arm and clear identity first, set identity last, only after the live
        # skin's own array names the section. Nothing is filled, projected or chunked.
        prepare = code(body("PrepareLegacySection"))
        self.assertEqual(prepare, LEGACY_RECORD)
        self.assertIn("void PrepareLegacySection(PickingCall& call, void* section)", SOURCE)
        triangle = code(body("LegacyPickingTriangle"))
        for forbidden in ("RefillPickingPositions", "TestPickingSection", "PreparePickingSection",
                          "kOffInstBonePalette", "prepareEpoch", "ready", "local[", "LocalIndex"):
            self.assertNotIn(forbidden, triangle)
        # Position repair and chunking are reachable only from the admitted branches.
        self.assertEqual(code(SOURCE).count("RefillPickingPositions(call)"), 1)
        self.assertIn("call.ready = RefillPickingPositions(call);", code(body("PreparePickingSection")))
        self.assertEqual(code(SOURCE).count("TestPickingSection(*call,"), 1)
        # Each record site is the only writer of its frame kind's arm and identity.
        flat = code(SOURCE)
        self.assertEqual(flat.count("call.pending = true;"), 2)
        self.assertEqual(flat.count("call.identified = true;"), 2)
        self.assertEqual(flat.count("->pending = false;"), 3) # failed prefill disarms before fallback

    def test_legacy_triangle_keeps_the_remap_and_the_crossing_skip(self):
        # The whole body: the gate, the pure plan, the skip, the remap and one forwarded call.
        # 66a64d5: the remapped range keeps the incoming vertex base; any call that is not the
        # recorded section's exact stock call goes on unchanged (window::PlanLegacyTriangle, whose
        # Virna and Kasumi fixtures run in vertex_window_test.cpp).
        triangle = code(body("LegacyPickingTriangle"))
        self.assertEqual(triangle, " ".join(LEGACY_TRIANGLE))
        self.assertNotRegex(triangle, r"\bvertexBase\s*=[^=]")
        self.assertNotRegex(triangle, r"\*bestDepth\s*=")
        plan = code(body("PlanLegacyTriangle", WINDOW))
        self.assertEqual(plan, "if (!StockTriangleCall(indices, s, begin, end, vertexBase)) "
                               "return { LegacyAction::Forward, 0 }; "
                               "if (CrossesWrap(s.vertexStart, s.vertexCount)) return { LegacyAction::Skip, 0 }; "
                               "return { LegacyAction::Remap, SectionTriangleStart(s, total) };")
        self.assertIn("LegacyAction action = LegacyAction::Forward;", WINDOW)
        self.assertEqual(code(body("StockTriangleCall", WINDOW)),
                         "return vertexBase == static_cast<int>(s.vertexStart) && begin >= indices "
                         "&& begin - indices == uintptr_t(s.indexStart) * sizeof(uint16_t) "
                         "&& end >= begin && end - begin == uintptr_t(s.indexCount) * sizeof(uint16_t);")
        # One placement and one fold serve drawing, the admitted reject remap and legacy frames.
        self.assertEqual(code(body("Placement")),
                         "return { s.level, s.vertexStart, s.vertexCount, s.indexStart, s.indexCount };")
        self.assertIn("uint16_t level, vertexStart, vertexCount, indexStart, indexCount;", WINDOW)
        self.assertEqual(code(body("TriangleStart")),
                         "return window::SectionTriangleStart(Placement(section), skin.indexCount);")
        # The whole body: set once, before the one-time line the Beta log contract keys on.
        self.assertEqual(code(body("WarnLegacySkip")), WARN_LEGACY_SKIP)

    def test_empty_admitted_section_forwards_without_warning(self):
        hook = code(body("hkSceneTriangleHitTest"))
        empty = hook.index(ADMITTED_EMPTY)
        self.assertLess(empty, hook.index(ADMITTED_READY))
        self.assertLess(empty, hook.index(ADMITTED_REJECT))
        # The hook itself never warns; only the reject and chunk paths spend the warning.
        self.assertNotIn("WarnPicking", hook)
        self.assertNotIn("RejectPicking(", hook)

    def test_reject_branch_remaps_low_sections_and_skips_the_rest(self):
        # The whole body. Only a matched call -- which includes a current certificate, so the
        # frame's skin is the live one -- walks the wide start and is forwarded remapped.
        self.assertEqual(code(body("RejectPickingSection")), " ".join(REJECT_SECTION))
        self.assertIn("WarnPicking(call, reason); LogPickingReject(call, reason, remapped);",
                      code(body("RejectPicking")))
        self.assertEqual(code(body("LogPickingReject")), LOG_PICKING_REJECT)
        # Reject-reason bits start above the low / crossing / above bits and the legacy-wide bit.
        self.assertIn("kLegacyWideLogBit = 8;", SOURCE)
        self.assertIn("Call = 1,", SOURCE)

    def test_prepare_epoch_stops_a_reprepared_section_between_chunks(self):
        self.assertIn("uint32_t prepareEpoch = 0;", SOURCE)
        prepare = code(body("PreparePickingSection"))
        self.assertLess(prepare.index("++call.prepareEpoch;"), prepare.index("CurrentPickingCall(call)"))
        test = code(body("TestPickingSection"))
        for token in ("const M2SkinSection sec = call.section;",
                      "const uint32_t triangleStart = call.triangleStart;",
                      "const M2SkinProfile* const skin = call.skin;",
                      "const uint32_t epoch = call.prepareEpoch;",
                      "const uint16_t* source = skin->indices + triangleStart;"):
            self.assertIn(token, test)
        self.assertNotIn("M2SkinSection& sec", test)
        loop = test[test.index("for (uint32_t done = 0;"):]
        guard = loop.index("if (!CurrentPickingCall(call) || call.prepareEpoch != epoch) "
                           "{ RejectPicking(call, PickingReject::Changed, false); return currentHit; }")
        self.assertLess(guard, loop.index("currentHit = g_origTriangleHitTest("))
        for frame_field in ("call.section", "call.triangleStart", "call.skin"):
            self.assertNotIn(frame_field, loop)

    def test_triangle_consumes_one_fill_and_checks_the_full_call(self):
        # The whole body, in order: lookup, consume (read, then clear) before either frame kind is
        # dispatched, the full admitted match, then the empty, ready and reject dispatch.
        self.assertEqual(code(body("hkSceneTriangleHitTest")), " ".join(TRIANGLE_HOOK))
        # The whole lookup: every stock argument the frame recorded, as one conjunction.
        self.assertEqual(code(body("PickingTriangleCall")), TRIANGLE_CALL)

    def test_entire_payload_is_validated_before_any_hit_can_change(self):
        test = body("TestPickingSection")
        check = test.index("window::LocalIndex(source[k], sec.vertexStart) >= sec.vertexCount")
        first_call = test.index("currentHit = g_origTriangleHitTest(")
        self.assertLess(check, first_call)
        self.assertIn("RejectPicking(call, PickingReject::Window, false);", test[:first_call])
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
        # Whole bodies: each record sets its per-note bit and counts against the shared cap;
        # each warning sets its process flag before it speaks.
        log = code(body("LogPicking"))
        self.assertEqual(log, LOG_PICKING)
        self.assertEqual(code(body("LogLegacyWide")), LOG_LEGACY_WIDE)
        self.assertEqual(code(body("WarnPicking")), WARN_PICKING)
        self.assertIn("kPickingLogLimit = 24;", SOURCE)
        self.assertIn("if (chunks && CurrentPickingCall(call) && call.prepareEpoch == epoch) "
                      "LogPicking(call, chunks);", body("TestPickingSection"))
        self.assertIn("if (!CurrentPickingCall(call) || call.prepareEpoch != epoch)",
                      body("TestPickingSection"))
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
        self.assertIn("unadmitted noted skins keep the triangle-start remap and skip crossing sections",
                      install)
        self.assertIn("M2_HitTestGeometryFn = int(__fastcall*)", OFFSETS)
        self.assertIn("M2_FillHitTestVerticesFn = void(__fastcall*)", OFFSETS)
        self.assertIn("M2_BlendHitTestMatrixFn = void(__cdecl*)", OFFSETS)


if __name__ == "__main__":
    unittest.main()
