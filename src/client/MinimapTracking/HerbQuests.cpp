// Two-hook, opt-in build trial. GPL-3.0-or-later.
#ifdef WXL_HERB_QUESTS_TRIAL
#include "HerbQuests.hpp"
#include <atomic>
#include <cmath>
#include <intrin.h>
#include "common/Config.hpp"
#include "common/Log.hpp"
#include "common/Mem.hpp"
#include "engine/hook/Hook.hpp"
#include "engine/hook/Registry.hpp"
#include "game/Script.hpp"

namespace
{
    namespace off = wxl::offsets::game::tracking;
    namespace policy = wxl::tracking;
    namespace script = wxl::game::script;
    script::Function g_setTracking = nullptr;
    off::SetServiceFn g_setService = nullptr;
    std::atomic<bool> g_active{false};
    unsigned g_preserveRecords = 0; // native tracking callbacks run on the client thread

    void __cdecl SetService(uintptr_t service)
    {
        const auto caller = reinterpret_cast<uintptr_t>(_ReturnAddress());
        const auto current = wxl::mem::Read<uintptr_t>(off::kCurrentService);
        const auto spell = wxl::mem::Read<uint32_t>(off::kCurrentSpell);
        if (g_active.load() && policy::Preserve(service, current, spell, caller))
        {
            // The native aura update already set the actual spell ID. Keep the real
            // service and its CVar; original still publishes MINIMAP_UPDATE_TRACKING.
            service = current;
            if (g_preserveRecords++ < 16)
                WLOG_INFO("herb-quests: herb aura update preserves native TrivialQuests");
        }
        g_setService(service);
    }

    int __cdecl SetTracking(void* state)
    {
        if (g_active.load() && script::IsNumber(state, 1))
        {
            const double number = script::ToNumber(state, 1);
            // Match native positive integer truncation, leaving invalid values to native.
            if (std::isfinite(number) && number >= 1 && number <= 2147483647)
            {
                const auto index = static_cast<uint32_t>(number) - 1;
                const auto count = wxl::mem::Read<uint32_t>(off::kSpellCount);
                if (index >= count && index - count < 15)
                {
                    // This native resolver retains the class-filtered service indexing.
                    const auto service = wxl::game::Native<off::ServiceByIndexFn>(off::kServiceByIndex)(index - count);
                    const auto current = wxl::mem::Read<uintptr_t>(off::kCurrentService);
                    const auto spell = wxl::mem::Read<uint32_t>(off::kCurrentSpell);
                    const auto choice = policy::Select(service, current, spell);
                    if (choice != policy::Choice::Native)
                    {
                        // Direct trampoline intentionally bypasses the herb-update filter.
                        // No aura cancellation, fake spell ID, addon check or server mask write.
                        g_setService(choice == policy::Choice::EnableQuest ? service : 0);
                        WLOG_INFO("herb-quests: native TrivialQuests %s; tracked spell=%u",
                                  choice == policy::Choice::EnableQuest ? "on" : "off", spell);
                        return 0;
                    }
                }
            }
        }
        // Includes every spell cast/toggle, None, invalid input and all other services.
        return g_setTracking(state);
    }

    bool Compatible()
    {
        if (reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)) != 0x00400000)
            return false;
        const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(0x00400000);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 0 || dos->e_lfanew > 0x1000)
            return false;
        const auto pe = reinterpret_cast<const IMAGE_NT_HEADERS*>(0x00400000 + dos->e_lfanew);
        if (pe->Signature != IMAGE_NT_SIGNATURE || pe->FileHeader.Machine != IMAGE_FILE_MACHINE_I386
            || pe->OptionalHeader.SizeOfImage < off::kCurrentSpell + 4 - 0x00400000)
            return false;
        for (const auto& site : off::kSites)
        {
            const auto bytes = reinterpret_cast<const uint8_t*>(site.address);
            uint64_t hash = 0xcbf29ce484222325ULL;
            for (size_t i = 0; i < site.size; ++i)
                hash = (hash ^ bytes[i]) * 0x100000001b3ULL;
            if (hash != site.hash)
            {
                WLOG_WARN("herb-quests: incompatible %s; tracking trial stays inactive", site.name);
                return false;
            }
        }
        return true;
    }

    bool Install()
    {
        if (!wxl::config::Env("WXL_HERB_QUESTS", true))
            return true;
        if (!Compatible())
        {
            WLOG_WARN("herb-quests: client compatibility not established; tracking unchanged");
            return false;
        }
        if (!wxl::hook::Install("HerbQuestsService", off::kSetService, &SetService, &g_setService)
            || !wxl::hook::Install("HerbQuestsSelection", off::kLuaSetTracking, &SetTracking, &g_setTracking)
            || !wxl::hook::Enable(off::kSetService) || !wxl::hook::Enable(off::kLuaSetTracking))
            return false;
        // A partial hook installation forwards unchanged, even if a later batch enables it.
        g_active.store(true);
        WLOG_INFO("herb-quests: two native hooks active; herbs + TrivialQuests only; visual acceptance pending");
        return true;
    }
}
WXL_REGISTER_FEATURE("herb-quests-trial", true, Install)
#endif
