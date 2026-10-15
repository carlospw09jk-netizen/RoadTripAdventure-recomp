// Pins authored player input mapping. No game data.
#include "../../src/native/gameplay/input.h"

#include <cstdio>

using namespace rt::native::gameplay;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

int main() {
    ControllerConfig cfg;
    AnalogAxes ax;
    CarInput n = playerCarInput(PadState{}, ax, cfg);
    CHECK(n.steer == 0 && !n.gas && !n.brake && !n.reverse);

    CHECK(playerCarInput(padWith(PhysicalButton::CROSS), ax, cfg).gas);
    CHECK(playerCarInput(padWith(PhysicalButton::SQUARE), ax, cfg).brake);
    CHECK(playerCarInput(padWith(PhysicalButton::CIRCLE), ax, cfg).reverse);
    CHECK(!playerCarInput(padWith(PhysicalButton::CROSS), ax, cfg).brake);

    AnalogAxes t; t.triggerGas = 1.f;
    CHECK(playerCarInput({}, t, cfg).gas);
    t = {}; t.triggerBrake = 1.f;
    CHECK(playerCarInput({}, t, cfg).brake);
    t = {}; t.triggerGas = 0.4f;
    CHECK(!playerCarInput({}, t, cfg).gas);

    AnalogAxes s; s.stickX = 0.5f;
    CHECK(playerCarInput({}, s, cfg).steer == 1);
    s.stickX = -0.5f;
    CHECK(playerCarInput({}, s, cfg).steer == -1);
    s.stickX = 0.1f;
    CHECK(playerCarInput({}, s, cfg).steer == 0);

    CHECK(!cfg.setButtonConfig(ControllerAction::ACCELERATE, PhysicalButton::TRIANGLE)); // already VIEW
    CHECK(cfg.setButtonConfig(ControllerAction::ACCELERATE, PhysicalButton::L3));
    PhysicalButton nb = cfg.getButtonConfig(ControllerAction::ACCELERATE);
    CHECK(nb == PhysicalButton::L3);
    CHECK(playerCarInput(padWith(nb), ax, cfg).gas);
    CHECK(!playerCarInput(padWith(PhysicalButton::CROSS), ax, cfg).gas);

    CarInput u = playerCarInput(padWith(PhysicalButton::START, PhysicalButton::SELECT), ax, cfg);
    CHECK(!u.gas && !u.brake && !u.reverse && u.steer == 0);

    if (g_fail == 0) std::printf("input tests passed\n");
    return g_fail ? 1 : 0;
}
