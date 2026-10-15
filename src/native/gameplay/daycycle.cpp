#include "gameplay/daycycle.h"

namespace rt::native::gameplay {

DayCycleEvent DayCycleStepper::advance() {
    if (finished_) return DayCycleEvent::NONE;

    // The initial lighting task has a target of zero, so its entry ends before
    // the shared cumulative counter begins advancing the timed steps.
    if (step_ == 0 && tickCount_ == 0u) {
        ++step_;
        fadeInFired_ = false;
        return DayCycleEvent::STEP_ADVANCED;
    }

    ++tickCount_;
    const std::uint32_t target = kDayCycleSteps[step_].targetTick;
    if (tickCount_ == target - 0x40u && !fadeInFired_) {
        fadeInFired_ = true;
        return DayCycleEvent::FADE_IN;
    }

    if (tickCount_ != target) return DayCycleEvent::NONE;

    if (step_ == 9) {
        finished_ = true;
        return DayCycleEvent::FINISHED;
    }

    ++step_;
    fadeInFired_ = false;
    return DayCycleEvent::STEP_ADVANCED;
}

}  // namespace rt::native::gameplay
