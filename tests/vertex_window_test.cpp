// Actual production arithmetic, independent of the WoW process or GPU.
// Copyright (C) 2026 WarcraftXL. GPL-3.0-or-later.
#include "client/CM2Shared/VertexWindow.hpp"

#include <cstdio>
#include <initializer_list>

namespace window = wxl::client::m2::window;

static_assert(window::LocalIndex(678, 678) == 0);
static_assert(window::LocalIndex(0, 65533) == 3);
static_assert(window::Fits(65536, 0, 3, 65539));
static_assert(!window::Fits(65536, 1, 3, 65539));
static_assert(!window::Fits(65536, 0, 4, 65539));
static_assert(!window::Fits(65540, 4, 1, 65539));
static_assert(!window::Fits(0, 0, 0, 65539));
static_assert(window::CrossesWrap(65466, 1089));  // Kasumi section 59.
static_assert(window::CrossesWrap(64610, 1572));  // Shadowheart V3 section 12.
static_assert(!window::CrossesWrap(646, 2418));   // Shadowheart V3 section 13, wholly above 65536.
static_assert(!window::CrossesWrap(65466, 70));   // Last vertex 65535: no wrap.
static_assert(window::CrossesWrap(65466, 71));
static_assert(!window::CrossesWrap(1, 65535));
static_assert(window::CrossesWrap(2, 65535));
static_assert(!window::CrossesWrap(0, 0));

// Picking checks use the same production helpers, including sections above later wraps.
static_assert(window::LocalIndex(0, 65466) == 70);
static_assert(window::LocalIndex(65530, 65466) == 64);
static_assert(window::LocalIndex(65535, 65466) == 69);
static_assert(window::NeedsPickingPositions(65466, 1089));
static_assert(window::NeedsPickingPositions(66182, 2418));
static_assert(window::NeedsPickingPositions(131000, 1000));
static_assert(!window::NeedsPickingPositions(65466, 70));
static_assert(!window::NeedsPickingPositions(65536, 0));
static_assert(window::NeedsPickingPositions(UINT32_MAX, 1));
static_assert(window::TriangleRange(310386, 6144, 400926));
static_assert(!window::TriangleRange(310386, 6143, 400926));
static_assert(window::TriangleRange(UINT32_MAX, 0, UINT32_MAX));
static_assert(!window::TriangleRange(UINT32_MAX - 2, 3, UINT32_MAX));
static_assert(!window::TriangleRange(10, 3, 9));
static_assert(window::PickingChunk(0) == 0);
static_assert(window::PickingChunk(3075) == 3072);
static_assert(window::PickingChunk(3) == 3);
static_assert(window::PickingBonesFit(0x00007F80, 0xFFFF0100, 2, false));
static_assert(window::PickingBonesFit(0x00FF00FF, 0x00FFFF00, 1, false)); // first zero stops
static_assert(!window::PickingBonesFit(0, 1, 1, false)); // slot zero is still read
static_assert(window::PickingBonesFit(0, 0xFFFFFF00, 1, true));
static_assert(!window::PickingBonesFit(255, 2, 2, true));

constexpr float identity[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
constexpr float point[3] = { 4,8,3 }, normal[3] = { 1,0,0 };
constexpr float ray[3] = { 0,0,1 }, tilted[3] = { 0.5f,0.25f,1 };
constexpr float rotated[16] = { 0,1,0,0, -1,0,0,0, 0,0,1,0, 100,200,300,1 };
constexpr auto flat = window::ProjectPickingPosition(point, normal, identity, 0, ray, 1);
static_assert(flat.x == 4 && flat.y == 8 && flat.depth == 2);
constexpr auto offset = window::ProjectPickingPosition(point, normal, rotated, 1, ray, 1);
static_assert(offset.x == 4 && offset.y == 9 && offset.depth == 2); // no second translation
constexpr auto projected = window::ProjectPickingPosition(point, normal, identity, 0, tilted, 2);
static_assert(projected.x == 1.5f && projected.y == 6.75f && projected.depth == 5);

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
        // The client's picking test computes index - start as a signed value; it goes negative
        // for some vertex of the section exactly when CrossesWrap says so.
        bool negative = false;
        for (uint32_t k = 0; k < c.count; ++k)
            negative |= int(static_cast<uint16_t>(c.first + k)) - int(low) < 0;
        if (negative != window::CrossesWrap(low, c.count)) return 9;
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
    uint32_t slot = 42;
    if (!window::ArraySlot(0x1030, 0x1000, 2, 48, slot) || slot != 1) return 10;
    for (uintptr_t bad : {uintptr_t(0x0FFF), uintptr_t(0x1001), uintptr_t(0x1060)})
        if (window::ArraySlot(bad, 0x1000, 2, 48, slot) || slot != 1) return 11;
    if (window::ArraySlot(0x1000, 0x1000, 2, 0, slot) || slot != 1) return 12;
    if (window::ArraySlot(0, 0, 2, 48, slot) || slot != 1) return 13;
    if (!window::ArraySlot(UINTPTR_MAX - 47, UINTPTR_MAX - 95, 2, 48, slot) || slot != 1)
        return 14;

    // Every representable complete section count partitions without dropping or splitting a
    // triangle. Poison values outside the chunk are never part of a submitted range.
    unsigned partitions = 0;
    for (uint32_t count = 0; count <= 65535; count += 3)
    {
        uint32_t done = 0;
        while (done < count)
        {
            const uint32_t take = window::PickingChunk(count - done);
            if (!take || take > window::kPickingIndexChunk || take % 3 || take > count - done)
                return 15;
            done += take;
        }
        if (done != count) return 16;
        ++partitions;
    }
    // All possible stored starts, with local indices on both sides of the wrap and at the
    // largest legal local slot. This supplements the full-window walks above.
    unsigned rebases = 0;
    for (uint32_t low = 0; low <= 65535; ++low)
        for (uint32_t local : {0u, 1u, 69u, 70u, 32768u, 65534u})
        {
            if (window::LocalIndex(static_cast<uint16_t>(low + local),
                                   static_cast<uint16_t>(low)) != local) return 17;
            ++rebases;
        }
    std::printf("Vertex window arithmetic passed: %u vertex identities, boundary and offset cases; "
                "%u picking partitions, %u additional rebases, address/bone/projection cases.\n",
                checked, partitions, rebases);
    return 0;
}
