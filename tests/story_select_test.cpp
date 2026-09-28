#include "client/StorySelect/Performer.hpp"
#include <cassert>
#include <limits>
#include <cstdio>
int main()
{
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
    std::puts("Story probe route arithmetic passed; no native rendering exercised.");
}
