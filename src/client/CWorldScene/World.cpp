// World lifecycle detours: publish world enter/leave and per-frame update events, plus a liquid-row guard.
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
#include "engine/diag/AssetProfile.hpp"

#include "common/Log.hpp"
#include "common/Mem.hpp"
#include "offsets/engine/Frame.hpp"
#include "offsets/game/ADT.hpp"
#include "offsets/game/World.hpp"

#include <cstdint>

namespace
{
    namespace ev    = wxl::events;
    namespace wld   = wxl::offsets::game::world;
    namespace frame = wxl::offsets::engine::frame;
    namespace adt   = wxl::offsets::game::adt;
    namespace aprof = wxl::runtime::assetprof;

    wld::World_MapEnterFn  g_origMapEnter  = nullptr;
    wld::World_MapUnloadFn g_origMapUnload = nullptr;
    frame::FramePumpFn     g_origFramePump = nullptr;

    /**
     * @brief Publishes leave while the old map is still loaded.
     *
     * Native transfers unload separately; direct SDK map loads replace the map themselves. The
     * native lifetime flag covers both without duplicating leave after unload, and excludes the
     * first login. The map id alone cannot do this: native unload leaves its previous value intact.
     */
    void EmitWorldLeave()
    {
        if (!wxl::mem::Read<uint32_t>(wld::kMapLoaded)) return;
        const auto mapId = static_cast<uint32_t>(wxl::mem::Read<int32_t>(wld::kCurrentMapId));
        ev::WorldLeaveArgs leave{ mapId };
        WLOG_DEBUG("[world] OnWorldLeave mapId=%u", mapId);
        ev::Emit(ev::Event::OnWorldLeave, &leave);
    }

    void __cdecl hkMapUnload()
    {
        EmitWorldLeave();
        g_origMapUnload();
    }

    void __cdecl hkMapEnter(const char* mapDir, int mapId)
    {
        EmitWorldLeave();
        g_origMapEnter(mapDir, mapId);
        const auto entered = static_cast<uint32_t>(wxl::mem::Read<int32_t>(wld::kCurrentMapId));
        ev::WorldEnterArgs enter{ entered };
        WLOG_DEBUG("[world] OnWorldEnter mapId=%u", entered);
        ev::Emit(ev::Event::OnWorldEnter, &enter);
    }

    /**
     * @brief Detours the frame-time tick, emitting OnUpdate once per frame with the frame delta.
     *
     * The delta and the timestamp arrive as ARGUMENTS and are forwarded from there rather than read
     * back out of the globals the original stores them in. Same values either way, but this way the
     * emit does not depend on the store having happened, and the two cannot drift if the client ever
     * takes a path that computes one without publishing the other.
     */
    void __cdecl hkFramePump(float deltaSeconds, uint32_t frameTimeMs)
    {
        g_origFramePump(deltaSeconds, frameTimeMs);
        ev::UpdateArgs a{ deltaSeconds, frameTimeMs };
        ev::Emit(ev::Event::OnUpdate, &a);
        aprof::RecordFrame(a.dt);
    }

    bool InstallWorld()
    {
        wxl::hook::Install("WorldMapUnload", wld::kMapUnload, &hkMapUnload, &g_origMapUnload);
        wxl::hook::Install("WorldMapEnter", wld::kMapEnter, &hkMapEnter, &g_origMapEnter);
        wxl::hook::Install("FramePump", frame::kFramePump, &hkFramePump, &g_origFramePump);

        // Liquid-row null guard: this one liquid consumer dereferences the LiquidType row flag without the
        // null check the others have, so an unknown liquid id (from any served source) faults. Skip the
        // flag test and make the branch unconditional (nop x4 + jz->jmp) -> default no-bump path.
        const uint8_t guard[5] = { 0x90, 0x90, 0x90, 0x90, 0xEB };
        wxl::mem::Patch(reinterpret_cast<void*>(adt::kLiquidRowFlagTest), guard, sizeof guard);
        return true;
    }
}

WXL_REGISTER_FEATURE("world", true, InstallWorld)
