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
    struct ProbeRoute { float legDistance, legSeconds; };
    // AUTHOR-CHOSEN PROBE CHOREOGRAPHY, not measured stride or model movingSpeed.
    // Preserve G1's original short-route intent: 2.5 * 0.6 = 1.5 local units.
    constexpr ProbeRoute kG1ProbeRoute{1.5f, 0.6f};
    inline bool ValidRoute(ProbeRoute route)
    {
        return std::isfinite(route.legDistance) && route.legDistance > 0 && route.legDistance <= 2 &&
            std::isfinite(route.legSeconds) && route.legSeconds >= 0.1f && route.legSeconds <= 2;
    }
    struct Sample { unsigned clip; float distance; bool returning; bool done; };
    inline Sample Evaluate(float time, float saluteSeconds, ProbeRoute route)
    {
        if (!ValidRoute(route) || !std::isfinite(time) || time < 0 ||
            !std::isfinite(saluteSeconds) || saluteSeconds <= 0 || saluteSeconds > 15)
            return {kStand, 0, false, true};
        if (time < saluteSeconds) return {kSalute, 0, false, false};
        const float walk = time - saluteSeconds;
        if (walk < route.legSeconds)
            return {kWalk, route.legDistance * (walk / route.legSeconds), false, false};
        if (walk < 2 * route.legSeconds)
            return {kWalk, route.legDistance * ((2 * route.legSeconds - walk) / route.legSeconds), true, false};
        return {kStand, 0, false, true};
    }
    inline bool ValidDelta(double value)
    { return std::isfinite(value) && value >= 0 && value <= 0.5; }
    inline float ClampedDelta(double value)
    { return static_cast<float>(std::min(value, 0.05)); } // Only after ValidDelta.

    // Author distance in the captured actor placement basis; preserve native scale.
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

// Source clip validity and raw metadata; route distance is independently authored.
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
