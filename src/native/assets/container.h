#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace rt::native::assets {

inline constexpr std::uint32_t kContainerVersion = 1;

struct Provenance {
    std::string sourceFile;
    std::uint64_t offset = 0;
    std::uint64_t length = 0;
    std::array<std::uint8_t, 32> sha256{};

    bool operator==(const Provenance&) const = default;
};

enum class PixelFormat : std::uint8_t { R8G8B8A8_UNORM = 0 };

struct ImageRecord {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t mipCount = 0;
    PixelFormat format = PixelFormat::R8G8B8A8_UNORM;
    std::vector<std::uint8_t> pixels;
    std::string sourceVariantId;
    Provenance provenance;

    bool operator==(const ImageRecord&) const = default;
};

struct Vertex {
    float px = 0.0f;
    float py = 0.0f;
    float pz = 0.0f;
    float nx = 0.0f;
    float ny = 0.0f;
    float nz = 0.0f;
    float u = 0.0f;
    float v = 0.0f;
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 0;

    bool operator==(const Vertex&) const = default;
};

struct MaterialRef {
    std::uint64_t firstIndex = 0;
    std::uint64_t indexCount = 0;
    std::uint32_t imageIndex = 0xFFFFFFFFu;

    bool operator==(const MaterialRef&) const = default;
};

struct MeshRecord {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<MaterialRef> materials;
    Provenance provenance;

    bool operator==(const MeshRecord&) const = default;
};

struct HostAssetContainer {
    std::uint32_t version = 0;
    std::vector<ImageRecord> images;
    std::vector<MeshRecord> meshes;

    bool operator==(const HostAssetContainer&) const = default;
};

enum class ContainerError : std::uint8_t {
    Ok = 0,
    BadMagic,
    BadVersion,
    Truncated,
    OffsetOverflow,
    CountOverflow,
    IndexOutOfBounds,
    MaterialImageOutOfRange,
    NonFiniteFloat,
    BadImageSize,
    MissingProvenance,
    TrailingData,
};

struct ContainerResult {
    ContainerError err = ContainerError::Ok;
    HostAssetContainer value;
};

std::vector<std::uint8_t> serialize(const HostAssetContainer& container);
ContainerResult deserialize(std::span<const std::uint8_t> bytes);
bool hasValidProvenance(const Provenance& provenance);

} // namespace rt::native::assets
