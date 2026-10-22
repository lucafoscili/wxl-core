// Exercise production verification, synthetic loaded records, and generated x86 restriction code.
// Only bounded original verification spans are reproduced; no client image or assets are loaded.
// GPL-3.0-or-later.
#include "client/CharCreate/RaceTables.cpp"
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace fixture
{
    namespace feature = wxl::client::racetables;
    namespace off = wxl::offsets::game::charcreate;
    int checks = 0, failures = 0, hooks = 0;
    std::vector<std::string> errors;
    const char* registeredName = nullptr;
    bool registeredEnabled = false;
    bool (*registeredInstall)() = nullptr;
    wxl::hook::Phase registeredPhase;
    void Check(bool condition, const char* message)
    {
        ++checks;
        if (!condition) { ++failures; std::printf("FAIL: %s\n", message); }
    }
    struct Span { uintptr_t address; const char* hex; };
    // Exact, bounded verification instructions, independently keyed by their original addresses.
    const Span spans[] = {
        {0x004E157A, "8b0485d0b0b600"}, {0x004E15B2, "890495d0b0b600"},
        {0x004E16A0, "8b048dd0b0b600"}, {0x004E1C39, "68d0b0b600"},
        {0x004E1E93, "bed0b0b600"}, {0x004E20EB, "8b0495d0b0b600"},
        {0x004E2124, "890495d0b0b600"}, {0x004E2227, "8b0c8dd0b0b600"},
        {0x004E1C33, "68b0000000"}, {0x004E1E98, "c745fc16000000"},
        {0x004CDA40, "8b0c8d8041b200"}, {0x004D9EAD, "8b04958041b200"},
        {0x004DFAF0, "0f873c010000"}, {0x004DFADB, "8d47ff"},
        {0x004DFAE1, "83f80a"}, {0x004DFAF6, "ff248560fc4d00"},
        {0x004DFC18, "83ec08dd1c2456e87ce63600"},
        {0x004DFC32, "6a155768845e9f0056e840f63600"},
        {0x0063AB00, "558bec83ec14568bf1"},
        {0x0063AD44, "c74604010000005e8be55dc20800"},
    };
    std::vector<std::vector<uint8_t>> original;
    void Verification()
    {
        Check(std::size(spans) == std::size(off::kSites), "fixture covers all twenty verification sites");
        if (std::size(spans) != std::size(off::kSites)) return;
        for (size_t s = 0; s < std::size(spans); ++s)
        {
            const auto& span = spans[s];
            const size_t length = std::strlen(span.hex) / 2;
            Check(off::kSites[s].address == span.address && off::kSites[s].size == length,
                  "site address and complete instruction length match independently recorded spans");
            original.emplace_back(length);
            for (size_t i = 0; i < length; ++i)
            {
                unsigned byte = 0;
                std::sscanf(span.hex + 2 * i, "%2x", &byte);
                original.back()[i] = static_cast<uint8_t>(byte);
            }
        }
        const auto read = [](uintptr_t address) -> uint8_t {
            for (size_t s = 0; s < std::size(spans); ++s)
                if (address >= spans[s].address && address - spans[s].address < original[s].size())
                    return original[s][address - spans[s].address];
            Check(false, "verification reads only declared instruction spans");
            return 0;
        };
        Check(feature::VerifySites(read) && errors.empty(), "all original spans verify without errors");
        for (size_t s = 0; s < std::size(spans); ++s)
            for (size_t i = 0; i < original[s].size(); ++i)
            {
                errors.clear(); original[s][i] ^= 0x80;
                Check(!feature::VerifySites(read), "every verification-byte mismatch is rejected");
                Check(errors.size() == 1 && errors[0].find(off::kSites[s].name) != std::string::npos,
                      "every mismatch emits exactly one error naming its site");
                Check(hooks == 0 && !feature::g_load && !feature::g_restrictionThunk,
                      "site verification performs no hook registration or thunk allocation");
                original[s][i] ^= 0x80;
            }
        errors.clear();
        Check(feature::VerifySites(read), "restored original spans verify again");
    }

    void Names()
    {
        // +0x18 deliberately differs from +0x2C, preventing prefix/fallback confusion.
        uint8_t records[46][0x40] = {};
        uintptr_t table[46] = {};
        const char* tokens[46] = {};
        char strings[46][16] = {};
        const char* stock[22] = {};
        char stockStrings[22][16] = {};
        for (size_t i = 0; i < 22; ++i)
        {
            std::snprintf(stockStrings[i], sizeof stockStrings[i], "stock-%u", static_cast<unsigned>(i));
            stock[i] = stockStrings[i]; feature::g_names[i] = stock[i];
        }
        for (size_t i = 0; i < 46; ++i)
        {
            std::snprintf(strings[i], sizeof strings[i], "race-%u", static_cast<unsigned>(i + 19));
            tokens[i] = i == 5 ? nullptr : i == 6 ? "" : strings[i];
            const char* wrong = "wrong-prefix";
            std::memcpy(records[i] + 0x18, &wrong, sizeof wrong);
            std::memcpy(records[i] + 0x2C, &tokens[i], sizeof tokens[i]);
            table[i] = i == 4 ? 0 : reinterpret_cast<uintptr_t>(records[i]);
        }
        std::fill(std::begin(feature::g_names) + 22, std::end(feature::g_names), "stale-name");
        feature::FillRaceNames(19, 64, reinterpret_cast<uintptr_t>(table));
        for (size_t i = 0; i < 22; ++i) Check(feature::g_names[i] == stock[i], "name fill preserves each stock pointer 0..21");
        for (size_t race = 22; race < 64; ++race)
            Check(feature::g_names[race] == (table[race - 19] ? tokens[race - 19] : nullptr),
                  "IDs 22..63 use ClientFileString at +0x2C, including sparse/null/empty rows");
        Check(feature::g_names[63] == tokens[44], "race 63 is included at the expanded upper boundary");
        feature::FillRaceNames(25, 62, reinterpret_cast<uintptr_t>(table + 6));
        Check(!feature::g_names[22] && !feature::g_names[23] && !feature::g_names[24] && !feature::g_names[63],
              "rows outside loaded min/max clear stale values without fallback");
        Check(feature::g_names[25] == tokens[6] && feature::g_names[62] == tokens[43],
              "min/max bounds are inclusive and table indexing subtracts minimum");
        feature::FillRaceNames(19, 64, 0);
        bool cleared = true;
        for (size_t i = 22; i < 64; ++i) cleared &= feature::g_names[i] == nullptr;
        Check(cleared, "a missing table clears every expanded name to null");
        Check(std::memcmp(stock, feature::g_names, sizeof stock) == 0,
              "clearing expanded names still preserves the stock range");
        std::memset(feature::g_names, 0, sizeof feature::g_names);
    }
    uint32_t RelativeTarget(const std::array<uint8_t,16>& code, size_t operand, uint32_t next)
    {
        uint32_t displacement = 0;
        std::memcpy(&displacement, code.data() + operand, 4);
        return next + displacement;
    }
    // This bridge records all GP registers and stack depth around the generated code. Synthetic
    // destination stubs only store an outcome (and pop the accepted zero), then return here.
    uintptr_t executeThunk = 0, beforeEsp = 0, afterEsp = 0;
    uint32_t inputEax = 0, afterEax = 0, afterEbx = 0, afterEcx = 0, afterEdx = 0;
    uint32_t afterEsi = 0, afterEdi = 0, afterEbp = 0, outcome = 0;
    float acceptedZero = -7, preservedX87 = -7;
    uint16_t x87Status = 0;
    void Execute()
    {
        __asm {
            pushad
            fninit
            fld1
            mov beforeEsp, esp
            mov eax, inputEax
            mov ebx, 11223344h
            mov ecx, 22334455h
            mov edx, 33445566h
            mov esi, 44556677h
            mov edi, 55667788h
            mov ebp, 66778899h
            call dword ptr executeThunk
            mov afterEsp, esp
            mov afterEax, eax
            mov afterEbx, ebx
            mov afterEcx, ecx
            mov afterEdx, edx
            mov afterEsi, esi
            mov afterEdi, edi
            mov afterEbp, ebp
            fstp preservedX87
            fnstsw x87Status
            popad
        }
    }
    __declspec(naked) void ResultStub()
    {
        __asm { fstp acceptedZero
                mov outcome, 1
                ret }
    }
    __declspec(naked) void ErrorStub()
    {
        __asm { mov outcome, 2
                ret }
    }
    void Thunk()
    {
        for (const uint32_t address : {0x00010000u,0x10000000u,0xF0000000u})
        {
            const auto code = feature::RestrictionThunk(address);
            Check(code.size() == 16 && code[0] == 0x83 && code[1] == 0xF8 && code[2] == 0x3E
                  && code[3] == 0x0F && code[4] == 0x87 && code[9] == 0xD9 && code[10] == 0xEE && code[11] == 0xE9,
                  "thunk is exactly cmp eax,62; near ja; fldz; near jmp");
            Check(RelativeTarget(code, 5, address + 9) == 0x004DFC32
                  && RelativeTarget(code, 12, address + 16) == 0x004DFC18,
                  "both rel32 operands reach exact original targets from low/high addresses");
        }
        auto* allocation = VirtualAlloc(nullptr, 16, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        Check(allocation != nullptr, "synthetic executable thunk allocation succeeds");
        if (!allocation) return;
        executeThunk = reinterpret_cast<uintptr_t>(allocation);
        auto code = feature::RestrictionThunk(executeThunk);
        Check(RelativeTarget(code, 5, executeThunk + 9) == 0x004DFC32
              && RelativeTarget(code, 12, executeThunk + 16) == 0x004DFC18,
              "actual allocation also generates original error/result destinations");
        // Redirect only the already-verified rel32 destinations to local return stubs. All
        // production comparison/branch/fldz opcodes are executed unchanged, at arbitrary VA.
        const uint32_t error = reinterpret_cast<uintptr_t>(&ErrorStub) - (executeThunk + 9);
        const uint32_t result = reinterpret_cast<uintptr_t>(&ResultStub) - (executeThunk + 16);
        std::memcpy(code.data() + 5, &error, 4); std::memcpy(code.data() + 12, &result, 4);
        std::memcpy(allocation, code.data(), code.size());
        FlushInstructionCache(GetCurrentProcess(), allocation, code.size());
        for (uint32_t race : {0u,12u,22u,63u,64u})
        {
            inputEax = race - 1; outcome = 0; acceptedZero = preservedX87 = -7;
            Execute();
            const bool accepted = race >= 12 && race <= 63;
            Check(outcome == (accepted ? 1u : 2u), "executed thunk admits 12..63 and rejects unsigned underflow/64");
            Check(acceptedZero == (accepted ? 0.0f : -7.0f), "only accepted races deliver x87 zero to result");
            Check(preservedX87 == 1.0f && (x87Status & 0x3841) == 0,
                  "thunk preserves prior x87 content without surplus values or stack faults");
            Check(beforeEsp == afterEsp && afterEax == inputEax && afterEbx == 0x11223344
                  && afterEcx == 0x22334455 && afterEdx == 0x33445566 && afterEsi == 0x44556677
                  && afterEdi == 0x55667788 && afterEbp == 0x66778899,
                  "executed thunk preserves stack depth and every general-purpose register");
        }
        VirtualFree(allocation, 0, MEM_RELEASE);
    }
}

namespace wxl::config { bool Env(const char*, bool fallback) { return fallback; } }
namespace wxl::hook
{
    void RegisterFeature(const char* name, bool enabled, bool (*install)(), Phase phase)
    {
        fixture::registeredName = name; fixture::registeredEnabled = enabled;
        fixture::registeredInstall = install; fixture::registeredPhase = phase;
    }
    bool Install(const char*, void*, void*, void**, int) { ++fixture::hooks; return true; }
}
namespace wxl::log
{
    bool Enabled(Level) { return true; }
    void Write(Level level, const char* format, ...)
    {
        if (level != Level::Error) return;
        char message[512]; va_list args; va_start(args, format);
        std::vsnprintf(message, sizeof message, format, args); va_end(args);
        fixture::errors.emplace_back(message);
    }
}
int main()
{
    static_assert(sizeof(void*) == 4, "race-table fixture requires Win32");
    fixture::Check(fixture::registeredName && std::strcmp(fixture::registeredName, "race-tables") == 0
                  && fixture::registeredEnabled && fixture::registeredInstall == &fixture::feature::Install
                  && fixture::registeredPhase == wxl::hook::Phase::Boot,
                  "feature registers for Boot before initialization and DBC loading");
    fixture::Verification();
    fixture::Names();
    fixture::Thunk();
    std::printf("race tables: %d checks, %d failures\n", fixture::checks, fixture::failures);
    return fixture::failures ? 1 : 0;
}
