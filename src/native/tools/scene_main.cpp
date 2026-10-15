// rt-asset-scene: GameLoop-driven field, road, car, and day/night offscreen frames.
// usage: rt-asset-scene <CAR file> <frames> <outdir>
#include "assets/model_converter.h"
#include "assets/model_loader.h"
#include "gameplay/game_loop.h"
#include "preview/preview.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

using namespace rt::native;

int main(int argc, char* argv[]) {
    if (argc != 4) { std::cerr << "usage: rt-asset-scene <CAR file> <frames> <outdir>\n"; return 2; }
    const int frames = std::atoi(argv[2]);
    if (frames <= 0) { std::cerr << "bad frame count\n"; return 2; }
    std::ifstream f(argv[1], std::ios::binary);
    if (!f) { std::cerr << "cannot open " << argv[1] << "\n"; return 1; }
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(f)), {});
    std::array<std::uint8_t, 32> digest{};
    std::uint64_t h = 0xcbf29ce484222325ull;
    for (const auto b : bytes) { h ^= b; h *= 0x100000001b3ull; }
    for (std::size_t i = 0; i < digest.size(); ++i) digest[i] = static_cast<std::uint8_t>((h >> ((i % 8) * 8)) | 1);
    const auto converted = assets::convertCarModels(bytes, argv[1], digest);
    if (converted.err != assets::CarModelError::Ok || converted.value.meshes.empty()) {
        std::cerr << "convert failed\n"; return 1;
    }
    const auto car = assets::loadModel(converted.value.meshes[0], converted.value.images);
    constexpr std::uint32_t W = 800, H = 600;
    std::string error, device;
    preview::Renderer* renderer = preview::previewInit(W, H, error, &device);
    if (!renderer) { std::cerr << "render init failed: " << error << "\n"; return 1; }
    std::filesystem::create_directories(argv[3]);
    gameplay::GameLoop loop;
    std::cout << "device=" << device << " physics_ticks_per_frame=" << loop.params().physicsTicksPerFrame << "\n";
    for (int frame = 0; frame < frames; ++frame) {
        const auto snapshot = loop.tick(1.f / 60.f, {});
        std::vector<std::uint8_t> rgba;
        if (!preview::previewRenderScene(renderer, car, snapshot, loop.world(), rgba, error)) {
            std::cerr << "render failed: " << error << "\n"; preview::previewDestroy(renderer); return 1;
        }
        char name[32]; std::snprintf(name, sizeof name, "frame_%04d.png", frame);
        const auto png = preview::encodePng(W, H, rgba);
        std::ofstream output(std::filesystem::path(argv[3]) / name, std::ios::binary);
        output.write(reinterpret_cast<const char*>(png.data()), static_cast<std::streamsize>(png.size()));
        if (!output) { std::cerr << "write failed\n"; preview::previewDestroy(renderer); return 1; }
        const auto& l = snapshot.lighting;
        std::cout << name << " clock=" << snapshot.clock << " weights=(day=" << l.dayWeight
                  << ",dusk=" << l.duskWeight << ",night=" << l.nightWeight << ") fog_far=" << l.fogFar << "\n";
    }
    preview::previewDestroy(renderer);
    return 0;
}
