// rt-asset-drive: self-driving demo. Authored AI -> car physics -> heading/position
// integration -> chase camera -> offscreen Vulkan render -> PNGs, around the default
// authored road circuit. usage: rt-asset-drive <CAR file> <frames> <outdir>

#include "assets/model_converter.h"
#include "assets/model_loader.h"
#include "preview/preview.h"

#include "gameplay/world.h"
#include "gameplay/ai.h"
#include "gameplay/camera.h"
#include "gameplay/car_physics.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

using namespace rt::native;
namespace gp = rt::native::gameplay;

static constexpr float kS = 0.1f;  // world units -> render units

static void addQuad(assets::NativeModel& m, const float xs[4], const float zs[4], float y, float nx, float ny) {
    const auto base = static_cast<std::uint32_t>(m.vertices.size());
    for (int k = 0; k < 4; ++k) {
        assets::Vertex v;
        v.px = xs[k]; v.py = y; v.pz = zs[k]; v.nx = nx; v.ny = ny;
        m.vertices.push_back(v);
    }
    for (std::uint32_t idx : {0u, 2u, 1u, 0u, 3u, 2u}) m.indices.push_back(base + idx);
}

// Grass lattice around the car; tiles inside the road corridor are left out so the
// road reads as a gap (the preview shader only tints y==0 geometry as ground).
static void addGround(assets::NativeModel& m, const gp::World& w, float cx, float cz) {
    constexpr float cell = 2.f, half = 0.9f;
    constexpr int range = 32;
    const float bx = std::floor(cx / cell) * cell, bz = std::floor(cz / cell) * cell;
    for (int i = -range; i <= range; ++i)
        for (int j = -range; j <= range; ++j) {
            const float x = bx + i * cell, z = bz + j * cell;
            if (w.isOnRoad(x / kS, z / kS)) continue;
            const float xs[4] = {x - half, x + half, x + half, x - half};
            const float zs[4] = {z - half, z - half, z + half, z + half};
            addQuad(m, xs, zs, 0.f, 0.f, 1.f);
        }
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "usage: rt-asset-drive <CAR file> <frames> <outdir>\n";
        return 2;
    }
    const std::string path = argv[1];
    const int frames = std::atoi(argv[2]);
    const std::filesystem::path outDir = argv[3];
    if (frames <= 0) { std::cerr << "bad frame count\n"; return 2; }
    std::filesystem::create_directories(outDir);

    std::ifstream f(path, std::ios::binary);
    if (!f) { std::cerr << "cannot open " << path << "\n"; return 1; }
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    std::array<std::uint8_t, 32> digest{};
    std::uint64_t h = 0xcbf29ce484222325ull;
    for (auto b : bytes) { h ^= b; h *= 0x100000001b3ull; }
    for (std::size_t i = 0; i < digest.size(); ++i) digest[i] = static_cast<std::uint8_t>((h >> ((i % 8) * 8)) | 1);

    auto res = assets::convertCarModels(bytes, path, digest);
    if (res.err != assets::CarModelError::Ok || res.value.meshes.empty()) { std::cerr << "convert failed\n"; return 1; }
    const auto car = assets::loadModel(res.value.meshes[0], res.value.images);
    const auto st = preview::computeStats(car);
    const float cxm = 0.5f * (st.bmin[0] + st.bmax[0]), czm = 0.5f * (st.bmin[2] + st.bmax[2]);
    const float scale = 4.4f / std::max(st.bmax[2] - st.bmin[2], 1e-3f);
    std::cout << "mesh 0 triangles=" << st.triangles << " scale=" << scale << "\n";

    constexpr std::uint32_t W = 800, H = 600;
    constexpr int kSubTicks = 8;
    constexpr float kUnitsPerSpeedTick = 4.f / 20000.f;  // world units per tick at kSpeedMax
    float kTurnRate = 0.2f;                   // rad/tick at full lock & full speed

    const float fov = 1.0472f, aspect = static_cast<float>(W) / H, zn = 0.1f, zf = 500.f;
    const float t = 1.f / std::tan(fov * 0.5f);
    float proj[16] = {};
    proj[0] = t / aspect; proj[5] = -t; proj[10] = zf / (zn - zf); proj[11] = -1.f;
    proj[14] = zn * zf / (zn - zf);

    gp::World world;
    const int n = static_cast<int>(world.roadWaypoints.size());
    gp::WaypointPath path2;
    for (auto p : world.roadWaypoints) path2.push_back({p.x, p.z});

    // Public AI params: larger arrival radius than the default (turn radius ~ 40).
    gp::AiParams ap;
    ap.arrivalRadius = 30.f;
    ap.brakeLookahead = 0.f;
    ap.steerSlewPerTick = 0.5f;
    ap.maxSpeedStraight = 20000.f;
    auto envF = [](const char* k, float& v) { if (const char* e = std::getenv(k)) v = static_cast<float>(std::atof(e)); };
    envF("RT_DRIVE_TURN", kTurnRate);
    envF("RT_DRIVE_ARRIVE", ap.arrivalRadius);
    envF("RT_DRIVE_VMAX", ap.maxSpeedStraight);
    envF("RT_DRIVE_SLEW", ap.steerSlewPerTick);
    const bool noRender = std::getenv("RT_DRIVE_NORENDER") != nullptr;

    gp::CarState state;
    gp::AiState ai;
    ai.position = path2[0];
    ai.waypointIndex = 1 % n;
    {
        const auto d = path2[ai.waypointIndex] - path2[0];
        ai.heading = std::atan2(d.x, d.z);
    }
    gp::CameraState cam;
    gp::CarPose pose;
    pose.position = {ai.position.x * kS, 0.f, ai.position.z * kS};
    pose.heading = ai.heading;

    // Distance to the closed centreline polyline.
    auto roadDist = [&](gp::Vec2 p) {
        float best = 1e30f;
        for (int i = 0; i < n; ++i) {
            const auto a = path2[i], b = path2[(i + 1) % n];
            const auto ab = b - a, ap_ = p - a;
            const float tt = std::clamp(gp::dot(ap_, ab) / gp::dot(ab, ab), 0.f, 1.f);
            const auto c = a + ab * tt;
            best = std::min(best, gp::length(p - c));
        }
        return best;
    };

    std::string initErr;
    preview::Renderer* renderer = noRender ? nullptr : preview::previewInit(W, H, initErr, nullptr);
    if (!renderer && !noRender) { std::cerr << "render init failed: " << initErr << "\n"; return 1; }

    int laps = 0, passed = 0, offRoadFrames = 0;
    float maxDev = 0.f;
    int frameCount = 0;
    for (int fr = 0; fr < frames; ++fr) {
        for (int k = 0; k < kSubTicks; ++k) {
            const int before = ai.waypointIndex;
            if (ai.waypointIndex >= n) { ai.waypointIndex = 0; ++laps; }
            const gp::AiOutput out = gp::tickAi(ai, state, path2, ap);
            if (ai.waypointIndex != before) ++passed;
            gp::CarInput in;
            in.gas = out.throttle;
            in.brake = out.brake;
            in.steer = out.steer;
            gp::tickCar(state, in, world.surfaceMaterialAt(ai.position.x, ai.position.z));
            const float sp = static_cast<float>(state.speed);
            const float steer = static_cast<float>(state.steeringAngle) / gp::kSteerLock;
            ai.heading = gp::wrapAngle(ai.heading + steer * (sp / 20000.f) * kTurnRate);
            ai.position.x += std::sin(ai.heading) * sp * kUnitsPerSpeedTick;
            ai.position.z += std::cos(ai.heading) * sp * kUnitsPerSpeedTick;
            maxDev = std::max(maxDev, roadDist(ai.position));
        }
        if (!world.isOnRoad(ai.position.x, ai.position.z)) ++offRoadFrames;
        pose.position = {ai.position.x * kS, 0.f, ai.position.z * kS};
        pose.heading = ai.heading;
        if (noRender) continue;
        gp::updateCamera(cam, pose, static_cast<float>(state.speed) * 8.f);
        const gp::Mat4 v = gp::lookAtMatrix(cam.eye, cam.target);
        float view[16];
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 4; ++c) view[c * 4 + r] = v.m[r * 4 + c];

        assets::NativeModel scene;
        const float sh = std::sin(pose.heading), ch = std::cos(pose.heading);
        for (auto vx : car.vertices) {
            const float lx = (vx.px - cxm) * scale, ly = (vx.py - st.bmin[1]) * scale, lz = (vx.pz - czm) * scale;
            vx.px = pose.position.x + lx * ch + lz * sh;
            vx.py = ly;
            vx.pz = pose.position.z - lx * sh + lz * ch;
            scene.vertices.push_back(vx);
        }
        scene.indices = car.indices;
        addGround(scene, world, pose.position.x, pose.position.z);

        std::vector<std::uint8_t> rgba;
        std::string err;
        if (!preview::previewRender(renderer, scene, view, proj, rgba, err)) {
            std::cerr << "render failed: " << err << "\n";
            preview::previewDestroy(renderer);
            return 1;
        }
        char name[32];
        std::snprintf(name, sizeof name, "frame_%04d.png", fr);
        const auto png = preview::encodePng(W, H, rgba);
        std::ofstream o(outDir / name, std::ios::binary);
        o.write(reinterpret_cast<const char*>(png.data()), static_cast<std::streamsize>(png.size()));
        if (!o) { std::cerr << "write failed\n"; preview::previewDestroy(renderer); return 1; }
        ++frameCount;
        if (fr % 20 == 0)
            std::cout << name << " speed=" << state.speed << " pos=(" << ai.position.x << "," << ai.position.z
                      << ") heading=" << ai.heading << " drive_waypoint_index=" << ai.waypointIndex
                      << " laps=" << laps << " dev=" << roadDist(ai.position) << "\n";
    }
    if (renderer) preview::previewDestroy(renderer);
    std::cout << "frames=" << frameCount << " drive_waypoint_index=" << ai.waypointIndex << " laps=" << laps
              << " waypoints_passed=" << passed << " max_road_deviation=" << maxDev
              << " road_half_width=" << world.roadHalfWidth << " offroad_frames=" << offRoadFrames << "\n";
    return maxDev <= world.roadHalfWidth ? 0 : 1;
}
