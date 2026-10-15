// FLD/COURSE field-tile header parsing.  Model, directory, and texture
// conversion remain deferred pending field-format reverse engineering.

#include "field_converter.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace rt::native::assets {
namespace {

constexpr std::size_t kHeaderBytes = 0x20;
constexpr std::size_t kDigestBytes = 32;

std::uint32_t readU32LE(std::span<const std::uint8_t> bytes, std::size_t offset) noexcept {
    return static_cast<std::uint32_t>(bytes[offset]) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

bool isAllZeroDigest(const std::array<std::uint8_t, kDigestBytes>& digest) noexcept {
    return std::all_of(digest.begin(), digest.end(),
                       [](std::uint8_t byte) { return byte == 0; });
}

} // namespace

expected<FieldTileHeader, FieldTileError>
parseFieldTileHeader(std::span<const std::uint8_t> tileFile) {
    const std::uint64_t actualFileSize = static_cast<std::uint64_t>(tileFile.size());
    if (actualFileSize < kHeaderBytes) {
        return FieldTileError::Truncated;
    }

    if (readU32LE(tileFile, 0x00) != kHeaderBytes) {
        return FieldTileError::BadHeader;
    }

    FieldTileHeader header{
        .offA = readU32LE(tileFile, 0x04),
        .offB = readU32LE(tileFile, 0x08),
        .offC = readU32LE(tileFile, 0x0C),
        .fileSize = readU32LE(tileFile, 0x10),
    };
    if (static_cast<std::uint64_t>(header.fileSize) != actualFileSize ||
        header.offA < kHeaderBytes || header.offA > header.offB ||
        header.offB > header.offC || header.offC > header.fileSize) {
        return FieldTileError::BadHeader;
    }
    return header;
}

FieldTileResult convertFieldTile(std::span<const std::uint8_t> tileFile,
                                 const std::string& sourceFile,
                                 const std::array<std::uint8_t, 32>& sha256) {
    FieldTileResult result{};
    if (sourceFile.empty() || isAllZeroDigest(sha256)) {
        result.err = FieldTileError::MissingProvenance;
        return result;
    }

    const auto header = parseFieldTileHeader(tileFile);
    if (!header) {
        result.err = header.error();
        return result;
    }

    // TODO: decode the field VIF region (differs from CAR; see disc-content-format.md).
    // TODO: parse the offA..offB / offB..offC directories and offC texture data.
    result.value.version = kContainerVersion;
    return result;
}

} // namespace rt::native::assets
