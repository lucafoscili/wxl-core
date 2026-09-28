// Bounded policy transitions; no client/server emulation. GPL-3.0-or-later.
#include "client/MinimapTracking/HerbQuests.hpp"
#include <cassert>
#include <iostream>

int main()
{
    using namespace wxl::tracking;
    constexpr auto quest = off::kTrivialQuests;
    constexpr auto herb = off::kFindHerbs;
    constexpr uintptr_t bank = 0xa11d40;
    constexpr uint32_t mineral = 2580;
    assert(Select(quest, 0, herb) == Choice::EnableQuest);
    assert(Select(quest, quest, herb) == Choice::DisableQuest);
    assert(Select(quest, quest, 0) == Choice::DisableQuest);
    assert(Select(quest, 0, 0) == Choice::Native);
    assert(Select(quest, 0, mineral) == Choice::Native);
    assert(Select(quest, quest, mineral) == Choice::Native);
    assert(Select(bank, quest, herb) == Choice::Native);
    assert(Select(0, quest, herb) == Choice::Native); // None uses actual cancellation
    assert(Preserve(0, quest, herb, off::kSpellUpdateReturn));
    assert(!Preserve(0, quest, 0, off::kSpellUpdateReturn)); // aura removed/rejected
    assert(!Preserve(0, quest, mineral, off::kSpellUpdateReturn));
    assert(!Preserve(0, bank, herb, off::kSpellUpdateReturn));
    assert(!Preserve(bank, quest, herb, off::kSpellUpdateReturn));
    assert(!Preserve(0, quest, herb, 0x57f4e6)); // explicit None must not be retained
    std::cout << "Herb/quest pair transitions and ordinary tracker exclusions passed.\n";
}
