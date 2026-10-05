#pragma once
#include <cstdint>
namespace wxl::input::hover
{
    // Existing window-input owner: normal builds are no-ops, the opt-in trial
    // refreshes action mouseover before subscribers/native input can consume it.
    void BeforeInput(uint32_t message);
}
