#pragma once

#include "container.h"
#include "native_model.h"

#include <span>

namespace rt::native::assets {

NativeModel loadModel(const MeshRecord& mesh, std::span<const ImageRecord> images);

} // namespace rt::native::assets
