#include "model_loader.h"

#include <cstdint>
#include <stdexcept>

namespace rt::native::assets {

NativeModel loadModel(const MeshRecord& mesh, std::span<const ImageRecord> images) {
    for (const std::uint32_t index : mesh.indices) {
        if (static_cast<std::uint64_t>(index) >= mesh.vertices.size()) {
            throw std::invalid_argument("mesh index is outside the vertex array");
        }
    }

    NativeModel model;
    model.vertices = mesh.vertices;
    model.indices = mesh.indices;
    model.materialImage.reserve(mesh.materials.size());

    const std::uint64_t indexCount = static_cast<std::uint64_t>(mesh.indices.size());
    for (const MaterialRef& material : mesh.materials) {
        if (material.firstIndex > indexCount ||
            material.indexCount > indexCount - material.firstIndex) {
            throw std::invalid_argument("material index range is outside the mesh index array");
        }
        if (material.imageIndex != 0xFFFFFFFFu &&
            static_cast<std::uint64_t>(material.imageIndex) >= images.size()) {
            throw std::invalid_argument("material image index is outside the image array");
        }
        model.materialImage.push_back(material.imageIndex);
    }

    return model;
}

} // namespace rt::native::assets
