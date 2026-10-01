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

    /**
     * @brief An armour paint over a transparent sheet, made exact by painting it twice.
     *
     * The native blits blend a semi-transparent armour pixel with whatever colour lies beneath and
     * report the result opaque, so over a transparent skin the hidden skin colour leaks into the
     * armour. Painting once over black and once over white separates the two: for a pixel of
     * coverage a and colour A, black gives a*A and white gives a*A + (1 - a) * 255, so their
     * difference is the transparency and the black result divided by a is the colour. A pixel that
     * already held coverage starts from its own colour premultiplied, so successive paints combine
     * as the client would; one the paint never touched comes back as it was. Opaque pixels are
     * painted once, over themselves, exactly as natively.
     */
    struct TwoPass
    {
        std::vector<uint32_t*> pixels;  ///< every pixel of the region
        std::vector<uint32_t> values;   ///< their values before the paint
        std::vector<size_t> clear;      ///< indices of the non-opaque ones
        std::vector<uint32_t> black;    ///< their results over black

        void Take(void* const* levels, uint32_t resolution, const Rect& region)
        {
            pixels.clear();
            values.clear();
            clear.clear();
            ForEachPixel(levels, resolution, region, [this](uint32_t* pixel)
            {
                if ((*pixel & kAlphaMask) != kAlphaMask)
                    clear.push_back(pixels.size());
                pixels.push_back(pixel);
                values.push_back(*pixel);
            });
        }

        /// False when the region is fully opaque: one native paint is already exact.
        bool Needed() const { return !clear.empty(); }

        /// Before the first paint: each non-opaque pixel as its colour over black.
        void OverBlack() const
        {
            for (size_t i : clear)
                *pixels[i] = Over(values[i], 0);
        }

        /// After the first paint: keep those results, put the region back, and lay them over white.
        void OverWhite()
        {
            black.resize(clear.size());
            for (size_t k = 0; k < clear.size(); ++k)
                black[k] = *pixels[clear[k]];
            for (size_t i = 0; i < pixels.size(); ++i)
                *pixels[i] = values[i];
            for (size_t i : clear)
                *pixels[i] = Over(values[i], 255);
        }

        /// After the second paint: coverage from the difference, colour from the black result.
        void Finish() const
        {
            for (size_t k = 0; k < clear.size(); ++k)
            {
                const uint32_t over_black = black[k], over_white = *pixels[clear[k]];
                int through = 0;
                for (int shift = 0; shift < 24; shift += 8)
                    through += static_cast<int>((over_white >> shift) & 0xFF) - static_cast<int>((over_black >> shift) & 0xFF);
                const int coverage = 255 - Clamp((through + 1) / 3);
                if (coverage <= 0)
                {
                    *pixels[clear[k]] = values[clear[k]] & kColourMask;  // untouched and see-through
                    continue;
                }
                uint32_t colour = 0;
                for (int shift = 0; shift < 24; shift += 8)
                {
                    const int premultiplied = static_cast<int>((over_black >> shift) & 0xFF);
                    colour |= static_cast<uint32_t>(Clamp((premultiplied * 255 + coverage / 2) / coverage)) << shift;
                }
                *pixels[clear[k]] = (static_cast<uint32_t>(coverage) << 24) | colour;
            }
        }

    private:
        static int Clamp(int value) { return value < 0 ? 0 : value > 255 ? 255 : value; }

        /// A pixel of some coverage, laid over a flat grey level, as an opaque pixel.
        static uint32_t Over(uint32_t pixel, int background)
        {
            const int alpha = static_cast<int>(pixel >> 24);
            uint32_t out = kAlphaMask;
            for (int shift = 0; shift < 24; shift += 8)
            {
                const int channel = static_cast<int>((pixel >> shift) & 0xFF);
                out |= static_cast<uint32_t>(Clamp((channel * alpha + background * (255 - alpha) + 127) / 255)) << shift;
            }
            return out;
        }
    };
}
