// Client-independent check of the skin-alpha sheet arithmetic; not deployed or part of the DLL.
#include "client/CharModel/SkinAlpha.hpp"

#include <cstdio>
#include <vector>

namespace sa = wxl::client::skinalpha;

namespace
{
    int failures = 0;
    void Check(bool ok, const char* what)
    {
        if (!ok)
        {
            std::printf("FAIL: %s\n", what);
            ++failures;
        }
    }

    /// A tiny mipped ARGB sheet: level n is (edge >> n) square.
    struct Sheet
    {
        uint32_t edge;
        std::vector<std::vector<uint32_t>> storage;
        std::vector<void*> levels;

        Sheet(uint32_t e, uint32_t fill) : edge(e)
        {
            for (uint32_t l = 0; (e >> l) > 0; ++l)
                storage.emplace_back(static_cast<size_t>(e >> l) * (e >> l), fill);
            for (auto& level : storage)
                levels.push_back(level.data());
        }
        uint32_t& At(uint32_t level, uint32_t x, uint32_t y) { return storage[level][y * (edge >> level) + x]; }
    };
}

int main()
{
    const sa::Rect region{4, 0, 4, 4};

    // A transparent skin: its region loses alpha at every level, nothing outside it does.
    Sheet sheet(8, 0xFF112233u);
    sa::ClearAlpha(sheet.levels.data(), sheet.edge, region);
    Check(sheet.At(0, 5, 1) == 0x00112233u, "skin region keeps colour at alpha zero");
    Check(sheet.At(0, 1, 1) == 0xFF112233u, "outside the region stays opaque");
    Check(sheet.At(1, 2, 0) == 0x00112233u, "the region shrinks with each level");
    Check(sheet.At(1, 1, 0) == 0xFF112233u, "level 1 outside the region stays opaque");

    // An armour paint over it: painted pixels become opaque, untouched ones keep alpha zero.
    sa::Snapshot snapshot;
    snapshot.Take(sheet.levels.data(), sheet.edge, region);
    Check(!snapshot.Empty(), "a transparent region is snapshotted");
    for (uint32_t y = 0; y < 4; ++y)
        for (uint32_t x = 4; x < 8; ++x)
            sheet.At(0, x, y) = (x == 5 && y == 1) ? 0xFFAA0000u : (sheet.At(0, x, y) | sa::kAlphaMask);
    snapshot.Restore();
    Check(sheet.At(0, 5, 1) == 0xFFAA0000u, "a painted pixel is opaque");
    Check(sheet.At(0, 6, 2) == 0x00112233u, "an untouched pixel is transparent again");

    // A stock character: the region is opaque, so there is nothing to keep.
    Sheet stock(8, 0xFF445566u);
    snapshot.Take(stock.levels.data(), stock.edge, region);
    Check(snapshot.Empty(), "an opaque region needs no snapshot");

    // A region beyond the sheet is never touched.
    Sheet small(4, 0xFF000000u);
    sa::ClearAlpha(small.levels.data(), small.edge, sa::Rect{2, 2, 4, 4});
    Check(small.At(0, 3, 3) == 0xFF000000u, "an out-of-bounds region is left alone");

    if (failures)
        return 1;
    std::printf("skin-alpha: all checks passed\n");
    return 0;
}
