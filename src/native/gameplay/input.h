// Authored player input: pad + analog axes -> CarInput (replaces IOP/pad path).
#pragma once
#include "car_state.h"
#include "menu.h"

namespace rt::native::gameplay {

struct AnalogAxes {
    float stickX = 0.f;       // -1..1
    float stickY = 0.f;       // -1..1
    float triggerGas = 0.f;   // 0..1
    float triggerBrake = 0.f; // 0..1
};

constexpr float kStickDeadzone = 0.2f;
constexpr float kTriggerThreshold = 0.5f;

CarInput playerCarInput(const PadState& pad, const AnalogAxes& axes, const ControllerConfig& cfg);

// Test/helper: set or clear a button bit in a PadState.
void setPressed(PadState& pad, PhysicalButton button, bool down = true);
PadState padWith(PhysicalButton a, PhysicalButton b = PhysicalButton::NONE,
                 PhysicalButton c = PhysicalButton::NONE);

} // namespace rt::native::gameplay
