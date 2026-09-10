"""Source-boundary regression checks; native rendering still requires the client."""
from pathlib import Path
import re
import unittest


SOURCE = (Path(__file__).resolve().parents[1]
          / "src/client/CM2Shared/WideIndices.cpp").read_text(encoding="utf-8")


def body(name):
    match = re.search(r"\bvoid\s+" + name + r"\([^)]*\)\s*\{", SOURCE)
    if not match:
        raise AssertionError(f"Missing function {name}")
    start, depth = match.end(), 1
    for index in range(start, len(SOURCE)):
        depth += (SOURCE[index] == "{") - (SOURCE[index] == "}")
        if depth == 0:
            return SOURCE[start:index]
    raise AssertionError(f"Unclosed function {name}")


class BufferBindingTests(unittest.TestCase):
    def test_unlock_does_not_bind_an_index_buffer(self):
        unlock = body("UnlockBuffer")
        self.assertIn("off::Gx_BufUnlockFn", unlock)
        self.assertIn("off::kOffGxBufBuilt) = 1", unlock)
        self.assertNotIn("PrimIndexPtr", unlock)

    def test_index_refills_keep_the_index_binding(self):
        commit = body("CommitIndexBuffer")
        self.assertIn("UnlockBuffer(device, buffer);", commit)
        self.assertIn("Native<off::Gx_PrimIndexPtrFn>", commit)
        for name in ("RefillInstanceIndices", "RefillSharedIndices"):
            with self.subTest(name=name):
                self.assertIn("CommitIndexBuffer(device, buffer);", body(name))

    def test_vertex_refill_never_uses_the_index_commit(self):
        refill = body("RefillWideVertices")
        self.assertIn("UnlockBuffer(device, buffer);", refill)
        self.assertNotIn("CommitIndexBuffer(", refill)
        self.assertNotIn("CommitBuffer(", refill)
        self.assertNotIn("Native<off::Gx_PrimIndexPtrFn>", refill)


if __name__ == "__main__":
    unittest.main()
