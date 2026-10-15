// Authored chase camera: plain C++, no guest RAM offsets, no PS2 types.
//
// Mirrors the documented original (docs/game-mechanics/car-ai-and-camera.md §2):
// a per-camera state with zoom/pitch/yaw channels and a mode field (the original
// keeps these as shorts at cam-state +0x14/+0x16/+0x1A/+0x1C), a spring block
// for the smoothed chase offset (original DAT_0172FF30: smoothed lateral offset
// and height plus their rate limits), pitch slewed at most a fixed step per
// frame (original ±0x80 in 0x10000 angle units), and a look-at view matrix
// built per tick (original buildCamera 0x21F698). All values here are authored
// floats/radians; no game data.
#pragma once

#include "gameplay/vec.h"

#include <cmath>
#include <cstdint>

namespace rt::native::gameplay {

// 4x4 matrix, row-major (m[row * 4 + col]).
struct Mat4 {
    float m[16] = {1.f, 0.f, 0.f, 0.f,
                   0.f, 1.f, 0.f, 0.f,
                   0.f, 0.f, 1.f, 0.f,
                   0.f, 0.f, 0.f, 1.f};
};

inline constexpr float kPi = 3.14159265358979323846f;
inline constexpr float kTwoPi = 2.f * kPi;

// Wrap an angle to (-pi, pi].
inline float wrapAngle(float a) {
    a = std::fmod(a + kPi, kTwoPi);
    if (a < 0.f) a += kTwoPi;
    return a - kPi;
}

// ---------------------------------------------------------------- inputs

// Car pose for the camera. Heading convention: forward = (sin h, 0, cos h),
// i.e. heading 0 looks down +Z. Position is in world units.
struct CarPose {
    Vec3 position;
    float heading = 0.f;  // radians
};

inline Vec3 carForward(const CarPose& car) {
    return {std::sin(car.heading), 0.f, std::cos(car.heading)};
}

// ---------------------------------------------------------------- tuning

struct CameraParams {
    // Speed scale: full-scale forward speed in car-physics units (the original
    // AI code treats 0x28000 as the speed reference).
    float speedRef = 163840.f;

    // Zoom (camera-to-car distance) eases between these as speed rises.
    float distanceNear = 9.f;    // at zero speed
    float distanceFar = 16.f;    // at speedRef and above
    float zoomEase = 0.08f;      // exponential approach per tick (0..1)
    float zoomSlewPerTick = 0.5f;  // hard rate limit, units/tick

    // Pitch eases from looking slightly down at rest to more overhead at speed.
    float pitchLow = 0.0873f;    // ~5 deg above the horizon plane, zero speed
    float pitchHigh = 0.4363f;   // ~25 deg, full speed
    // Original slews pitch at most ±0x80/frame in 0x10000 angle units
    // (0x80/0x10000 * 2pi ~= 0.031 rad); keep the same feel.
    float pitchSlewPerTick = 0.0307f;

    // Yaw follows the car heading through the shortest arc.
    float yawEase = 0.15f;

    // Look-at point rides this far above the car origin.
    float lookAtHeight = 1.5f;
    // Base camera height offset (the original spring block's third float fixes
    // the height when the spring mode is off).
    float baseHeight = 2.0f;

    // Spring block (original DAT_0172FF30 + constants DAT_00332C30..C44):
    // rate += (target - value) * k; rate *= damping; clamp; value += rate.
    float springK = 0.09f;
    float springDamping = 0.82f;
    float springRateLimit = 0.35f;  // units/tick, per channel
};

// ---------------------------------------------------------------- state

// Camera modes, mirroring the original cam-state mode int (+0x1C):
// mode 0 = fixed chase offset (spring bypassed), nonzero = spring-smoothed.
enum CameraMode : std::int32_t {
    kCameraModeFixed = 0,
    kCameraModeSpring = 1,
};

struct CameraState {
    // Eased channels (original: zoom float +0x10, pitch short +0x16, yaw-follow
    // short +0x1A, mode +0x1C).
    float distance = 9.f;  // current camera-to-target distance (zoom)
    float pitch = 0.0873f; // radians, camera elevation over the car
    float yaw = 0.f;       // radians, heading the camera orbits on
    std::int32_t mode = kCameraModeSpring;

    // Spring block: smoothed lateral offset / height + their rates.
    float springLateral = 0.f;
    float springLateralRate = 0.f;
    float springHeight = 0.f;
    float springHeightRate = 0.f;

    // Per-tick outputs (the original writes these into the camera block).
    Vec3 eye;       // camera position
    Vec3 target;    // look-at point
    Vec3 forward;   // normalized view direction (target - eye)
    Mat4 view;      // right-handed look-at view matrix (camera looks down -Z)

    bool initialized = false;  // first tick snaps instead of easing
};

// ---------------------------------------------------------------- update

// One camera tick: eases zoom/pitch/yaw toward speed-scaled targets, runs the
// spring block, and rebuilds the look-at basis and view matrix. The camera is
// kept behind (against carForward) and above the car.
void updateCamera(CameraState& cam, const CarPose& car, float speed,
                  const CameraParams& params = {});

// Right-handed look-at view matrix (OpenGL convention: camera looks down -Z).
Mat4 lookAtMatrix(Vec3 eye, Vec3 target, Vec3 up = {0.f, 1.f, 0.f});

}  // namespace rt::native::gameplay
