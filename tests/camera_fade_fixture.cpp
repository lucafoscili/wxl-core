// Execute the production x86 thunk and byte guard with synthetic camera/frame data.
// No WoW process, installation or client API call. GPL-3.0-or-later.
#define WXL_CAMERA_FADE_FIXTURE 1
#include "client/CWorldScene/CameraFade.cpp"
#include <cstdio>
#include <initializer_list>

namespace cf = wxl::client::camerafade;
namespace off = wxl::offsets::engine::camera;
namespace
{
    int checks = 0, failures = 0;
    void Check(bool condition, const char* message)
    {
        ++checks;
        if (!condition) { ++failures; std::printf("FAIL: %s\n", message); }
    }
    uint64_t g_player = 0x1122334455667788ull;
    uint32_t g_clobberMxcsr = 0x1F80;
    // Deliberately destroy volatile state inside the helper's native-call substitute.
    __declspec(naked) uint64_t __cdecl PlayerGuid()
    {
        __asm
        {
            fninit
            fldz
            pxor xmm0, xmm0
            pxor xmm1, xmm1
            pxor xmm2, xmm2
            pxor xmm3, xmm3
            pxor xmm4, xmm4
            pxor xmm5, xmm5
            pxor xmm6, xmm6
            pxor xmm7, xmm7
            ldmxcsr g_clobberMxcsr
            mov ecx, 88888888h
            cmp ecx, 0
            mov eax, dword ptr [g_player]
            mov edx, dword ptr [g_player + 4]
            ret
        }
    }
    uint32_t g_context = 0xFEDCBA98;
    uint32_t g_registers[8], g_flags, g_expectedEsp;
    uintptr_t g_entry = reinterpret_cast<uintptr_t>(&cf::DistanceFadeHook);
    uint32_t g_testMxcsr = 0x3F80; // non-default rounding, exceptions masked
    __declspec(align(16)) uint8_t g_callerFp[512], g_beforeFp[512], g_afterFp[512];

    __declspec(naked) void ReturnedFromHook()
    {
        __asm
        {
            mov [g_registers + 0], eax
            mov [g_registers + 4], ecx
            mov [g_registers + 8], edx
            mov [g_registers + 12], ebx
            mov [g_registers + 16], esp
            mov [g_registers + 20], ebp
            mov [g_registers + 24], esi
            mov [g_registers + 28], edi
            pushfd
            pop g_flags
            fxsave [g_afterFp]
            ret
        }
    }
    __declspec(naked) void __cdecl RunHook(const uint8_t*, void*)
    {
        __asm
        {
            push ebp
            push ebx
            push esi
            push edi
            fxsave [g_callerFp]
            fninit
            fld1
            fldpi
            pcmpeqd xmm0, xmm0
            movdqa xmm1, xmm0
            movdqa xmm2, xmm0
            movdqa xmm3, xmm0
            movdqa xmm4, xmm0
            movdqa xmm5, xmm0
            movdqa xmm6, xmm0
            movdqa xmm7, xmm0
            ldmxcsr g_testMxcsr
            fxsave [g_beforeFp]
            mov esi, [esp + 20]
            mov ebp, [esp + 24]
            mov eax, 11111111h
            mov ecx, 22222222h
            mov edx, 33333333h
            mov ebx, 44444444h
            mov edi, 77777777h
            push 246h
            popfd
            mov g_expectedEsp, esp
            sub g_expectedEsp, 4
            // SUB changed flags; set them immediately before entering the thunk.
            push 246h
            popfd
            call dword ptr [g_entry]
            fxrstor [g_callerFp]
            pop edi
            pop esi
            pop ebx
            pop ebp
            ret
        }
    }

    void CheckEncoding()
    {
        // Independent literals verified in Wow.exe.orig (build 12340), 7 October 2026.
        const uint8_t stolen[] = {0x8B, 0x45, 0xF4, 0x8B, 0x0D, 0x6C, 0x43, 0xB7, 0x00};
        Check(sizeof stolen == sizeof off::kDistanceFadeOriginal
              && !std::memcmp(stolen, off::kDistanceFadeOriginal, sizeof stolen),
              "verified stock instruction bytes");
        Check(off::kDistanceFadeSubmit == 0x006079FD && off::kDistanceFadeResume == 0x00607A06
              && off::kDistanceFadeSubmitContext == 0x00B7436C
              && off::kFollowTargetGuid == 0x88 && off::kFollowDistance == 0x128
              && cf::unit::kActivePlayerGuid == 0x004D3790, "verified client addresses and fields");

        // Exact naked-thunk encoding; resolve only the three linker-dependent operands.
        // pushfd/pushad, aligned FXSAVE, cdecl helper, FXRSTOR/popad/popfd, MOVs, indirect JMP.
        uint8_t thunk[] = {
            0x9C, 0x60, 0x8B, 0xFC, 0x81, 0xEC, 0x10, 0x02, 0x00, 0x00,
            0x83, 0xE4, 0xF0, 0x0F, 0xAE, 0x04, 0x24, 0xDB, 0xE3, 0xFC,
            0x8D, 0x45, 0xF4, 0x50, 0x56, 0xE8, 0, 0, 0, 0,
            0x83, 0xC4, 0x08, 0x0F, 0xAE, 0x0C, 0x24, 0x8B, 0xE7, 0x61, 0x9D,
            0x8B, 0x45, 0xF4, 0x8B, 0x0D, 0, 0, 0, 0, 0x8B, 0x09,
            0xFF, 0x25, 0, 0, 0, 0
        };
        const uintptr_t entry = reinterpret_cast<uintptr_t>(&cf::DistanceFadeHook);
        const uint32_t call = static_cast<uint32_t>(
            reinterpret_cast<uintptr_t>(&cf::OverrideDistanceAlpha) - (entry + 0x1E));
        const uintptr_t context = reinterpret_cast<uintptr_t>(&cf::g_submitContext);
        const uintptr_t resume = reinterpret_cast<uintptr_t>(&cf::g_resume);
        std::memcpy(thunk + 0x1A, &call, sizeof call);
        std::memcpy(thunk + 0x2E, &context, sizeof context);
        std::memcpy(thunk + 0x36, &resume, sizeof resume);
        Check(!std::memcmp(reinterpret_cast<const void*>(entry), thunk, sizeof thunk),
              "compiled thunk encoding and relocated operands");
    }

    void Case(uint64_t target, uint32_t distanceBits, uint8_t nativeAlpha, uint8_t expected)
    {
        uint8_t camera[0x130], frame[0x40], originalCamera[0x130], originalFrame[0x40];
        std::memset(camera, 0xA5, sizeof camera);
        std::memset(frame, 0x5A, sizeof frame);
        std::memcpy(camera + off::kFollowTargetGuid, &target, sizeof target);
        std::memcpy(camera + off::kFollowDistance, &distanceBits, sizeof distanceBits);
        // Native EBP is frame+0x20, so [EBP-0xC] is frame+0x14.
        frame[0x14] = nativeAlpha;
        std::memcpy(originalCamera, camera, sizeof camera);
        std::memcpy(originalFrame, frame, sizeof frame);
        RunHook(camera, frame + 0x20);
        Check(frame[0x14] == expected, "distance/identity policy");
        originalFrame[0x14] = expected;
        Check(!std::memcmp(frame, originalFrame, sizeof frame), "only the alpha byte changes");
        Check(!std::memcmp(camera, originalCamera, sizeof camera), "camera stays untouched");
        uint32_t replayAlpha;
        std::memcpy(&replayAlpha, frame + 0x14, sizeof replayAlpha);
        Check(g_registers[0] == replayAlpha && g_registers[1] == g_context, "displaced MOV results");
        Check(g_registers[2] == 0x33333333 && g_registers[3] == 0x44444444
              && g_registers[4] == g_expectedEsp
              && g_registers[5] == reinterpret_cast<uintptr_t>(frame + 0x20)
              && g_registers[6] == reinterpret_cast<uintptr_t>(camera)
              && g_registers[7] == 0x77777777, "registers and stack preserved");
        Check((g_flags & 0xCD5) == (0x246 & 0xCD5), "arithmetic/direction flags preserved");
        Check(!std::memcmp(g_beforeFp, g_afterFp, sizeof g_beforeFp), "x87/XMM/MXCSR preserved");
    }
}

int main()
{
    CheckEncoding();
    cf::g_activePlayerGuid = PlayerGuid;
    cf::g_resume = reinterpret_cast<uintptr_t>(&ReturnedFromHook);
    cf::g_submitContext = reinterpret_cast<uintptr_t>(&g_context);
    // Positive values include the native hidden region and an alpha rounded to zero.
    for (uint32_t bits : {0x40000000u, 0x3F800000u, 0x3A83126Fu, 1u})
        Case(g_player, bits, 0, 255);
    Case(g_player, 0, 255, 0);
    Case(g_player, 0x80000000u, 255, 0); // negative zero
    Case(g_player, 0xBF800000u, 127, 0); // defensive finite negative
    for (uint32_t bits : {0x7F800000u, 0xFF800000u, 0x7FC00000u, 0x7F800001u})
        Case(g_player, bits, 73, 73); // infinities, quiet and signaling NaN
    Case(g_player ^ 1ull, 0x3F800000u, 73, 73);
    Case(g_player ^ (1ull << 32), 0, 73, 73); // compare both GUID halves
    Case(0, 0x3F800000u, 73, 73);
    g_player = 0;
    Case(0, 0x3F800000u, 73, 73); // no active player is never a match

    // Exercise the same installer used by production on a disposable executable page.
    auto* site = static_cast<uint8_t*>(VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE,
                                                  PAGE_EXECUTE_READWRITE));
    Check(site != nullptr, "fixture page allocation");
    if (site)
    {
        for (size_t i = 0; i < sizeof off::kDistanceFadeOriginal; ++i)
        {
            std::memcpy(site, off::kDistanceFadeOriginal, sizeof off::kDistanceFadeOriginal);
            site[i] ^= 1;
            uint8_t before[sizeof off::kDistanceFadeOriginal];
            std::memcpy(before, site, sizeof before);
            Check(!cf::InstallAt(site) && !std::memcmp(site, before, sizeof before),
                  "each unexpected byte refuses installation without writes");
        }
        std::memcpy(site, off::kDistanceFadeOriginal, sizeof off::kDistanceFadeOriginal);
        site[sizeof off::kDistanceFadeOriginal] = 0xCC;
        DWORD previous;
        VirtualProtect(site, 4096, PAGE_EXECUTE_READ, &previous);
        Check(cf::InstallAt(site), "exact bytes install on a protected page");
        uint32_t relative;
        std::memcpy(&relative, site + 1, sizeof relative);
        Check(site[0] == 0xE9 && reinterpret_cast<uintptr_t>(site) + 5 + relative
              == reinterpret_cast<uintptr_t>(&cf::DistanceFadeHook), "jump targets the production thunk");
        Check(site[5] == 0x90 && site[6] == 0x90 && site[7] == 0x90 && site[8] == 0x90
              && site[9] == 0xCC, "nine-byte patch leaves continuation untouched");
        MEMORY_BASIC_INFORMATION info;
        VirtualQuery(site, &info, sizeof info);
        Check(info.Protect == PAGE_EXECUTE_READ, "page protection restored");
        Check(!cf::InstallAt(site), "already patched bytes refuse a second install");
        g_entry = reinterpret_cast<uintptr_t>(site);
        g_player = 0x1122334455667788ull;
        Case(g_player, 0x3A83126Fu, 0, 255); // execute the installed E9 as well
        Case(g_player, 0, 255, 0);
        VirtualFree(site, 0, MEM_RELEASE);
    }
    std::printf("camera-fade: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
