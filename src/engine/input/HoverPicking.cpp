// Opt-in passive world-hover trial; native eligibility/selection stay authoritative.
#include "engine/input/HoverPicking.hpp"
#if defined(WXL_PASSIVE_HOVER_TRIAL) && WXL_PASSIVE_HOVER_TRIAL
#include "offsets/game/World.hpp"
#include "offsets/game/Unit.hpp"
#include "offsets/engine/Lua.hpp"
#include <intrin.h>
#include <cstring>
#ifndef WXL_HOVER_FIXTURE
#include "engine/hook/Hook.hpp"
#include "engine/hook/Registry.hpp"
#include "engine/events/Event.hpp"
#include "common/Log.hpp"
#include <windows.h>
#endif

namespace wxl::input::hover
{
    namespace off = wxl::offsets::game::world;
    namespace unit = wxl::offsets::game::unit;
    namespace lua = wxl::offsets::engine::lua;
    constexpr uintptr_t kFrameUpdate=off::kFrameLayerUpdate, kHoverContinue=0x4FA05C,
        kPassiveReturn=0x4FA13F, kHoverTail=0x4FA32E, kHoverEpilogue=0x4FA368,
        kResolveUnit=0x60ABF0, kInteractUnit=0x527F00, kTargetingActive=0x7FD620,
        kSetMouseover=0x4F5980, kExecuteMacro=0x564DB0;
    constexpr uint32_t kPassiveIntervalMs = 100;
    constexpr unsigned kInputOwnerOffset=0x78, kCursorFlagsOffset=0x31C;
    using FrameUpdateFn = void(__thiscall*)(void*,float);
    using ResolveUnitFn = int(__cdecl*)(const char*,uint32_t*,int);
    using InteractUnitFn = int(__cdecl*)(void*);
    using SetMouseoverFn = void(__thiscall*)(void*,uint32_t,uint32_t);
    using ExecuteMacroFn = void(__cdecl*)(void*,const char*);
    off::PickAtScreenFn g_originalPick = nullptr;
    FrameUpdateFn g_originalFrame = nullptr;
    ResolveUnitFn g_originalResolve = nullptr;
    InteractUnitFn g_originalInteract = nullptr;
    ExecuteMacroFn g_originalMacro = nullptr;
    SetMouseoverFn g_originalSetMouseover = nullptr;
    void* g_originalTail = nullptr;
    uintptr_t g_hoverContinue=kHoverContinue, g_hoverEpilogue=kHoverEpilogue, g_returnAddress=0;
    bool g_enabled = false;
    __declspec(thread) bool g_refreshing = false;
    __declspec(thread) unsigned g_publishing = 0;
    // Retain value data (GUID, coordinates/ray, timestamp), never mesh/object/
    // skin/palette pointers. Frame/input identify the GUI context, not geometry.
    struct Retained { bool valid; uint32_t timeMs; void* frame; void* input;
        int type; uint32_t result[12]; };
    Retained g_retained = {};
    void Invalidate() { g_retained.valid=false; }

    bool WorldOwnsInput(void* frame)
    {
        if (!frame || frame!=*reinterpret_cast<void**>(off::kWorldFrame)
            || *reinterpret_cast<uint32_t*>(off::kLoadActive)) return false;
        void* input=*reinterpret_cast<void**>(static_cast<char*>(frame)+off::kWorldFrameInput);
        return input && *reinterpret_cast<void**>(static_cast<char*>(input)+kInputOwnerOffset)==frame;
    }
#ifdef WXL_HOVER_FIXTURE
    uint32_t g_fixtureTime=0;
    uint32_t TimeMs() { return g_fixtureTime; }
#else
    uint32_t TimeMs() { return GetTickCount(); }
#endif
    bool TargetStillEligible()
    {
        const auto* guid=reinterpret_cast<const uint32_t*>(unit::kMouseoverGuid);
        if (!g_retained.valid || guid[0]!=g_retained.result[0] || guid[1]!=g_retained.result[1]) return false;
        const auto id=(static_cast<unsigned long long>(guid[1])<<32)|guid[0];
        // Only test residency: no resolved pointer survives this call.
        return reinterpret_cast<unit::GetObjectFn>(unit::kGetObjectByGuid)(
            id,unit::kTypeMaskObject,"wxl-hover",0)!=nullptr;
    }
    bool PassiveEligible(void* frame)
    {
        return WorldOwnsInput(frame)
            && !(*reinterpret_cast<uint32_t*>(static_cast<char*>(frame)+kCursorFlagsOffset)&2)
            && !reinterpret_cast<int(__cdecl*)()>(kTargetingActive)();
    }
    int __fastcall PickHook(void* frame,void*,float x,float y,int mode,uint32_t* result)
    {
        // Mode alone never proves passive intent. Unknown query contexts stay fresh.
        const bool passive=reinterpret_cast<uintptr_t>(_ReturnAddress())==kPassiveReturn && mode==1;
        if (!g_enabled || !passive) return g_originalPick(frame,x,y,mode,result);
        if (!PassiveEligible(frame))
        { Invalidate(); return g_originalPick(frame,x,y,mode,result); }
        void* input=*reinterpret_cast<void**>(static_cast<char*>(frame)+off::kWorldFrameInput);
        const uint32_t now=TimeMs();
        if (!g_refreshing && g_retained.valid && g_retained.frame==frame && g_retained.input==input
            && uint32_t(now-g_retained.timeMs)<kPassiveIntervalMs && TargetStillEligible())
        {
            for (unsigned i=0;i<12;++i) result[i]=g_retained.result[i];
            return g_retained.type;
        }
        Invalidate();
        const int type=g_originalPick(frame,x,y,mode,result);
        // Forced actions also seed the passive interval with their fresh values;
        // actions themselves never read this cache. Miss/terrain stay fresh.
        // Next-frame native mouseover
        // identity and residency must match before a retained object can be reused.
        if ((type==2 || type==3) && (result[0] || result[1]))
        {
            g_retained.frame=frame; g_retained.input=input; g_retained.type=type;
            g_retained.timeMs=now;
            for (unsigned i=0;i<12;++i) g_retained.result[i]=result[i];
            g_retained.valid=true;
        }
        return type;
    }
    // Supply the verified native frame ABI. Selection/publication execute in Wow.exe.
    __declspec(naked) void NativeHoverOnly()
    {
        __asm { push ebp } __asm { mov ebp,esp } __asm { sub esp,3Ch }
        __asm { push ebx } __asm { push esi } __asm { push edi }
        __asm { mov ebx,ecx } __asm { jmp dword ptr [g_hoverContinue] }
    }
    __declspec(naked) void __cdecl InvokeNativeHover(void* frame)
    {
        __asm { mov ecx,[esp+4] }
        __asm { mov eax,offset returnedFromHover }
        __asm { mov g_returnAddress,eax }
        __asm { push 0 } __asm { call NativeHoverOnly }
returnedFromHover:
        __asm { ret }
    }
    __declspec(naked) void TailHook()
    {
        __asm { pushfd } __asm { push eax }
        __asm { mov eax,g_returnAddress } __asm { cmp [ebp+4],eax }
        __asm { pop eax } __asm { jne ordinaryFrame }
        __asm { popfd } __asm { jmp dword ptr [g_hoverEpilogue] }
ordinaryFrame:
        __asm { popfd } __asm { jmp dword ptr [g_originalTail] }
    }
    void Refresh()
    {
        // A nested consumer runs after native publication; it must not erase the
        // fresh result the outer refresh can contribute to passive presentation.
        if (!g_enabled || g_refreshing || g_publishing) return;
        Invalidate();
        void* frame=*reinterpret_cast<void**>(off::kWorldFrame);
        // UI hover is never delayed. Extra UI Lua replay could invoke an action
        // before publishing its target, so supplemental refresh stays world-only.
        if (!WorldOwnsInput(frame)) return;
        g_refreshing=true; InvokeNativeHover(frame); g_refreshing=false;
    }
    bool MouseoverToken(const char* token)
    {
        constexpr char name[]="mouseover";
        if (!token) return false;
        for (unsigned i=0;i<sizeof(name)-1;++i)
        {
            char c=token[i]; if (c>='A' && c<='Z') c+='a'-'A';
            if (c!=name[i]) return false;
        }
        return true; // native resolver admits suffixed tokens too
    }
    int __cdecl ResolveHook(const char* token,uint32_t* guid,int flags)
    {
        // Exact native action argument reads, not Unit*/tooltip/conditional queries.
        // Registered CastSpellByID, CastSpellByName and UseAction callers all
        // resolve their target before forwarding it to the action executor.
        const uintptr_t caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
        if (MouseoverToken(token) && (caller==0x53E0D0 || caller==0x540380 || caller==0x5AC048)) Refresh();
        return g_originalResolve(token,guid,flags);
    }
    void __fastcall SetMouseoverHook(void* frame,void*,uint32_t lo,uint32_t hi)
    {
        // Native publication exposes the new global before committing frame GUID.
        // Keep callbacks inside that whole transaction from starting it again.
        ++g_publishing; g_originalSetMouseover(frame,lo,hi); --g_publishing;
    }
    void __cdecl MacroHook(void* macro,const char* button)
    {
        // Shared native executor used by RunMacro/Text and the action-slot route.
        // Publish before it dispatches the first macro line/conditional to Lua.
        Refresh(); g_originalMacro(macro,button);
    }
    int __cdecl InteractHook(void* state)
    {
        const char* token=reinterpret_cast<lua::LuaToStringFn>(lua::kLuaToString)(state,1,nullptr);
        if (MouseoverToken(token)) Refresh();
        return g_originalInteract(state);
    }
    void __fastcall FrameHook(void* frame,void*,float dt)
    { if (g_enabled && !PassiveEligible(frame)) Invalidate(); g_originalFrame(frame,dt); }
    void BeforeInput(uint32_t message)
    {
        // Run before OnInput subscribers/native action handling. Cursor movement
        // deliberately does not restart the accepted 100 ms visual age.
        if (message==0x201 || message==0x204 || message==0x207 || message==0x20B) Refresh();
        else if (message==0x100 || message==0x104 || message==0x08 || message==0x1F
                 || message==0x215 || message==0x1C) Invalidate();
    }
#ifndef WXL_HOVER_FIXTURE
    void OnInvalidation(void*,const void*) { Invalidate(); }
    bool Install()
    {
        // Exact continuation ABI: bounded check at the hook boundary, once.
        constexpr uint8_t prologue[]={0x55,0x8B,0xEC,0x83,0xEC,0x3C};
        constexpr uint8_t tail[]={0xD9,0x45,0x08,0x51,0xD9,0x1C,0x24};
        constexpr uint8_t epilogue[]={0x5F,0x5E,0x5B,0x8B,0xE5,0x5D,0xC2,0x04,0x00};
        constexpr uint8_t setter[]={0x55,0x8B,0xEC,0x83,0xEC,0x10};
        constexpr uint8_t macro[]={0x55,0x8B,0xEC,0x81,0xEC,0x04,0x04,0x00,0x00};
        if (std::memcmp(reinterpret_cast<void*>(kFrameUpdate),prologue,sizeof(prologue))
            || std::memcmp(reinterpret_cast<void*>(kHoverTail),tail,sizeof(tail))
            || std::memcmp(reinterpret_cast<void*>(kHoverEpilogue),epilogue,sizeof(epilogue))
            || std::memcmp(reinterpret_cast<void*>(kSetMouseover),setter,sizeof(setter))
            || std::memcmp(reinterpret_cast<void*>(kExecuteMacro),macro,sizeof(macro)))
        { WLOG_WARN("passive-hover: native continuation ABI mismatch; trial inactive"); return false; }
        // These are complete relative call instructions at the admitted action
        // argument boundaries. Unknown client bytes leave this trial inactive.
        constexpr uintptr_t actionCalls[]={0x53E0CB,0x54037B,0x5AC043};
        for (uintptr_t call : actionCalls)
            if (*reinterpret_cast<const uint8_t*>(call)!=0xE8
                || call+5+*reinterpret_cast<const int32_t*>(call+1)!=kResolveUnit)
            { WLOG_WARN("passive-hover: native action boundary mismatch; trial inactive"); return false; }
        namespace hook=wxl::hook;
        const bool ok=hook::Install("HoverTail",kHoverTail,reinterpret_cast<void*>(&TailHook),&g_originalTail)
            && hook::Install("HoverPick",off::kPickAtScreen,reinterpret_cast<void*>(&PickHook),reinterpret_cast<void**>(&g_originalPick))
            && hook::Install("HoverFrame",kFrameUpdate,reinterpret_cast<void*>(&FrameHook),reinterpret_cast<void**>(&g_originalFrame))
            && hook::Install("HoverUnitToken",kResolveUnit,&ResolveHook,&g_originalResolve)
            && hook::Install("HoverInteract",kInteractUnit,&InteractHook,&g_originalInteract)
            && hook::Install("HoverPublication",kSetMouseover,reinterpret_cast<void*>(&SetMouseoverHook),reinterpret_cast<void**>(&g_originalSetMouseover))
            && hook::Install("HoverMacro",kExecuteMacro,&MacroHook,&g_originalMacro);
        if (!ok) return false;
        namespace ev=wxl::events;
        ev::Subscribe(ev::Event::OnWorldLeave,OnInvalidation,nullptr);
        ev::Subscribe(ev::Event::OnWorldEnter,OnInvalidation,nullptr);
        ev::Subscribe(ev::Event::OnObjectDestroy,OnInvalidation,nullptr);
        g_enabled=true;
        WLOG_INFO("passive-hover: immediate-first 100 ms Beta trial active; actions fresh");
        return true;
    }
#endif
}
#ifndef WXL_HOVER_FIXTURE
WXL_REGISTER_FEATURE("passive-hover-trial",true,wxl::input::hover::Install)
#endif
#else
namespace wxl::input::hover { void BeforeInput(uint32_t) {} }
#endif
