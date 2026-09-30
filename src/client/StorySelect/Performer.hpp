// Bounded, disposable selector capability probe. GPL-3.0-or-later.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace wxl::story
{
    // IDs resolved from the client's AnimationData.dbc, not emote/gameplay IDs.
    constexpr unsigned kStand = 0, kWalk = 4, kSalute = 113;
    constexpr float kLegSeconds = 0.6f;
    struct Sample { unsigned clip; float distance; bool returning; bool done; };
    inline Sample Evaluate(float time, float saluteSeconds, float walkSpeed)
    {
        if (time < saluteSeconds) return {kSalute, 0, false, false};
        const float walk = time - saluteSeconds;
        if (walk < kLegSeconds) return {kWalk, walk * walkSpeed, false, false};
        if (walk < 2 * kLegSeconds)
            return {kWalk, (2 * kLegSeconds - walk) * walkSpeed, true, false};
        return {kStand, 0, false, true};
    }
    inline bool ValidDelta(double value)
    { return std::isfinite(value) && value >= 0 && value <= 0.5; }

    // Work in the captured actor placement basis; no authored scene coordinates.
    // Row-vector matrix, matching the existing M2 placement owner.
    inline void Placement(float out[16], const float origin[16], Sample sample)
    {
        std::copy(origin, origin + 16, out);
        for (unsigned i = 0; i < 3; ++i)
        {
            out[12 + i] += origin[i] * sample.distance;
            if (sample.returning) { out[i] = -origin[i]; out[4 + i] = -origin[4 + i]; }
        }
    }
}

// Existing admission and bounded first-failure evidence; no travel-policy change.
namespace wxl::story
{
    struct ClipRecord
    {
        uint16_t variation = 0;
        uint32_t flags = 0, durationMs = 0;
        float speed = 0;
    };
    struct ClipDiagnostic
    {
        const char* gate = "not-read";
        unsigned clip = 0, matches = 0, variation = 0;
        bool matchesRead = false, variationRead = false, recordRead = false;
        ClipRecord record;
        char model[277]{}; // Existing inline M2 model stem, not a pointer stored for later.
    };
    // The source adapter performs the existing native reads in the original gate order.
    // Tests supply a source with explicit missing stages, not client memory or new offsets.
    template<class Source>
    bool AdmitClip(Source& source, unsigned id, ClipDiagnostic& out)
    {
        out = {}; out.clip = id;
        if (!source.Live()) { out.gate = "instance-not-live"; return false; }
        if (!source.Shared(out.model, sizeof(out.model))) { out.gate = "shared-missing"; return false; }
        if (!source.Header()) { out.gate = "header-missing"; return false; }
        unsigned count = 0;
        if (!source.Table(count)) { out.gate = "table-invalid"; return false; }
        int chosen = -1;
        for (unsigned i = 0; i < count; ++i)
        {
            if (source.Id(i) != id) continue;
            ++out.matches;
            out.variation = source.Variation(i); out.variationRead = true;
            if (out.variation == 0) chosen = int(i);
        }
        out.matchesRead = true;
        if (chosen < 0) { out.gate = out.matches ? "clip-base-missing" : "clip-missing"; return false; }
        out.record = source.Record(unsigned(chosen)); out.recordRead = true;
        out.variation = out.record.variation;
        // Retain the native picker constraint and reject aliases instead of substituting idle.
        if (id != kStand && out.matches != 1) { out.gate = "clip-variation"; return false; }
        if (out.record.flags & 0x40) { out.gate = "clip-alias"; return false; }
        const float seconds = out.record.durationMs / 1000.0f;
        if (!std::isfinite(seconds) || seconds <= 0 || seconds > 15)
        { out.gate = "clip-duration"; return false; }
        if (!std::isfinite(out.record.speed)) { out.gate = "clip-speed"; return false; }
        out.gate = "accepted"; return true;
    }
    inline bool AdmitWalkSpeed(ClipDiagnostic& out)
    {
        if (out.record.speed <= 0 || out.record.speed > 8)
        {
            out.gate = out.record.speed == 0 ? "walk-speed-zero" : "walk-speed-range";
            return false;
        }
        return true;
    }
    inline void FormatDiagnostic(const ClipDiagnostic& detail, char* out, size_t capacity)
    {
        char matches[24] = "?", variation[24] = "?", duration[24] = "?", speed[32] = "?", flags[24] = "?";
        if (detail.matchesRead) std::snprintf(matches, sizeof(matches), "%u", detail.matches);
        if (detail.variationRead) std::snprintf(variation, sizeof(variation), "%u", detail.variation);
        if (detail.recordRead)
        {
            std::snprintf(duration, sizeof(duration), "%u", detail.record.durationMs);
            std::snprintf(speed, sizeof(speed), "%.9g", detail.record.speed);
            std::snprintf(flags, sizeof(flags), "%u", detail.record.flags);
        }
        std::snprintf(out, capacity, "v1;gate=%s;clip=%u;model=%s;matches=%s;variation=%s;durationMs=%s;speed=%s;flags=%s",
            detail.gate, detail.clip, detail.model[0] ? detail.model : "?", matches, variation, duration, speed, flags);
    }
}
