"""Source contracts for the temporary Beta logs, not native rendering tests."""
import re
import unittest

from test_wide_indices_buffer_binding import SOURCE


def body(name):
    match = re.search(r"\bvoid\s+(?:__fastcall\s+)?" + name + r"\([^)]*\)\s*\{", SOURCE)
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
        self.assertIn("!NeedsWideVertices(&skin) || section > 1 || used >= limit", refill)
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


if __name__ == "__main__":
    unittest.main()
