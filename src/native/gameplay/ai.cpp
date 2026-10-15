// Authored AI traffic brain implementation. See ai.h for the model and the
// mapping to the documented original (docs/game-mechanics/car-ai-and-camera.md
// §1). Pure host C++, no game data.
#include "gameplay/ai.h"

#include <algorithm>
#include <numbers>

namespace rt::native::gameplay {
namespace {

constexpr float kPi = std::numbers::pi_v<float>;
constexpr float kTwoPi = 2.f * kPi;

// Same shortest-arc wrapping used by the chase camera.
float wrapAngle(float a) {
    a = std::fmod(a + kPi, kTwoPi);
    if (a < 0.f) a += kTwoPi;
    return a - kPi;
}

// Camera-style bounded approach: move `value` toward `target` by at most
// `maxStep` per tick.
float slewToward(float value, float target, float maxStep) {
    const float d = target - value;
    if (std::fabs(d) <= maxStep) return target;
    return value + (d > 0.f ? maxStep : -maxStep);
}

}  // namespace

AiOutput tickAi(AiState& ai, const CarState& car, const WaypointPath& path,
                const AiParams& params) {
    AiOutput out{};

    // No path or already past the end: coast to a stop.
    if (ai.waypointIndex >= static_cast<int>(path.size())) {
        if (car.speed > params.speedMargin) out.brake = true;
        return out;
    }

    const Vec2 target = path[ai.waypointIndex];
    const Vec2 toTarget = target - ai.position;
    const float dist = length(toTarget);

    // CheckCarArrivedAtZone equivalent: advance when inside the arrival radius.
    if (dist < params.arrivalRadius) {
        ++ai.waypointIndex;
        if (ai.waypointIndex >= static_cast<int>(path.size())) {
            // Final waypoint reached: brake to stop.
            out.brake = true;
            return out;
        }
    }

    // ComputeWaypointTargetAngle equivalent: heading to the current waypoint.
    const Vec2 currentTarget = path[ai.waypointIndex];
    const Vec2 toCurrent = currentTarget - ai.position;
    const float targetHeading = std::atan2(toCurrent.x, toCurrent.z);
    const float headingError = wrapAngle(targetHeading - ai.heading);
    const float distCurrent = length(toCurrent);

    // Steering: slew the internal command toward the sign of the heading error.
    // This mirrors the original's discrete steer bits but with the camera's
    // per-tick bound, preventing the command from snapping between extremes.
    float desiredSteer = 0.f;
    if (headingError > 0.005f) desiredSteer = 1.f;
    else if (headingError < -0.005f) desiredSteer = -1.f;

    ai.steerAccum = slewToward(ai.steerAccum, desiredSteer, params.steerSlewPerTick);
    if (ai.steerAccum > 0.5f) out.steer = 1;
    else if (ai.steerAccum < -0.5f) out.steer = -1;

    // CalculateAiSteeringAndThrottle equivalent: speed cap lowers for sharp
    // heading deltas. The cap interpolates from straight to corner speed as
    // the absolute heading error grows to cornerErrorRad.
    const float errorRatio =
        std::clamp(std::fabs(headingError) / params.cornerErrorRad, 0.f, 1.f);
    const float speedCap =
        params.maxSpeedStraight +
        (params.maxSpeedCorner - params.maxSpeedStraight) * errorRatio;

    // Throttle/brake hysteresis around the speed cap, matching the original's
    // three-level output (coast / gas / brake).
    bool wantThrottle = car.speed < speedCap - params.speedMargin;
    bool wantBrake = car.speed > speedCap + params.speedMargin;

    // Additional zone braking: if we are approaching the waypoint too fast,
    // brake so we can make the next segment cleanly.
    if (distCurrent < params.brakeLookahead &&
        car.speed > params.maxSpeedCorner + params.speedMargin) {
        wantBrake = true;
    }

    // Navigator fix: gas and brake must be mutually exclusive. When both are
    // requested (e.g. below the speed cap but inside the brake zone), slowing
    // wins and we pick brake.
    if (wantThrottle && wantBrake) wantThrottle = false;
    out.throttle = wantThrottle;
    out.brake = wantBrake;

    return out;
}

}  // namespace rt::native::gameplay
