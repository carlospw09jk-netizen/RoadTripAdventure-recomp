#include "image_loader.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace rt::native::assets {
namespace {

std::size_t checkedMipBytes(std::uint32_t width, std::uint32_t height,
                            std::uint32_t mipCount) {
    const std::size_t maxSize = std::numeric_limits<std::size_t>::max();
    std::size_t total = 0;

    for (std::uint32_t level = 0; level < mipCount; ++level) {
        if (static_cast<std::uint64_t>(width) > maxSize ||
            static_cast<std::uint64_t>(height) > maxSize) {
            throw std::invalid_argument("image dimensions exceed host addressable size");
        }
        const std::size_t levelWidth = static_cast<std::size_t>(width);
        const std::size_t levelHeight = static_cast<std::size_t>(height);
        if (levelHeight != 0 && levelWidth > maxSize / levelHeight) {
            throw std::invalid_argument("image mip size overflows host addressable size");
        }
        const std::size_t pixels = levelWidth * levelHeight;
        if (pixels > maxSize / 4) {
            throw std::invalid_argument("image mip size overflows host addressable size");
        }
        const std::size_t levelBytes = pixels * 4;
        if (total > maxSize - levelBytes) {
            throw std::invalid_argument("image mip sum overflows host addressable size");
        }
        total += levelBytes;

        width = width > 1 ? width / 2 : 1;
        height = height > 1 ? height / 2 : 1;
    }

    return total;
}

std::uint32_t completeMipCount(std::uint32_t width, std::uint32_t height) {
    std::uint32_t count = 1;
    while (width > 1 || height > 1) {
        width = width > 1 ? width / 2 : 1;
        height = height > 1 ? height / 2 : 1;
        ++count;
    }
    return count;
}

} // namespace

NativeImage loadImage(const ImageRecord& image) {
    if (image.format != PixelFormat::R8G8B8A8_UNORM || image.width == 0 || image.height == 0 ||
        image.mipCount == 0 || image.mipCount > completeMipCount(image.width, image.height)) {
        throw std::invalid_argument("image format, dimensions, or mip count is invalid");
    }

    const std::size_t expectedBytes = checkedMipBytes(image.width, image.height, image.mipCount);
    if (image.pixels.size() != expectedBytes) {
        throw std::invalid_argument("image pixel byte count does not match tightly packed RGBA8 mips");
    }

    NativeImage native;
    native.width = image.width;
    native.height = image.height;
    native.mipCount = image.mipCount;
    native.pixels = image.pixels;
    native.variantId = image.sourceVariantId;
    return native;
}

} // namespace rt::native::assets
