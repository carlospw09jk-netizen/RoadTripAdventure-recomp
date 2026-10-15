// rt-asset-animate: scripted-input driving slice. Authored car physics ->
// yaw integrator -> authored chase camera -> offscreen Vulkan render -> PNGs.
// usage: rt-asset-animate <CAR file> <frames> <outdir>

#include "assets/model_converter.h"
#include "assets/model_loader.h"
#include "gameplay/camera.h"
#include "gameplay/car_physics.h"
#include "preview/preview.h"

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
using gameplay::Vec3;

static void addGround(assets::NativeModel& m, float cx, float cz) {
    constexpr float cell = 4.f, half = 1.8f;
    constexpr int range = 16;
    const float bx = std::floor(cx / cell) * cell, bz = std::floor(cz / cell) * cell;
    for (int i = -range; i <= range; ++i)
        for (int j = -range; j <= range; ++j) {
            const float x = bx + i * cell, z = bz + j * cell;
            const auto base = static_cast<std::uint32_t>(m.vertices.size());
            const float xs[4] = {x - half, x + half, x + half, x - half};
            const float zs[4] = {z - half, z - half, z + half, z + half};
            for (int k = 0; k < 4; ++k) {
                assets::Vertex v;
                v.px = xs[k]; v.py = 0.f; v.pz = zs[k]; v.ny = 1.f;
                m.vertices.push_back(v);
            }
            for (std::uint32_t idx : {0u, 2u, 1u, 0u, 3u, 2u}) m.indices.push_back(base + idx);
        }
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "usage: rt-asset-animate <CAR file> <frames> <outdir>\n";
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
    if (res.err != assets::CarModelError::Ok) { std::cerr << "convert failed\n"; return 1; }
    std::size_t pick = 0, best = 0;
    for (std::size_t i = 0; i < res.value.meshes.size(); ++i) {
        const auto s = preview::computeStats(assets::loadModel(res.value.meshes[i], res.value.images));
        if (s.triangles > best) { best = s.triangles; pick = i; }
    }
    const auto car = assets::loadModel(res.value.meshes[pick], res.value.images);
    const auto st = preview::computeStats(car);
    // Model space: y up, z along the body. Centre x/z, sit the lowest point on y=0, scale to ~4.4 long.
    const float cxm = 0.5f * (st.bmin[0] + st.bmax[0]), czm = 0.5f * (st.bmin[2] + st.bmax[2]);
    const float len = std::max(st.bmax[2] - st.bmin[2], 1e-3f);
    const float scale = 4.4f / len;
    std::cout << "mesh " << pick << " triangles=" << st.triangles << " scale=" << scale << "\n";

    constexpr std::uint32_t W = 800, H = 600;
    constexpr int kSubTicks = 4;
    constexpr float kUnitsPerSpeedTick = 0.35f / 20000.f;  // world units per tick at kSpeedMax
    constexpr float kTurnRate = 0.045f;                    // rad/tick at full lock & full speed

    // Column-major perspective (Vulkan: y down, depth 0..1).
    const float fov = 1.0472f, aspect = static_cast<float>(W) / H, zn = 0.1f, zf = 500.f;
    const float t = 1.f / std::tan(fov * 0.5f);
    float proj[16] = {};
    proj[0] = t / aspect; proj[5] = -t; proj[10] = zf / (zn - zf); proj[11] = -1.f;
    proj[14] = zn * zf / (zn - zf);

    gameplay::CarState state;
    gameplay::CameraState cam;
    gameplay::CarPose pose;
    float prevSx = 0, prevSy = 0;
    bool havePrev = false;
    float minSx = 1e9f, maxSx = -1e9f, minSy = 1e9f, maxSy = -1e9f, camMove = 0;
    Vec3 prevEye;
    int inFrame = 0;

    std::string initErr;
    preview::Renderer* renderer = preview::previewInit(W, H, initErr, nullptr);
    if (!renderer) { std::cerr << "render init failed: " << initErr << "\n"; return 1; }
    std::cout << "renderer context created once\n";

    for (int fr = 0; fr < frames; ++fr) {
        gameplay::CarInput in;
        in.gas = true;
        in.steer = fr < frames / 5 ? 0 : 1;  // straight, then constant right steering sweep
        for (int k = 0; k < kSubTicks; ++k) {
            gameplay::tickCar(state, in);
            const float sp = static_cast<float>(state.speed);
            const float steer = static_cast<float>(state.steeringAngle) / gameplay::kSteerLock;
            pose.heading = gameplay::wrapAngle(pose.heading + steer * (sp / 20000.f) * kTurnRate);
            pose.position = pose.position + gameplay::carForward(pose) * (sp * kUnitsPerSpeedTick);
        }
        gameplay::updateCamera(cam, pose, static_cast<float>(state.speed) * 8.f);
        const gameplay::Mat4 v = gameplay::lookAtMatrix(cam.eye, cam.target);
        float view[16];
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 4; ++c) view[c * 4 + r] = v.m[r * 4 + c];

        // World-space scene: transformed car (model +z -> carForward) + ground tiles.
        assets::NativeModel scene;
        const float sh = std::sin(pose.heading), ch = std::cos(pose.heading);
        scene.vertices.reserve(car.vertices.size() + 33 * 33 * 4);
        for (auto vx : car.vertices) {
            const float lx = (vx.px - cxm) * scale, ly = (vx.py - st.bmin[1]) * scale, lz = (vx.pz - czm) * scale;
            vx.px = pose.position.x + lx * ch + lz * sh;
            vx.py = ly;
            vx.pz = pose.position.z - lx * sh + lz * ch;
            scene.vertices.push_back(vx);
        }
        scene.indices = car.indices;
        addGround(scene, pose.position.x, pose.position.z);

        std::vector<std::uint8_t> rgba;
        std::string err;
        if (!preview::previewRender(renderer, scene, view, proj, rgba, err)) {
            std::cerr << "render failed: " << err << "\n";
            preview::previewDestroy(renderer);
            return 1;
        }
        // Screen position of the car centre (0.7 up) through the same matrices.
        const float wx = pose.position.x, wy = 0.7f, wz = pose.position.z;
        float vp[4], cp[4];
        for (int r = 0; r < 4; ++r) vp[r] = view[r] * wx + view[4 + r] * wy + view[8 + r] * wz + view[12 + r];
        for (int r = 0; r < 4; ++r) cp[r] = proj[r] * vp[0] + proj[4 + r] * vp[1] + proj[8 + r] * vp[2] + proj[12 + r] * vp[3];
        const float sx = (cp[0] / cp[3] * 0.5f + 0.5f) * W, sy = (cp[1] / cp[3] * 0.5f + 0.5f) * H;
        const bool ok = cp[3] > 0 && sx >= 0 && sx < W && sy >= 0 && sy < H;
        inFrame += ok;
        minSx = std::min(minSx, sx); maxSx = std::max(maxSx, sx);
        minSy = std::min(minSy, sy); maxSy = std::max(maxSy, sy);
        if (fr) camMove += length(cam.eye - prevEye);
        prevEye = cam.eye;

        char name[32];
        std::snprintf(name, sizeof name, "frame_%04d.png", fr);
        const auto png = preview::encodePng(W, H, rgba);
        std::ofstream o(outDir / name, std::ios::binary);
        o.write(reinterpret_cast<const char*>(png.data()), static_cast<std::streamsize>(png.size()));
        if (!o) { std::cerr << "write failed\n"; preview::previewDestroy(renderer); return 1; }
        std::cout << name << " speed=" << state.speed << " steer=" << state.steeringAngle
                  << " pos=(" << pose.position.x << "," << pose.position.z << ") heading=" << pose.heading
                  << " carScreen=(" << sx << "," << sy << ")"
                  << (havePrev ? " delta=(" + std::to_string(sx - prevSx) + "," + std::to_string(sy - prevSy) + ")" : "")
                  << (ok ? "" : " OFFSCREEN") << "\n";
        prevSx = sx; prevSy = sy; havePrev = true;
    }
    preview::previewDestroy(renderer);
    std::cout << "frames=" << frames << " inFrame=" << inFrame << " carScreenX[" << minSx << "," << maxSx
              << "] Y[" << minSy << "," << maxSy << "] cameraTravel=" << camMove << "\n";
    return inFrame == frames ? 0 : 1;
}
