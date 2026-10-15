#pragma once

#include "assets/native_model.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace rt::native::preview {

struct ModelStats {
    std::uint64_t triangles = 0;
    std::array<float, 3> bmin{};
    std::array<float, 3> bmax{};
};

ModelStats computeStats(const assets::NativeModel& model);

// RGBA8 8-bit PNG (stored deflate blocks; no external dependency).
std::vector<std::uint8_t> encodePng(std::uint32_t width, std::uint32_t height,
                                    const std::vector<std::uint8_t>& rgba);

// Persistent offscreen Vulkan context (instance/device/pipeline/targets created once).
struct Renderer;
Renderer* previewInit(std::uint32_t width, std::uint32_t height, std::string& error,
                      std::string* deviceName);
// Re-uses the context; null view/proj => auto-fit camera. Geometry is re-uploaded per call.
bool previewRender(Renderer* r, const assets::NativeModel& model, const float* view, const float* proj,
                   std::vector<std::uint8_t>& rgbaOut, std::string& error);
void previewDestroy(Renderer* r);

// One-shot convenience wrappers (init -> render -> destroy).
// Offscreen Vulkan render (no window/swapchain) -> tightly packed RGBA8.
// Returns false and fills `error` on failure.
bool renderModel(const assets::NativeModel& model, std::uint32_t width, std::uint32_t height,
                 std::vector<std::uint8_t>& rgbaOut, std::string& error, std::string* deviceName);

// Same, but with an explicit camera: `view` and `proj` are column-major 4x4
// (16 floats each); the model is drawn as-is in world space (clip = proj*view*p,
// Vulkan clip conventions: y down, depth 0..1).
bool renderFrame(const assets::NativeModel& model, const float* view, const float* proj,
                 std::uint32_t width, std::uint32_t height, std::vector<std::uint8_t>& rgbaOut,
                 std::string& error, std::string* deviceName);

} // namespace rt::native::preview
