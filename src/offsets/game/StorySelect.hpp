// Landmarks traced on Velora's exact Beta/Home 12340 executables, 2026-09-28.
// GPL-3.0-or-later. Evidence owner: Velora wow/clients/story_select/README.md.
#pragma once
#include <cstddef>
#include <cstdint>
namespace wxl::offsets::game::story
{
    constexpr uintptr_t kFrame = 0x00B6B1FC;
    constexpr uintptr_t kCount = 0x00B6B23C;
    constexpr uintptr_t kRows = 0x00B6B240;
    constexpr uintptr_t kSelected = 0x00AC436C;
    constexpr uintptr_t kRefresh = 0x004E4610;
    constexpr uintptr_t kSelectCharacter = 0x004E4580;
    constexpr size_t kRowStride = 0x198, kCustomization = 0x188, kActor = 0x38;
    struct Site { const char* name; uintptr_t address; size_t size; uint64_t hash; };
    // Generated from the inspected executable by the feature's read-only inspector.
    inline constexpr Site kSites[] = {
        {"refresh", 0x4e4610, 0x1d8, 0xDB07F80DD822068CULL},
        {"setFrame", 0x4e2f60, 0x69, 0xD0FD452398EB440DULL},
        {"selectActor", 0x4e3cd0, 0x809, 0x9A505E1CF7DF7040ULL},
        {"selectLua", 0x4e4580, 0x87, 0x8976FAA6B3032930ULL},
        {"rosterInfo", 0x4e3170, 0x224, 0x3DFDF712DDEEC55CULL},
        {"actorFacing", 0x4e2e70, 0x73, 0x5A91F0A9085DA54DULL},
        {"placement", 0x8251d0, 0x86, 0x4996ED0168AA0B53ULL},
        {"modelSequence", 0x95f5e0, 0x28, 0x9992F21B78882233ULL},
        {"methods", 0x9603d0, 0x20, 0x959B03C1099A5AA4ULL},
        {"validateCallback", 0x86b5a0, 0x50, 0xC14F2A29AB8A146AULL},
    };
}
