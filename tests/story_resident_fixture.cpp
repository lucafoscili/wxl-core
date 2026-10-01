// Compile the owning implementation, not a hand-written surrogate of its row context.
// Offline Win32 emulator fixture only. Never load into WoW. GPL-3.0-or-later.
#define WXL_STORY_SELECT_TRIAL 1
#define WXL_CHARACTER_CAPACITY_TRIAL 1
#include "engine/hook/Registry.hpp"
#undef WXL_REGISTER_FEATURE_PHASED
#define WXL_REGISTER_FEATURE_PHASED(...)
#include "client/StorySelect/StorySelect.cpp" // -I chooses owning checkout or prepared composition
#include <new>

__declspec(thread) alignas(RowScope) uint8_t fixtureScope[sizeof(RowScope)];
extern "C" __declspec(dllexport) void __cdecl SetContext(int row, int initialize)
{
    g_rowScope=nullptr;
    if (row>=0) new (fixtureScope) RowScope(row,initialize!=0);
}
extern "C" __declspec(dllexport) unsigned __cdecl GetRedirect(unsigned index, uint8_t* output)
{
    if (index>=kRedirects) return 0;
    const auto patch=ResidentRedirect(index);
    std::memcpy(output,&patch.site,4);
    unsigned size=patch.size;
    std::memcpy(output+4,&size,4);
    RedirectBytes(patch,output+8);
    return 1;
}
extern "C" __declspec(dllexport) void __cdecl InitializeRow(int index)
{
    RowScope scope(index,true);
    wxl::game::Native<InitializeFn>(off::kInitialize)();
}
extern "C" __declspec(dllexport) void __cdecl InitializeNormal()
{
    g_initialize=wxl::game::Native<InitializeFn>(off::kInitialize);
    InitializeSelected();
}
extern "C" __declspec(dllexport) const char* __cdecl BeginPair(int index)
{
    g_residentsReady=true;
    g_initialize=wxl::game::Native<InitializeFn>(off::kInitialize);
    return BeginResidents(index);
}
extern "C" __declspec(dllexport) const char* __cdecl BeginPage(const ResidentRequest* requests, unsigned count, unsigned revision)
{
    g_residentsReady=true;
    g_initialize=wxl::game::Native<InitializeFn>(off::kInitialize);
    return BeginGroup(requests,count,revision,true);
}
extern "C" __declspec(dllexport) unsigned __cdecl PageCount() { return g_residents.count; }
extern "C" __declspec(dllexport) unsigned __cdecl PageRevision() { return g_rosterRevision; }
extern "C" __declspec(dllexport) int __cdecl PageIndex(unsigned member)
{ return member<g_residents.count ? g_residents.members[member].actor.index : -1; }
extern "C" __declspec(dllexport) const char* __cdecl ActPage(unsigned token, int index, unsigned action)
{ return ActResidents(token,index,action); }
extern "C" __declspec(dllexport) int __cdecl CallResidents(void* lua)
{
    g_residentsReady=true;
    g_initialize=wxl::game::Native<InitializeFn>(off::kInitialize);
    return ResidentsMethod(lua);
}
extern "C" __declspec(dllexport) const char* __cdecl StepPair(unsigned token, float delta)
{ return StepResidents(token,delta); }
extern "C" __declspec(dllexport) const char* __cdecl ActPair(unsigned token, int index, int salute)
{ return ActResidents(token,index,salute!=0); }
extern "C" __declspec(dllexport) unsigned __cdecl PairToken() { return g_residents.token; }
extern "C" __declspec(dllexport) void __cdecl StopPair() { StopResidents(); }
extern "C" __declspec(dllexport) const char* __cdecl CameraPair(unsigned token, const char* stem, unsigned duration)
{ return BeginCamera(token,stem,duration); }
extern "C" __declspec(dllexport) void __cdecl StopCameraPair() { StopCamera(g_residents); }
extern "C" __declspec(dllexport) unsigned __cdecl PairCameraActive() { return g_residents.trialCamera!=0; }
void __cdecl FixtureRefresh() {}
extern "C" __declspec(dllexport) void __cdecl RefreshPair()
{ g_refresh=FixtureRefresh; Refresh(); }
int fixtureSelection;
int __cdecl FixtureSelect(void*)
{ *reinterpret_cast<int*>(off::kSelected)=fixtureSelection; return 77; }
extern "C" __declspec(dllexport) int __cdecl SelectPair(int index)
{ g_ready=true; g_select=FixtureSelect; fixtureSelection=index; return Select(nullptr); }
extern "C" __declspec(dllexport) void __cdecl AttachFixture(void* model, void* background, unsigned slot)
{ wxl::game::m2::AttachToScene(model,background,slot,true); }
extern "C" __declspec(dllexport) void __cdecl LightPair(void* model)
{
    wxl::game::Native<LightingFn>(Read<uintptr_t>(reinterpret_cast<uintptr_t>(model)+m2::kOffInstLightingCallbackFn))(
        model,reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(model)+0x1D4),
        reinterpret_cast<void*>(Read<uintptr_t>(reinterpret_cast<uintptr_t>(model)+m2::kOffInstLightingUserData)));
}
