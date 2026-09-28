// Bounded selection policy. Native spells, menu queries and rendering remain native.
// GPL-3.0-or-later.
#pragma once
#include "offsets/game/MinimapTracking.hpp"

namespace wxl::tracking
{
    namespace off = wxl::offsets::game::tracking;
    enum class Choice { Native, EnableQuest, DisableQuest };

    constexpr Choice Select(uintptr_t requested, uintptr_t current, uint32_t spell)
    {
        if (requested != off::kTrivialQuests || (spell != 0 && spell != off::kFindHerbs))
            return Choice::Native;
        if (current == off::kTrivialQuests)
            return Choice::DisableQuest;
        return spell == off::kFindHerbs ? Choice::EnableQuest : Choice::Native;
    }

    constexpr bool Preserve(uintptr_t requested, uintptr_t current, uint32_t spell, uintptr_t caller)
    {
        return requested == 0 && current == off::kTrivialQuests && spell == off::kFindHerbs
            && caller == off::kSpellUpdateReturn;
    }
}
