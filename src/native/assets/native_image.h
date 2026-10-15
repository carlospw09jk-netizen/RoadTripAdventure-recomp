#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace rt::native::assets {

struct NativeImage {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t mipCount = 0;
    std::vector<std::uint8_t> pixels;
    std::string variantId;
};

} // namespace rt::native::assets
