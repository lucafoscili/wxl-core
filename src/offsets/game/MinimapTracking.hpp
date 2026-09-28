// Exact-client landmarks for the bounded herb + trivial-quest trial. GPL-3.0-or-later.
#pragma once
#include <cstddef>
#include <cstdint>

namespace wxl::offsets::game::tracking
{
    constexpr uintptr_t kLuaSetTracking = 0x0057F380;
    constexpr uintptr_t kSetService = 0x0057E070;
    constexpr uintptr_t kServiceByIndex = 0x0057EB00;
    constexpr uintptr_t kSpellUpdateReturn = 0x0057EA46;
    constexpr uintptr_t kCurrentService = 0x00BEBA64;
    constexpr uintptr_t kCurrentSpell = 0x00BEBA68;
    constexpr uintptr_t kSpellCount = 0x00BE8DE8;
    constexpr uintptr_t kTrivialQuests = 0x00A11D68;
    constexpr uint32_t kFindHerbs = 2383;
    using SetServiceFn = void(__cdecl*)(uintptr_t service);
    using ServiceByIndexFn = uintptr_t(__cdecl*)(uint32_t index);

    // FNV-1a compatibility fingerprints, NOT a security or provenance checksum.
    // Full relevant native routines/table were compared on both exact PC clients.
    // See docs/herb-quests.md. Never infer compatibility from build 12340 alone.
    struct Site { const char* name; uintptr_t address; size_t size; uint64_t hash; };
    constexpr Site kSites[] = {
        {"SetTracking", 0x0057F380, 0x170, 0x61D22ED14C115345ULL},
        {"SetService", 0x0057E070, 0x87, 0x475BBE5C27CD490CULL},
        {"UpdateSpell", 0x0057EA30, 0x2A, 0xB6B0A91EFA8A0CE4ULL},
        {"ServiceByIndex", 0x0057EB00, 0x7A, 0x14BF7E04464433EEULL},
        {"GetTrackingInfo", 0x0057F1B0, 0x1CA, 0x5EDD3A9560C08677ULL},
        {"Collector", 0x0057F7F0, 0x3D2, 0xDCEDBF3EC302F091ULL},
        {"ResourcePredicate", 0x006DCA90, 0xA2, 0x708C38F75A9F899BULL},
        {"TrivialPredicate", 0x0057BF30, 0x17, 0x9246E7DB1FB3FF5FULL},
        {"ServiceTable", 0x00A11C50, 0x12C, 0x999ADDB3B2D49D56ULL},
        {"LuaIsNumber", 0x0084DF20, 0x10, 0xE2CE6DB5F34CBABEULL},
        {"LuaToNumber", 0x0084E030, 0x10, 0xBD6D7DB8B1CB8469ULL},
    };
}
