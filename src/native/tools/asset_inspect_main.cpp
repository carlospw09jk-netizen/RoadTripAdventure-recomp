// src/native/tools/asset_inspect_main.cpp
//
// rt-asset-inspect: minimal proposed main for the texture side of the native
// asset pipeline. Takes one disc file path argument -- the user's own
// extracted texture upload chain (the CNT..END DMA region a CAR/FLD container
// points at); no game data ships with this tool -- decodes the chain's first
// texture with convertImage() and prints a summary. Links rt_native_assets
// only (authored code; no PS2/guest dependencies).
//
// Exit codes: 0 summary printed, 1 unreadable file or rejected chain,
// 2 usage error.

#include "assets/container.h"
#include "assets/image_converter.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
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
        std::cerr << "rt-asset-inspect: cannot open '" << path << "'\n";
        return false;
    }
    out.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    if (file.bad()) {
        std::cerr << "rt-asset-inspect: error reading '" << path << "'\n";
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

const char* pixelFormatName(rt::native::assets::PixelFormat format) {
    switch (format) {
        case rt::native::assets::PixelFormat::R8G8B8A8_UNORM: return "R8G8B8A8_UNORM";
    }
    return "PixelFormat(unknown)";
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "usage: rt-asset-inspect <texture upload chain file "
                     "(CNT..END DMA region)>\n";
        return kExitUsage;
    }
    const std::string path = argv[1];

    std::vector<std::uint8_t> bytes;
    if (!readFileBytes(path, bytes)) {
        return EXIT_FAILURE;
    }

    // Provenance for the chain file read whole: offset 0 (the file starts at
    // the chain's first tag) and the full length.
    rt::native::assets::Provenance provenance;
    provenance.sourceFile = path;
    provenance.offset = 0;
    provenance.length = bytes.size();
    provenance.sha256 = placeholderDigest(bytes);

    // variantId: the filename plus the first-texture index, so the printed
    // record says where it came from.
    const std::string variantId = std::filesystem::path(path).filename().string() + "#0";

    const rt::native::assets::ImageRecord image =
        rt::native::assets::convertImage(bytes, provenance, variantId);

    // Failure contract (image_converter.h): malformed, truncated or
    // unsupported input yields the default-constructed record (width ==
    // height == mipCount == 0, empty pixels); no partial output.
    if (image.width == 0 || image.height == 0 || image.pixels.empty()) {
        std::cerr << "rt-asset-inspect: " << path
                  << ": no decodable texture (malformed, truncated or unsupported chain)\n";
        return EXIT_FAILURE;
    }

    std::cout << "rt-asset-inspect: " << path << ": decoded " << image.width << 'x'
              << image.height << ' ' << pixelFormatName(image.format) << ", " << image.mipCount
              << " mip(s), " << image.pixels.size() << " pixel bytes, variant '"
              << image.sourceVariantId << "' [provenance '" << image.provenance.sourceFile
              << "' span 0x" << std::hex << image.provenance.offset << "+0x"
              << image.provenance.length << std::dec << "]\n";
    return EXIT_SUCCESS;
}
