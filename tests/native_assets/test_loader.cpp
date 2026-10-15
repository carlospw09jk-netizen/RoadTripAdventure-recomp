// Synthetic contract tests for the shipped native asset loaders.
//
// These tests construct host records in memory and use no game or disc data. They are
// intentionally standalone and do not depend on container.cpp or deserialize().
// NOT RUN: test execution and all build/check commands are prohibited for this task.

#include "../../src/native/assets/container.h"
#include "../../src/native/assets/image_loader.h"
#include "../../src/native/assets/model_loader.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

using rt::native::assets::ImageRecord;
using rt::native::assets::MaterialRef;
using rt::native::assets::MeshRecord;
using rt::native::assets::NativeImage;
using rt::native::assets::NativeModel;
using rt::native::assets::PixelFormat;
using rt::native::assets::Vertex;
using rt::native::assets::loadImage;
using rt::native::assets::loadModel;

namespace {

int g_checks = 0;
int g_failures = 0;

void check(bool condition, const std::string& what) {
    ++g_checks;
    if (!condition) {
        ++g_failures;
        std::cerr << "FAIL: " << what << '\n';
    }
}

bool throws_invalid_argument(auto&& operation) {
    try {
        operation();
    } catch (const std::invalid_argument&) {
        return true;
    } catch (...) {
        return false;
    }
    return false;
}

// Independent test-side RGBA8 mip-size calculation. Each level uses tightly packed
// four-byte pixels; dimensions floor-halve and clamp to one after every level.
std::size_t expected_mip_size(std::uint32_t width, std::uint32_t height,
                              std::uint32_t mip_count) {
    std::size_t total = 0;
    for (std::uint32_t level = 0; level < mip_count; ++level) {
        total += static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4;
        width = width > 1 ? width / 2 : 1;
        height = height > 1 ? height / 2 : 1;
    }
    return total;
}

ImageRecord make_image(std::uint32_t width = 2, std::uint32_t height = 2,
                       std::uint32_t mip_count = 1) {
    ImageRecord image{};
    image.width = width;
    image.height = height;
    image.mipCount = mip_count;
    image.format = PixelFormat::R8G8B8A8_UNORM;
    image.pixels.resize(expected_mip_size(width, height, mip_count));
    for (std::size_t i = 0; i < image.pixels.size(); ++i) {
        image.pixels[i] = static_cast<std::uint8_t>((i * 17u + 3u) & 0xFFu);
    }
    image.sourceVariantId = "synthetic-variant";
    return image;
}

MeshRecord make_mesh() {
    MeshRecord mesh{};
    Vertex first{};
    first.px = 1.0f;
    first.r = 10;
    Vertex second{};
    second.py = 2.0f;
    second.g = 20;
    Vertex third{};
    third.pz = 3.0f;
    third.b = 30;
    mesh.vertices = {first, second, third};
    mesh.indices = {0, 1, 2};
    mesh.materials = {MaterialRef{.firstIndex = 0, .indexCount = 3, .imageIndex = 0}};
    return mesh;
}

void test_load_model_resolves_material_image() {
    const ImageRecord image = make_image();
    const MeshRecord mesh = make_mesh();
    const NativeModel model = loadModel(mesh, std::span<const ImageRecord>(&image, 1));
    check(model.vertices == mesh.vertices, "loadModel preserves the three vertices");
    check(model.indices == mesh.indices, "loadModel preserves mesh indices");
    check(model.materialImage == std::vector<std::uint32_t>{0},
          "loadModel resolves the single material to image zero");
}

void test_load_model_rejects_index_out_of_range() {
    MeshRecord mesh = make_mesh();
    mesh.indices[2] = static_cast<std::uint32_t>(mesh.vertices.size());
    check(throws_invalid_argument([&] { (void)loadModel(mesh, {}); }),
          "loadModel rejects an index equal to vertex count");
}

void test_load_model_rejects_material_range_out_of_bounds() {
    MeshRecord mesh = make_mesh();
    mesh.materials[0].firstIndex = 2;
    mesh.materials[0].indexCount = 2;
    check(throws_invalid_argument([&] { (void)loadModel(mesh, {}); }),
          "loadModel rejects a material index range beyond mesh.indices");
}

void test_load_model_rejects_material_image_out_of_range() {
    MeshRecord mesh = make_mesh();
    mesh.materials[0].imageIndex = 1;
    const ImageRecord image = make_image();
    check(throws_invalid_argument([&] {
              (void)loadModel(mesh, std::span<const ImageRecord>(&image, 1));
          }),
          "loadModel rejects a material image index equal to image count");
}

void test_load_model_preserves_unresolved_material_image() {
    MeshRecord mesh = make_mesh();
    mesh.materials[0].imageIndex = 0xFFFFFFFFu;
    const NativeModel model = loadModel(mesh, {});
    check(model.materialImage == std::vector<std::uint32_t>{0xFFFFFFFFu},
          "loadModel preserves the unresolved image sentinel");
}

void test_load_image_preserves_valid_rgba8_image() {
    const ImageRecord image = make_image();
    const NativeImage native = loadImage(image);
    check(native.width == 2 && native.height == 2 && native.mipCount == 1,
          "loadImage preserves dimensions and mip count");
    check(native.pixels == image.pixels, "loadImage preserves all 16 RGBA8 bytes");
    check(native.variantId == image.sourceVariantId, "loadImage passes through variant id");
}

void test_load_image_rejects_wrong_pixel_size() {
    ImageRecord image = make_image();
    image.pixels.pop_back();
    check(throws_invalid_argument([&] { (void)loadImage(image); }),
          "loadImage rejects a 15-byte payload for a 2x2 RGBA8 image");
}

void test_load_image_rejects_zero_width() {
    ImageRecord image = make_image(0, 2, 1);
    check(throws_invalid_argument([&] { (void)loadImage(image); }),
          "loadImage rejects zero width");
}

void test_load_image_rejects_too_many_mips() {
    ImageRecord image = make_image(2, 2, 3);
    check(throws_invalid_argument([&] { (void)loadImage(image); }),
          "loadImage rejects a mip count beyond the complete 2x2 chain");
}

void test_load_image_rejects_non_rgba8_format() {
    ImageRecord image = make_image();
    image.format = static_cast<PixelFormat>(1);
    check(throws_invalid_argument([&] { (void)loadImage(image); }),
          "loadImage rejects a non-R8G8B8A8_UNORM format");
}

void test_load_image_accepts_valid_multi_mip_image() {
    // (4,2), (2,1), (1,1) => 32 + 8 + 4 = 44 bytes.
    const ImageRecord image = make_image(4, 2, 3);
    check(expected_mip_size(4, 2, 3) == 44, "independent fixture size is 44 bytes");
    const NativeImage native = loadImage(image);
    check(native.width == 4 && native.height == 2 && native.mipCount == 3,
          "loadImage accepts all three complete mip levels");
    check(native.pixels == image.pixels, "loadImage preserves the multi-mip pixel bytes");
}

} // namespace

int main() {
    test_load_model_resolves_material_image();
    test_load_model_rejects_index_out_of_range();
    test_load_model_rejects_material_range_out_of_bounds();
    test_load_model_rejects_material_image_out_of_range();
    test_load_model_preserves_unresolved_material_image();
    test_load_image_preserves_valid_rgba8_image();
    test_load_image_rejects_wrong_pixel_size();
    test_load_image_rejects_zero_width();
    test_load_image_rejects_too_many_mips();
    test_load_image_rejects_non_rgba8_format();
    test_load_image_accepts_valid_multi_mip_image();

    if (g_failures > 0) {
        std::cerr << "test_loader: " << g_failures << " of " << g_checks << " checks FAILED\n";
        return EXIT_FAILURE;
    }
    std::cout << "test_loader: " << g_checks << " checks passed\n";
    return EXIT_SUCCESS;
}
