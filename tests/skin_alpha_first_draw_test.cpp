// Synthetic first-sheet admission through the actual SkinAlpha hooks; no client execution.
// Build as Win32: offsets describe 32-bit native objects.
#include "../src/client/CharModel/SkinAlpha.cpp"
#include <cstdio>
#include <cstdlib>

namespace {
    int checks = 0, logs = 0, submits = 0, paints = 0;
    bool ready = false, paintedCustom = false;
    uint32_t observedWait = 99;
    void* observedComponent = nullptr;
    void* observedEdx = nullptr;
    alignas(4) uint8_t component[0x530]{}, instance[0x30]{}, shared[0x154]{}, request[0x120]{};
    void Check(bool ok) { ++checks; if (!ok) { std::printf("FAIL %d\n", checks); std::exit(1); } }
    template<class T> T& At(void* base, size_t offset) {
        return *reinterpret_cast<T*>(static_cast<uint8_t*>(base) + offset);
    }
    void Reset(const char* stem = nullptr) {
        std::memset(component, 0, sizeof(component));
        std::memset(instance, 0, sizeof(instance));
        std::memset(shared, 0, sizeof(shared));
        At<uint32_t>(component, m2::kOffCharComponentFormat) = m2::kGxTexFormatDxt1;
        At<void*>(component, m2::kOffCharComponentInstance) = instance;
        At<void*>(instance, m2::kOffInstShared) = shared;
        if (stem) strcpy_s(reinterpret_cast<char*>(shared + m2::kOffModelPathStem), m2::kOffModelHeader - m2::kOffModelPathStem, stem);
    }
    bool __fastcall NativeCheck(void* c, void* e, uint32_t wait) {
        observedComponent = c; observedEdx = e; observedWait = wait; return ready;
    }
    void* __cdecl NativeAllocate() { return request; }
    void __fastcall NativeSubmit(void* c, void*) {
        ++submits;
        void* r = hkAllocateRequest();
        At<void*>(c, m2::kOffCharComponentComposeRequest) = r;
        At<uint32_t>(r, m2::kOffComposeRequestFormat) = Field(c, m2::kOffCharComponentFormat);
    }
    void __cdecl NativePaint(void*) { ++paints; paintedCustom = t_customSheet; }
}
namespace wxl::config { bool Env(const char*, bool fallback) { return fallback; } }
namespace wxl::log {
    bool Enabled(Level) { return true; }
    void Write(Level, const char* format, ...) { if (!std::strncmp(format, "skin-alpha: uncompressed", 24)) ++logs; }
    void Flush() {}
}
namespace wxl::hook {
    void RegisterFeature(const char*, bool, bool(*)(), Phase) {}
    bool Install(const char*, void*, void*, void**, int) { return true; }
}
int main() {
    g_origCheckBaseTextures = NativeCheck;
    g_origSubmitRequest = NativeSubmit;
    g_origAllocateRequest = NativeAllocate;
    g_origComposeRequest = NativePaint;
    Reset();
    UncompressCustomBody(component); // CharInit before identity arrives
    Check(Field(component, m2::kOffCharComponentFormat) == m2::kGxTexFormatDxt1 && logs == 0);
    strcpy_s(reinterpret_cast<char*>(shared + m2::kOffModelPathStem), m2::kOffModelHeader - m2::kOffModelPathStem, "Creature\\Donor\\Donor");
    At<uint32_t>(component, m2::kOffCharComponentRebuild) = 5;
    At<uint32_t>(component, m2::kOffCharComponentDirty) = 0x3ff;
    void* edx = reinterpret_cast<void*>(0x1234);
    Check(!hkCheckBaseTextures(component, edx, 0));
    Check(observedComponent == component && observedEdx == edx && observedWait == 0);
    Check(Field(component, m2::kOffCharComponentFormat) == m2::kGxTexFormatArgb8888 && logs == 1);
    Check(Field(component, m2::kOffCharComponentRebuild) == 5 && Field(component, m2::kOffCharComponentDirty) == 0x3ff);
    ready = true;
    for (int i = 0; i < 100; ++i) Check(hkCheckBaseTextures(component, edx, 1));
    Check(observedWait == 1 && logs == 1 && submits == 0);
    hkSubmitRequest(component, edx);
    Check(submits == 1 && Field(request, m2::kOffComposeRequestFormat) == m2::kGxTexFormatArgb8888);
    hkComposeRequest(request);
    Check(paints == 1 && paintedCustom && !t_customSheet);
    hkComposeRequest(request);
    Check(!paintedCustom); // capture is consumed once
    Reset("Creature\\Donor\\Donor"); // last-chance submission without readiness hook
    hkSubmitRequest(component, edx);
    Check(Field(request, m2::kOffComposeRequestFormat) == m2::kGxTexFormatArgb8888);
    hkComposeRequest(request); Check(paintedCustom);
    const char* nativeStems[] = {nullptr, "", "Character\\Human\\HumanMale"};
    for (const char* stem : nativeStems) {
        Reset(stem); hkCheckBaseTextures(component, edx, 0);
        Check(Field(component, m2::kOffCharComponentFormat) == m2::kGxTexFormatDxt1);
        hkSubmitRequest(component, edx); hkComposeRequest(request); Check(!paintedCustom);
    }
    Reset("Creature\\Donor\\Donor");
    At<void*>(component, m2::kOffCharComponentComposeRequest) = request;
    At<uint32_t>(request, 0) = 1;
    hkCheckBaseTextures(component, edx, 0);
    Check(Field(component, m2::kOffCharComponentFormat) == m2::kGxTexFormatDxt1);
    At<uint32_t>(request, 0) = 3; // completed request still owns pixels
    hkCheckBaseTextures(component, edx, 0);
    Check(Field(component, m2::kOffCharComponentFormat) == m2::kGxTexFormatDxt1);
    At<void*>(component, m2::kOffCharComponentComposeRequest) = nullptr;
    At<uint32_t>(component, m2::kOffCharComponentSheet) = 1;
    hkCheckBaseTextures(component, edx, 0);
    Check(Field(component, m2::kOffCharComponentFormat) == m2::kGxTexFormatDxt1);
    At<uint32_t>(component, m2::kOffCharComponentSheet) = 0;
    hkCheckBaseTextures(component, edx, 0);
    Check(Field(component, m2::kOffCharComponentFormat) == m2::kGxTexFormatArgb8888);
    Reset("Creature\\Donor\\Donor");
    At<uint32_t>(component, m2::kOffCharComponentFormat) = 7;
    hkCheckBaseTextures(component, edx, 0); Check(Field(component, m2::kOffCharComponentFormat) == 7);
    Check(hkCheckBaseTextures(nullptr, edx, 0));
    std::printf("%d first-draw checks passed\n", checks);
}
