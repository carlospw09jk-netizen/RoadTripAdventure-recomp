// Authored chase camera implementation. See camera.h for the model and the
// mapping to the documented original (docs/game-mechanics/car-ai-and-camera.md
// §2). Pure host C++, no game data.
#include "gameplay/camera.h"

#include <algorithm>

namespace rt::native::gameplay {
namespace {

float clamp01(float v) { return std::clamp(v, 0.f, 1.f); }

// Move `value` toward `target` by at most `maxStep` (slew limit), mirroring the
// original's per-frame pitch clamp (±0x80 in 0x10000 angle units).
float slewToward(float value, float target, float maxStep) {
    const float d = target - value;
    if (std::fabs(d) <= maxStep) return target;
    return value + (d > 0.f ? maxStep : -maxStep);
}

// One spring-damper tick (original chase spring block): accelerate toward the
// target, damp, clamp the rate, integrate.
void springStep(float& value, float& rate, float target, const CameraParams& p) {
    rate += (target - value) * p.springK;
    rate *= p.springDamping;
    rate = std::clamp(rate, -p.springRateLimit, p.springRateLimit);
    value += rate;
}

}  // namespace

Mat4 lookAtMatrix(Vec3 eye, Vec3 target, Vec3 up) {
    const Vec3 f = normalized(target - eye);   // view direction
    const Vec3 r = normalized(cross(f, up));   // right
    const Vec3 u = cross(r, f);                // recomputed up

    Mat4 m{};
    // Rotation rows; camera looks down -Z so the third row is -f.
    m.m[0] = r.x;  m.m[1] = r.y;  m.m[2] = r.z;  m.m[3] = -dot(r, eye);
    m.m[4] = u.x;  m.m[5] = u.y;  m.m[6] = u.z;  m.m[7] = -dot(u, eye);
    m.m[8] = -f.x; m.m[9] = -f.y; m.m[10] = -f.z; m.m[11] = dot(f, eye);
    m.m[12] = 0.f; m.m[13] = 0.f; m.m[14] = 0.f;  m.m[15] = 1.f;
    return m;
}

void updateCamera(CameraState& cam, const CarPose& car, float speed,
                  const CameraParams& params) {
    const float speedNorm = clamp01(speed / params.speedRef);

    // Speed-scaled targets: zoom out and raise pitch as speed rises.
    const float targetDistance =
        params.distanceNear + (params.distanceFar - params.distanceNear) * speedNorm;
    const float targetPitch =
        params.pitchLow + (params.pitchHigh - params.pitchLow) * speedNorm;

    // First tick snaps every channel (the original snaps the spring state on a
    // preset switch so the camera doesn't swing).
    if (!cam.initialized) {
        cam.distance = targetDistance;
        cam.pitch = targetPitch;
        cam.yaw = car.heading;
        cam.springLateral = 0.f;
        cam.springLateralRate = 0.f;
        cam.springHeight = params.baseHeight;
        cam.springHeightRate = 0.f;
        cam.initialized = true;
    } else {
        // Zoom: exponential ease with a hard per-tick slew limit.
        const float zoomStep = (targetDistance - cam.distance) * params.zoomEase;
        cam.distance += std::clamp(zoomStep, -params.zoomSlewPerTick,
                                   params.zoomSlewPerTick);

        // Pitch: slew-limited ease (bounded per-tick delta).
        cam.pitch = slewToward(cam.pitch, targetPitch, params.pitchSlewPerTick);

        // Yaw: follow the car heading through the shortest arc.
        cam.yaw = wrapAngle(cam.yaw + wrapAngle(car.heading - cam.yaw) * params.yawEase);
    }

    // Spring block: smooth the lateral offset (always relaxed to 0 — authored
    // equivalent of the original's smoothed chase offset) and the camera height.
    const float targetHeight = params.baseHeight;
    if (cam.mode == kCameraModeSpring) {
        springStep(cam.springLateral, cam.springLateralRate, 0.f, params);
        springStep(cam.springHeight, cam.springHeightRate, targetHeight, params);
    } else {
        // Fixed mode: no smoothing, offset sits directly at the target.
        cam.springLateral = 0.f;
        cam.springLateralRate = 0.f;
        cam.springHeight = targetHeight;
        cam.springHeightRate = 0.f;
    }

    // Camera basis: orbit on the eased yaw behind the car, pitched up, riding
    // the spring-smoothed height. Yaw 0 == car heading puts the camera exactly
    // behind the car (against carForward) and above it.
    const float sy = std::sin(cam.yaw), cy = std::cos(cam.yaw);
    const float cp = std::cos(cam.pitch), sp = std::sin(cam.pitch);
    // Backward direction (opposite the car's forward at yaw cam.yaw).
    const Vec3 back{-sy, 0.f, -cy};
    const Vec3 rightDir{cy, 0.f, -sy};  // lateral axis in the orbit plane

    const Vec3 offset = back * (cam.distance * cp)
                      + rightDir * cam.springLateral
                      + Vec3{0.f, cam.distance * sp + cam.springHeight, 0.f};

    cam.eye = car.position + offset;
    cam.target = car.position + Vec3{0.f, params.lookAtHeight, 0.f};
    cam.forward = normalized(cam.target - cam.eye);
    cam.view = lookAtMatrix(cam.eye, cam.target);
}

}  // namespace rt::native::gameplay
