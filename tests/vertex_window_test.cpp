// Actual production arithmetic, independent of the WoW process or GPU.
// Copyright (C) 2026 WarcraftXL. GPL-3.0-or-later.
#include "client/CM2Shared/VertexWindow.hpp"

#include <cstdio>

namespace window = wxl::client::m2::window;

static_assert(window::LocalIndex(678, 678) == 0);
static_assert(window::LocalIndex(0, 65533) == 3);
static_assert(window::Fits(65536, 0, 3, 65539));
static_assert(!window::Fits(65536, 1, 3, 65539));
static_assert(!window::Fits(65536, 0, 4, 65539));
static_assert(!window::Fits(65540, 4, 1, 65539));
static_assert(!window::Fits(0, 0, 0, 65539));

int main()
{
    struct Case { uint32_t first; uint16_t count; uint32_t total; };
    const Case cases[] = {
        {0, 678, 65539}, {678, 659, 65539},
        {65533, 6, 65539}, // A section whose interior crosses the boundary.
        {65536, 3, 65539}, {65540, 65535, 131075},
    };
    unsigned checked = 0;
    for (const auto& c : cases)
    {
        const auto low = static_cast<uint16_t>(c.first);
        if (!window::Fits(c.first, low, c.count, c.total)) return 1;
        for (uint32_t k = 0; k < c.count; ++k)
        {
            const auto raw = static_cast<uint16_t>(c.first + k);
            const auto local = window::LocalIndex(raw, low);
            if (local != k || local >= c.count || c.first + local >= c.total) return 2;
            ++checked;
        }
        // A first index beyond this section must not be accepted as a local vertex.
        if (window::LocalIndex(static_cast<uint16_t>(c.first + c.count), low) < c.count)
            return 3;
    }

    uint32_t offset = 123;
    if (!window::StreamOffset(480, 65536, 48, offset) || offset != 3146208) return 4;
    if (!window::StreamOffset(480, 678, 48, offset) || offset != 33024) return 5;
    const uint32_t previous = offset;
    if (window::StreamOffset(UINT32_MAX - 47, 1, 48, offset) || offset != previous)
        return 6;
    if (window::StreamOffset(0, 1, 0, offset) || offset != previous) return 7;
    if (!window::StreamOffset(UINT32_MAX, 0, 48, offset) || offset != UINT32_MAX)
        return 8;
    std::printf("Vertex window arithmetic passed: %u vertex identities, boundary and offset cases.\n", checked);
    return 0;
}
