// Authored scripted day/night step machine: plain C++, no PS2/guest types;
// guest addresses appear in comments only.
//
// Primary spec: build/scratch/decomp-audit/daycycle-steps.md (Ghidra
// decompile of ApplyTimeOfDayLighting 0x26CA48 and AdvanceDayCycleStep
// 0x26D1C0). This models the cumulative counter and step transitions only;
// callers use currentLightingClock() to feed updateTimeOfDayLight each frame.
#pragma once

#include <cstdint>

namespace rt::native::gameplay {

struct DayCycleStep {
    std::uint32_t startClock;
    std::uint16_t targetTick;
};

inline constexpr DayCycleStep kDayCycleSteps[10] = {
    {72000u, 0u}, {81000u, 1260u}, {108000u, 2520u},
    {117000u, 3780u}, {126000u, 5040u}, {135000u, 6300u},
    {162000u, 7560u}, {163000u, 8820u}, {171000u, 10080u},
    {189000u, 12033u},
};

enum class DayCycleEvent {
    NONE,
    FADE_IN,
    STEP_ADVANCED,
    FINISHED,
};

class DayCycleStepper {
public:
    // Advances one step-machine frame. Step 0's zero target transitions before
    // the first counter increment, matching its immediate zero-duration entry.
    DayCycleEvent advance();

    [[nodiscard]] int step() const { return step_; }
    [[nodiscard]] std::uint32_t tickCount() const { return tickCount_; }
    [[nodiscard]] bool finished() const { return finished_; }
    [[nodiscard]] std::uint32_t currentLightingClock() const {
        return kDayCycleSteps[step_].startClock;
    }

private:
    int step_ = 0;
    std::uint32_t tickCount_ = 0;
    bool finished_ = false;
    bool fadeInFired_ = false;
};

}  // namespace rt::native::gameplay
