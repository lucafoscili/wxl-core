// Arithmetic shared by dense vertex-window upload and draw.
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
