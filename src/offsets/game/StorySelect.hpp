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
    constexpr uintptr_t kInitialize = 0x004E3CD0, kLighting = 0x004E3A20;
    constexpr uintptr_t kDetachParent = 0x008274F0;
    constexpr size_t kBackground = 0x2A0, kMount = 0x18C;
    constexpr size_t kRowStride = 0x198, kCustomization = 0x188, kActor = 0x38;
    enum class RowRegister { EAX, EBX, ECX, EDX, ESI };
    struct RowRead { uintptr_t address; uint8_t size; RowRegister target; };
    // Inspected MOVs only: replace each with a flag/register-preserving context read.
    // No native selection storage is redirected or changed.
    inline constexpr RowRead kRowReads[] = {
        {0x004E3CDB,6,RowRegister::ESI}, {0x004E3D21,6,RowRegister::ESI},
        {0x004E3D43,6,RowRegister::ESI}, {0x004E3D9F,6,RowRegister::ESI},
        {0x004E3DBC,5,RowRegister::EAX}, {0x004E3E62,5,RowRegister::EAX},
        {0x004E3E81,6,RowRegister::EDX}, {0x004E3EBA,6,RowRegister::EDX},
        {0x004E3EE8,6,RowRegister::EBX}, {0x004E3FDA,6,RowRegister::EDX},
        {0x004E4007,6,RowRegister::EDX}, {0x004E405F,6,RowRegister::EDX},
        {0x004E4089,6,RowRegister::EBX}, {0x004E40C8,6,RowRegister::EBX},
        {0x004E4133,6,RowRegister::EBX}, {0x004E4168,6,RowRegister::EBX},
        {0x004E419A,5,RowRegister::EAX}, {0x004E41CF,6,RowRegister::ECX},
        {0x004E4218,6,RowRegister::ECX}, {0x004E4303,6,RowRegister::EDX},
        {0x004E432E,6,RowRegister::EDX}, {0x004E4362,6,RowRegister::EDX},
        {0x004E4390,6,RowRegister::EDX}, {0x004E43B1,6,RowRegister::ECX},
        {0x004E43E8,6,RowRegister::EDX}, {0x004E4446,6,RowRegister::ECX},
        {0x004E4479,5,RowRegister::EAX}, {0x004E44B1,5,RowRegister::EAX},
        {0x004E3A23,5,RowRegister::EAX}, // per-resident ghost lighting
    };
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
    // Extra opt-in resident experiment sites; the existing capacity/G1 gate is unchanged.
    inline constexpr Site kResidentSites[] = {
        {"residentLighting",0x4e3a20,0x1e5,0xAD27F7A3FC647F24ULL},
        {"residentDetach",0x8274f0,0x63,0x345C9319B925E91FULL},
    };
}
