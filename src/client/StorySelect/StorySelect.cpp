// Actual roster residents through native equipment policy; no world unit or network operation.
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
    // Only the stock initializer and its lighting callback consume this scoped row.
    // TLS keeps native composition/other threads on their real selected index.
    struct RowScope;
    __declspec(thread) RowScope* g_rowScope = nullptr;
    struct RowScope
    {
        int index;
        bool initializing;
        float facingSink = 0;
        uint8_t selectorSink[0x130]; // initializer only writes receiver+0x12C; never reads it
        RowScope* previous;
        RowScope(int row, bool initialize) : index(row), initializing(initialize), previous(g_rowScope)
        { g_rowScope = this; }
        ~RowScope() { g_rowScope = previous; }
        RowScope(const RowScope&) = delete;
    };
    bool InitializingRow() { return g_rowScope && g_rowScope->initializing; }
    __declspec(noinline) int __cdecl ContextIndex()
    { return g_rowScope ? g_rowScope->index : Read<int>(off::kSelected); }
    __declspec(noinline) float* __cdecl FacingTarget()
    { return InitializingRow() ? &g_rowScope->facingSink : reinterpret_cast<float*>(0xB6B204); }
    // PUSHAD layout: EDI, ESI, EBP, ESP, EBX, EDX, ECX, EAX. Preserve flags and every
    // register except the MOV's original destination, and leave the x87 stack intact.
#define ROW_READER(name, savedOffset) \
    __declspec(naked) void name() { \
        __asm { pushfd } __asm { pushad } __asm { call ContextIndex } \
        __asm { mov [esp + savedOffset], eax } __asm { popad } __asm { popfd } __asm { ret } }
    ROW_READER(ReadRowEAX,28)
    ROW_READER(ReadRowEBX,16)
    ROW_READER(ReadRowECX,24)
    ROW_READER(ReadRowEDX,20)
    ROW_READER(ReadRowESI,4)
#undef ROW_READER
    __declspec(naked) void ResetFacing()
    {
        __asm { pushfd }
        __asm { pushad }
        __asm { call FacingTarget }
        __asm { fstp dword ptr [eax] }
        __asm { popad }
        __asm { popfd }
        __asm { ret }
    }
    using InitializeFn = void(__cdecl*)();
    InitializeFn g_initialize = nullptr;
    void StopResidents();
    void __cdecl InitializeSelected()
    {
        // A normal/nested engine entry always keeps the original selection meaning.
        StopResidents();
        auto previous = g_rowScope;
        g_rowScope = nullptr;
        g_initialize();
        g_rowScope = previous;
    }
    void __fastcall InitializeDetach(void* parent, void*, uint32_t slot)
    { if (!InitializingRow()) wxl::game::m2::DetachSlot(parent,slot); }
    void __cdecl InitializeFacing(float facing)
    {
        if (!InitializingRow())
            wxl::game::Native<void(__cdecl*)(float)>(0x4E2E70)(facing);
    }
    void __cdecl InitializeCurve(void* curve)
    {
        if (!InitializingRow())
            wxl::game::Native<void(__cdecl*)(void*)>(0x8C02E0)(curve);
    }
    void* __cdecl InitializeSelectorState()
    {
        return InitializingRow() ? g_rowScope->selectorSink :
            wxl::game::Native<void*(__cdecl*)()>(0x7ECEF0)();
    }
    void __cdecl InitializeFade(void* component, int created)
    {
        // Extra residents use their per-component nonblocking readiness path in Step.
        // Never hide/change the selected actor or queue the shared selector fade.
        if (!InitializingRow())
            wxl::game::Native<void(__cdecl*)(void*,int)>(0x4E6AE0)(component,created);
    }
    struct Redirect { uintptr_t site, target; uint8_t size; };
    constexpr size_t kRowReads = sizeof(off::kRowReads)/sizeof(off::kRowReads[0]);
    constexpr size_t kRedirects = kRowReads + 8;
    Redirect ResidentRedirect(size_t index)
    {
        if (index < kRowReads)
        {
            const auto& row = off::kRowReads[index];
            void(*reader)() = nullptr;
            switch (row.target)
            {
                case off::RowRegister::EAX: reader=ReadRowEAX; break;
                case off::RowRegister::EBX: reader=ReadRowEBX; break;
                case off::RowRegister::ECX: reader=ReadRowECX; break;
                case off::RowRegister::EDX: reader=ReadRowEDX; break;
                case off::RowRegister::ESI: reader=ReadRowESI; break;
            }
            return {row.address,reinterpret_cast<uintptr_t>(reader),row.size};
        }
        const Redirect sideEffects[] = {
            {0x4E3D1C,reinterpret_cast<uintptr_t>(InitializeDetach),5},
            {0x4E3D3E,reinterpret_cast<uintptr_t>(InitializeDetach),5},
            {0x4E4474,reinterpret_cast<uintptr_t>(InitializeFacing),5},
            {0x4E440F,reinterpret_cast<uintptr_t>(InitializeCurve),5},
            {0x4E4423,reinterpret_cast<uintptr_t>(InitializeCurve),5},
            {0x4E43E3,reinterpret_cast<uintptr_t>(InitializeSelectorState),5},
            {0x4E44CB,reinterpret_cast<uintptr_t>(InitializeFade),5},
            {0x4E3D56,reinterpret_cast<uintptr_t>(ResetFacing),6},
        };
        return sideEffects[index-kRowReads];
    }
    void RedirectBytes(const Redirect& patch, uint8_t bytes[6])
    {
        const uint32_t displacement = uint32_t(patch.target - patch.site - 5);
        bytes[0]=0xE8; std::memcpy(bytes+1,&displacement,4); bytes[5]=0x90;
    }
    bool InstallResidentRedirects()
    {
        uint8_t original[kRedirects][6]{};
        size_t applied=0;
        for (; applied<kRedirects; ++applied)
        {
            const auto patch=ResidentRedirect(applied);
            std::memcpy(original[applied],reinterpret_cast<void*>(patch.site),patch.size);
            uint8_t bytes[6]; RedirectBytes(patch,bytes);
            if (!wxl::mem::Patch(reinterpret_cast<void*>(patch.site),bytes,patch.size)) break;
        }
        if (applied==kRedirects) return true;
        while (applied)
        {
            const auto patch=ResidentRedirect(--applied);
            if (!wxl::mem::Patch(reinterpret_cast<void*>(patch.site),original[applied],patch.size))
                WLOG_ERROR("story-residents: redirect rollback failed at %08x",unsigned(patch.site));
        }
        return false;
    }
    struct Actor
    {
        uintptr_t frame = 0, model = 0, component = 0, mount = 0;
        uint64_t guid = 0; int index = -1;
    };
    struct Session
    {
        Actor actor;
        float origin[16]{};
        float time = 0, salute = 0;
        motion::ProbeRoute route = motion::kG1ProbeRoute;
        motion::ClipRecord walkMetadata; // Keep raw source speed, including zero; never use it for travel.
        unsigned clip = motion::kStand;
        uint32_t token = 0;
    };
    Session g_session;
    uint32_t g_generation = 0;
    bool g_ready = false, g_probeReady = false, g_capacityReady = false, g_residentsReady = false;
    uint32_t g_rosterRevision = 0;
    using RefreshFn = void (__cdecl*)();
    RefreshFn g_refresh = nullptr;
    glue::RegisterMethodsFn g_register = nullptr;
    script::ValidateCallbackFn g_validate = nullptr;
    script::Function g_select = nullptr;

    Actor Resident(int index)
    {
        Actor result;
        result.frame = Read<uintptr_t>(off::kFrame);
        result.index = index;
        const auto count = Read<uint32_t>(off::kCount);
        const auto rows = Read<uintptr_t>(off::kRows);
        if (!result.frame || !rows || result.index < 0 || unsigned(result.index) >= count || count > wxl::offsets::game::capacity::kMaximum)
            return {};
        const auto row = rows + result.index * off::kRowStride;
        result.guid = Read<uint64_t>(row);
        result.component = Read<uintptr_t>(row + off::kCustomization);
        result.mount = Read<uintptr_t>(row + off::kMount);
        if (result.component) result.model = Read<uintptr_t>(result.component + off::kActor);
        return result;
    }
    Actor Selected() { return Resident(Read<int>(off::kSelected)); }
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

    // Two native cache owners, in the same native background. These records own only
    // temporary placement/activity and the extra resident's lighting callback binding.
    // The native row continues to own component/model memory, including cold-row caches.
    using LightingFn = void(__cdecl*)(void*,void*,void*);
    struct ResidentState
    {
        Actor actor;
        float origin[2][16]{};
        uintptr_t lighting[2]{}, userData[2]{};
        unsigned clip = motion::kStand;
        float saluteSeconds = 0, remaining = 0;
    };
    struct Residents
    {
        ResidentState members[2];
        uintptr_t rows = 0, background = 0;
        uint32_t revision = 0, token = 0;
        float waiting = 0;
        bool placed = false;
        uintptr_t trialCamera = 0;
        unsigned cameraDuration = 0;
        float cameraTime = 0;
    };
    Residents g_residents;
    void StopCamera(Residents& owner)
    {
        if (!owner.trialCamera) return;
        const auto camera=owner.trialCamera;
        owner.trialCamera=0; owner.cameraTime=0; owner.cameraDuration=0;
        const auto frame=owner.members[0].actor.frame;
        // Do not dereference an old background/model after native replacement.
        if (frame!=Read<uintptr_t>(off::kFrame) || !frame || Read<uintptr_t>(frame+off::kBackground)!=owner.background) return;
        wxl::game::Native<void(__fastcall*)(void*,void*,unsigned)>(off::kFrameSetSequence)(reinterpret_cast<void*>(frame),nullptr,0);
        if (Read<uintptr_t>(frame+off::kFrameCamera)==camera)
            wxl::game::Native<void(__fastcall*)(void*,void*,unsigned)>(off::kFrameSetCamera)(reinterpret_cast<void*>(frame),nullptr,0);
    }
    bool LiveResident(const Residents& owner, const Actor& actor)
    {
        // Never dereference captured model pointers after native rows/frame changed.
        if (owner.rows!=Read<uintptr_t>(off::kRows) || owner.revision!=g_rosterRevision ||
            actor.frame!=Read<uintptr_t>(off::kFrame)) return false;
        const Actor current=Resident(actor.index);
        return Same(current,actor) && current.component==actor.component && current.mount==actor.mount;
    }
    void __cdecl ResidentLighting(void* instance, void* lighting, void* userData)
    {
        const auto& owner = g_residents;
        const auto& resident = owner.members[1];
        if (!owner.token || userData != &g_residents.members[1] || !LiveResident(owner,resident.actor)) return;
        const unsigned part = reinterpret_cast<uintptr_t>(instance) == resident.actor.model ? 0 : 1;
        if (part && reinterpret_cast<uintptr_t>(instance) != resident.actor.mount) return;
        if (!resident.lighting[part]) return;
        RowScope scope(resident.actor.index,false);
        wxl::game::Native<LightingFn>(resident.lighting[part])(
            instance,lighting,reinterpret_cast<void*>(resident.userData[part]));
    }
    void StopResidents()
    {
        StopCamera(g_residents); // relinquish time before native actors/background change
        const Residents old = g_residents;
        g_residents = {}; // invalidate callbacks before restoring native bindings
        if (!old.token) return;
        ++g_generation;
        for (unsigned member=0; member<2; ++member)
        {
            const auto& resident=old.members[member];
            if (!LiveResident(old,resident.actor)) continue;
            const uintptr_t parts[] = {resident.actor.model,resident.actor.mount};
            for (unsigned part=0; part<2; ++part)
            {
                const auto model=parts[part];
                if (!model) continue;
                if (member && Read<uintptr_t>(model+m2::kOffInstLightingCallbackFn)==reinterpret_cast<uintptr_t>(ResidentLighting) &&
                    Read<uintptr_t>(model+m2::kOffInstLightingUserData)==reinterpret_cast<uintptr_t>(&g_residents.members[1]))
                {
                    *reinterpret_cast<uintptr_t*>(model+m2::kOffInstLightingCallbackFn)=resident.lighting[part];
                    *reinterpret_cast<uintptr_t*>(model+m2::kOffInstLightingUserData)=resident.userData[part];
                }
                if (old.placed) Place(model,resident.origin[part]);
                if (member && Read<uintptr_t>(model+m2::kOffInstParent)==old.background)
                    wxl::game::Native<void(__fastcall*)(void*,void*)>(off::kDetachParent)(reinterpret_cast<void*>(model),nullptr);
            }
            if (old.placed) Sequence(resident.actor.model,motion::kStand);
        }
    }
    bool ResidentsValid()
    {
        return g_residents.token && Same(Selected(),g_residents.members[0].actor) &&
            LiveResident(g_residents,g_residents.members[0].actor) && LiveResident(g_residents,g_residents.members[1].actor) &&
            Read<uintptr_t>(g_residents.members[0].actor.frame+off::kBackground)==g_residents.background;
    }
    const char* BeginCamera(double token, const char* stem, double duration)
    {
        if (!g_residents.token || token!=g_residents.token) return "stale-generation";
        if (!ResidentsValid()) { StopResidents(); return "interrupted"; }
        if (!g_residents.placed) return "loading";
        const auto background=g_residents.background, frame=g_residents.members[0].actor.frame;
        ClipSource source{background}; motion::ClipDiagnostic detail;
        char model[256]{};
        if (!stem || !motion::AdmitClip(source,motion::kStand,detail) || !source.Shared(model,sizeof(model)) ||
            _stricmp(model,stem) || Read<unsigned>(source.header+off::kHeaderCameraCount)!=2 ||
            !std::isfinite(duration) || duration<=0 || duration!=std::floor(duration) || duration>=detail.record.durationMs)
            return "camera-unavailable";
        StopCamera(g_residents);
        using CameraFn=void*(__fastcall*)(void*,void*,unsigned);
        const auto original=reinterpret_cast<uintptr_t>(wxl::game::Native<CameraFn>(m2::kGetCameraByIndex)(reinterpret_cast<void*>(background),nullptr,0));
        const auto trial=reinterpret_cast<uintptr_t>(wxl::game::Native<CameraFn>(m2::kGetCameraByIndex)(reinterpret_cast<void*>(background),nullptr,1));
        if (!original || !trial || Read<uintptr_t>(frame+off::kFrameCamera)!=original) return "camera-unavailable";
        wxl::game::Native<void(__fastcall*)(void*,void*,unsigned)>(off::kFrameSetCamera)(reinterpret_cast<void*>(frame),nullptr,1);
        if (Read<uintptr_t>(frame+off::kFrameCamera)!=trial) return "camera-unavailable";
        g_residents.trialCamera=trial; g_residents.cameraDuration=unsigned(duration); g_residents.cameraTime=0;
        wxl::game::Native<void(__fastcall*)(void*,void*,unsigned,unsigned)>(off::kFrameSetSequenceTime)(reinterpret_cast<void*>(frame),nullptr,0,0);
        return "camera";
    }
    const char* BeginResidents(int index)
    {
        Stop(); StopResidents();
        const Actor selected=Selected(), extra=Resident(index);
        if (!g_residentsReady || !g_initialize) return "unavailable";
        if (!selected.model || !selected.guid || !extra.guid) return "actor-not-ready";
        if (index==selected.index || selected.guid==extra.guid) return "same-resident";
        const auto background=Read<uintptr_t>(selected.frame+off::kBackground);
        if (!background || Read<uintptr_t>(selected.model+m2::kOffInstParent)!=background) return "scene-not-ready";
        if (extra.model && Read<uintptr_t>(extra.model+m2::kOffInstParent)) return "resident-already-attached";
        const auto rows=Read<uintptr_t>(off::kRows);
        const auto revision=g_rosterRevision, generation=++g_generation;
        { RowScope scope(index,true); g_initialize(); } // exact native equipment policy; no selection write
        const Actor initialized=Resident(index);
        if (rows!=Read<uintptr_t>(off::kRows) || revision!=g_rosterRevision || generation!=g_generation ||
            !Same(selected,Selected()) || initialized.guid!=extra.guid || initialized.frame!=extra.frame)
            return "interrupted";
        if (!initialized.model) return "initializer-failed";
        if (initialized.model==selected.model) return "same-model";
        g_residents.rows=rows; g_residents.revision=revision;
        g_residents.background=background; g_residents.token=generation;
        g_residents.members[0].actor=selected; g_residents.members[1].actor=initialized;
        for (unsigned member=0; member<2; ++member)
        {
            auto& resident=g_residents.members[member];
            const uintptr_t parts[] = {resident.actor.model,resident.actor.mount};
            for (unsigned part=0; part<2; ++part)
            {
                const auto model=parts[part]; if (!model) continue;
                if (Read<uintptr_t>(model+m2::kOffInstParent)!=background) { StopResidents(); return "attachment-failed"; }
                std::memcpy(resident.origin[part],reinterpret_cast<void*>(model+m2::kOffInstPlacement),sizeof(resident.origin[part]));
                for (float value : resident.origin[part])
                    if (!std::isfinite(value)) { StopResidents(); return "invalid-placement"; }
                if (!member) continue;
                resident.lighting[part]=Read<uintptr_t>(model+m2::kOffInstLightingCallbackFn);
                resident.userData[part]=Read<uintptr_t>(model+m2::kOffInstLightingUserData);
                if (resident.lighting[part] && resident.lighting[part]!=off::kLighting)
                { StopResidents(); return "lighting-unavailable"; }
                *reinterpret_cast<uintptr_t*>(model+m2::kOffInstLightingUserData)=reinterpret_cast<uintptr_t>(&resident);
                *reinterpret_cast<uintptr_t*>(model+m2::kOffInstLightingCallbackFn)=reinterpret_cast<uintptr_t>(ResidentLighting);
            }
        }
        return "loading";
    }
    const char* StepResidents(double token, double delta)
    {
        if (!g_residents.token || token!=g_residents.token) return "stale-generation";
        if (!ResidentsValid() || !motion::ValidDelta(delta)) { StopResidents(); return "interrupted"; }
        if (!g_residents.placed)
        {
            const auto& extra=g_residents.members[1].actor;
            // Stock native per-component compose/geoset path, without shared UI fade or blocking waits.
            const bool prepared=wxl::game::Native<bool(__fastcall*)(void*,void*,int)>(m2::kCharRenderPrep)(
                reinterpret_cast<void*>(extra.component),nullptr,0);
            bool drawable=prepared;
            for (const auto& resident : g_residents.members)
                for (uintptr_t model : {resident.actor.model,resident.actor.mount})
                    if (model && !wxl::game::Native<m2::M2_IsDrawableFn>(m2::kIsDrawable)(reinterpret_cast<void*>(model),nullptr,0,1)) drawable=false;
            g_residents.waiting+=static_cast<float>(delta);
            if (!drawable)
            {
                if (g_residents.waiting>15) { StopResidents(); return "loading-timeout"; }
                return "loading";
            }
            for (auto& resident : g_residents.members)
            {
                ClipSource source{resident.actor.model}; motion::ClipDiagnostic detail;
                if (!motion::AdmitClip(source,motion::kStand,detail) || !motion::AdmitClip(source,motion::kSalute,detail))
                { StopResidents(); return "unsupported-clips"; }
                resident.saluteSeconds=detail.record.durationMs/1000.0f;
            }
            // Author a small side-by-side offset in the captured selected actor's local Y basis.
            // Keep each native model's basis/scale, including any mount, and leave the camera alone.
            for (unsigned member=0; member<2; ++member)
            {
                const auto& resident=g_residents.members[member];
                const uintptr_t parts[] = {resident.actor.model,resident.actor.mount};
                const auto& center=g_residents.members[0].origin[0];
                const float offset=(member ? 0.5f : -0.5f)*motion::kG1ProbeRoute.legDistance;
                for (unsigned part=0; part<2; ++part)
                {
                    if (!parts[part]) continue;
                    float placed[16]; std::memcpy(placed,resident.origin[part],sizeof(placed));
                    for (unsigned axis=0; axis<3; ++axis)
                        placed[12+axis]+=center[12+axis]-resident.origin[0][12+axis]+center[4+axis]*offset;
                    Place(parts[part],placed);
                }
                Sequence(resident.actor.model,motion::kStand);
            }
            g_residents.placed=true;
        }
        for (auto& resident : g_residents.members)
        {
            if (resident.clip!=motion::kSalute) continue;
            resident.remaining-=motion::ClampedDelta(delta);
            if (resident.remaining<=0) { Sequence(resident.actor.model,motion::kStand); resident.clip=motion::kStand; }
        }
        if (g_residents.trialCamera)
        {
            g_residents.cameraTime+=motion::ClampedDelta(delta)*1000;
            const auto frame=g_residents.members[0].actor.frame;
            if (g_residents.cameraTime>=g_residents.cameraDuration || Read<uintptr_t>(frame+off::kFrameCamera)!=g_residents.trialCamera)
                StopCamera(g_residents);
            else
                wxl::game::Native<void(__fastcall*)(void*,void*,unsigned,unsigned)>(off::kFrameSetSequenceTime)(
                    reinterpret_cast<void*>(frame),nullptr,0,unsigned(g_residents.cameraTime));
        }
        return "ready";
    }
    const char* ActResidents(double token, int index, bool salute)
    {
        if (!g_residents.token || token!=g_residents.token) return "stale-generation";
        if (!ResidentsValid()) { StopResidents(); return "interrupted"; }
        if (!g_residents.placed) return "loading";
        for (auto& resident : g_residents.members)
        {
            if (resident.actor.index!=index) continue;
            resident.clip=salute ? motion::kSalute : motion::kStand;
            resident.remaining=salute ? resident.saluteSeconds : 0;
            Sequence(resident.actor.model,resident.clip);
            return salute ? "salute" : "stand";
        }
        return "wrong-resident";
    }
    int __cdecl ResidentsMethod(void* lua)
    {
        const char* status="unavailable";
        const char* action=script::ToString(lua,2);
        if (g_residentsReady && glue::MethodSelf(lua)==reinterpret_cast<void*>(Read<uintptr_t>(off::kFrame)) && action)
        {
            if (!std::strcmp(action,"stop")) { StopResidents(); status="stock"; }
            else if (!std::strcmp(action,"step")) status=StepResidents(script::ToNumber(lua,3),script::ToNumber(lua,4));
            else if (!std::strcmp(action,"camera")) status=BeginCamera(script::ToNumber(lua,3),script::ToString(lua,4),script::ToNumber(lua,5));
            else if (!std::strcmp(action,"camera-stop"))
            {
                if (g_residents.token && script::ToNumber(lua,3)==g_residents.token)
                { StopCamera(g_residents); status="ready"; }
                else status="stale-generation";
            }
            else
            {
                const double requested=script::ToNumber(lua,!std::strcmp(action,"begin") ? 3 : 4);
                if (std::isfinite(requested) && requested>=1 && requested<=wxl::offsets::game::capacity::kMaximum && requested==std::floor(requested))
                {
                    if (!std::strcmp(action,"begin")) status=BeginResidents(int(requested)-1);
                    else if (!std::strcmp(action,"salute") || !std::strcmp(action,"stand"))
                        status=ActResidents(script::ToNumber(lua,3),int(requested)-1,!std::strcmp(action,"salute"));
                    else status="unknown-action";
                }
                else status="wrong-resident";
            }
        }
        const bool ok=!std::strcmp(status,"stock") || !std::strcmp(status,"ready") || !std::strcmp(status,"loading") ||
            !std::strcmp(status,"salute") || !std::strcmp(status,"stand") || !std::strcmp(status,"camera");
        script::PushBoolean(lua,ok); script::PushString(lua,status); script::PushNumber(lua,g_residents.token);
        for (const auto& resident : g_residents.members)
        {
            char guid[17]{};
            if (resident.actor.guid) std::snprintf(guid,sizeof(guid),"%016llx",static_cast<unsigned long long>(resident.actor.guid));
            script::PushString(lua,guid); script::PushNumber(lua,resident.actor.index+1);
        }
        script::PushBoolean(lua,g_residents.trialCamera!=0);
        return 8;
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
            StopResidents();
            Stop();
            ClipSource source{actor.model};
            motion::ClipDiagnostic detail;
            if (!motion::AdmitClip(source, motion::kStand, detail))
                return Reply(lua, false, "unsupported-clips", actor, &detail);
            if (!motion::AdmitClip(source, motion::kSalute, detail))
                return Reply(lua, false, "unsupported-clips", actor, &detail);
            const float salute = detail.record.durationMs / 1000.0f;
            if (!motion::AdmitClip(source, motion::kWalk, detail))
                return Reply(lua, false, "unsupported-clips", actor, &detail);
            Session next;
            next.actor = actor; next.salute = salute; next.walkMetadata = detail.record;
            if (!motion::ValidRoute(next.route)) return Reply(lua, false, "invalid-route", actor);
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
        g_session.time += motion::ClampedDelta(delta);
        const auto sample = motion::Evaluate(g_session.time, g_session.salute, g_session.route);
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
        StopResidents();
        Stop(); // restore while old rows still own their actors
        ++g_rosterRevision; // distinguish native refresh selection from user selection
        g_refresh();
    }
    void __cdecl Register(void* target)
    {
        g_register(target);
        static const glue::Method probe[] = {{"WXLStorySelectProbe", Probe}};
        static const glue::Method identity[] = {{"WXLRosterIdentity", Identity}};
        static const glue::Method residents[] = {{"WXLStorySelectResidents", ResidentsMethod}};
        if (g_probeReady) glue::AddMethods(target, probe, 1);
        if (g_capacityReady) glue::AddMethods(target, identity, 1);
        if (g_residentsReady) glue::AddMethods(target, residents, 1);
    }
    void __cdecl Validate(uintptr_t function)
    {
        if ((g_probeReady && function == reinterpret_cast<uintptr_t>(&Probe)) ||
            (g_capacityReady && function == reinterpret_cast<uintptr_t>(&Identity))) return;
        if (g_residentsReady && function == reinterpret_cast<uintptr_t>(&ResidentsMethod)) return;
        g_validate(function); // never broaden the callback permission to the entire DLL
    }
    int __cdecl Select(void* lua)
    {
        if (g_ready) { StopResidents(); Stop(); } // original actors still owned, before native changes/freeing
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
        if (probe && wxl::config::Env("WXL_STORY_RESIDENTS",true))
        {
            bool compatible=true;
            for (const auto& site : off::kResidentSites)
            {
                uint64_t value=0xcbf29ce484222325ULL;
                for (size_t i=0; i<site.size; ++i) value=(value^Read<uint8_t>(site.address+i))*0x100000001b3ULL;
                if (value!=site.hash) { WLOG_WARN("story-residents: incompatible %s; pair unavailable",site.name); compatible=false; }
            }
            if (compatible && wxl::hook::Install("StorySelectInitializer",off::kInitialize,&InitializeSelected,&g_initialize))
                g_residentsReady=InstallResidentRedirects();
        }
        WLOG_INFO("story-select: capacity50=%d actor-probe=%d residents=%d; native acceptance pending", capacity, probe,g_residentsReady);
        return true;
    }
}
// Register methods before the client's Glue metatables are built.
WXL_REGISTER_FEATURE_PHASED("story-select", true, Install, ::wxl::hook::Phase::Boot)
#endif
