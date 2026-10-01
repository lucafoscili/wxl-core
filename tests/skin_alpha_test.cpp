// Client-independent check of the skin-alpha sheet arithmetic; not deployed or part of the DLL.
#include "client/CharModel/DonorHair.hpp"
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

    // Exact armour over a transparent sheet: the native blend (>> 8, alpha stated 255) run twice.
    {
        Sheet exact(8, 0x00C8A080u);  // the hidden skin colour, see-through
        const uint32_t armour = 0x00204060u;
        auto paint = [&](uint32_t coverage) {  // one native armour blit over the region at level 0
            for (uint32_t y = 0; y < 4; ++y)
                for (uint32_t x = 4; x < 8; ++x)
                {
                    if (x == 7)
                        continue;  // a column the armour does not cover
                    uint32_t& dst = exact.At(0, x, y), out = 0xFF000000u;
                    for (int s = 0; s < 24; s += 8)
                    {
                        const uint32_t a = (armour >> s) & 0xFF, b = (dst >> s) & 0xFF;
                        out |= ((a * coverage + b * (255 - coverage)) >> 8) << s;
                    }
                    dst = out;
                }
        };
        sa::TwoPass pass;
        pass.Take(exact.levels.data(), exact.edge, region);
        Check(pass.Needed(), "a transparent region needs the exact paint");
        pass.OverBlack(); paint(128); pass.OverWhite(); paint(128); pass.Finish();
        const uint32_t half = exact.At(0, 5, 1);
        Check((half >> 24) > 120 && (half >> 24) < 136, "half coverage stays half");
        Check(((half >> 16) & 0xFF) < 0x28 && ((half >> 8) & 0xFF) > 0x38 && (half & 0xFF) > 0x58,
              "its colour is the armour's, not mixed with the hidden skin");
        Check(exact.At(0, 7, 1) == 0x00C8A080u, "an unpainted pixel stays see-through and unchanged");

        Sheet full(8, 0x00C8A080u);
        sa::TwoPass solid;
        solid.Take(full.levels.data(), full.edge, region);
        auto opaque = [&]() { for (uint32_t x = 4; x < 8; ++x) full.At(0, x, 0) = 0xFF000000u | armour; };
        solid.OverBlack(); opaque(); solid.OverWhite(); opaque(); solid.Finish();
        Check(full.At(0, 5, 0) == (0xFF000000u | armour), "solid armour is solid and its own colour");
    }

    // Donor hair: a helm's race mask decides, and every vertex-window level is covered.
    {
        namespace dh = wxl::client::donorhair;
        Check(dh::HidesHair(1u << 1, 1), "a helm hiding human hair hides it for a human");
        Check(!dh::HidesHair(1u << 4, 1), "a helm hiding only night elf hair leaves a human's");
        Check(!dh::HidesHair(0xFFFFFFFFu, 40), "an out-of-range race is never hidden");
        Check(dh::VolumeId(0) == 9001 && dh::VolumeId(1) == 65536 + 9001, "the volume id per level");
        Check(dh::kHairVolumeGeoset > 2000, "the volume sits above the client's blanket-hidden range");
    }

    // Only custom bodies get an uncompressed sheet.
    Check(!sa::IsCustomBody("Character\\Human\\Female\\HumanFemale"), "a stock race model is stock");
    Check(!sa::IsCustomBody("CHARACTER\\Velora\\Personal\\umbra\\VeloraStock_x"), "a stock-derived model is stock");
    Check(sa::IsCustomBody("Creature\\Ayane\\Ayane"), "a donor model is custom");
    Check(!sa::IsCustomBody(""), "no model is not custom");
    Check(!sa::IsCustomBody(nullptr), "a missing model is not custom");

    if (failures)
        return 1;
    std::printf("skin-alpha: all checks passed\n");
    return 0;
}
