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

// Triangle-start fold: level is a high half only while the widened range fits the array.
static_assert(window::SectionTriangleStart({ 1, 0, 3, 0, 3 }, 65539) == 65536);
static_assert(window::SectionTriangleStart({ 1, 0, 3, 1, 3 }, 65539) == 1); // would pass the end
static_assert(window::SectionTriangleStart({ 0xFFFF, 0, 3, 0xFFFF, 3 }, UINT32_MAX) == 0xFFFF);
static_assert(window::SectionTriangleStart({ 0, 0, 3, 700, 3 }, 65539) == 700);

// Legacy frames. VirnaAoA00.skin (sha256 28afd6b3e1cb..., display 100010): 27,731 vertices and
// 133,929 indices, 44,643 triangles -- the model of the 2026-09-07 crash at 0x0081D569. The index
// fills note it, but no conversion certificate admits a skin of at most 65,536 vertices, so it
// picks through a legacy frame. Sections 2 and 3 write wide triangle starts (level 1).
constexpr uint32_t kVirnaIndices = 133929;
constexpr window::SectionPlacement kVirna[] = {
    { 0, 0, 10451, 0, 57123 }, { 0, 10451, 1935, 57123, 8913 },
    { 1, 12386, 13533, 500, 60000 }, { 1, 25919, 1812, 60500, 7893 },
};
// Kasumi (83,182 vertices, 400,926 indices) when its admission fails: section 59 crosses 65,536
// (stored start 65,466, 1,089 vertices); section 60 lies wholly above (stored start 1,019).
constexpr uint32_t kKasumiIndices = 400926;
constexpr window::SectionPlacement kKasumi59 = { 4, 65466, 1089, 48242, 6144 };
constexpr window::SectionPlacement kKasumi60 = { 4, 1019, 380, 54386, 1860 };

// The folded starts tile Virna's triangle array end to end, so the remap targets each section's
// own triangles, while the stock 16-bit range of sections 2 and 3 ends before their own triangles
// begin: it reads earlier sections' triangles, whose vertices lie below the section's start (the
// negative scratch slots).
constexpr bool TilesVirna()
{
    uint32_t next = 0;
    for (const auto& s : kVirna)
    {
        if (window::SectionTriangleStart(s, kVirnaIndices) != next) return false;
        next += s.indexCount;
    }
    return next == kVirnaIndices;
}
static_assert(TilesVirna());
static_assert(window::SectionTriangleStart(kVirna[2], kVirnaIndices) == 66036);
static_assert(window::SectionTriangleStart(kVirna[3], kVirnaIndices) == 126036);
static_assert(kVirna[2].indexStart + kVirna[2].indexCount <= 66036);
static_assert(kVirna[3].indexStart + kVirna[3].indexCount <= 126036);
static_assert(window::SectionTriangleStart(kKasumi59, kKasumiIndices) == 310386);
static_assert(window::SectionTriangleStart(kKasumi60, kKasumiIndices) == 316530);

// The stock call the native geometry routine makes for a section, at an arbitrary array address.
constexpr uintptr_t kIndices = 0x10000000u;
constexpr uintptr_t StockBegin(const window::SectionPlacement& s)
{
    return kIndices + uintptr_t(s.indexStart) * sizeof(uint16_t);
}
constexpr uintptr_t StockEnd(const window::SectionPlacement& s)
{
    return StockBegin(s) + uintptr_t(s.indexCount) * sizeof(uint16_t);
}
constexpr window::LegacyTriangle Plan(const window::SectionPlacement& s, uint32_t total,
                                      uintptr_t begin, uintptr_t end, int vertexBase)
{
    return window::PlanLegacyTriangle(kIndices, total, s, begin, end, vertexBase);
}
constexpr window::LegacyTriangle StockPlan(const window::SectionPlacement& s, uint32_t total)
{
    return Plan(s, total, StockBegin(s), StockEnd(s), s.vertexStart);
}
constexpr bool Is(window::LegacyTriangle plan, window::LegacyAction action, uint32_t start = 0)
{
    return plan.action == action && plan.triangleStart == start;
}
using window::LegacyAction;
static_assert(Is(window::LegacyTriangle{}, LegacyAction::Forward)); // an unarmed frame forwards
// The exact stock call is remapped to the section's own triangles (66a64d5) ...
static_assert(Is(StockPlan(kVirna[2], kVirnaIndices), LegacyAction::Remap, 66036));
static_assert(Is(StockPlan(kVirna[3], kVirnaIndices), LegacyAction::Remap, 126036));
static_assert(Is(StockPlan(kVirna[1], kVirnaIndices), LegacyAction::Remap, 57123)); // same range
static_assert(Is(StockPlan(kKasumi60, kKasumiIndices), LegacyAction::Remap, 316530));
// ... except a crossing section, which keeps the stock no-hit result (e9c68f6).
static_assert(Is(StockPlan(kKasumi59, kKasumiIndices), LegacyAction::Skip));
// Any call that is not the recorded section's exact stock call goes on unchanged.
constexpr const window::SectionPlacement& kWide = kVirna[2];
static_assert(Is(Plan(kWide, kVirnaIndices, StockBegin(kWide), StockEnd(kWide), 0),
                 LegacyAction::Forward));
static_assert(Is(Plan(kWide, kVirnaIndices, StockBegin(kWide), StockEnd(kWide), 12386 + 65536),
                 LegacyAction::Forward));
static_assert(Is(Plan(kWide, kVirnaIndices, kIndices + 66036 * 2, kIndices + 126036 * 2, 12386),
                 LegacyAction::Forward)); // already at the widened start
static_assert(Is(Plan(kWide, kVirnaIndices, StockBegin(kWide), StockEnd(kVirna[3]), 12386),
                 LegacyAction::Forward)); // the same begin with another section's end
static_assert(Is(Plan(kVirna[3], kVirnaIndices, StockBegin(kWide), StockEnd(kWide), 12386),
                 LegacyAction::Forward)); // another section's call
static_assert(Is(Plan(kVirna[0], kVirnaIndices, kIndices - 2, StockEnd(kVirna[0]) - 2, 0),
                 LegacyAction::Forward)); // before the array
static_assert(Is(Plan(kKasumi59, kKasumiIndices, StockBegin(kKasumi59), StockEnd(kKasumi59), 0),
                 LegacyAction::Forward)); // a crossing section is skipped only for its own call

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

    uint32_t streamOffset = 123;
    if (!window::StreamOffset(480, 65536, 48, streamOffset) || streamOffset != 3146208) return 4;
    if (!window::StreamOffset(480, 678, 48, streamOffset) || streamOffset != 33024) return 5;
    const uint32_t previous = streamOffset;
    if (window::StreamOffset(UINT32_MAX - 47, 1, 48, streamOffset) || streamOffset != previous)
        return 6;
    if (window::StreamOffset(0, 1, 0, streamOffset) || streamOffset != previous) return 7;
    if (!window::StreamOffset(UINT32_MAX, 0, 48, streamOffset) || streamOffset != UINT32_MAX)
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
    // Every single-argument change to a legacy section's stock call, and every other section's
    // call, leaves it unchanged; only its own exact call is skipped or remapped.
    unsigned plans = 0;
    const window::SectionPlacement fixtures[] = { kVirna[0], kVirna[1], kVirna[2], kVirna[3],
                                                  kKasumi59, kKasumi60 };
    const uint32_t totals[] = { kVirnaIndices, kVirnaIndices, kVirnaIndices, kVirnaIndices,
                                kKasumiIndices, kKasumiIndices };
    for (size_t i = 0; i < 6; ++i)
    {
        const window::SectionPlacement& s = fixtures[i];
        const uint32_t total = totals[i];
        const uintptr_t begin = StockBegin(s), end = StockEnd(s);
        const int base = s.vertexStart;
        const bool crossing = window::CrossesWrap(s.vertexStart, s.vertexCount);
        const window::LegacyTriangle exact = Plan(s, total, begin, end, base);
        if (crossing ? !Is(exact, LegacyAction::Skip)
                     : !Is(exact, LegacyAction::Remap, window::SectionTriangleStart(s, total)))
            return 18;
        const window::LegacyTriangle changed[] = {
            Plan(s, total, begin + 2, end + 2, base), Plan(s, total, begin + 1, end + 1, base),
            Plan(s, total, begin, end + 2, base), Plan(s, total, begin, end - 2, base),
            Plan(s, total, begin, begin, base), Plan(s, total, end, begin, base),
            Plan(s, total, begin, end, base + 1), Plan(s, total, begin, end, base - 1),
            Plan(s, total, begin, end, base + 65536), Plan(s, total, begin, end, base - 65536),
        };
        for (const window::LegacyTriangle& plan : changed)
        {
            if (!Is(plan, LegacyAction::Forward)) return 19;
            ++plans;
        }
        for (size_t j = 0; j < 6; ++j)
        {
            if (j == i || totals[j] != total) continue;
            if (!Is(Plan(s, total, StockBegin(fixtures[j]), StockEnd(fixtures[j]),
                         fixtures[j].vertexStart), LegacyAction::Forward)) return 20;
            ++plans;
        }
    }
    std::printf("Vertex window arithmetic passed: %u vertex identities, boundary and offset cases; "
                "%u picking partitions, %u additional rebases, %u legacy-frame call plans, "
                "address/bone/projection cases.\n",
                checked, partitions, rebases, plans);
    return 0;
}
