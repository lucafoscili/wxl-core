// Arithmetic shared by dense vertex-window upload, draw and picking.
// Copyright (C) 2026 WarcraftXL. GPL-3.0-or-later.
#pragma once

#include <cstdint>

namespace wxl::client::m2::window
{
    // Unsigned conversion recovers the local index even when the source wraps at 65536.
    constexpr uint16_t LocalIndex(uint16_t raw, uint16_t startLow)
    {
        return static_cast<uint16_t>(raw - startLow);
    }

    // True when a section's vertices pass a multiple of 65536, so its 16-bit indices wrap below its
    // 16-bit start. LocalIndex stays correct (unsigned); the signed index - start that the client's
    // picking test computes goes negative for those indices.
    constexpr bool CrossesWrap(uint16_t firstLow, uint16_t count)
    {
        return static_cast<uint32_t>(firstLow) + count > 0x10000u;
    }

    constexpr bool Fits(uint32_t first, uint16_t firstLow, uint16_t count, uint32_t total)
    {
        return count != 0 && static_cast<uint16_t>(first) == firstLow
               && first <= total && count <= total - first;
    }

    // A crossing section and every section above it need dense CPU positions. Testing only
    // CrossesWrap would miss the latter; subtraction avoids an overflowing first + count.
    constexpr bool NeedsPickingPositions(uint32_t first, uint16_t count)
    {
        return count != 0 && (first >= 0x10000u || count > 0x10000u - first);
    }

    // Integer-address membership does not subtract or order unrelated C++ pointers, and never
    // forms base + count * stride. The result is untouched on failure.
    constexpr bool ArraySlot(uintptr_t address, uintptr_t base, uint32_t count, uint32_t stride,
                             uint32_t& result)
    {
        if (!base || !stride || address < base) return false;
        const uintptr_t offset = address - base;
        if (offset % stride || offset / stride >= count) return false;
        result = static_cast<uint32_t>(offset / stride);
        return true;
    }

    constexpr bool TriangleRange(uint32_t first, uint32_t count, uint32_t total)
    {
        return count % 3 == 0 && first <= total && count <= total - first;
    }

    // The 16-bit placement fields of one skin section (M2SkinSection +0x02 to +0x0A, in order).
    struct SectionPlacement
    {
        uint16_t level, vertexStart, vertexCount, indexStart, indexCount;
    };

    // A section's first triangle index. level is the high half only when the widened range still
    // lies inside the skin's triangle array (total); otherwise it is an older LOD / sub-batch
    // marker and the client's own 16-bit start stands. Phrased as a subtraction so a garbage level
    // cannot wrap the bound it is being checked against.
    constexpr uint32_t SectionTriangleStart(const SectionPlacement& s, uint32_t total)
    {
        const uint32_t wide = (static_cast<uint32_t>(s.level) << 16) | s.indexStart;
        return wide <= total && s.indexCount <= total - wide ? wide : s.indexStart;
    }

    // The native geometry routine's own triangle call for one section (0x0081DCEB-0x0081DD19):
    // begin = indices + u16 indexStart, end = begin + u16 indexCount, vertex base = u16
    // vertexStart. Integer addresses, as in ArraySlot: an unrelated pointer simply fails to match.
    constexpr bool StockTriangleCall(uintptr_t indices, const SectionPlacement& s, uintptr_t begin,
                                     uintptr_t end, int vertexBase)
    {
        return vertexBase == static_cast<int>(s.vertexStart)
               && begin >= indices && begin - indices == uintptr_t(s.indexStart) * sizeof(uint16_t)
               && end >= begin && end - begin == uintptr_t(s.indexCount) * sizeof(uint16_t);
    }

    // A legacy frame belongs to a noted skin that no conversion certificate admits: every skin of
    // 65,536 vertices or fewer with wide triangle starts, and any larger one whose admission fails.
    // Only the exact stock call of the section its filler recorded changes. A crossing section keeps
    // the stock no-hit result, because the client's signed index - start goes negative (e9c68f6,
    // 0x0081D55C); any other goes to its widened triangle start with the incoming vertex base
    // (66a64d5; the stock range starts 65536 * level indices early, 0x0081D569). The caller asks
    // only for a pending, identified and current frame; everything else is forwarded unchanged.
    enum class LegacyAction { Forward, Skip, Remap };
    struct LegacyTriangle
    {
        LegacyAction action = LegacyAction::Forward;
        uint32_t triangleStart = 0; // Remap only
    };

    constexpr LegacyTriangle PlanLegacyTriangle(uintptr_t indices, uint32_t total,
                                                const SectionPlacement& s, uintptr_t begin,
                                                uintptr_t end, int vertexBase)
    {
        if (!StockTriangleCall(indices, s, begin, end, vertexBase)) return { LegacyAction::Forward, 0 };
        if (CrossesWrap(s.vertexStart, s.vertexCount)) return { LegacyAction::Skip, 0 };
        return { LegacyAction::Remap, SectionTriangleStart(s, total) };
    }

    // One stack block, not an allocation per section or a 128 KiB maximum-section stack frame.
    // A validated triangle count makes every chunk a complete, ordered set of triangles.
    constexpr uint32_t kPickingIndexChunk = 3072;
    static_assert(kPickingIndexChunk % 3 == 0);
    constexpr uint32_t PickingChunk(uint32_t remaining)
    {
        return remaining < kPickingIndexChunk ? remaining : kPickingIndexChunk;
    }

    // The native blend always reads slot zero, then stops at the first zero later weight.
    // It does not renormalize or visit unused later slots. A single-bone fill ignores weights.
    constexpr bool PickingBonesFit(uint32_t weights, uint32_t bones, uint32_t paletteCount,
                                   bool single)
    {
        for (uint32_t slot = 0; slot < (single ? 1u : 4u); ++slot)
        {
            if (slot && ((weights >> (slot * 8)) & 0xFFu) == 0) break;
            if (((bones >> (slot * 8)) & 0xFFu) >= paletteCount) return false;
        }
        return true;
    }

    struct PickingPosition { float x, y, depth; };
    static_assert(sizeof(PickingPosition) == 12);

    // After the native affine point transform, mode adds the rotated normal (no translation).
    // These are projected coordinates and ray depth, not an ordinary world-space xyz triple.
    // The expression order follows the listings, but C++ need not keep x87 extended temporaries.
    constexpr PickingPosition ProjectPickingPosition(const float* point, const float* normal,
                                                     const float* matrix, int mode,
                                                     const float* projection, float distance)
    {
        float p[3] = { point[0], point[1], point[2] };
        if (mode)
            for (uint32_t axis = 0; axis < 3; ++axis)
                p[axis] += (normal[2] * matrix[8 + axis] + normal[1] * matrix[4 + axis])
                           + normal[0] * matrix[axis];
        const float depth = ((projection[2] * p[2] + projection[1] * p[1])
                             + projection[0] * p[0]) - distance;
        return { p[0] - projection[0] * depth, p[1] - projection[1] * depth, depth };
    }

    // Keep the native allocation offset; only add this section's vertex displacement.
    // On failure leave the caller's output untouched.
    constexpr bool StreamOffset(uint32_t incoming, uint32_t first, uint32_t stride,
                                uint32_t& result)
    {
        const uint64_t offset = uint64_t(incoming) + uint64_t(first) * stride;
        if (!stride || offset > UINT32_MAX) return false;
        result = static_cast<uint32_t>(offset);
        return true;
    }
}
