// Expand raw race-indexed creator storage and name tokens to IDs 0..63.
// GPL-3.0-or-later. Client offsets/verification remain owned by CharCreate.hpp and DB2.hpp.
#include "common/Config.hpp"
#include "common/Log.hpp"
#include "common/Mem.hpp"
#include "engine/hook/Hook.hpp"
#include "engine/hook/Registry.hpp"
#include "offsets/game/CharCreate.hpp"

#include <array>
#include <cstring>

namespace wxl::client::racetables
{
    namespace off = wxl::offsets::game::charcreate;
    namespace races = wxl::offsets::game::db2::chrraces;
    using wxl::mem::Read;

    void* g_preferences[off::kExpandedRaces * off::kPreferenceSexes]{};
    const char* g_names[off::kExpandedRaces]{};
    using LoadFn = void(__fastcall*)(void*, void*, const char*, uint32_t);
    LoadFn g_load = nullptr;
    void* g_restrictionThunk = nullptr;
    static_assert(sizeof(void*) == 4 && sizeof(g_preferences) == 0x200);

    template<class ReadByte>
    bool VerifySites(ReadByte read)
    {
        for (const auto& site : off::kSites)
        {
            uint64_t value = 0xcbf29ce484222325ULL;
            for (size_t i = 0; i < site.size; ++i)
                value = (value ^ read(site.address + i)) * 0x100000001b3ULL;
            if (value != site.hash)
            {
                WLOG_ERROR("race-tables: incompatible %s at 0x%08X; no patches applied",
                           site.name, static_cast<unsigned>(site.address));
                return false;
            }
        }
        return true;
    }

    std::array<uint8_t, 16> RestrictionThunk(uintptr_t address)
    {
        // EAX=race-1 > 10 on entry. Unsigned above 62 also catches race 0's underflow.
        // cmp eax,62; ja error; fldz; jmp result. No register or stack changes.
        std::array<uint8_t, 16> code{0x83, 0xF8, 0x3E, 0x0F, 0x87, 0, 0, 0, 0,
                                     0xD9, 0xEE, 0xE9, 0, 0, 0, 0};
        const uint32_t error = static_cast<uint32_t>(off::kRestrictionError - (address + 9));
        const uint32_t result = static_cast<uint32_t>(off::kRestrictionResult - (address + 16));
        std::memcpy(code.data() + 5, &error, sizeof error);
        std::memcpy(code.data() + 12, &result, sizeof result);
        return code;
    }

    void FillRaceNames(int32_t minimum, int32_t maximum, uintptr_t table)
    {
        for (uint32_t race = off::kPreferenceRaces; race < off::kExpandedRaces; ++race)
        {
            uintptr_t row = 0;
            if (table && static_cast<int32_t>(race) >= minimum && static_cast<int32_t>(race) <= maximum)
                row = Read<uintptr_t>(table + (race - static_cast<uint32_t>(minimum)) * sizeof(void*));
            g_names[race] = row ? Read<const char*>(row + races::kOffClientFileString) : nullptr;
        }
    }

    void __fastcall LoadRaces(void* storage, void* edx, const char* source, uint32_t line)
    {
        g_load(storage, edx, source, line);
        // The native loader builds row pointers BEFORE reading the strings they point into.
        // Wait for its return, before startup can continue to Glue or either name reader.
        if (!Read<uint32_t>(races::kStorageLoadedFlag))
        {
            WLOG_ERROR("race-tables: chrRacesLoaded is unset after ChrRaces loader returned");
            return;
        }
        FillRaceNames(Read<int32_t>(races::kMinId), Read<int32_t>(races::kMaxId),
                      Read<uintptr_t>(races::kIdTable));
        WLOG_INFO("race-tables: ChrRaces names ready for IDs 22..63");
    }

    bool Install()
    {
        if (!wxl::config::Env("WXL_RACE_TABLES", true))
        {
            WLOG_INFO("race-tables: disabled by WXL_RACE_TABLES");
            return true;
        }
        if (!VerifySites(Read<uint8_t>)) return false;

        g_restrictionThunk = VirtualAlloc(nullptr, 16, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (!g_restrictionThunk)
        {
            WLOG_ERROR("race-tables: restrictionBranch thunk allocation failed");
            return false;
        }
        const auto code = RestrictionThunk(reinterpret_cast<uintptr_t>(g_restrictionThunk));
        std::memcpy(g_restrictionThunk, code.data(), code.size());
        FlushInstructionCache(GetCurrentProcess(), g_restrictionThunk, code.size());
        if (!wxl::hook::Install("RaceTablesChrRaces", races::kLoader, &LoadRaces, &g_load))
        {
            WLOG_ERROR("race-tables: chrRacesLoad hook installation failed");
            VirtualFree(g_restrictionThunk, 0, MEM_RELEASE);
            g_restrictionThunk = nullptr;
            return false;
        }

        std::memcpy(g_names, reinterpret_cast<const void*>(off::kRaceNames),
                    off::kPreferenceRaces * sizeof(void*));
        const uint32_t preferences = reinterpret_cast<uintptr_t>(g_preferences);
        const uint32_t names = reinterpret_cast<uintptr_t>(g_names);
        const uint32_t branch = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(g_restrictionThunk)
                                                     - (off::kRestrictionBranch + 6));
        // All operands are in the same .text section. One protect/write window publishes the
        // complete relocation before client startup; a failed unprotect writes nothing.
        const auto begin = off::kRaceNameBaseSites[0];
        const auto end = off::kPreferenceBaseSites[7] + sizeof(uint32_t);
        if (!wxl::mem::WithWritable(reinterpret_cast<void*>(begin), end - begin, [&] {
            for (const auto site : off::kPreferenceBaseSites)
                std::memcpy(reinterpret_cast<void*>(site), &preferences, sizeof preferences);
            std::memcpy(reinterpret_cast<void*>(off::kPreferenceInitLengthSite),
                        &off::kExpandedPreferenceBytes, sizeof(uint32_t));
            std::memcpy(reinterpret_cast<void*>(off::kPreferenceFreeRaceCountSite),
                        &off::kExpandedRaces, sizeof(uint32_t));
            for (const auto site : off::kRaceNameBaseSites)
                std::memcpy(reinterpret_cast<void*>(site), &names, sizeof names);
            std::memcpy(reinterpret_cast<void*>(off::kRestrictionBranch + 2), &branch, sizeof branch);
        }))
        {
            WLOG_ERROR("race-tables: raceName..preferenceRaceRestore code window is not writable");
            return false;
        }
        WLOG_INFO("race-tables: IDs 0..63 installed; names fill after ChrRaces load");
        return true;
    }
}

// Boot runs before creator initialization and DBC loading; EnableAll arms the queued loader hook
// before client startup resumes. Normal is a graphics-waiting worker and would race both.
WXL_REGISTER_FEATURE_PHASED("race-tables", true, wxl::client::racetables::Install, ::wxl::hook::Phase::Boot)
