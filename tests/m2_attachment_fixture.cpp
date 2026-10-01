// Offline ABI fixture only: replay these compiled SDK calls in an isolated x86 emulator.
// GPL-3.0-or-later. Never load this library into the client.
#include "game/M2.hpp"

extern "C" __declspec(dllexport) void __cdecl Attach(void* child, void* parent, unsigned slot, unsigned force)
{
    wxl::game::m2::AttachToScene(child, parent, slot, force != 0);
}
extern "C" __declspec(dllexport) void __cdecl Detach(void* parent, unsigned slot)
{
    wxl::game::m2::DetachSlot(parent, slot);
}
