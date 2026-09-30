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
#include "game/M2.hpp"
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

    struct ClipSource
    {
        uintptr_t actor, shared = 0, header = 0, rows = 0;
        bool Live() { return (Read<uint32_t>(actor + m2::kOffInstInitFlags) & m2::kInstFlagLive) != 0; }
        bool Shared(char* model, size_t capacity)
        {
            shared = Read<uintptr_t>(actor + m2::kOffInstShared);
            if (!shared) return false;
            // Reuse the proven typed path reader and bound it to the inline field.
            const char* stem = wxl::game::m2::M2Model(reinterpret_cast<void*>(shared)).GetPathStem();
            const size_t bound = m2::kOffModelHeader - m2::kOffModelPathStem;
            const auto end = static_cast<const char*>(std::memchr(stem, 0, bound));
            if (end && end != stem && size_t(end - stem) < capacity)
                std::memcpy(model, stem, size_t(end - stem) + 1);
            return true; // Unavailable stem never changes the original admission rule.
        }
        bool Header() { header = Read<uintptr_t>(shared + m2::kOffModelHeader); return header != 0; }
        bool Table(unsigned& count)
        {
            count = Read<uint32_t>(header + m2::kOffHdrSeqCount);
            rows = Read<uintptr_t>(header + m2::kOffHdrSeqPtr);
            return rows && count && count <= 4096;
        }
        uintptr_t Row(unsigned index) { return rows + index * m2::kSeqStride; }
        unsigned Id(unsigned index) { return Read<uint16_t>(Row(index) + m2::kOffSeqId); }
        unsigned Variation(unsigned index) { return Read<uint16_t>(Row(index) + m2::kOffSeqSubId); }
        motion::ClipRecord Record(unsigned index)
        {
            const auto row = Row(index);
            return {Read<uint16_t>(row + m2::kOffSeqSubId), Read<uint32_t>(row + m2::kOffSeqFlags),
                Read<uint32_t>(row + m2::kOffSeqLength), Read<float>(row + m2::kOffSeqMovingSpeed)};
        }
    };
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
    int Reply(void* lua, bool ok, const char* status, const Actor& actor = {}, const motion::ClipDiagnostic* detail = nullptr)
    {
        char guid[17]{};
        if (actor.guid) std::snprintf(guid, sizeof(guid), "%016llx", static_cast<unsigned long long>(actor.guid));
        script::PushBoolean(lua, ok);
        script::PushString(lua, status);
        script::PushString(lua, guid);
        script::PushNumber(lua, g_session.token);
        script::PushNumber(lua, actor.index + 1);
        if (!detail) return 5;
        char encoded[512]{};
        motion::FormatDiagnostic(*detail, encoded, sizeof(encoded));
        script::PushString(lua, encoded); // Optional sixth value; the original five positions/statuses stay intact.
        return 6;
    }
    int __cdecl Probe(void* lua)
    {
        if (!g_probeReady) return Reply(lua, false, "unavailable");
        const auto self = reinterpret_cast<uintptr_t>(glue::MethodSelf(lua));
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
            ClipSource source{actor.model};
            motion::ClipDiagnostic detail;
            if (!motion::AdmitClip(source, motion::kStand, detail))
                return Reply(lua, false, "unsupported-clips", actor, &detail);
            if (!motion::AdmitClip(source, motion::kSalute, detail))
                return Reply(lua, false, "unsupported-clips", actor, &detail);
            const float salute = detail.record.durationMs / 1000.0f;
            if (!motion::AdmitClip(source, motion::kWalk, detail) || !motion::AdmitWalkSpeed(detail))
                return Reply(lua, false, "unsupported-clips", actor, &detail);
            const float speed = detail.record.speed;
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
        if (g_capacityReady && glue::MethodSelf(lua) == reinterpret_cast<void*>(Read<uintptr_t>(off::kFrame)) &&
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
