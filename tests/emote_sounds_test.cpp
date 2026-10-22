// Exercise production emote-sound patching on disposable bytes and bounded x86 instructions.
// No client executable, data files, process or installation is loaded. GPL-3.0-or-later.
#include <windows.h>
BOOL WINAPI FixtureVirtualProtect(LPVOID, SIZE_T, DWORD, PDWORD);
#define VirtualProtect FixtureVirtualProtect
#include "client/SE2/Sound.cpp"
#undef VirtualProtect

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>

namespace fixture
{
    namespace feature = wxl::client::emotesounds;
    namespace off = wxl::offsets::engine::sound::emotesounds;
    int checks = 0, failures = 0, hooks = 0, protects = 0, envCalls = 0;
    bool enabled = true, failProtect = false, correctEnv = true;
    std::vector<std::string> errors, infos;
    const char* registeredName = nullptr;
    bool registeredEnabled = false;
    bool (*registeredInstall)() = nullptr;
    wxl::hook::Phase registeredPhase;
    constexpr uintptr_t kBase = 0x004C0000;
    constexpr size_t kWindow = 0x10000;
    uint8_t* window = nullptr;

    void Check(bool condition, const char* message)
    {
        ++checks;
        if (!condition) { ++failures; std::printf("FAIL: %s\n", message); }
    }
    struct Span
    {
        uintptr_t address;
        const char* hex;
        uintptr_t operand;
        size_t width;
        uint32_t value;
    };
    // Independently recorded original verification spans and the ten intended operand writes.
    const Span spans[] = {
        {0x004CEA11, "69f6b4000000", 0x004CEA13, 4, 0x204},
        {0x004CEA6A, "b92d0000008bf8f3a5", 0x004CEA6B, 4, 0x81},
        {0x004CEA76, "81c2b4000000", 0x004CEA78, 4, 0x204},
        {0x004CEB4E, "83fa1673108b4d1083f90273088d14518b0490", 0x004CEB50, 1, 0x40},
        {0x004CEB8E, "83fa1673108b4d1083f90273088d14518b0490", 0x004CEB90, 1, 0x40},
        {0x004CEC1C, "69f6b4000000", 0x004CEC1E, 4, 0x204},
        {0x004CEC2D, "68b0000000", 0x004CEC2E, 4, 0x200},
        {0x004CEC35, "c786b000000000000000", 0x004CEC37, 4, 0x200},
        {0x004CED9D, "8990b0000000", 0x004CED9F, 4, 0x200},
        {0x004CEDAF, "83f91673198b460c83f80273118b76108d04488b0f893481", 0x004CEDB1, 1, 0x40},
    };
    std::vector<uint8_t> Decode(const char* hex)
    {
        std::vector<uint8_t> bytes(std::strlen(hex) / 2);
        for (size_t i = 0; i < bytes.size(); ++i)
        {
            unsigned value = 0;
            std::sscanf(hex + i * 2, "%2x", &value);
            bytes[i] = static_cast<uint8_t>(value);
        }
        return bytes;
    }
    void ResetWindow()
    {
        DWORD previous;
        Check(VirtualProtect(window, kWindow, PAGE_EXECUTE_READWRITE, &previous) != FALSE,
              "fixture window becomes writable");
        std::memset(window, 0xCC, kWindow);
        for (const auto& span : spans)
        {
            const auto bytes = Decode(span.hex);
            std::memcpy(reinterpret_cast<void*>(span.address), bytes.data(), bytes.size());
        }
        errors.clear(); infos.clear(); protects = hooks = envCalls = 0;
        enabled = correctEnv = true; failProtect = false;
    }
    std::vector<uint8_t> Snapshot() { return {window, window + kWindow}; }
    bool Unchanged(const std::vector<uint8_t>& before)
    {
        return std::memcmp(before.data(), window, kWindow) == 0;
    }
    void Verification()
    {
        Check(std::size(spans) == std::size(off::kSites), "fixture covers exactly ten production sites");
        if (std::size(spans) != std::size(off::kSites)) return;
        for (size_t s = 0; s < std::size(spans); ++s)
        {
            const auto bytes = Decode(spans[s].hex);
            const auto& site = off::kSites[s];
            Check(site.address == spans[s].address && site.expected.size() == bytes.size()
                  && std::memcmp(site.expected.data(), bytes.data(), bytes.size()) == 0,
                  "addresses and complete verification spans match independent literals");
            Check(site.address + site.operand == spans[s].operand && site.width == spans[s].width
                  && site.value == spans[s].value, "production operand metadata matches independent writes");
        }
        const auto read = [](uintptr_t address) -> uint8_t {
            for (const auto& span : spans)
                if (address >= span.address && address - span.address < std::strlen(span.hex) / 2)
                    return *reinterpret_cast<const uint8_t*>(address);
            Check(false, "verification reads only declared instruction bytes");
            return 0;
        };
        Check(feature::VerifySites(read) && errors.empty(), "original bounded spans pass production verification");
        for (size_t s = 0; s < std::size(spans); ++s)
            for (size_t i = 0; i < std::strlen(spans[s].hex) / 2; ++i)
            {
                auto* byte = reinterpret_cast<uint8_t*>(spans[s].address + i);
                *byte ^= 0x80;
                errors.clear();
                Check(!feature::VerifySites(read), "every changed verification byte is rejected");
                Check(errors.size() == 1 && errors[0].find(off::kSites[s].name) != std::string::npos,
                      "verification logs exactly one error naming the mismatched site");
                const auto before = Snapshot();
                errors.clear(); protects = 0;
                Check(!feature::Install(), "installer refuses every changed verification byte");
                Check(Unchanged(before) && protects == 0 && hooks == 0,
                      "mismatch refuses before protection changes, operand writes or hooks");
                Check(errors.size() == 1 && errors[0].find(off::kSites[s].name) != std::string::npos,
                      "installer logs exactly one error naming the mismatched site");
                *byte ^= 0x80;
            }
    }
    void Installation()
    {
        ResetWindow();
        enabled = false;
        DWORD previous;
        Check(VirtualProtect(window, kWindow, PAGE_NOACCESS, &previous) != FALSE,
              "disabled-path fixture makes all target bytes inaccessible");
        Check(feature::Install(), "disabled install succeeds without touching inaccessible target bytes");
        Check(protects == 0 && hooks == 0 && errors.empty() && infos.size() == 1
              && infos[0].find("disabled") != std::string::npos,
              "disabled install performs no writes or hooks and logs once at info level");
        Check(envCalls == 1 && correctEnv, "installer reads WXL_EMOTE_SOUNDS with default true");

        ResetWindow();
        const auto before = Snapshot();
        failProtect = true;
        Check(!feature::Install(), "failure to acquire writable window refuses installation");
        Check(protects == 1 && Unchanged(before) && hooks == 0 && errors.size() == 1,
              "writable-window failure leaves every byte unchanged and logs one error");

        ResetWindow();
        auto expected = Snapshot();
        for (const auto& span : spans)
            for (size_t i = 0; i < span.width; ++i)
                expected[span.operand - kBase + i] = static_cast<uint8_t>(span.value >> (8 * i));
        Check(VirtualProtect(window, kWindow, PAGE_EXECUTE_READ, &previous) != FALSE,
              "successful install begins with executable read-only target memory");
        Check(feature::Install() && errors.empty(), "all original bytes install successfully");
        Check(Unchanged(expected) && hooks == 0,
              "whole 64 KiB window changes only the ten expected operands");
        MEMORY_BASIC_INFORMATION memory = {};
        VirtualQuery(window + 0xEA11, &memory, sizeof memory);
        Check(memory.Protect == PAGE_EXECUTE_READ, "successful install restores original page protection");
        const auto installed = Snapshot();
        errors.clear(); protects = 0;
        Check(!feature::Install() && Unchanged(installed) && protects == 0 && errors.size() == 1,
              "a second installation rejects already patched bytes without writes");
    }

    // Run only the verified instruction fragments, with fixture-owned argument setup and returns.
    // The fixed-address window remains the installed production result; snippets copy its bytes.
    struct Code
    {
        uint8_t* allocation = nullptr;
        explicit Code(const std::vector<uint8_t>& bytes)
        {
            allocation = static_cast<uint8_t*>(VirtualAlloc(nullptr, bytes.size(), MEM_COMMIT | MEM_RESERVE,
                                                           PAGE_EXECUTE_READWRITE));
            Check(allocation != nullptr, "synthetic instruction allocation succeeds");
            if (allocation)
            {
                std::memcpy(allocation, bytes.data(), bytes.size());
                FlushInstructionCache(GetCurrentProcess(), allocation, bytes.size());
            }
        }
        ~Code() { if (allocation) VirtualFree(allocation, 0, MEM_RELEASE); }
    };
    void Append(std::vector<uint8_t>& code, std::initializer_list<uint8_t> bytes)
    {
        code.insert(code.end(), bytes);
    }
    void AppendSpan(std::vector<uint8_t>& code, size_t site)
    {
        const auto* start = reinterpret_cast<const uint8_t*>(spans[site].address);
        code.insert(code.end(), start, start + std::strlen(spans[site].hex) / 2);
    }
    void RecordInstructions()
    {
        Check(off::kRaces == 64 && off::kSoundBytes == 0x200 && off::kRecordBytes == 0x204
              && off::kRecordDwords == 0x81, "record layout has 128 sound cells and one trailing ID");
        for (size_t site : {size_t{0}, size_t{5}})
        {
            // push esi; mov esi,[esp+8]; IMUL; mov eax,esi; pop esi; ret.
            std::vector<uint8_t> bytes{0x56, 0x8B, 0x74, 0x24, 0x08};
            AppendSpan(bytes, site); Append(bytes, {0x8B, 0xC6, 0x5E, 0xC3});
            Code code(bytes);
            if (code.allocation)
                for (uint32_t count : {0u, 1u, 3u})
                    Check(reinterpret_cast<uint32_t(__cdecl*)(uint32_t)>(code.allocation)(count) == count * 0x204,
                          "both allocation multipliers reserve complete expanded records");
        }
        std::vector<uint8_t> stride{0x8B, 0x54, 0x24, 0x04};
        AppendSpan(stride, 2); Append(stride, {0x8B, 0xC2, 0xC3});
        Code advance(stride);
        if (advance.allocation)
            for (uint32_t address : {0x1000u, 0x1204u, 0x1408u})
                Check(reinterpret_cast<uint32_t(__cdecl*)(uint32_t)>(advance.allocation)(address) == address + 0x204,
                      "record iteration advances by the expanded record stride");

        // Preserve ESI/EDI, load cdecl destination/source, execute the complete REP MOVSD span.
        std::vector<uint8_t> copy{0x56, 0x57, 0x8B, 0x44, 0x24, 0x0C, 0x8B, 0x74, 0x24, 0x10, 0xFC};
        AppendSpan(copy, 1); Append(copy, {0x5F, 0x5E, 0xC3});
        Code copier(copy);
        std::vector<uint8_t> source(16 + 3 * 0x204 + 16), destination(source.size(), 0xA5);
        for (size_t i = 0; i < source.size(); ++i) source[i] = static_cast<uint8_t>(i * 29 + 7);
        auto expected = destination;
        if (copier.allocation)
            for (size_t record = 0; record < 3; ++record)
            {
                const size_t offset = 16 + record * 0x204;
                reinterpret_cast<void(__cdecl*)(void*, const void*)>(copier.allocation)
                    (destination.data() + offset, source.data() + offset);
                std::memcpy(expected.data() + offset, source.data() + offset, 0x204);
                Check(destination == expected, "REP MOVSD copies all sound cells plus ID without touching adjacent records/canaries");
            }
        std::vector<uint8_t> payload;
        AppendSpan(payload, 6); Append(payload, {0x58, 0xC3});
        Code size(payload);
        if (size.allocation)
            Check(reinterpret_cast<uint32_t(__cdecl*)()>(size.allocation)() == 0x200,
                  "pushed initialization size covers all 128 sound cells");

        std::vector<uint8_t> zero{0x56, 0x8B, 0x74, 0x24, 0x08};
        AppendSpan(zero, 7); Append(zero, {0x5E, 0xC3});
        Code clearer(zero);
        std::vector<uint8_t> setId{0x8B, 0x44, 0x24, 0x04, 0x8B, 0x54, 0x24, 0x08};
        AppendSpan(setId, 8); Append(setId, {0xC3});
        Code setter(setId);
        if (clearer.allocation && setter.allocation)
            for (size_t record = 0; record < 3; ++record)
            {
                const size_t offset = 16 + record * 0x204;
                reinterpret_cast<void(__cdecl*)(void*)>(clearer.allocation)(destination.data() + offset);
                std::memset(expected.data() + offset + 0x200, 0, 4);
                Check(destination == expected, "ID initialization zeros only the trailing ID of each expanded record");
                const uint32_t id = 0x12340000u + static_cast<uint32_t>(record);
                reinterpret_cast<void(__cdecl*)(void*, uint32_t)>(setter.allocation)(destination.data() + offset, id);
                std::memcpy(expected.data() + offset + 0x200, &id, 4);
                Check(destination == expected, "ID assignment preserves sound cells, adjacent records and canaries");
            }
    }
    void IndexInstructions()
    {
        uint32_t guarded[130];
        for (size_t i = 0; i < std::size(guarded); ++i) guarded[i] = 0xAB000000u + static_cast<uint32_t>(i);
        for (size_t site : {size_t{3}, size_t{4}})
        {
            // EBP gives the original [EBP+16] sex argument. Both original rejection branches
            // land at span+21; the success POP EBP/RET occupies exactly span+19..20.
            std::vector<uint8_t> bytes{0x55, 0x8B, 0xEC, 0x8B, 0x45, 0x08, 0x8B, 0x55, 0x0C};
            AppendSpan(bytes, site);
            Append(bytes, {0x5D, 0xC3, 0x31, 0xC0, 0x5D, 0xC3});
            Code code(bytes);
            if (!code.allocation) continue;
            for (uint32_t race : {21u, 22u, 23u, 63u, 64u})
                for (uint32_t sex : {0u, 1u, 2u})
                {
                    const uint32_t result = reinterpret_cast<uint32_t(__cdecl*)(uint32_t*, uint32_t, uint32_t)>
                        (code.allocation)(guarded + 1, race, sex);
                    Check(result == (race < 64 && sex < 2 ? guarded[1 + race * 2 + sex] : 0),
                          "both lookup fragments admit races through 63, retain sex<2, and index race*2+sex");
                }
        }
        // Setter receives a row with sex at +12 and sound at +16; [EDI] is the sound table.
        // Its two rejection branches land at span+30, beyond the success return and padding.
        std::vector<uint8_t> bytes{0x56, 0x57, 0x8B, 0x7C, 0x24, 0x0C,
                                   0x8B, 0x4C, 0x24, 0x10, 0x8B, 0x74, 0x24, 0x14};
        AppendSpan(bytes, 9);
        Append(bytes, {0x5F, 0x5E, 0xC3, 0x90, 0x90, 0x90, 0x90, 0x5F, 0x5E, 0xC3});
        Code setter(bytes);
        if (!setter.allocation) return;
        for (uint32_t race : {21u, 22u, 23u, 63u, 64u})
            for (uint32_t sex : {0u, 1u, 2u})
            {
                uint32_t expected[130]; std::memcpy(expected, guarded, sizeof guarded);
                uint32_t row[5] = {0, 0, 0, sex, 0x12345678};
                uint32_t* table = guarded + 1;
                if (race < 64 && sex < 2) expected[1 + race * 2 + sex] = row[4];
                reinterpret_cast<void(__cdecl*)(uint32_t**, uint32_t, uint32_t*)>
                    (setter.allocation)(&table, race, row);
                Check(std::memcmp(expected, guarded, sizeof guarded) == 0,
                      "setter changes only race*2+sex cell and rejects race 64/sex 2 without touching canaries");
            }
    }
}

BOOL WINAPI FixtureVirtualProtect(LPVOID address, SIZE_T size, DWORD protection, PDWORD previous)
{
    ++fixture::protects;
    if (fixture::failProtect) { SetLastError(ERROR_ACCESS_DENIED); return FALSE; }
    return VirtualProtect(address, size, protection, previous);
}
namespace wxl::config
{
    bool Env(const char* name, bool fallback)
    {
        ++fixture::envCalls;
        fixture::correctEnv &= std::strcmp(name, "WXL_EMOTE_SOUNDS") == 0 && fallback;
        return fixture::enabled;
    }
}
namespace wxl::hook
{
    void RegisterFeature(const char* name, bool enabled, bool (*install)(), Phase phase)
    {
        if (std::strcmp(name, "emote-sounds") != 0) return;
        fixture::registeredName = name; fixture::registeredEnabled = enabled;
        fixture::registeredInstall = install; fixture::registeredPhase = phase;
    }
    bool Install(const char*, void*, void*, void**, int) { ++fixture::hooks; return true; }
}
namespace wxl::events { void Emit(Event, const void*) {} }
namespace wxl::log
{
    bool Enabled(Level) { return true; }
    void Write(Level level, const char* format, ...)
    {
        char message[512]; va_list args; va_start(args, format);
        std::vsnprintf(message, sizeof message, format, args); va_end(args);
        if (level == Level::Error) fixture::errors.emplace_back(message);
        if (level == Level::Info) fixture::infos.emplace_back(message);
    }
}
int main()
{
    static_assert(sizeof(void*) == 4, "emote-sound fixture requires Win32");
    fixture::Check(fixture::registeredName && fixture::registeredEnabled
                  && fixture::registeredInstall == &fixture::feature::Install
                  && fixture::registeredPhase == wxl::hook::Phase::Boot,
                  "emote-sounds registers enabled at Boot before client sound initialization");
    fixture::window = static_cast<uint8_t*>(VirtualAlloc(reinterpret_cast<void*>(fixture::kBase), fixture::kWindow,
                                                        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    fixture::Check(reinterpret_cast<uintptr_t>(fixture::window) == fixture::kBase,
                  "synthetic fixed-address window allocated (link fixture at /BASE:0x10000000)");
    if (fixture::window)
    {
        fixture::ResetWindow();
        fixture::Verification();
        fixture::Installation();
        fixture::RecordInstructions();
        fixture::IndexInstructions();
        VirtualFree(fixture::window, 0, MEM_RELEASE);
    }
    std::printf("emote sounds: %d checks, %d failures\n", fixture::checks, fixture::failures);
    return fixture::failures ? 1 : 0;
}
