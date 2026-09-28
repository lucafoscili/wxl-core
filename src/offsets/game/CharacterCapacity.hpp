// Exact Beta 12340 native capacity boundary; GPL-3.0-or-later.
#pragma once
#include "offsets/game/StorySelect.hpp"
namespace wxl::offsets::game::capacity
{
    constexpr uint8_t kMaximum = 50;
    // 80 7D FF 0A: cmp byte ptr [ebp-1], 10. Retain stock failure path.
    constexpr uintptr_t kLimitImmediate = 0x00464C4F;
    using Site = wxl::offsets::game::story::Site;
    inline constexpr Site kSites[] = {
        {"decode", 0x464c10, 0x33d, 0xCA81FECF58C4D436ULL},
        {"connectionAllocate", 0x4644c0, 0x9a, 0x1EA65FCB7809F586ULL},
        {"connectionResize", 0x464b30, 0x82, 0xE01E32941B72CEFAULL},
        {"enumerate", 0x6b1560, 0x5b, 0x9FD199028C0B48CAULL},
        {"glueAppend", 0x4e3760, 0x7a, 0xC58D359A99D36B0BULL},
        {"glueAllocate", 0x4e25c0, 0x12b, 0xB37D0394F93A6239ULL},
        {"glueCopy", 0x4e3ca0, 0x2d, 0xBE6AF208CBF5B06AULL},
    };
}
