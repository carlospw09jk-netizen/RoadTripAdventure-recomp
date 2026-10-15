// src/native/tools/asset_convert_main.cpp
//
// rt-asset-convert: minimal proposed main for the CAR model side of the
// native asset pipeline. Takes one disc file path argument -- the user's own
// extracted CAR container (CARn/Qnn.BIN); no game data ships with this tool --
// converts its six models with convertCarModels() and prints a summary of the
// mesh-only container. Nothing is written: emitting the serialized .rtac
// container is left to the pipeline step that owns it. Links rt_native_assets
// only (authored code; no PS2/guest dependencies).
//
// Exit codes: 0 summary printed, 1 unreadable file or rejected conversion,
// 2 usage error.

#include "assets/container.h"
#include "assets/model_converter.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <string>
#include <vector>

namespace {

constexpr int kExitUsage = 2;

// Whole file as bytes; prints its own error and returns false when the file
// cannot be read.
bool readFileBytes(const std::string& path, std::vector<std::uint8_t>& out) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "rt-asset-convert: cannot open '" << path << "'\n";
        return false;
    }
    out.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    if (file.bad()) {
        std::cerr << "rt-asset-convert: error reading '" << path << "'\n";
        return false;
    }
    return true;
}

// Placeholder provenance digest: FNV-1a over the bytes, expanded to 32 bytes
// by a xorshift64 stream seeded non-zero. NOT SHA-256: hasValidProvenance()
// only rejects the all-zero digest (a missing-provenance sentinel check, not
// cryptographic verification). Replace with a real SHA-256 when the
// pipeline grows one.
std::array<std::uint8_t, 32> placeholderDigest(std::span<const std::uint8_t> bytes) {
    std::uint64_t hash = 0xcbf29ce484222325ull; // FNV-1a 64-bit offset basis
    for (std::uint8_t byte : bytes) {
        hash ^= byte;
        hash *= 0x100000001b3ull; // FNV-1a 64-bit prime
    }
    std::array<std::uint8_t, 32> digest{};
    std::uint64_t state = hash | 1ull; // never zero, so the digest is never all-zero
    for (std::size_t i = 0; i < digest.size(); ++i) {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17; // xorshift64
        digest[i] = static_cast<std::uint8_t>((state >> 24) & 0xFFu);
    }
    return digest;
}

const char* carModelErrorName(rt::native::assets::CarModelError err) {
    switch (err) {
        case rt::native::assets::CarModelError::Ok: return "Ok";
        case rt::native::assets::CarModelError::Truncated: return "Truncated";
        case rt::native::assets::CarModelError::BadHeader: return "BadHeader";
        case rt::native::assets::CarModelError::ChainDecode: return "ChainDecode";
        case rt::native::assets::CarModelError::BadBatch: return "BadBatch";
        case rt::native::assets::CarModelError::MissingProvenance: return "MissingProvenance";
    }
    return "CarModelError(unknown)";
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "usage: rt-asset-convert <CAR container file (CARn/Qnn.BIN)>\n";
        return kExitUsage;
    }
    const std::string path = argv[1];

    std::vector<std::uint8_t> bytes;
    if (!readFileBytes(path, bytes)) {
        return EXIT_FAILURE;
    }

    const rt::native::assets::CarModelResult result =
        rt::native::assets::convertCarModels(bytes, path, placeholderDigest(bytes));
    if (result.err != rt::native::assets::CarModelError::Ok) {
        std::cerr << "rt-asset-convert: " << path << ": conversion failed: "
                  << carModelErrorName(result.err);
        // errorOffset is the offset being validated when the error was found;
        // 0 also stands for "not applicable" (MissingProvenance, or a file too
        // short to hold the header), so it is only printed when non-zero.
        if (result.errorOffset != 0) {
            std::cerr << " (near file offset 0x" << std::hex << result.errorOffset
                      << std::dec << ')';
        }
        std::cerr << '\n';
        return EXIT_FAILURE;
    }

    // Mesh-only container (model_converter.h): images stay empty and every
    // MaterialRef keeps the unresolved imageIndex sentinel 0xFFFFFFFFu; image
    // binding is a separate pipeline step.
    const rt::native::assets::HostAssetContainer& container = result.value;
    std::uint64_t totalVertices = 0;
    std::uint64_t totalIndices = 0;
    std::uint64_t totalMaterials = 0;
    for (std::size_t i = 0; i < container.meshes.size(); ++i) {
        const rt::native::assets::MeshRecord& mesh = container.meshes[i];
        totalVertices += mesh.vertices.size();
        totalIndices += mesh.indices.size();
        totalMaterials += mesh.materials.size();
        std::cout << "  mesh " << i << ": " << mesh.vertices.size() << " vertices, "
                  << mesh.indices.size() << " indices, " << mesh.materials.size()
                  << " materials [provenance '" << mesh.provenance.sourceFile
                  << "' span 0x" << std::hex << mesh.provenance.offset << "+0x"
                  << mesh.provenance.length << std::dec << "]\n";
    }
    std::cout << "rt-asset-convert: " << path << ": converted " << container.meshes.size()
              << " meshes, " << totalVertices << " vertices, " << totalIndices << " indices, "
              << totalMaterials << " materials (image indices unresolved: image binding is a "
                                 "separate step), "
              << container.images.size() << " images\n";
    return EXIT_SUCCESS;
}
