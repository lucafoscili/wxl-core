// Bounded, disposable selector capability probe. GPL-3.0-or-later.
#pragma once
#include <algorithm>
#include <cmath>

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
