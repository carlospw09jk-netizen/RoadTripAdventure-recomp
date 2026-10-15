// rt-asset-preview: converts a CAR container (user's own disc file) and renders
// the largest mesh offscreen via Vulkan/MoltenVK straight from the authored
// NativeModel (no PS2/GS/VU emulation), writing a PNG.
// usage: rt-asset-preview <CAR file> [out.png] [meshIndex]

#include "assets/model_converter.h"
#include "assets/model_loader.h"
#include "preview/preview.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

int main(int argc, char* argv[]) {
    using namespace rt::native;
    if (argc < 2 || argc > 4) {
        std::cerr << "usage: rt-asset-preview <CAR file> [out.png] [meshIndex]\n";
        return 2;
    }
    const std::string path = argv[1];
    const std::string out = argc > 2 ? argv[2] : "preview.png";
    std::ifstream f(path, std::ios::binary);
    if (!f) { std::cerr << "cannot open " << path << "\n"; return 1; }
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    // Non-cryptographic nonzero digest (provenance sentinel only), as in rt-asset-convert.
    std::array<std::uint8_t, 32> digest{};
    std::uint64_t h = 0xcbf29ce484222325ull;
    for (auto b : bytes) { h ^= b; h *= 0x100000001b3ull; }
    for (std::size_t i = 0; i < digest.size(); ++i) digest[i] = static_cast<std::uint8_t>((h >> ((i % 8) * 8)) | 1);

    auto res = assets::convertCarModels(bytes, path, digest);
    if (res.err != assets::CarModelError::Ok) { std::cerr << "convert failed\n"; return 1; }
    std::size_t pick = 0, best = 0;
    for (std::size_t i = 0; i < res.value.meshes.size(); ++i) {
        const auto m = assets::loadModel(res.value.meshes[i], res.value.images);
        const auto s = preview::computeStats(m);
        std::cout << "mesh " << i << ": " << s.triangles << " triangles, " << m.vertices.size() << " vertices\n";
        if (s.triangles > best) { best = s.triangles; pick = i; }
    }
    if (argc > 3) pick = static_cast<std::size_t>(std::strtoul(argv[3], nullptr, 10));
    if (pick >= res.value.meshes.size()) { std::cerr << "bad mesh index\n"; return 2; }
    const auto model = assets::loadModel(res.value.meshes[pick], res.value.images);
    const auto s = preview::computeStats(model);
    std::cout << "rendering mesh " << pick << ": triangles=" << s.triangles << " bbox min=("
              << s.bmin[0] << ", " << s.bmin[1] << ", " << s.bmin[2] << ") max=("
              << s.bmax[0] << ", " << s.bmax[1] << ", " << s.bmax[2] << ")\n";

    constexpr std::uint32_t W = 800, H = 600;
    std::vector<std::uint8_t> rgba;
    std::string err, dev;
    if (!preview::renderModel(model, W, H, rgba, err, &dev)) {
        std::cerr << "render failed: " << err << "\n";
        return 1;
    }
    std::size_t lit = 0;
    for (std::size_t i = 0; i < rgba.size(); i += 4)
        if (rgba[i] != 20 || rgba[i + 1] != 23 || rgba[i + 2] != 31) ++lit;
    const auto png = preview::encodePng(W, H, rgba);
    std::ofstream o(out, std::ios::binary);
    o.write(reinterpret_cast<const char*>(png.data()), static_cast<std::streamsize>(png.size()));
    std::cout << "device: " << dev << "\nwrote " << out << " (" << png.size() << " bytes, " << W << "x" << H
              << ", " << lit << " non-background pixels)\n";
    return o ? 0 : 1;
}
