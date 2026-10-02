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
    using AllocateRequestFn = void*(__cdecl*)();

    PaintRegionFn g_origPaintRegion = nullptr;
    PaintRegionFn g_origPaintFromOrigin = nullptr;
    CreateBaseTextureFn g_origCreateBaseTexture = nullptr;
    CharInitFn g_origCharInit = nullptr;
    SectionWalkFn g_origPrepSections = nullptr;
    SectionWalkFn g_origUpdateSections = nullptr;
    ComposeRequestFn g_origComposeRequest = nullptr;
    SectionWalkFn g_origSubmitRequest = nullptr;
    AllocateRequestFn g_origAllocateRequest = nullptr;
    sa::RequestSheets g_requestSheets;

    /**
     * Whether the sheet being painted on this thread is a custom body's. The painters are not handed
     * the component, so section walks identify the body and format. A native queued request carries
     * no model identity: allocation inside its submission scope captures that decision for the
     * worker. Format alone is insufficient; stock sheets can also be ARGB with compression off.
     */
    thread_local bool t_customSheet = false;

    uint32_t Field(void* object, size_t offset)
    {
        return object ? *reinterpret_cast<const uint32_t*>(static_cast<const uint8_t*>(object) + offset) : 0;
    }

    using wxl::client::custombody::ModelStem;

    bool CustomSheet(void* component)
    {
        return sa::IsCustomSheet(ModelStem(component),
            Field(component, m2::kOffCharComponentFormat) == m2::kGxTexFormatArgb8888);
    }

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
     * Only on an identified custom body's ARGB sheet, the skin contributes no
     * alpha: after the native paint its region keeps the skin's colour (so armour edges blend
     * against it) at alpha zero, and only armour makes the sheet opaque.
     */
    void __cdecl hkPaintRegion(uint32_t regionIndex, void* source, void** levels)
    {
        sa::PaintRegion(t_customSheet, regionIndex, source, levels,
                        g_origPaintRegion, Region, SheetResolution);
    }

    /**
     * @brief The region-authored painter, which paints armour and other overlays.
     *
     * Stock/unidentified sheets remain native. Custom sheets recover armour colour and coverage
     * with the existing TwoPass algorithm, independently of the hidden skin colour.
     */
    void __cdecl hkPaintFromOrigin(uint32_t regionIndex, void* source, void** levels)
    {
        sa::PaintFromOrigin(t_customSheet, regionIndex, source, levels,
                            g_origPaintFromOrigin, Region, SheetResolution);
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
        sa::SheetScope scope(t_customSheet, CustomSheet(component));
        g_origPrepSections(component, edx);
    }

    void __fastcall hkUpdateSections(void* component, void* edx)
    {
        sa::SheetScope scope(t_customSheet, CustomSheet(component));
        g_origUpdateSections(component, edx);
    }

    void __cdecl hkComposeRequest(void* request)
    {
        const bool custom = g_requestSheets.Consume(request,
            Field(request, m2::kOffComposeRequestFormat) == m2::kGxTexFormatArgb8888);
        sa::SheetScope scope(t_customSheet, custom);
        g_origComposeRequest(request);
    }

    void __fastcall hkSubmitRequest(void* component, void* edx)
    {
        sa::SheetScope scope(t_customSheet, CustomSheet(component));
        g_origSubmitRequest(component, edx);
    }

    void* __cdecl hkAllocateRequest()
    {
        void* request = g_origAllocateRequest();
        g_requestSheets.Capture(request, t_customSheet);
        return request;
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
        wxl::hook::Install("CharComposeRequestSubmit", m2::kCharComposeRequestSubmit,
                           &hkSubmitRequest, &g_origSubmitRequest);
        wxl::hook::Install("CharComposeRequestAllocate", m2::kCharComposeRequestAllocate,
                           &hkAllocateRequest, &g_origAllocateRequest);
        return true;
    }
}

WXL_REGISTER_FEATURE("skin-alpha", true, InstallSkinAlpha)
