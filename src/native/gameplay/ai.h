// Authored AI traffic brain: waypoint-following controller. Pure C++, no guest
// RAM offsets. Mirrors the documented original
// (docs/game-mechanics/car-ai-and-camera.md §1): ComputeWaypointTargetAngle,
// CalculateAiSteeringAndThrottle, and UpdateVehicleWaypointProgress are folded
// into one host-side tick that emits a CarInput-compatible control word.
#pragma once

#include "car_state.h"
#include "gameplay/vec.h"

#include <cmath>
#include <cstdint>
#include <vector>

namespace rt::native::gameplay {

// ---------------------------------------------------------------- types

using WaypointPath = std::vector<Vec2>;

// Mutable AI state for one car. Maps to the original per-car fields:
//   waypointIndex ~= car +0x24A (current nav-graph node)
//   position       ~= car +0x90/+0x98 (world X/Z)
//   heading        ~= car +0x1D4 (heading accumulator)
// The slew accumulator has no direct guest equivalent; it keeps the authored
// steering command smooth between ticks.
struct AiState {
    Vec2 position;          // car world position in the X/Z plane
    float heading = 0.f;    // radians; 0 = +Z, increases counter-clockwise
    int waypointIndex = 0;  // current target waypoint in the path
    float steerAccum = 0.f; // internal slew state for the steering command
};

// Control word produced for one AI tick. Maps to CarInput:
//   steer    -> CarInput.steer (-1, 0, +1)
//   throttle -> CarInput.gas
//   brake    -> CarInput.brake
struct AiOutput {
    int steer = 0;       // -1, 0, +1
    bool throttle = false;
    bool brake = false;
};

// Tunable constants. The defaults are authored values chosen so the AI follows
// a path cleanly; they do not correspond to any one original data table.
struct AiParams {
    // Speed cap on a straight segment (car-physics speed units).
    float maxSpeedStraight = 120000.f;

    // Speed cap used when the heading error is at cornerErrorRad or more.
    float maxSpeedCorner = 30000.f;

    // Distance within which the current waypoint is considered reached and the
    // controller advances to the next one (original CheckCarArrivedAtZone).
    float arrivalRadius = 10.f;

    // Distance at which the controller begins braking for the waypoint zone.
    float brakeLookahead = 25.f;

    // Maximum change in the internal steering command per tick. Reuses the
    // same camera-style slew used by updateCamera (camera.cpp slewToward).
    float steerSlewPerTick = 0.12f;

    // Heading-error angle (radians) that selects the corner speed cap.
    // error <= 0        -> straight cap
    // error >= this     -> corner cap
    // 0 < error < this  -> linear blend
    float cornerErrorRad = 1.57079632679f;  // pi/2

    // Hysteresis band for throttle/brake around the speed cap.
    float speedMargin = 5000.f;
};

// One AI tick. Advances waypointIndex when the car enters the arrival radius
// of the current target, then emits steering/throttle/brake commands that:
//   - steer toward the current waypoint with a per-tick bounded slew,
//   - cap speed (and brake) for sharp heading deltas,
//   - brake when approaching a waypoint zone too fast.
AiOutput tickAi(AiState& ai, const CarState& car, const WaypointPath& path,
                const AiParams& params = {});

}  // namespace rt::native::gameplay
