// Character skin alpha: the composite-sheet arithmetic, free of client addresses so it can be tested.
// Copyright (C) 2026 WarcraftXL
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <cctype>
#include <cstdint>
#include <cstring>
#include <vector>

/**
 * The stock composition writes every sheet pixel opaque: the skin blit forces it, and the armour
 * blits state the destination's alpha as 255 whatever they blend. A model that draws the sheet
 * as an overlay (a custom body wearing its own full-resolution skin underneath) needs the sheet
 * transparent wherever no armour landed. These helpers keep that alpha:
 *
 *  - on a custom body's sheet, or from a skin authored WITH alpha, the skin leaves its
 *    region transparent;
 *  - an armour paint keeps the alpha of every pixel whose colour it did not change, so only
 *    the pixels it actually painted become opaque.
 *
 * Stock bodies draw the sheet with opaque materials, which never read its alpha, so neither rule
 * can change how a stock character looks.
 */
namespace wxl::client::skinalpha
{
    /**
     * @brief Whether a character model is a custom body rather than a stock race model.
     *
     * Stock race and stock-derived models live under Character\. A custom body (a donor) is
     * the only kind that draws the sheet as an overlay on its own skin, so only its sheet needs to
     * keep alpha; the stock ones keep their compressed sheet.
     */
    inline bool IsCustomBody(const char* pathStem)
    {
        static constexpr char kStock[] = "character\\";
        if (!pathStem || !*pathStem)
            return false;
        for (size_t i = 0; i + 1 < sizeof(kStock); ++i)
            if (std::tolower(static_cast<unsigned char>(pathStem[i])) != kStock[i])
                return true;
        return false;
    }

    /// One composition region on the sheet at level 0: {x, y, width, height}.
    struct Rect
    {
        int32_t x, y, w, h;
    };

    /// The ARGB8888 alpha byte (pixels are 0xAARRGGBB words).
    constexpr uint32_t kAlphaMask = 0xFF000000u;
    constexpr uint32_t kColourMask = 0x00FFFFFFu;

    /**
     * @brief Visits every pixel of one region across the sheet's mip levels.
     *
     * Level n of a sheet whose edge is `resolution` is (resolution >> n) pixels square, rows packed;
     * the region scales with it and the walk stops at the first level where it vanishes.
     * @param levels      the sheet's level pointers, as the painters receive them.
     * @param resolution  the sheet edge at level 0.
     * @param region      the region at level 0.
     * @param visit       called with each pixel's address.
     */
    template <class Visit>
    void ForEachPixel(void* const* levels, uint32_t resolution, const Rect& region, Visit&& visit)
    {
        if (!levels || !resolution || region.w <= 0 || region.h <= 0 || region.x < 0 || region.y < 0)
            return;
        for (uint32_t level = 0; (resolution >> level) > 0; ++level)
        {
            const uint32_t edge = resolution >> level;
            const int32_t x = region.x >> level, y = region.y >> level;
            const int32_t w = region.w >> level, h = region.h >> level;
            if (w <= 0 || h <= 0 || !levels[level])
                break;
            if (static_cast<uint32_t>(x + w) > edge || static_cast<uint32_t>(y + h) > edge)
                break;  // A region outside the sheet is never touched.
            auto* rows = static_cast<uint32_t*>(levels[level]);
            for (int32_t row = 0; row < h; ++row)
            {
                uint32_t* pixel = rows + static_cast<size_t>(y + row) * edge + x;
                for (int32_t column = 0; column < w; ++column)
                    visit(pixel + column);
            }
        }
    }

    /// Makes a region fully transparent, keeping its colour for blending at armour edges.
    inline void ClearAlpha(void* const* levels, uint32_t resolution, const Rect& region)
    {
        ForEachPixel(levels, resolution, region, [](uint32_t* pixel) { *pixel &= kColourMask; });
    }

    /// The region's non-opaque pixels before an armour paint, to give back after it.
    struct Snapshot
    {
        std::vector<uint32_t*> pixels;
        std::vector<uint32_t> values;

        void Take(void* const* levels, uint32_t resolution, const Rect& region)
        {
            pixels.clear();
            values.clear();
            ForEachPixel(levels, resolution, region, [this](uint32_t* pixel)
            {
                if ((*pixel & kAlphaMask) != kAlphaMask)
                {
                    pixels.push_back(pixel);
                    values.push_back(*pixel);
                }
            });
        }

        /// True when the region was fully opaque, as on every stock character: nothing to keep.
        bool Empty() const { return pixels.empty(); }

        /// After the paint: a pixel whose colour is unchanged was not painted and keeps its alpha.
        void Restore() const
        {
            for (size_t i = 0; i < pixels.size(); ++i)
                if ((*pixels[i] & kColourMask) == (values[i] & kColourMask))
                    *pixels[i] = values[i];
        }
    };
}
