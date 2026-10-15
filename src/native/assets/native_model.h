#pragma once

#include "container.h"

#include <cstdint>
#include <vector>

namespace rt::native::assets {

struct NativeModel {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<std::uint32_t> materialImage;
};

} // namespace rt::native::assets
