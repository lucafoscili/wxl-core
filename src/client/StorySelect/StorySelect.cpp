// Actual selected/equipped actor only; no new model, world unit or network operation.
// GPL-3.0-or-later.
#if defined(WXL_STORY_SELECT_TRIAL) || defined(WXL_CHARACTER_CAPACITY_TRIAL)
#include "Performer.hpp"
#include <cstdio>
#include <cstring>
#include "common/Config.hpp"
#include "common/Log.hpp"
#include "common/Mem.hpp"
#include "engine/hook/Hook.hpp"
#include "engine/hook/Registry.hpp"
#include "game/Glue.hpp"
#include "game/Script.hpp"
#include "offsets/game/M2.hpp"
#include "offsets/game/StorySelect.hpp"
#include "offsets/game/CharacterCapacity.hpp"

namespace
{
    namespace off = wxl::offsets::game::story;
    namespace m2 = wxl::offsets::game::m2;
    namespace glue = wxl::game::glue;
    namespace script = wxl::game::script;
    namespace motion = wxl::story;
    using wxl::mem::Read;
    struct Actor { uintptr_t frame = 0, model = 0; uint64_t guid = 0; int index = -1; };
    struct Session
    {
        Actor actor;
        float origin[16]{};
        float time = 0, salute = 0, speed = 0;
        unsigned clip = motion::kStand;
        uint32_t token = 0;
    };
    Session g_session;
    uint32_t g_generation = 0;
    bool g_ready = false, g_probeReady = false, g_capacityReady = false;
    uint32_t g_rosterRevision = 0;
    using RefreshFn = void (__cdecl*)();
    RefreshFn g_refresh = nullptr;
    glue::RegisterMethodsFn g_register = nullptr;
    script::ValidateCallbackFn g_validate = nullptr;
    script::Function g_select = nullptr;

    Actor Selected()
    {
        Actor result;
        result.frame = Read<uintptr_t>(off::kFrame);
        result.index = Read<int>(off::kSelected);
        const auto count = Read<uint32_t>(off::kCount);
        const auto rows = Read<uintptr_t>(off::kRows);
        if (!result.frame || !rows || result.index < 0 || unsigned(result.index) >= count || count > wxl::offsets::game::capacity::kMaximum)
            return {};
        const auto row = rows + result.index * off::kRowStride;
        result.guid = Read<uint64_t>(row);
        const auto customization = Read<uintptr_t>(row + off::kCustomization);
        if (customization) result.model = Read<uintptr_t>(customization + off::kActor);
        return result;
    }
    bool Same(const Actor& a, const Actor& b)
    { return a.model && a.guid && a.model == b.model && a.guid == b.guid && a.frame == b.frame && a.index == b.index; }

    bool Clip(uintptr_t actor, unsigned id, float& seconds, float& speed)
    {
        if (!(Read<uint32_t>(actor + m2::kOffInstInitFlags) & m2::kInstFlagLive)) return false;
        const auto shared = Read<uintptr_t>(actor + m2::kOffInstShared);
        if (!shared) return false;
        const auto header = Read<uintptr_t>(shared + m2::kOffModelHeader);
        if (!header) return false;
        const auto count = Read<uint32_t>(header + m2::kOffHdrSeqCount);
        const auto rows = Read<uintptr_t>(header + m2::kOffHdrSeqPtr);
        if (!rows || !count || count > 4096) return false;
        uintptr_t chosen = 0;
        unsigned matches = 0;
        for (unsigned i = 0; i < count; ++i)
        {
            const auto row = rows + i * m2::kSeqStride;
            if (Read<uint16_t>(row + m2::kOffSeqId) != id) continue;
            ++matches;
            if (Read<uint16_t>(row + m2::kOffSeqSubId) == 0) chosen = row;
        }
        // The native variation picker must not choose a different duration/speed.
        if (!chosen || (id != motion::kStand && matches != 1)) return false;
        // No fallback/alias may silently turn a requested action into idle.
        if (Read<uint32_t>(chosen + m2::kOffSeqFlags) & 0x40) return false;
        seconds = Read<uint32_t>(chosen + m2::kOffSeqLength) / 1000.0f;
        speed = Read<float>(chosen + m2::kOffSeqMovingSpeed);
        return std::isfinite(seconds) && seconds > 0 && seconds <= 15 && std::isfinite(speed);
    }
    void Sequence(uintptr_t actor, unsigned clip)
    {
        // Same arguments as native selected-actor initialization, except verified clip ID.
        wxl::game::Native<m2::M2_SetBoneSequenceFn>(m2::kSetBoneSequence)(
            reinterpret_cast<void*>(actor), nullptr, uint32_t(-1), clip, uint32_t(-1), 0, 1.0f, 1, 1);
    }
    void Place(uintptr_t actor, const float matrix[16])
    {
        // Exact inline placement/dirty contract of native SetWorldTransformSimple.
        // Keep parent/attachment, camera, scene and equipment completely native.
        std::memcpy(reinterpret_cast<void*>(actor + m2::kOffInstPlacement), matrix, 16 * sizeof(float));
        *reinterpret_cast<uint32_t*>(actor + m2::kOffInstInitFlags) |= 0x8000;
    }
    void Stop()
    {
        const Session old = g_session;
        g_session = {};
        ++g_generation; // invalidate before any restoration or native callback
        if (old.token && Same(Selected(), old.actor))
        {
            Place(old.actor.model, old.origin);
            Sequence(old.actor.model, motion::kStand);
        }
    }
    int Reply(void* lua, bool ok, const char* status, const Actor& actor = {})
    {
        char guid[17]{};
        if (actor.guid) std::snprintf(guid, sizeof(guid), "%016llx", static_cast<unsigned long long>(actor.guid));
        script::PushBoolean(lua, ok);
        script::PushString(lua, status);
        script::PushString(lua, guid);
        script::PushNumber(lua, g_session.token);
        script::PushNumber(lua, actor.index + 1);
        return 5;
    }
    int __cdecl Probe(void* lua)
    {
        if (!g_probeReady) return Reply(lua, false, "unavailable");
        const auto self = reinterpret_cast<uintptr_t>(glue::MethodSelf());
        if (!self || self != Read<uintptr_t>(off::kFrame)) return Reply(lua, false, "wrong-frame");
        const char* action = script::ToString(lua, 2);
        if (!action) return Reply(lua, false, "missing-action");
        if (!std::strcmp(action, "stop")) { Stop(); return Reply(lua, true, "stock"); }
        const Actor actor = Selected();
        if (!actor.model || !actor.guid) { Stop(); return Reply(lua, false, "actor-not-ready", actor); }
        if (!std::strcmp(action, "inspect")) return Reply(lua, true, "selected", actor);
        if (!std::strcmp(action, "begin"))
        {
            Stop();
            float stand = 0, unused = 0, salute = 0, speed = 0, walk = 0;
            if (!Clip(actor.model, motion::kStand, stand, unused) ||
                !Clip(actor.model, motion::kSalute, salute, unused) ||
                !Clip(actor.model, motion::kWalk, walk, speed) || speed <= 0 || speed > 8)
                return Reply(lua, false, "unsupported-clips", actor);
            Session next;
            next.actor = actor; next.salute = salute; next.speed = speed;
            std::memcpy(next.origin, reinterpret_cast<void*>(actor.model + m2::kOffInstPlacement), sizeof(next.origin));
            for (float value : next.origin) if (!std::isfinite(value)) return Reply(lua, false, "invalid-placement", actor);
            next.clip = motion::kSalute; next.token = ++g_generation;
            g_session = next;
            Sequence(actor.model, next.clip);
            return Reply(lua, true, "salute", actor);
        }
        if (std::strcmp(action, "step")) return Reply(lua, false, "unknown-action", actor);
        const double token = script::ToNumber(lua, 3), delta = script::ToNumber(lua, 4);
        // A stale callback cannot stop or move a newer selection's performance.
        if (!g_session.token || token != g_session.token) return Reply(lua, false, "stale-generation", actor);
        if (!Same(actor, g_session.actor) || !motion::ValidDelta(delta))
        { Stop(); return Reply(lua, false, "interrupted", actor); }
        g_session.time += static_cast<float>(std::min(delta, 0.05));
        const auto sample = motion::Evaluate(g_session.time, g_session.salute, g_session.speed);
        if (sample.done) { Stop(); return Reply(lua, true, "stock", actor); }
        if (sample.clip != g_session.clip) { Sequence(actor.model, sample.clip); g_session.clip = sample.clip; }
        float placed[16]; motion::Placement(placed, g_session.origin, sample); Place(actor.model, placed);
        return Reply(lua, true, sample.clip == motion::kSalute ? "salute" : (sample.returning ? "return" : "walk"), actor);
    }
    // Read the live native row, never a copied roster or persisted identity mapping.
    int __cdecl Identity(void* lua)
    {
        const double requested = script::ToNumber(lua, 2);
        const auto count = Read<uint32_t>(off::kCount);
        const auto rows = Read<uintptr_t>(off::kRows);
        char guid[17]{};
        if (g_capacityReady && glue::MethodSelf() == reinterpret_cast<void*>(Read<uintptr_t>(off::kFrame)) &&
            rows && count <= wxl::offsets::game::capacity::kMaximum &&
            requested >= 1 && requested <= count && requested == std::floor(requested))
        {
            const auto value = Read<uint64_t>(rows + (static_cast<unsigned>(requested) - 1) * off::kRowStride);
            if (value) std::snprintf(guid, sizeof(guid), "%016llx", static_cast<unsigned long long>(value));
        }
        script::PushString(lua, guid);
        script::PushNumber(lua, g_rosterRevision);
        script::PushNumber(lua, count);
        script::PushNumber(lua, g_capacityReady ? wxl::offsets::game::capacity::kMaximum : 10);
        return 4;
    }
    void __cdecl Refresh()
    {
        Stop(); // restore while old rows still own their actors
        ++g_rosterRevision; // distinguish native refresh selection from user selection
        g_refresh();
    }
    void __cdecl Register(void* target)
    {
        g_register(target);
        static const glue::Method probe[] = {{"WXLStorySelectProbe", Probe}};
        static const glue::Method identity[] = {{"WXLRosterIdentity", Identity}};
        if (g_probeReady) glue::AddMethods(target, probe, 1);
        if (g_capacityReady) glue::AddMethods(target, identity, 1);
    }
    void __cdecl Validate(uintptr_t function)
    {
        if ((g_probeReady && function == reinterpret_cast<uintptr_t>(&Probe)) ||
            (g_capacityReady && function == reinterpret_cast<uintptr_t>(&Identity))) return;
        g_validate(function); // never broaden the callback permission to the entire DLL
    }
    int __cdecl Select(void* lua)
    {
        if (g_ready) Stop(); // original actor still owned, before native changes/freeing
        return g_select(lua);
    }
    bool Install()
    {
        bool probe = false, capacity = false;
#ifdef WXL_STORY_SELECT_TRIAL
        probe = wxl::config::Env("WXL_STORY_SELECT", true);
#endif
#ifdef WXL_CHARACTER_CAPACITY_TRIAL
        capacity = wxl::config::Env("WXL_CHARACTER_CAPACITY", true);
#endif
        if (!probe && !capacity) return true;
        if (reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)) != 0x400000) return false;
        const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(0x400000);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 0 || dos->e_lfanew > 0x1000) return false;
        const auto pe = reinterpret_cast<const IMAGE_NT_HEADERS*>(0x400000 + dos->e_lfanew);
        if (pe->Signature != IMAGE_NT_SIGNATURE || pe->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
            pe->OptionalHeader.SizeOfImage < 0x00DCE42C - 0x400000) return false;
        for (const auto& site : off::kSites)
        {
            uint64_t value = 0xcbf29ce484222325ULL;
            for (size_t i = 0; i < site.size; ++i) value = (value ^ Read<uint8_t>(site.address + i)) * 0x100000001b3ULL;
            if (value != site.hash) { WLOG_WARN("story-select: incompatible %s; stock only", site.name); return false; }
        }
        if (capacity)
        {
            for (const auto& site : wxl::offsets::game::capacity::kSites)
            {
                uint64_t value = 0xcbf29ce484222325ULL;
                for (size_t i = 0; i < site.size; ++i) value = (value ^ Read<uint8_t>(site.address + i)) * 0x100000001b3ULL;
                if (value != site.hash) { WLOG_WARN("character-capacity: incompatible %s; stock only", site.name); return false; }
            }
        }
        if (!wxl::hook::Install("StorySelectValidate", script::kValidateCallbackSeam, &Validate, &g_validate) ||
            !wxl::hook::Install("StorySelectSelection", off::kSelectCharacter, &Select, &g_select) ||
            !wxl::hook::Install("StorySelectRefresh", off::kRefresh, &Refresh, &g_refresh) ||
            !wxl::hook::Install("StorySelectMethods", glue::kRegisterMethods, &Register, &g_register)) return false;
        // Boot's EnableAll publishes the complete shared hook chains once.
        if (capacity)
        {
            const uint8_t maximum = wxl::offsets::game::capacity::kMaximum;
            if (!wxl::mem::Patch(reinterpret_cast<void*>(wxl::offsets::game::capacity::kLimitImmediate), &maximum, 1))
                return false;
        }
        g_probeReady = probe;
        g_capacityReady = capacity;
        g_ready = true;
        WLOG_INFO("story-select: capacity50=%d actor-probe=%d; native acceptance pending", capacity, probe);
        return true;
    }
}
// Register methods before the client's Glue metatables are built.
WXL_REGISTER_FEATURE_PHASED("story-select", true, Install, ::wxl::hook::Phase::Boot)
#endif
