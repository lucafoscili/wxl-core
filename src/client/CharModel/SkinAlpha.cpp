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
#include "client/CharModel/CustomBody.hpp"
#include "client/CharModel/SkinAlpha.hpp"
#include "common/Config.hpp"
#include "common/Log.hpp"
#include "engine/hook/Hook.hpp"
#include "engine/hook/Registry.hpp"
#include "game/M2.hpp"
#include "offsets/game/M2.hpp"

#include <cstdint>
#include <cstring>

namespace
{
    namespace m2 = wxl::offsets::game::m2;
    namespace sa = wxl::client::skinalpha;

    /// Both region painters: __cdecl (regionIndex, source texture entry, sheet level pointers).
    using PaintRegionFn = void(__cdecl*)(uint32_t regionIndex, void* source, void** levels);

    /// Creates a component's sheet texture: __thiscall, no stack args.
    using CreateBaseTextureFn = void(__fastcall*)(void* component, void* edx);
    /// Binds a component to its model and setup data: __thiscall, 2 stack args, returns success.
    using CharInitFn = bool(__fastcall*)(void* component, void* edx, void* setup, uint32_t flags);
    /// The section walks: __thiscall, the component in ecx, no stack args.
    using SectionWalkFn = void(__fastcall*)(void* component, void* edx);
    /// The composition thread's per-request paint: __cdecl (request).
    using ComposeRequestFn = void(__cdecl*)(void* request);

    PaintRegionFn g_origPaintRegion = nullptr;
    PaintRegionFn g_origPaintFromOrigin = nullptr;
    CreateBaseTextureFn g_origCreateBaseTexture = nullptr;
    CharInitFn g_origCharInit = nullptr;
    SectionWalkFn g_origPrepSections = nullptr;
    SectionWalkFn g_origUpdateSections = nullptr;
    ComposeRequestFn g_origComposeRequest = nullptr;

    /**
     * Whether the sheet being painted on this thread is a custom body's. The painters are not handed
     * the component, so the three callers that run them mark it: the section walks from the
     * component's format, the composition thread from the request's copy of it. An uncompressed
     * format is only ever set by UncompressCustomBody below, so it identifies a custom body.
     */
    thread_local bool t_customSheet = false;

    struct CustomSheetScope
    {
        bool previous;
        explicit CustomSheetScope(uint32_t format) : previous(t_customSheet)
        { t_customSheet = format == m2::kGxTexFormatArgb8888; }
        ~CustomSheetScope() { t_customSheet = previous; }
    };

    uint32_t Field(void* object, size_t offset)
    {
        return object ? *reinterpret_cast<const uint32_t*>(static_cast<const uint8_t*>(object) + offset) : 0;
    }

    using wxl::client::custombody::ModelStem;

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
     * On a custom body's sheet, or from a skin authored with alpha, the skin contributes no
     * alpha: after the native paint its region keeps the skin's colour (so armour edges blend
     * against it) at alpha zero, and only armour makes the sheet opaque.
     */
    void __cdecl hkPaintRegion(uint32_t regionIndex, void* source, void** levels)
    {
        g_origPaintRegion(regionIndex, source, levels);
        sa::Rect region;
        if (!source || !levels || !Region(regionIndex, region))
            return;
        if (!t_customSheet && *(static_cast<const uint8_t*>(source) + m2::kOffTexEntryAlphaBits) == 0)
            return;  // Every stock character: nothing to do.
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
        thread_local sa::TwoPass exact;
        sa::Rect region;
        if (!levels || !Region(regionIndex, region))
        {
            g_origPaintFromOrigin(regionIndex, source, levels);
            return;
        }
        if (t_customSheet)
        {
            // A custom body's armour lies over its own skin: its colour and coverage must be the
            // armour's alone, not blended with the sheet's hidden skin colour (TwoPass).
            exact.Take(levels, SheetResolution(), region);
            if (!exact.Needed())
            {
                g_origPaintFromOrigin(regionIndex, source, levels);
                return;
            }
            exact.OverBlack();
            g_origPaintFromOrigin(regionIndex, source, levels);
            exact.OverWhite();
            g_origPaintFromOrigin(regionIndex, source, levels);
            exact.Finish();
            return;
        }
        snapshot.Take(levels, SheetResolution(), region);
        g_origPaintFromOrigin(regionIndex, source, levels);
        if (!snapshot.Empty())
            snapshot.Restore();
    }

    /**
     * @brief Gives a custom body an uncompressed sheet, so the composed alpha reaches the card.
     *
     * The stock sheet is DXT1 whenever componentCompress is on, which drops alpha; the component's
     * format alone decides that (kOffCharComponentFormat). Stock bodies keep it. A threaded
     * composition request copies the format when it starts, so the switch is made only while none
     * is in flight: a request already carrying DXT1 data must not feed an ARGB texture.
     */
    void UncompressCustomBody(void* component)
    {
        if (!component)
            return;
        auto* base = static_cast<uint8_t*>(component);
        auto* format = reinterpret_cast<uint32_t*>(base + m2::kOffCharComponentFormat);
        if (*format != m2::kGxTexFormatDxt1)
            return;
        if (*reinterpret_cast<void**>(base + m2::kOffCharComponentComposeRequest))
            return;
        const char* stem = ModelStem(component);
        if (!sa::IsCustomBody(stem))
            return;
        *format = m2::kGxTexFormatArgb8888;
        WLOG_INFO("skin-alpha: uncompressed sheet for %s", stem);
    }

    /// As the component attaches to its model: the earliest point its model is known.
    bool __fastcall hkCharInit(void* component, void* edx, void* setup, uint32_t flags)
    {
        const bool ok = g_origCharInit(component, edx, setup, flags);
        if (ok)
            UncompressCustomBody(component);
        return ok;
    }

    void __fastcall hkPrepSections(void* component, void* edx)
    {
        CustomSheetScope scope(Field(component, m2::kOffCharComponentFormat));
        g_origPrepSections(component, edx);
    }

    void __fastcall hkUpdateSections(void* component, void* edx)
    {
        CustomSheetScope scope(Field(component, m2::kOffCharComponentFormat));
        g_origUpdateSections(component, edx);
    }

    void __cdecl hkComposeRequest(void* request)
    {
        CustomSheetScope scope(Field(request, m2::kOffComposeRequestFormat));
        g_origComposeRequest(request);
    }

    /// Before the sheet texture is made, for a component whose model arrived after its init.
    void __fastcall hkCreateBaseTexture(void* component, void* edx)
    {
        UncompressCustomBody(component);
        g_origCreateBaseTexture(component, edx);
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
        wxl::hook::Install("CharCreateBaseTexture", m2::kCharCreateBaseTexture,
                           &hkCreateBaseTexture, &g_origCreateBaseTexture);
        wxl::hook::Install("CharInit", m2::kCharInit, &hkCharInit, &g_origCharInit);
        wxl::hook::Install("CharPrepSections", m2::kCharPrepSections, &hkPrepSections, &g_origPrepSections);
        wxl::hook::Install("CharUpdateSections", m2::kCharUpdateSections, &hkUpdateSections, &g_origUpdateSections);
        wxl::hook::Install("CharComposeRequestPaint", m2::kCharComposeRequestPaint,
                           &hkComposeRequest, &g_origComposeRequest);
        return true;
    }
}

WXL_REGISTER_FEATURE("skin-alpha", true, InstallSkinAlpha)
