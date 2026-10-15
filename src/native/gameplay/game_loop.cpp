#include "gameplay/game_loop.h"

#include "gameplay/car_physics.h"

#include <algorithm>
#include <cmath>

namespace rt::native::gameplay {

GameLoop::GameLoop(const GameLoopParams& params) : params_(params) { reset(); }

void GameLoop::reset() {
    tasks_ = TaskPool();
    car_ = CarState();
    camera_ = CameraState();
    lapCount_ = 0;
    waypointsPassed_ = 0;
    frame_ = 0;
    pipelineTicks_ = 0;

    path_ = world_.roadWaypoints;
    const int n = static_cast<int>(world_.roadWaypoints.size());
    ai_ = AiState();
    if (n > 0) {
        ai_.position = world_.roadWaypoints[0];
        ai_.waypointIndex = 1 % n;
        const Vec2 d = world_.roadWaypoints[ai_.waypointIndex] - world_.roadWaypoints[0];
        ai_.heading = std::atan2(d.x, d.z);
    }
    // Frame task stands in for the walker's per-frame handlers.
    tasks_.spawn([this](Task&, TaskPool&) { ++pipelineTicks_; });
}

float GameLoop::roadDistance(Vec2 p) const {
    const auto& w = world_.roadWaypoints;
    const std::size_t n = w.size();
    float best = 1e30f;
    for (std::size_t i = 0; i < n; ++i) {
        const Vec2 a = w[i], b = w[(i + 1) % n];
        const Vec2 ab = b - a, ap = p - a;
        const float denom = std::max(dot(ab, ab), 1e-12f);
        const float t = std::clamp(dot(ap, ab) / denom, 0.f, 1.f);
        best = std::min(best, std::sqrt(lengthSq(p - (a + ab * t))));
    }
    return best;
}

FrameSnapshot GameLoop::tick(float /*dt*/, const MenuInput& /*menuInput*/) {
    AiParams aiParams;
    aiParams.arrivalRadius = params_.arrivalRadius;
    aiParams.steerSlewPerTick = params_.steerSlewPerTick;
    aiParams.maxSpeedStraight = params_.maxSpeedStraight;
    aiParams.brakeLookahead = params_.brakeLookahead;
    const int n = static_cast<int>(world_.roadWaypoints.size());

    tasks_.tick();  // (a) task walker pass

    for (int k = 0; k < params_.physicsTicksPerFrame; ++k) {
        const int before = ai_.waypointIndex;
        if (n > 0 && ai_.waypointIndex >= n) {  // tickAi stops at the last waypoint: wrap = lap
            ai_.waypointIndex = 0;
            ++lapCount_;
        }
        const AiOutput cmd = tickAi(ai_, car_, path_, aiParams);  // (b)
        if (ai_.waypointIndex != before) ++waypointsPassed_;
        CarInput in;
        in.gas = cmd.throttle;
        in.brake = cmd.brake;
        in.steer = cmd.steer;
        tickCar(car_, in, world_.surfaceMaterialAt(ai_.position.x, ai_.position.z));
        // (c) heading/position integration
        const float sp = static_cast<float>(car_.speed);
        const float steer = static_cast<float>(car_.steeringAngle) / static_cast<float>(kSteerLock);
        ai_.heading = wrapAngle(ai_.heading + steer * (sp / params_.speedRef) * params_.turnRate);
        ai_.position.x += std::sin(ai_.heading) * sp * params_.unitsPerSpeedTick;
        ai_.position.z += std::cos(ai_.heading) * sp * params_.unitsPerSpeedTick;
    }
    ++frame_;

    // (d) camera, in render space
    CarPose pose;
    pose.position = {ai_.position.x * params_.renderScale, 0.f, ai_.position.z * params_.renderScale};
    pose.heading = ai_.heading;
    updateCamera(camera_, pose, static_cast<float>(car_.speed) * params_.cameraSpeedScale);
    const Mat4 v = lookAtMatrix(camera_.eye, camera_.target);

    FrameSnapshot s;
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c) s.view[c * 4 + r] = v.m[r * 4 + c];
    const float t = 1.f / std::tan(params_.fovY * 0.5f);
    s.proj[0] = t / params_.aspect;
    s.proj[5] = -t;
    s.proj[10] = params_.farZ / (params_.nearZ - params_.farZ);
    s.proj[11] = -1.f;
    s.proj[14] = params_.nearZ * params_.farZ / (params_.nearZ - params_.farZ);

    s.carPosition = ai_.position;
    s.heading = ai_.heading;
    s.speed = static_cast<float>(car_.speed);
    s.cameraEye = camera_.eye;
    s.cameraTarget = camera_.target;
    s.visibleCells = world_.visibleCells(s.carPosition, params_.viewDistance);  // (e)
    s.waypointIndex = ai_.waypointIndex;  // (f)
    s.lapCount = lapCount_;
    s.waypointsPassed = waypointsPassed_;
    s.frame = frame_;
    return s;
}

}  // namespace rt::native::gameplay
