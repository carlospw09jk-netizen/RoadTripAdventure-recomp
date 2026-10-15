// Authored AI controller contract tests (src/native/gameplay/ai.*).
// Concrete numeric assertions; no game data, no framework.
#include "../../src/native/gameplay/ai.h"
#include "../../src/native/gameplay/car_physics.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace rt::native::gameplay;

static int g_fail = 0;
#define CHECK(c)                                                            \
    do {                                                                    \
        if (!(c)) {                                                         \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); \
            ++g_fail;                                                       \
        }                                                                   \
    } while (0)

static bool nearEq(float a, float b, float eps) { return std::fabs(a - b) <= eps; }

// ---------------------------------------------------------------- kinematics
// Simple integration model used only by these tests. `kSpeedFactor` converts
// car-physics speed units into world units per tick; `kTurnRate` is the angular
// change produced by a full left/right steer command.
static constexpr float kSpeedFactor = 0.0001f;
static constexpr float kTurnRate = 0.04f;

struct Sim {
    AiState ai;
    CarState car;
};

static AiOutput step(Sim& s, const WaypointPath& path, const AiParams& params) {
    AiOutput out = tickAi(s.ai, s.car, path, params);

    CarInput in;
    in.steer = out.steer;
    in.gas = out.throttle;
    in.brake = out.brake;
    tickCar(s.car, in);

    // Turn command -> heading change.
    s.ai.heading += out.steer * kTurnRate;

    // Forward motion along the current heading.
    s.ai.position.x += std::sin(s.ai.heading) * s.car.speed * kSpeedFactor;
    s.ai.position.z += std::cos(s.ai.heading) * s.car.speed * kSpeedFactor;

    return out;
}

// ---------------------------------------------------------------- tests

// The AI follows a straight northward line of waypoints: it converges its
// heading to the target, advances through the waypoints in order, and keeps
// the car within a narrow corridor.
static void testStraightLine() {
    WaypointPath path = {{0.f, 100.f}, {0.f, 200.f}, {0.f, 300.f}, {0.f, 400.f}};

    Sim s;
    s.ai.position = {0.f, 0.f};
    s.ai.heading = 0.02f;  // slightly off target
    s.car.speed = 8000;

    AiParams params;
    params.maxSpeedStraight = 12000.f;
    params.maxSpeedCorner = 3000.f;
    params.arrivalRadius = 8.f;
    params.brakeLookahead = 20.f;

    int reached = 0;
    float maxCorridor = 0.f;
    for (int i = 0; i < 350; ++i) {
        step(s, path, params);
        maxCorridor = std::max(maxCorridor, std::fabs(s.ai.position.x));
        if (s.ai.waypointIndex > reached) reached = s.ai.waypointIndex;
    }

    CHECK(reached >= 2);
    CHECK(maxCorridor < 3.f);
    CHECK(nearEq(std::sin(s.ai.heading), 0.f, 0.05f));
}

// The AI slows for a sharp 90-degree left turn: after entering the turn
// segment, speed drops well below the peak speed seen on the straight.
static void testSharpTurnSlows() {
    // North then a long west segment so the AI has room to decelerate.
    WaypointPath path = {{0.f, 50.f}, {0.f, 100.f}, {-300.f, 100.f}};

    Sim s;
    s.ai.position = {0.f, 0.f};
    s.ai.heading = 0.f;
    s.car.speed = 15000;

    AiParams params;
    params.maxSpeedStraight = 20000.f;
    params.maxSpeedCorner = 2000.f;
    params.arrivalRadius = 8.f;
    params.brakeLookahead = 20.f;

    float maxStraightSpeed = 0.f;
    float minTurnSpeedAfterStart = 1e9f;
    bool enteredTurn = false;

    for (int i = 0; i < 1600; ++i) {
        step(s, path, params);

        if (s.ai.waypointIndex < 2) {
            maxStraightSpeed = std::max(maxStraightSpeed, static_cast<float>(s.car.speed));
        } else {
            enteredTurn = true;
            // Ignore the first part of the turn where the car is still carrying
            // straight-line speed; measure the minimum once it is committed.
            if (s.ai.position.x < -20.f) {
                minTurnSpeedAfterStart =
                    std::min(minTurnSpeedAfterStart, static_cast<float>(s.car.speed));
            }
        }
    }

    CHECK(enteredTurn);
    CHECK(maxStraightSpeed > 15000.f);
    CHECK(minTurnSpeedAfterStart < maxStraightSpeed - 10000.f);
}

// The AI brakes when approaching a waypoint zone and advances the waypoint
// index once the car is inside the arrival radius.
static void testArrivalRadiusAndBrake() {
    WaypointPath path = {{0.f, 30.f}};

    Sim s;
    s.ai.position = {0.f, 0.f};
    s.ai.heading = 0.f;
    s.car.speed = 6000;

    AiParams params;
    params.maxSpeedStraight = 10000.f;
    params.maxSpeedCorner = 3000.f;
    params.arrivalRadius = 10.f;
    params.brakeLookahead = 20.f;

    bool braked = false;
    int reachedTick = -1;
    for (int i = 0; i < 400; ++i) {
        AiOutput out = step(s, path, params);
        if (out.brake) braked = true;

        const float dx = path[0].x - s.ai.position.x;
        const float dz = path[0].z - s.ai.position.z;
        const float dist = std::sqrt(dx * dx + dz * dz);
        if (dist < params.arrivalRadius && reachedTick < 0) reachedTick = i;
    }

    CHECK(braked);
    CHECK(reachedTick >= 0);
    CHECK(s.ai.waypointIndex == 1);
}

// The internal steering command is bounded per tick by the slew cap, even
// when the car is instantly reversed relative to the waypoint (180 deg error).
// This catches a regression in the camera-style slew; the discrete output is
// just a thresholded view of the slewed accumulator.
static void testSteerBoundedPerTick() {
    WaypointPath path = {{0.f, 50.f}};

    Sim s;
    s.ai.position = {0.f, 0.f};
    s.ai.heading = 3.14159f;  // facing south, waypoint is north: ~180 deg error
    s.car.speed = 0;

    AiParams params;
    params.steerSlewPerTick = 0.1f;

    float prevAccum = s.ai.steerAccum;
    bool bounded = true;
    bool becameFullSteer = false;
    for (int i = 0; i < 60; ++i) {
        AiOutput out = tickAi(s.ai, s.car, path, params);
        const float delta = std::fabs(s.ai.steerAccum - prevAccum);
        if (delta > params.steerSlewPerTick + 1e-6f) bounded = false;
        prevAccum = s.ai.steerAccum;
        if (out.steer != 0) becameFullSteer = true;
    }

    CHECK(bounded);
    CHECK(becameFullSteer);
    CHECK(std::fabs(s.ai.steerAccum) > 0.99f);
}

int main() {
    testStraightLine();
    testSharpTurnSlows();
    testArrivalRadiusAndBrake();
    testSteerBoundedPerTick();

    if (g_fail) {
        std::fprintf(stderr, "%d failure(s)\n", g_fail);
        return 1;
    }
    std::puts("ai: all checks passed");
    return 0;
}
