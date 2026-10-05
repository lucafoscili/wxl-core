// Offline x86 continuation experiment. Never load this DLL into a client.
// The client retains selection/eligibility/publication; only the native frame ABI
// is supplied here. An identifiable return address excludes the frame-only suffix.
static unsigned g_returnAddress;
static void* g_originalTail;
static unsigned g_hoverContinue = 0x004FA05C;
static unsigned g_hoverEpilogue = 0x004FA368;

__declspec(naked) static void NativeHoverOnly()
{
    __asm { push ebp }
    __asm { mov ebp, esp }
    __asm { sub esp, 3Ch }
    __asm { push ebx }
    __asm { push esi }
    __asm { push edi }
    __asm { mov ebx, ecx }
    __asm { jmp dword ptr [g_hoverContinue] }
}

extern "C" __declspec(dllexport, naked) void __cdecl Refresh(void* frame)
{
    __asm { mov ecx, [esp + 4] }
    __asm { test ecx, ecx }
    __asm { jz finished }
    __asm { cmp dword ptr [ecx + 0A0h], 0 }
    __asm { jz finished }
    // UI hover keeps its ordinary native Lua processing; supplementary action
    // refresh is only for the world-owned passive path the throttle can delay.
    __asm { mov eax, [ecx + 0A0h] }
    __asm { cmp [eax + 78h], ecx }
    __asm { jne finished }
    __asm { mov eax, offset returnedFromHover }
    __asm { mov g_returnAddress, eax }
    __asm { push 0 }
    __asm { call NativeHoverOnly }
returnedFromHover:
finished:
    __asm { ret }
}

extern "C" __declspec(dllexport, naked) void TailHook()
{
    __asm { pushfd }
    __asm { push eax }
    __asm { mov eax, g_returnAddress }
    __asm { cmp [ebp + 4], eax }
    __asm { pop eax }
    __asm { jne ordinaryFrame }
    __asm { popfd }
    __asm { jmp dword ptr [g_hoverEpilogue] }
ordinaryFrame:
    __asm { popfd }
    __asm { jmp dword ptr [g_originalTail] }
}

extern "C" __declspec(dllexport) void __cdecl SetOriginalTail(void* address)
{ g_originalTail = address; }
