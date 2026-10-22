// Sound system addresses: the master-volume control and its init guard.
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

#include <cstdint>
#include <cstddef>
#include <string_view>

// INTERNAL to the core. The client sound-system master-volume control. Modules never include this; they
// use wxl::game::sound.
namespace wxl::offsets::engine::sound
{
    // Master-volume setter (volume in 0..1): writes the value to the sound group and flags it dirty;
    // the mixer applies it on the next audio tick. No-ops while the sound system is not up.
    constexpr uintptr_t kSetMasterVolume = 0x00879460;
    using SetMasterVolumeFn = void(__cdecl*)(float volume);

    // PlaySound script function: plays a 2D UI sound by id/name. __cdecl(scriptState); the sound id/name
    // is on the script stack. A detour observes (or, by skipping the original, suppresses) UI/world sounds.
    constexpr uintptr_t kPlaySound = 0x009858B0;
    using PlaySoundFn = int(__cdecl*)(void* scriptState);

    // Non-zero once the sound system is initialized.
    constexpr uintptr_t kSoundActiveFlag = 0x00D43814;

    // SoundKitID playback entry: resolves the given SoundKitID against the loaded SoundEntriesRec index
    // and, on a hit with a populated file-variant list, plays it. Covers spell/creature/UI
    // SoundKitID-driven playback. Returns 5 when the id is not found, 6 when the record has no file
    // variant for the current locale/gender -- both before any file I/O is attempted.
    constexpr uintptr_t kPlaySoundKit = 0x004C6A40;
    using PlaySoundKitFn = int(__cdecl*)(int soundKitId, int p2, int p3, int* p4, int p5,
                                          uint32_t* p6, uint32_t p7, int p8);

    // TEXTEMOTELOOKUP: raw race/sex sound slots followed by the emote ID. The record array
    // (0x00B4ADE4) and emote-ID pointer array (0x00B4ADF4) stay owned by the native allocator.
    // SE2/README.md records the closed consumer set and the unchanged pointer/free paths.
    namespace emotesounds
    {
        constexpr uint32_t kRaces = 64;
        constexpr uint32_t kSoundBytes = kRaces * 2 * sizeof(uint32_t);
        constexpr uint32_t kRecordBytes = kSoundBytes + sizeof(uint32_t);
        constexpr uint32_t kRecordDwords = kRecordBytes / sizeof(uint32_t);

        struct Site
        {
            const char* name;
            uintptr_t address;
            std::string_view expected;
            size_t operand;
            size_t width;
            uint32_t value;
        };
        using namespace std::literals::string_view_literals;
        // Exact 12340 instruction spans. Reader/fill spans also verify sex bounds and indexing;
        // the copy span includes rep movsd, whose count is DWORDs, not bytes. Patch operands only.
        inline constexpr Site kSites[] = {
            {"allocationSize", 0x004CEA11, "\x69\xF6\xB4\x00\x00\x00"sv,
                2, 4, kRecordBytes},
            {"copyDwords", 0x004CEA6A, "\xB9\x2D\x00\x00\x00\x8B\xF8\xF3\xA5"sv,
                1, 4, kRecordDwords},
            {"copyStride", 0x004CEA76, "\x81\xC2\xB4\x00\x00\x00"sv,
                2, 4, kRecordBytes},
            {"lookupRace", 0x004CEB4E,
                "\x83\xFA\x16\x73\x10\x8B\x4D\x10\x83\xF9\x02\x73\x08\x8D\x14\x51\x8B\x04\x90"sv,
                2, 1, kRaces},
            {"soundRace", 0x004CEB8E,
                "\x83\xFA\x16\x73\x10\x8B\x4D\x10\x83\xF9\x02\x73\x08\x8D\x14\x51\x8B\x04\x90"sv,
                2, 1, kRaces},
            {"appendStride", 0x004CEC1C, "\x69\xF6\xB4\x00\x00\x00"sv,
                2, 4, kRecordBytes},
            {"zeroSounds", 0x004CEC2D, "\x68\xB0\x00\x00\x00"sv,
                1, 4, kSoundBytes},
            {"zeroEmoteId", 0x004CEC35, "\xC7\x86\xB0\x00\x00\x00\x00\x00\x00\x00"sv,
                2, 4, kSoundBytes},
            {"storeEmoteId", 0x004CED9D, "\x89\x90\xB0\x00\x00\x00"sv,
                2, 4, kSoundBytes},
            {"fillRace", 0x004CEDAF,
                "\x83\xF9\x16\x73\x19\x8B\x46\x0C\x83\xF8\x02\x73\x11\x8B\x76\x10\x8D\x04\x48\x8B\x0F\x89\x34\x81"sv,
                2, 1, kRaces},
        };
    }

    // Sound-group array pointer; the first group's field at +0x08 holds the live master-volume float.
    constexpr uintptr_t kSoundGroupArrayPtr  = 0x00D438FC;
    constexpr size_t    kOffGroupMasterVolume = 0x08;

    // --- typed view over a sound-group record ---
    // The constants above are the curated landmarks; this struct gives named, typed access to the same
    // field, with the member offset checked against the constant at compile time. Only the field of
    // interest is named; the lead-in is explicit padding.
#pragma pack(push, 1)
    /** @brief Sound-group record (an element of the kSoundGroupArrayPtr target). */
    struct SoundGroup
    {
        uint8_t  _pad00[kOffGroupMasterVolume];
        float    masterVolume;     // kOffGroupMasterVolume (live master volume, 0..1)
    };
    static_assert(offsetof(SoundGroup, masterVolume) == kOffGroupMasterVolume, "SoundGroup.masterVolume");
#pragma pack(pop)
}
