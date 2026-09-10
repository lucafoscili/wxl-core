"""Source contracts for Beta logs and shared windows, not native rendering tests."""
import re
import unittest

from test_wide_indices_buffer_binding import SOURCE


def body(name):
    match = re.search(r"\b(?:void|bool|uint32_t|WideSkinNote\s*\*)\s*(?:__fastcall\s+)?"
                      + name + r"\([^)]*\)\s*\{", SOURCE)
    if not match:
        raise AssertionError(f"Missing function {name}")
    start, depth = match.end(), 1
    for index in range(start, len(SOURCE)):
        depth += (SOURCE[index] == "{") - (SOURCE[index] == "}")
        if depth == 0:
            return SOURCE[start:index]
    raise AssertionError(f"Unclosed function {name}")


class WideDiagnosticTests(unittest.TestCase):
    def test_logs_are_bounded_and_wide_only(self):
        for name, limit in (("SharedRefill", 16), ("InstanceRefill", 16),
                            ("VertexRefill", 8), ("Draw", 96)):
            self.assertIn(f"k{name}LogLimit = {limit};", SOURCE)
        refill = body("LogIndexRefill")
        self.assertIn("!NeedsWideVertices(&skin)", refill)
        self.assertIn("section > 1 && skin.submeshCount - section > 3", refill)
        self.assertIn("used >= limit", refill)
        self.assertIn("shared ? kSharedRefillLogLimit : kInstanceRefillLogLimit", refill)
        self.assertEqual(refill.count("++used;"), 1)
        self.assertIn("NeedsWideVertices(&skin) && g_vertexRefillLogs < kVertexRefillLogLimit",
                      body("RefillWideVertices"))
        draw = body("hkDeviceDraw")
        self.assertIn("NeedsWideVertices(g_drawSkin)", draw)
        self.assertIn("g_drawLogs < kDrawLogLimit", draw)
        self.assertEqual(draw.count("++g_drawLogs;"), 1)

    def test_context_restores_across_nested_batches(self):
        draw = body("hkDrawBatch")
        call = draw.index("g_origDrawBatch(ctx, edx);")
        for suffix in ("Section", "Skin", "Model", "Instance"):
            self.assertIn(f"prev{suffix} = g_draw{suffix}",
                          re.sub(r"\s+", " ", draw[:call]))
            self.assertRegex(draw[call:], rf"g_draw{suffix}\s*=\s*prev{suffix};")

    def test_refill_observer_only_reads_completed_triangle(self):
        observer = body("LogIndexRefill")
        self.assertIn("count >= 3 && start <= skin.indexCount", observer)
        self.assertIn("count <= skin.indexCount - start", observer)
        self.assertNotRegex(observer, r"(?:raw|emitted)\s*\[[^]]+\]\s*=")
        for operation in ("LockBuffer(", "UnlockBuffer(", "CommitIndexBuffer(", "EmitBlock("):
            self.assertNotIn(operation, observer)
        for name in ("RefillSharedIndices", "RefillInstanceIndices"):
            refill = body(name)
            self.assertLess(refill.index("EmitBlock("), refill.index("LogIndexRefill("))
            self.assertLess(refill.index("LogIndexRefill("), refill.index("dst +="))
        self.assertIn("wxl::log::Flush();", observer)

    def test_vertex_observation_brackets_lock_without_rebinding(self):
        refill = body("RefillWideVertices")
        self.assertLess(refill.index("offsetBefore ="), refill.index("LockBuffer("))
        self.assertLess(refill.index("UnlockBuffer(device, buffer);"),
                        refill.index('WLOG_INFO("m2wide-beta: vertex-refill'))
        self.assertNotIn("CommitIndexBuffer(", refill)
        self.assertIn("wxl::log::Flush();", refill)

    def test_draw_keeps_base_arithmetic_and_original_call(self):
        draw = body("hkDeviceDraw")
        self.assertEqual(draw.count("*streamOffset = wideStart * stride;"), 1)
        self.assertEqual(draw.count("if (streamOffset) *streamOffset = savedOffset;"), 1)
        self.assertEqual(draw.count("g_origDeviceDraw(device, edx, batch, indexed);"), 1)
        self.assertLess(draw.index('WLOG_INFO("m2wide-beta: draw'),
                        draw.index("g_origDeviceDraw(device, edx, batch, indexed);"))
        self.assertIn("gxoff::kGxDeviceIndexBuffer", draw)
        self.assertIn("wxl::log::Flush();", draw)

    def test_section_base_override_rejects_global_indices(self):
        draw = body("hkDeviceDraw")
        guard = re.search(
            r"if\s*\(indexed\s*&&\s*g_drawSection\s*&&\s*NeedsWideVertices\(g_drawSkin\)"
            r"\s*&&\s*g_drawModel\s*&&\s*!UsesGlobalIndices\(g_drawModel\)\)\s*\{", draw)
        self.assertIsNotNone(guard)
        start, depth = guard.end(), 1
        for end in range(start, len(draw)):
            depth += (draw[end] == "{") - (draw[end] == "}")
            if depth == 0:
                break
        self.assertEqual(depth, 0)
        self.assertIn("*streamOffset = wideStart * stride;", draw[start:end])
        # Bypassed global draws still produce the diagnostic evidence.
        log_gate = draw[draw.index("const bool logDraw"):draw.index(";", draw.index("const bool logDraw"))]
        self.assertNotIn("UsesGlobalIndices", log_gate)

    def test_window_fill_requires_true_overflow_and_valid_dense_source(self):
        eligibility = body("CanWindowSharedIndices")
        for check in ("skin->vertexCount <= 0x10000u", "!UsesGlobalIndices(model)",
                      "off::kOffSharedInstanceCopies) != 1", "off::kEnableShaders",
                      "window::Fits(first, s.vertexStart, s.vertexCount, skin->vertexCount)",
                      "window::LocalIndex(skin->indices[start + k], s.vertexStart) >= s.vertexCount",
                      "skin->vertexLookup[first + k] != uint16_t(first + k)",
                      "first == skin->vertexCount && written == skin->indexCount"):
            self.assertIn(check, eligibility)
        hook = body("hkSharedSetIndices")
        self.assertIn("if (!UsesWideStarts(skin) && !windows) return result;", hook)
        self.assertIn("windows ? note : nullptr", hook)

    def test_conversion_marker_is_cleared_before_rebuild_and_set_after_commit(self):
        hook = body("hkSharedSetIndices")
        self.assertLess(hook.index("if (rebuilding) ClearSharedConversion(model);"),
                        hook.index("g_origSharedSetIndices(model, edx)"))
        refill = body("RefillSharedIndices")
        self.assertLess(refill.index("if (!dst) return;"), refill.index("if (windowNote)"))
        self.assertLess(refill.index("CommitIndexBuffer(device, buffer);"),
                        refill.index("windowNote->convertedSharedIb = buffer;"))
        note = body("NoteWideSkin")
        self.assertIn("if (g_wideSkinCount == kMaxWideSkins) return nullptr;", note)
        # An unrelated instance note preserves a conversion unless skin identity changed.
        self.assertIn("n.indices != skin->indices || n.indexCount != skin->indexCount", note)
        self.assertIn("n.vertexCount != skin->vertexCount", note)
        self.assertNotIn("ClearSharedConversion", body("hkSetModelIndices"))

    def test_converted_draw_requires_recorded_binding_and_restores_descriptor(self):
        draw = body("hkDeviceDraw")
        self.assertIn("ConvertedBinding(*At<void*>(device, gxoff::kGxDeviceIndexBuffer))", draw)
        for check in ("converted->model == g_drawModel && converted->skin == g_drawSkin",
                      "converted->vertexCount == g_drawSkin->vertexCount",
                      "converted->convertedSharedIb == *At<void*>(g_drawModel, off::kOffSharedIndexBuf)",
                      "off::kOffSharedInstanceCopies) == 1", "off::kEnableShaders",
                      "stream == *At<void*>(g_drawModel, off::kOffSharedVertexBuf)",
                      "stride == off::kModelVertexStride", "window::StreamOffset(",
                      "window::Fits(wideStart, s.vertexStart, s.vertexCount, g_drawSkin->vertexCount)"):
            self.assertIn(check, draw)
        self.assertRegex(draw, r"if \(!supported\)[\s\S]*?WLOG_WARN\([\s\S]*?return;")
        call = draw.index("g_origDeviceDraw(device, edx, batch, indexed);")
        for suffix, field in (("Min", "MinIndex"), ("Max", "MaxIndex")):
            self.assertIn(f"saved{suffix} = *At<uint16_t>(batch, gxoff::kGxBatch{field});", draw[:call])
            self.assertIn(f"*At<uint16_t>(batch, gxoff::kGxBatch{field}) = saved{suffix};", draw[call:])
        self.assertIn("if (windowedDraw)", draw[call:])


if __name__ == "__main__":
    unittest.main()
