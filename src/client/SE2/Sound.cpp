// Sound detours: publish the play-sound event and log unresolved SoundKit ids for diagnosis.
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
#include "engine/hook/Hook.hpp"
#include "engine/hook/Registry.hpp"
#include "engine/events/Event.hpp"

#include "common/Log.hpp"
#include "common/Config.hpp"
#include "common/Mem.hpp"
#include "offsets/engine/Sound.hpp"

#include <cstdint>
#include <iterator>
#include <unordered_set>

namespace
{
    namespace ev  = wxl::events;
    namespace snd = wxl::offsets::engine::sound;

    snd::PlaySoundFn    g_origPlaySound    = nullptr;
    snd::PlaySoundKitFn g_origPlaySoundKit = nullptr;

    /**
     * @brief Detours the play-sound API, emitting OnSoundPlay before the sound starts.
     *
     * Fires on every UI/world sound. Logs the first fire only.
     * @param scriptState  script state the call ran on (the sound id/name is on its stack).
     * @return the native function result.
     */
    int __cdecl hkPlaySound(void* scriptState)
    {
        ev::SoundPlayArgs a{ scriptState };
        ev::Emit(ev::Event::OnSoundPlay, &a);

        static bool logged = false;
        if (!logged) { logged = true; WLOG_INFO("sound: play hook active"); }
        return g_origPlaySound(scriptState);
    }

    /**
     * @brief Diagnostic-only: logs every SoundKitID play attempt and its result code.
     *
     * Disambiguates a data resolution gap (returns 5/6, no file I/O ever attempted) from a
     * cache-served hit (returns success with no corresponding Storage_FileOpen) for the in-world
     * interface/spell/creature sound investigation.
     */
    int __cdecl hkPlaySoundKit(int soundKitId, int p2, int p3, int* p4, int p5,
                               uint32_t* p6, uint32_t p7, int p8)
    {
        int r = g_origPlaySoundKit(soundKitId, p2, p3, p4, p5, p6, p7, p8);
        // Each failing kit id is logged once; the raw stream repeats the same missing ids dozens of
        // times per minute and the per-line file write is itself a frame cost.
        if (r != 0)
        {
            static std::unordered_set<int> loggedKits;
            if (loggedKits.insert(soundKitId).second)
                WLOG_INFO("audio-diag: PlaySoundKit id=%d -> %d", soundKitId, r);
        }
        return r;
    }

    bool InstallSound()
    {
        wxl::hook::Install("PlaySound", snd::kPlaySound, &hkPlaySound, &g_origPlaySound);
        wxl::hook::Install("PlaySoundKit", snd::kPlaySoundKit, &hkPlaySoundKit, &g_origPlaySoundKit);
        return true;
    }
}

WXL_REGISTER_FEATURE("sound", true, InstallSound)

namespace wxl::client::emotesounds
{
    namespace off = wxl::offsets::engine::sound::emotesounds;

    template<class ReadByte>
    bool VerifySites(ReadByte read)
    {
        for (const auto& site : off::kSites)
            for (size_t i = 0; i < site.expected.size(); ++i)
                if (read(site.address + i) != static_cast<uint8_t>(site.expected[i]))
                {
                    WLOG_ERROR("emote-sounds: incompatible %s at 0x%08X; no patches applied",
                               site.name, static_cast<unsigned>(site.address));
                    return false;
                }
        return true;
    }

    bool Install()
    {
        if (!wxl::config::Env("WXL_EMOTE_SOUNDS", true))
        {
            WLOG_INFO("emote-sounds: disabled by WXL_EMOTE_SOUNDS");
            return true;
        }
        if (!VerifySites(wxl::mem::Read<uint8_t>)) return false;

        // One .text write window: a failed unprotect changes nothing. Boot runs before native
        // lookup allocation/loading; no live records exist to migrate and no detour is needed.
        const auto& first = off::kSites[0];
        const auto& last = off::kSites[std::size(off::kSites) - 1];
        if (!wxl::mem::WithWritable(reinterpret_cast<void*>(first.address),
                                   last.address + last.expected.size() - first.address, [&] {
            for (const auto& site : off::kSites)
                std::memcpy(reinterpret_cast<void*>(site.address + site.operand),
                            &site.value, site.width);
        }))
        {
            WLOG_ERROR("emote-sounds: allocationSize..fillRace code window at 0x%08X is not writable; no patches applied",
                       static_cast<unsigned>(first.address));
            return false;
        }
        WLOG_INFO("emote-sounds: IDs 0..63 installed; 128 sound slots, record stride 0x204");
        return true;
    }
}

WXL_REGISTER_FEATURE_PHASED("emote-sounds", true, wxl::client::emotesounds::Install, ::wxl::hook::Phase::Boot)
