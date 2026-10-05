// Compile the production owner verbatim into an emulator-only DLL. Never install.
#define WXL_PASSIVE_HOVER_TRIAL 1
#define WXL_HOVER_FIXTURE 1
#include "engine/input/HoverPicking.cpp"
namespace hover=wxl::input::hover;
extern "C" __declspec(dllexport) void __cdecl Refresh(void*) { hover::Refresh(); }
extern "C" __declspec(dllexport,naked) void TailHook() { __asm { jmp hover::TailHook } }
extern "C" __declspec(dllexport) void __cdecl SetOriginalTail(void* p) { hover::g_originalTail=p; hover::g_enabled=true; }
extern "C" __declspec(dllexport) void __cdecl SetOriginalPick(void* p) { hover::g_originalPick=reinterpret_cast<hover::off::PickAtScreenFn>(p); }
extern "C" __declspec(dllexport) void __cdecl SetOriginalFrame(void* p) { hover::g_originalFrame=reinterpret_cast<hover::FrameUpdateFn>(p); }
extern "C" __declspec(dllexport) void* __cdecl PickAddress() { return &hover::PickHook; }
extern "C" __declspec(dllexport) void* __cdecl FrameAddress() { return &hover::FrameHook; }
extern "C" __declspec(dllexport) void __cdecl SetTime(unsigned n) { hover::g_fixtureTime=n; }
extern "C" __declspec(dllexport) void __cdecl Invalidate() { hover::Invalidate(); }
extern "C" __declspec(dllexport) int __cdecl Cached() { return hover::g_retained.valid; }
extern "C" __declspec(dllexport) void __cdecl SetOriginalResolve(void* p) { hover::g_originalResolve=reinterpret_cast<hover::ResolveUnitFn>(p); }
extern "C" __declspec(dllexport) void __cdecl SetOriginalInteract(void* p) { hover::g_originalInteract=reinterpret_cast<hover::InteractUnitFn>(p); }
extern "C" __declspec(dllexport) void* __cdecl ResolveAddress() { return &hover::ResolveHook; }
extern "C" __declspec(dllexport) void* __cdecl InteractAddress() { return &hover::InteractHook; }
extern "C" __declspec(dllexport) void __cdecl BeforeInput(unsigned n) { hover::BeforeInput(n); }
extern "C" __declspec(dllexport) void __cdecl SetOriginalPublication(void* p) { hover::g_originalSetMouseover=reinterpret_cast<hover::SetMouseoverFn>(p); }
extern "C" __declspec(dllexport) void* __cdecl PublicationAddress() { return &hover::SetMouseoverHook; }
extern "C" __declspec(dllexport) void __cdecl SetOriginalMacro(void* p) { hover::g_originalMacro=reinterpret_cast<hover::ExecuteMacroFn>(p); }
extern "C" __declspec(dllexport) void* __cdecl MacroAddress() { return &hover::MacroHook; }
