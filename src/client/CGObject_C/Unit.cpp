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
#include <initializer_list>

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

    /**
     * Trial trace and kick for the same stall: a unit shown through a Velora display (id >= 100000)
     * whose bound instance exists, yet which has no character component and no
     * pending setup, gets the pending bit set, up to kSetupKicks times; setup's outcome for such
     * units is logged whenever its state changes. Bounded: 16 units, 96 lines.
     */
    unit::UnitPrepFn g_origUnitPrep = nullptr;
    unit::CharSetupFn g_origCharSetup = nullptr;
    struct SetupWatch { void* unit; uint64_t guid; uint32_t kicks; uint32_t signature; };
    SetupWatch g_watches[16] = {};
    uint32_t g_watchLines = 0;
    constexpr uint32_t kSetupKicks = 600;

    SetupWatch* Watch(void* self)
    {
        const void* fields = Read<void*>(self, unit::kOffUnitDescriptor);
        // Velora displays clone their race's display row, which carries no extended row: the
        // display id alone selects these units.
        if (!self || !fields || Read<uint32_t>(fields, unit::kOffFieldDisplayId) < 100000u)
            return nullptr;
        const uint64_t id = Read<uint64_t>(Read<void*>(self, unit::kOffObjectFields), 0);
        for (auto& watch : g_watches)
            if (watch.unit == self && watch.guid == id) return &watch;
        for (auto& watch : g_watches)
            if (!watch.unit || watch.unit == self) { watch = { self, id, 0, 0 }; return &watch; }
        return nullptr;
    }

    void TraceSetup(const char* where, void* self, SetupWatch& watch, int result)
    {
        void* instance = Read<void*>(self, unit::kOffUnitInstance);
        void* shared = Read<void*>(instance, 0x2C);
        const void* fields = Read<void*>(self, unit::kOffUnitDescriptor);
        const uint32_t flags = Read<uint32_t>(self, unit::kOffUnitCharFlags);
        const uint32_t initFlags = Read<uint32_t>(instance, 0x10), sharedFlags = Read<uint32_t>(shared, 8);
        void* component = Read<void*>(self, unit::kOffUnitCharComponent);
        uint32_t signature = 2166136261u;
        for (const char* c = where; *c; ++c) signature = (signature ^ uint8_t(*c)) * 16777619u;
        for (const uint32_t value : { uint32_t(result + 2), flags & (unit::kCharSetupPending | 0x20000u),
                                      uint32_t(instance != nullptr), initFlags & 1u, sharedFlags & 6u,
                                      uint32_t(component != nullptr) })
            signature = (signature ^ value) * 16777619u;
        if (signature == watch.signature || g_watchLines >= 96)
            return;
        watch.signature = signature;
        ++g_watchLines;
        WLOG_INFO("unit: setup-trace record=%u where=%s result=%d unit=%p display=%u flags=0x%X flags2=0x%X "
                  "instance=%p initFlags=0x%X shared=%p sharedFlags=0x%X component=%p kicks=%u",
                  g_watchLines, where, result, self, Read<uint32_t>(fields, unit::kOffFieldDisplayId), flags,
                  Read<uint32_t>(fields, unit::kOffFieldFlags2), instance, initFlags, shared, sharedFlags,
                  component, watch.kicks);
        wxl::log::Flush();
    }

    void __fastcall hkUnitPrep(void* self, void* edx, void* a, void* b, void* c)
    {
        if (SetupWatch* watch = Watch(self))
        {
            TraceSetup("prep", self, *watch, 0);
            auto& flags = *reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(self) + unit::kOffUnitCharFlags);
            if (!Read<void*>(self, unit::kOffUnitCharComponent) && Read<void*>(self, unit::kOffUnitInstance)
                && !(flags & (unit::kCharSetupPending | 0x20000u)) && watch->kicks < kSetupKicks)
            {
                TraceSetup("kick", self, *watch, -1);
                ++watch->kicks;
                flags |= unit::kCharSetupPending;
            }
        }
        g_origUnitPrep(self, edx, a, b, c);
    }

    int __fastcall hkCharSetup(void* self, void* edx)
    {
        const int result = g_origCharSetup(self, edx);
        if (SetupWatch* watch = Watch(self))
            TraceSetup("setup", self, *watch, result);
        return result;
    }

    bool InstallUnit()
    {
        wxl::hook::Install("UnitPrep", unit::kUnitPrep, &hkUnitPrep, &g_origUnitPrep);
        wxl::hook::Install("CharSetup", unit::kCharSetup, &hkCharSetup, &g_origCharSetup);
        wxl::hook::Install("CharSetupCreate", unit::kCharSetupCreate, &hkCharSetupCreate, &g_origCharSetupCreate);
        wxl::hook::Install("ObjectUpdate", unit::kObjectUpdateHandler, &hkObjUpdate, &g_origObjUpdate);
        wxl::hook::Install("ObjectDestroy", unit::kObjectDestroyHandler, &hkObjDestroy, &g_origObjDestroy);
        wxl::hook::Install("TargetSet", unit::kTargetSet, &hkTargetSet, &g_origTargetSet);
        return true;
    }
}

WXL_REGISTER_FEATURE("unit", true, InstallUnit)
