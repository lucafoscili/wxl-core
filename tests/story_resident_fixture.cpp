// Compile the owning implementation, not a hand-written surrogate of its row context.
// Offline Win32 emulator fixture only. Never load into WoW. GPL-3.0-or-later.
#define WXL_STORY_SELECT_TRIAL 1
#define WXL_CHARACTER_CAPACITY_TRIAL 1
#include "engine/hook/Registry.hpp"
#undef WXL_REGISTER_FEATURE_PHASED
#define WXL_REGISTER_FEATURE_PHASED(...)
#include "../src/client/StorySelect/StorySelect.cpp"
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
