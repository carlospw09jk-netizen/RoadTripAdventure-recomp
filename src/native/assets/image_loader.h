#pragma once

#include "container.h"
#include "native_image.h"

namespace rt::native::assets {

NativeImage loadImage(const ImageRecord& image);

} // namespace rt::native::assets
