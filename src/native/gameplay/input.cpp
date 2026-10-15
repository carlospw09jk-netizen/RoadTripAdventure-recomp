#include "input.h"

namespace rt::native::gameplay {

namespace {
bool down(const PadState& pad, PhysicalButton b) {
    return b != PhysicalButton::NONE && (pad.pressedButtons & buttonMask(b)) != 0;
}
}

CarInput playerCarInput(const PadState& pad, const AnalogAxes& axes, const ControllerConfig& cfg) {
    CarInput in{};
    in.steer = axes.stickX >= kStickDeadzone ? 1 : (axes.stickX <= -kStickDeadzone ? -1 : 0);
    in.gas = down(pad, cfg.getButtonConfig(ControllerAction::ACCELERATE)) || axes.triggerGas >= kTriggerThreshold;
    in.brake = down(pad, cfg.getButtonConfig(ControllerAction::BRAKE)) || axes.triggerBrake >= kTriggerThreshold;
    in.reverse = down(pad, cfg.getButtonConfig(ControllerAction::REVERSE));
    return in;
}

void setPressed(PadState& pad, PhysicalButton button, bool isDown) {
    const std::uint16_t m = buttonMask(button);
    if (isDown) pad.pressedButtons = static_cast<std::uint16_t>(pad.pressedButtons | m);
    else pad.pressedButtons = static_cast<std::uint16_t>(pad.pressedButtons & ~m);
}

PadState padWith(PhysicalButton a, PhysicalButton b, PhysicalButton c) {
    PadState p;
    setPressed(p, a); setPressed(p, b); setPressed(p, c);
    return p;
}

} // namespace rt::native::gameplay
