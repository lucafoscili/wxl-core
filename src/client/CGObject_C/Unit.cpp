// Unit/object detours: publish object update/destroy and target-change events from the message handlers.
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
#include "offsets/game/Unit.hpp"

#include <cstdint>

namespace
{
    namespace ev   = wxl::events;
    namespace unit = wxl::offsets::game::unit;

    unit::ObjectMsgHandlerFn g_origObjUpdate  = nullptr;
    unit::ObjectMsgHandlerFn g_origObjDestroy = nullptr;
    unit::TargetSetFn        g_origTargetSet  = nullptr;

    /**
     * @brief Detours the server object update-block handler, emitting OnObjectUpdate after the parse.
     *
     * One fire per update message (a batch of created/updated objects). Logs the first fire only.
     * @param ctx     handler context.
     * @param opcode  message opcode.
     * @param msg     message id.
     * @param packet  inbound message reader.
     * @return the native handler result.
     */
    int __cdecl hkObjUpdate(void* ctx, int opcode, int msg, void* packet)
    {
        const int r = g_origObjUpdate(ctx, opcode, msg, packet);

        ev::ObjectUpdateArgs a{ packet, opcode };
        ev::Emit(ev::Event::OnObjectUpdate, &a);

        static bool logged = false;
        if (!logged) { logged = true; WLOG_INFO("object: update stream active"); }
        return r;
    }

    /**
     * @brief Detours the object destroy handler, emitting OnObjectDestroy before the despawn.
     *
     * One fire per despawn, while the object is still resident. Logs the first fire only.
     * @param ctx     handler context.
     * @param opcode  message opcode.
     * @param msg     message id.
     * @param packet  inbound message reader (object GUID + on-death flag).
     * @return the native handler result.
     */
    int __cdecl hkObjDestroy(void* ctx, int opcode, int msg, void* packet)
    {
        ev::ObjectDestroyArgs a{ packet, opcode };
        ev::Emit(ev::Event::OnObjectDestroy, &a);

        static bool logged = false;
        if (!logged) { logged = true; WLOG_INFO("object: destroy hook active"); }
        return g_origObjDestroy(ctx, opcode, msg, packet);
    }

    /**
     * @brief Detours the target-set API, emitting OnTargetChanged after the new target is applied.
     * @param scriptState  script state the call ran on.
     * @return the native function result.
     */
    int __cdecl hkTargetSet(void* scriptState)
    {
        const int r = g_origTargetSet(scriptState);

        ev::TargetChangedArgs a{ scriptState };
        ev::Emit(ev::Event::OnTargetChanged, &a);

        // Log the first fire only: target changes are a per-combat-action event.
        static bool logged = false;
        if (!logged) { logged = true; WLOG_INFO("target: hook live (first change)"); }
        return r;
    }

    /**
     * Trial candidate for the fresh-login invisibility (wow/queued/features/login-invisibility):
     * a player unit shown through an extended display row whose setup returned before CharInit gets
     * its pending bit back, so deferred prep retries it, up to kSetupRetries times per unit. The
     * native bypass needs a local-player bit (+0xF42) that is not yet set when companions arrive at
     * login. Bounded log lines say each unit's first refusal, its eventual setup and any exhaustion.
     */
    unit::CharSetupCreateFn g_origCharSetupCreate = nullptr;
    struct SetupRetry { void* unit; uint64_t guid; uint32_t tries; bool logged; };
    SetupRetry g_setupRetries[16] = {};
    uint32_t g_setupLogs = 0;
    constexpr uint32_t kSetupRetries = 900;

    template<class T> T Read(const void* base, size_t offset)
    {
        return base ? *reinterpret_cast<const T*>(static_cast<const uint8_t*>(base) + offset) : T{};
    }

    int __fastcall hkCharSetupCreate(void* self, void* edx, void* appearance, int extended)
    {
        const int result = g_origCharSetupCreate(self, edx, appearance, extended);
        void* extra = Read<void*>(self, unit::kOffUnitDisplayExtra);
        const void* fields = Read<void*>(self, unit::kOffObjectFields);
        const bool player = (Read<uint32_t>(fields, 8) & unit::kTypeMaskPlayer) != 0;
        if (!self || !extra || !extended || appearance || !player)
            return result;
        // A unit is its address and GUID: a new unit at a reused address starts afresh. A slot whose
        // budget is spent is the first to give way, so spent units never starve later ones.
        const uint64_t id = Read<uint64_t>(fields, 0);
        SetupRetry* slot = nullptr;
        for (auto& retry : g_setupRetries)
            if (retry.unit == self && retry.guid == id) { slot = &retry; break; }
        if (!slot)
            for (auto& retry : g_setupRetries)
                if (!retry.unit || retry.unit == self || retry.tries > kSetupRetries) { slot = &retry; break; }
        if (!slot)
            return result;
        if (slot->unit != self || slot->guid != id) *slot = { self, id, 0, false };
        const char* bake = Read<const char*>(extra, unit::kOffDisplayExtraBakeName);
        const auto guid = reinterpret_cast<unit::ActivePlayerGuidFn>(unit::kActivePlayerGuid)();
        void* local = guid ? reinterpret_cast<unit::GetObjectFn>(unit::kGetObjectByGuid)(guid, unit::kTypeMaskObject,
                                                                                         "wxl-setup", 0) : nullptr;
        const uint32_t localBit = (Read<uint8_t>(local, unit::kOffPlayerSetupFlags) >> 1) & 1u;
        if (result)
        {
            if (slot->tries && g_setupLogs < 64)
            {
                ++g_setupLogs;
                WLOG_INFO("unit: character setup unit=%p display=%u extra=%u succeeded after %u retries (localBit=%u)",
                          self, Read<uint32_t>(Read<void*>(self, unit::kOffUnitDisplayInfo), 0),
                          Read<uint32_t>(extra, 0), slot->tries, localBit);
                wxl::log::Flush();
            }
            slot->unit = nullptr;
            return result;
        }
        if (!slot->logged && g_setupLogs < 64)
        {
            slot->logged = true;
            ++g_setupLogs;
            WLOG_INFO("unit: character setup unit=%p display=%u extra=%u returned before CharInit: localBit=%u "
                      "extraFlags=0x%X modelFlags=0x%X bake=%s; retrying", self,
                      Read<uint32_t>(Read<void*>(self, unit::kOffUnitDisplayInfo), 0), Read<uint32_t>(extra, 0),
                      localBit, Read<uint32_t>(extra, unit::kOffDisplayExtraFlags),
                      Read<uint32_t>(Read<void*>(self, unit::kOffUnitModelData), unit::kOffModelDataFlags),
                      bake && *bake ? bake : "(empty)");
            wxl::log::Flush();
        }
        if (slot->tries++ < kSetupRetries)
            *reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(self) + unit::kOffUnitCharFlags) |= unit::kCharSetupPending;
        else if (slot->tries == kSetupRetries + 1 && g_setupLogs < 64)
        {
            ++g_setupLogs;
            WLOG_WARN("unit: character setup unit=%p gave up after %u retries (localBit=%u)", self, kSetupRetries, localBit);
        }
        return result;
    }

    bool InstallUnit()
    {
        wxl::hook::Install("CharSetupCreate", unit::kCharSetupCreate, &hkCharSetupCreate, &g_origCharSetupCreate);
        wxl::hook::Install("ObjectUpdate", unit::kObjectUpdateHandler, &hkObjUpdate, &g_origObjUpdate);
        wxl::hook::Install("ObjectDestroy", unit::kObjectDestroyHandler, &hkObjDestroy, &g_origObjDestroy);
        wxl::hook::Install("TargetSet", unit::kTargetSet, &hkTargetSet, &g_origTargetSet);
        return true;
    }
}

WXL_REGISTER_FEATURE("unit", true, InstallUnit)
