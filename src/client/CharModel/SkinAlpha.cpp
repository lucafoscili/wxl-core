// Character skin alpha: keep the composited sheet transparent where a skin authored with alpha shows.
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

#include "config.hpp"
#include "client/CharModel/SkinAlpha.hpp"
#include "common/Config.hpp"
#include "common/Log.hpp"
#include "engine/hook/Hook.hpp"
#include "engine/hook/Registry.hpp"
#include "offsets/game/M2.hpp"

#include <cstdint>

namespace
{
    namespace m2 = wxl::offsets::game::m2;
    namespace sa = wxl::client::skinalpha;

    /// Both region painters: __cdecl (regionIndex, source texture entry, sheet level pointers).
    using PaintRegionFn = void(__cdecl*)(uint32_t regionIndex, void* source, void** levels);

    PaintRegionFn g_origPaintRegion = nullptr;
    PaintRegionFn g_origPaintFromOrigin = nullptr;

    uint32_t SheetResolution()
    {
        return *reinterpret_cast<const uint32_t*>(m2::kCharSheetResolution);
    }

    bool Region(uint32_t index, sa::Rect& out)
    {
        if (index >= m2::kCharRegionCount)
            return false;
        out = *reinterpret_cast<const sa::Rect*>(m2::kCharRegionRects + index * m2::kCharRegionRectStride);
        return true;
    }

    /**
     * @brief The sheet-covering painter, which paints the base skin (and face and scalp) opaque.
     *
     * A skin authored with alpha is the request for a transparent sheet: after the native paint,
     * its region keeps the skin's colour (so armour edges blend against it) at alpha zero.
     */
    void __cdecl hkPaintRegion(uint32_t regionIndex, void* source, void** levels)
    {
        g_origPaintRegion(regionIndex, source, levels);
        sa::Rect region;
        if (!source || !levels || !Region(regionIndex, region))
            return;
        if (*(static_cast<const uint8_t*>(source) + m2::kOffTexEntryAlphaBits) == 0)
            return;  // Every stock skin: nothing to do.
        sa::ClearAlpha(levels, SheetResolution(), region);
    }

    /**
     * @brief The region-authored painter, which paints armour and other overlays.
     *
     * The native blits write alpha 255 over their whole rectangle. Pixels whose colour the paint
     * left unchanged were not painted, so they get their alpha back.
     */
    void __cdecl hkPaintFromOrigin(uint32_t regionIndex, void* source, void** levels)
    {
        thread_local sa::Snapshot snapshot;
        sa::Rect region;
        if (!levels || !Region(regionIndex, region))
        {
            g_origPaintFromOrigin(regionIndex, source, levels);
            return;
        }
        snapshot.Take(levels, SheetResolution(), region);
        g_origPaintFromOrigin(regionIndex, source, levels);
        if (!snapshot.Empty())
            snapshot.Restore();
    }

    bool InstallSkinAlpha()
    {
        if (!wxl::config::Env("WXL_SKIN_ALPHA", true))
        {
            WLOG_INFO("skin-alpha: disabled by WXL_SKIN_ALPHA=0");
            return true;
        }
        wxl::hook::Install("CharPaintRegion", m2::kCharPaintRegion, &hkPaintRegion, &g_origPaintRegion);
        wxl::hook::Install("CharPaintRegionFromOrigin", m2::kCharPaintRegionFromOrigin,
                           &hkPaintFromOrigin, &g_origPaintFromOrigin);
        return true;
    }
}

WXL_REGISTER_FEATURE("skin-alpha", true, InstallSkinAlpha)
