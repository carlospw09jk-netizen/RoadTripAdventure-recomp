// Pins the authored scripted day/night step machine (AdvanceDayCycleStep 0x26D1C0).
// No game data.
#include "../../src/native/gameplay/daycycle.h"

#include <cstdio>

using namespace rt::native::gameplay;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static void testInitialStateAndImmediateStepZeroTransition() {
    DayCycleStepper stepper;
    CHECK(stepper.step() == 0);
    CHECK(stepper.tickCount() == 0u);
    CHECK(stepper.currentLightingClock() == 72000u);
    CHECK(!stepper.finished());

    CHECK(stepper.advance() == DayCycleEvent::STEP_ADVANCED);
    CHECK(stepper.step() == 1);
    CHECK(stepper.tickCount() == 0u);
    CHECK(stepper.currentLightingClock() == 81000u);
}

static void testStepOneFadeAndTransitionUseCumulativeTicks() {
    DayCycleStepper stepper;
    CHECK(stepper.advance() == DayCycleEvent::STEP_ADVANCED);  // step 0 -> 1

    // The binary condition is target - 0x40: 1260 - 64 = 1196.
    for (std::uint32_t tick = 1; tick < 1196u; ++tick) {
        CHECK(stepper.advance() == DayCycleEvent::NONE);
    }
    CHECK(stepper.tickCount() == 1195u);
    CHECK(stepper.advance() == DayCycleEvent::FADE_IN);
    CHECK(stepper.tickCount() == 1196u);
    CHECK(stepper.step() == 1);

    for (std::uint32_t tick = 1197; tick < 1260u; ++tick) {
        CHECK(stepper.advance() == DayCycleEvent::NONE);
    }
    CHECK(stepper.advance() == DayCycleEvent::STEP_ADVANCED);
    CHECK(stepper.tickCount() == 1260u);
    CHECK(stepper.step() == 2);
    CHECK(stepper.currentLightingClock() == 108000u);
}

static void testFadeInFiresOnlyOncePerStep() {
    DayCycleStepper stepper;
    stepper.advance();  // step 0 -> 1
    for (std::uint32_t tick = 1; tick < 1196u; ++tick) stepper.advance();
    CHECK(stepper.advance() == DayCycleEvent::FADE_IN);
    CHECK(stepper.advance() == DayCycleEvent::NONE);
    CHECK(stepper.advance() == DayCycleEvent::NONE);
}

static void testScriptedTransitionsAndFinalCompletion() {
    DayCycleStepper stepper;
    constexpr std::uint32_t kClocks[] = {
        72000u, 81000u, 108000u, 117000u, 126000u,
        135000u, 162000u, 163000u, 171000u, 189000u,
    };
    constexpr std::uint32_t kFadeTicks[] = {
        1196u, 2456u, 3716u, 4976u, 6236u,
        7496u, 8756u, 10016u, 11969u,
    };
    constexpr std::uint32_t kTransitionTicks[] = {
        1260u, 2520u, 3780u, 5040u, 6300u,
        7560u, 8820u, 10080u,
    };

    CHECK(stepper.currentLightingClock() == kClocks[0]);
    CHECK(stepper.advance() == DayCycleEvent::STEP_ADVANCED);
    CHECK(stepper.step() == 1);
    CHECK(stepper.tickCount() == 0u);
    CHECK(stepper.currentLightingClock() == kClocks[1]);

    int expectedStep = 1;
    std::size_t fadeIndex = 0;
    std::size_t transitionIndex = 0;
    for (;;) {
        const DayCycleEvent event = stepper.advance();
        if (event == DayCycleEvent::FADE_IN) {
            CHECK(fadeIndex < sizeof(kFadeTicks) / sizeof(kFadeTicks[0]));
            if (fadeIndex < sizeof(kFadeTicks) / sizeof(kFadeTicks[0])) {
                CHECK(stepper.tickCount() == kFadeTicks[fadeIndex]);
            }
            ++fadeIndex;
        } else if (event == DayCycleEvent::STEP_ADVANCED) {
            CHECK(transitionIndex < sizeof(kTransitionTicks) / sizeof(kTransitionTicks[0]));
            if (transitionIndex < sizeof(kTransitionTicks) / sizeof(kTransitionTicks[0])) {
                CHECK(stepper.tickCount() == kTransitionTicks[transitionIndex]);
            }
            ++transitionIndex;
            ++expectedStep;
            CHECK(stepper.step() == expectedStep);
            CHECK(stepper.currentLightingClock() == kClocks[stepper.step()]);
        }
        if (event == DayCycleEvent::FINISHED) break;
    }

    CHECK(fadeIndex == sizeof(kFadeTicks) / sizeof(kFadeTicks[0]));
    CHECK(transitionIndex == sizeof(kTransitionTicks) / sizeof(kTransitionTicks[0]));
    CHECK(stepper.step() == 9);
    CHECK(stepper.tickCount() == 12033u);
    CHECK(stepper.finished());
    CHECK(stepper.currentLightingClock() == 189000u);
    CHECK(stepper.advance() == DayCycleEvent::NONE);
    CHECK(stepper.tickCount() == 12033u);
}

int main() {
    testInitialStateAndImmediateStepZeroTransition();
    testStepOneFadeAndTransitionUseCumulativeTicks();
    testFadeInFiresOnlyOncePerStep();
    testScriptedTransitionsAndFinalCompletion();
    if (g_fail) { std::fprintf(stderr, "%d failure(s)\n", g_fail); return 1; }
    std::puts("daycycle: all checks passed");
    return 0;
}
