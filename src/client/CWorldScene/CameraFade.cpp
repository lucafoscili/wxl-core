// Local-player camera opacity; native unit opacity and timed fades stay authoritative.
// Copyright (C) 2026 WarcraftXL. GPL-3.0-or-later.
#include "common/Mem.hpp"
#include "offsets/engine/Camera.hpp"
#include "offsets/game/Unit.hpp"
#ifndef WXL_CAMERA_FADE_FIXTURE
#include "common/Log.hpp"
#include "engine/hook/Registry.hpp"
#endif

namespace wxl::client::camerafade
{
    namespace off = wxl::offsets::engine::camera;
    namespace unit = wxl::offsets::game::unit;

    // Variables let the synthetic fixture supply a player and a continuation without
    // calling client code. Production uses only the existing authoritative offsets.
    unit::ActivePlayerGuidFn g_activePlayerGuid =
        reinterpret_cast<unit::ActivePlayerGuidFn>(unit::kActivePlayerGuid);
    uintptr_t g_resume = off::kDistanceFadeResume;
    uintptr_t g_submitContext = off::kDistanceFadeSubmitContext;

    __declspec(noinline) void __cdecl OverrideDistanceAlpha(const uint8_t* camera, uint8_t* alpha)
    {
        const auto player = g_activePlayerGuid();
        if (!player || wxl::mem::Read<uint64_t>(reinterpret_cast<uintptr_t>(camera)
                                              + off::kFollowTargetGuid) != player)
            return;

        // Classify the native IEEE-754 distance as bits: even a signaling NaN must
        // leave the native factor and floating-point exceptions alone.
        const uint32_t bits = wxl::mem::Read<uint32_t>(reinterpret_cast<uintptr_t>(camera)
                                                    + off::kFollowDistance);
        const uint32_t magnitude = bits & 0x7FFFFFFFu;
        if (magnitude >= 0x7F800000u) return; // NaN / infinity: leave native alpha
        *alpha = magnitude && !(bits & 0x80000000u) ? 255 : 0;
    }

    __declspec(naked) void DistanceFadeHook()
    {
        __asm
        {
            pushfd
            pushad
            mov edi, esp
            sub esp, 528
            and esp, -16
            fxsave [esp]
            // The helper gets an empty x87 stack; FXRSTOR returns the exact native
            // x87/MMX/XMM/MXCSR state, including any live values and exception flags.
            fninit
            cld
            lea eax, [ebp - 0Ch]
            push eax
            push esi
            call OverrideDistanceAlpha
            add esp, 8
            fxrstor [esp]
            mov esp, edi
            popad
            popfd
            // Replay the two displaced MOVs without changing native flags. The
            // extra indirection reads the same absolute submit-context global.
            mov eax, [ebp - 0Ch]
            mov ecx, dword ptr [g_submitContext]
            mov ecx, [ecx]
            jmp dword ptr [g_resume]
        }
    }

    bool InstallAt(void* site)
    {
        if (std::memcmp(site, off::kDistanceFadeOriginal, sizeof off::kDistanceFadeOriginal))
        {
#ifndef WXL_CAMERA_FADE_FIXTURE
            WLOG_WARN("camera-fade: native distance-submit bytes mismatch; hook inactive");
#endif
            return false;
        }
        uint8_t jump[sizeof off::kDistanceFadeOriginal];
        std::memset(jump, 0x90, sizeof jump);
        jump[0] = 0xE9;
        const uint32_t relative = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&DistanceFadeHook)
            - (reinterpret_cast<uintptr_t>(site) + 5));
        std::memcpy(jump + 1, &relative, sizeof relative);
        return wxl::mem::Patch(site, jump, sizeof jump);
    }

#ifndef WXL_CAMERA_FADE_FIXTURE
    bool Install()
    {
        void* site = reinterpret_cast<void*>(off::kDistanceFadeSubmit);
        if (!InstallAt(site))
        {
            WLOG_ERROR("camera-fade: could not patch distance-submit site");
            return false;
        }
        WLOG_INFO("camera-fade: local player opaque at positive distance; zero hides");
        return true;
    }
#endif
}

#ifndef WXL_CAMERA_FADE_FIXTURE
WXL_REGISTER_FEATURE("camera-fade", true, wxl::client::camerafade::Install)
#endif
