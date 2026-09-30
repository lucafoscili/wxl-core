#include "client/StorySelect/Performer.hpp"
#include <cassert>
#include <limits>
#include <cstdio>
#include <cstring>
#include <vector>

namespace
{
    struct Source
    {
        int failStage = 0, reached = 0, recordReads = 0;
        struct Row { unsigned id; wxl::story::ClipRecord record; };
        std::vector<Row> rows{{0,{0,0x20,6667,0}}, {0,{1,0x20,3333,0}},
            {113,{0,0x81,2000,0}}, {4,{0,0x20,1000,2.5f}}};
        bool Stage(int stage) { reached = stage; return failStage != stage; }
        bool Live() { return Stage(1); }
        bool Shared(char* model, size_t capacity)
        {
            if (!Stage(2)) return false;
            std::snprintf(model, capacity, "Character\\Test\\Actor"); return true;
        }
        bool Header() { return Stage(3); }
        bool Table(unsigned& count) { count = unsigned(rows.size()); return Stage(4); }
        unsigned Id(unsigned index) { return rows[index].id; }
        unsigned Variation(unsigned index) { return rows[index].record.variation; }
        wxl::story::ClipRecord Record(unsigned index) { ++recordReads; return rows[index].record; }
    };
    void AdmissionTests()
    {
        using namespace wxl::story;
        const char* early[] = {"instance-not-live", "shared-missing", "header-missing", "table-invalid"};
        for (int stage = 1; stage <= 4; ++stage)
        {
            Source source; source.failStage = stage; ClipDiagnostic detail;
            assert(!AdmitClip(source, kStand, detail));
            assert(!std::strcmp(detail.gate, early[stage-1]));
            assert(detail.clip == kStand && source.reached == stage && source.recordReads == 0);
            assert(!detail.recordRead && !detail.matchesRead);
            char encoded[512]; FormatDiagnostic(detail, encoded, sizeof(encoded));
            assert(std::strstr(encoded, ";speed=?;") && std::strstr(encoded, ";durationMs=?;"));
            if (stage <= 2) assert(std::strstr(encoded, ";model=?;"));
        }
        Source source; ClipDiagnostic detail;
        assert(AdmitClip(source, kStand, detail) && detail.matches == 2); // Stand variation allowance retained.
        assert(AdmitClip(source, kSalute, detail) && detail.record.durationMs == 2000);
        assert(AdmitClip(source, kWalk, detail) && AdmitWalkSpeed(detail));
        assert(!AdmitClip(source, 99, detail) && !std::strcmp(detail.gate, "clip-missing"));
        assert(detail.matchesRead && detail.matches == 0 && !detail.recordRead);
        source.rows.back().record.variation = 1;
        assert(!AdmitClip(source, kWalk, detail) && !std::strcmp(detail.gate, "clip-base-missing"));
        assert(detail.matches == 1 && !detail.recordRead);
        char missingBase[512]; FormatDiagnostic(detail, missingBase, sizeof(missingBase));
        assert(detail.variationRead && std::strstr(missingBase, ";variation=1;"));
        assert(std::strstr(missingBase, ";speed=?;"));
        source.rows.back().record.variation = 0;
        source.rows.push_back({4,{1,0x20,1500,3}});
        assert(!AdmitClip(source, kWalk, detail) && !std::strcmp(detail.gate, "clip-variation"));
        assert(detail.matches == 2 && detail.recordRead && detail.record.speed == 2.5f);
        source.rows.pop_back(); source.rows.back().record.flags = 0x60;
        assert(!AdmitClip(source, kWalk, detail) && !std::strcmp(detail.gate, "clip-alias"));
        source.rows.back().record.flags = 0x20;
        for (uint32_t duration : {0u, 15001u})
        {
            source.rows.back().record.durationMs = duration;
            assert(!AdmitClip(source, kWalk, detail) && !std::strcmp(detail.gate, "clip-duration"));
            assert(detail.record.durationMs == duration);
        }
        source.rows.back().record.durationMs = 15000;
        assert(AdmitClip(source, kWalk, detail));
        for (float speed : {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()})
        {
            source.rows.back().record.speed = speed;
            assert(!AdmitClip(source, kWalk, detail) && !std::strcmp(detail.gate, "clip-speed"));
            assert(detail.recordRead && !std::isfinite(detail.record.speed));
        }
        for (float speed : {0.f, -1.f, 8.01f})
        {
            source.rows.back().record.speed = speed;
            assert(AdmitClip(source, kWalk, detail) && !AdmitWalkSpeed(detail));
            assert(!std::strcmp(detail.gate, speed == 0 ? "walk-speed-zero" : "walk-speed-range"));
            if (speed == 0)
            {
                char encoded[512]; FormatDiagnostic(detail, encoded, sizeof(encoded));
                assert(std::strstr(encoded, ";speed=0;") && std::strstr(encoded, ";variation=0;"));
            }
        }
        source.rows.back().record.speed = 8;
        assert(AdmitClip(source, kWalk, detail) && AdmitWalkSpeed(detail));
        // Multiple bad fields must report the same first gate as the original short circuit.
        source.rows.back().record.flags = 0x60;
        source.rows.back().record.durationMs = 0;
        source.rows.back().record.speed = std::numeric_limits<float>::quiet_NaN();
        source.rows.push_back({4,{1,0x20,1000,2}});
        assert(!AdmitClip(source, kWalk, detail) && !std::strcmp(detail.gate, "clip-variation"));
        source.rows.pop_back();
        assert(!AdmitClip(source, kWalk, detail) && !std::strcmp(detail.gate, "clip-alias"));
        source.rows.back().record.flags = 0x20;
        assert(!AdmitClip(source, kWalk, detail) && !std::strcmp(detail.gate, "clip-duration"));
    }
}
int main()
{
    AdmissionTests();
    using namespace wxl::story;
    assert(!ValidDelta(std::numeric_limits<double>::quiet_NaN()));
    assert(!ValidDelta(-1) && !ValidDelta(10) && ValidDelta(0.016));
    assert(Evaluate(0, 2, 2.5).clip == kSalute);
    assert(Evaluate(2.3f, 2, 2.5).clip == kWalk);
    auto back = Evaluate(2.9f, 2, 2.5);
    assert(back.returning && std::abs(back.distance - 0.75f) < 0.0001f);
    float origin[16] = {0,2,0,0,-2,0,0,0,0,0,2,0,7,8,9,1}, out[16];
    Placement(out, origin, back);
    assert(out[12] == 7 && std::abs(out[13] - 9.5f) < 0.0001f && out[14] == 9);
    assert(out[1] == -2 && out[4] == 2); // face along return direction, preserve scale
    for (unsigned loop = 0; loop < 50; ++loop)
    {
        const auto end = Evaluate(20, 2, 2.5);
        assert(end.done && end.clip == kStand);
        Placement(out, origin, end);
        for (unsigned i = 0; i < 16; ++i) assert(out[i] == origin[i]);
    }
    std::puts("Story probe admission diagnostics and route arithmetic passed; no native rendering exercised.");
}
