#include "audio/vag_loader.h"

#include "audio/vag.h"

#include <cstring>
#include <fstream>
#include <iterator>

namespace rt::native::audio {
namespace {

std::uint32_t readBigEndian32(const std::uint8_t* data) {
    return (std::uint32_t(data[0]) << 24) | (std::uint32_t(data[1]) << 16) |
           (std::uint32_t(data[2]) << 8) | std::uint32_t(data[3]);
}

bool hasValidRawBlockHeaders(const std::uint8_t* data, std::size_t size) {
    for (std::size_t offset = 0; offset < size; offset += kVagBlockBytes) {
        const std::uint8_t header = data[offset];
        if ((header & 0x0f) > 12 || (header >> 4) > 4) return false;
    }
    return true;
}

}  // namespace

bool loadVagToPcm(const std::vector<std::uint8_t>& vagData, PcmBuffer& out) {
    if (vagData.empty()) return false;

    const std::uint8_t* data = vagData.data();
    std::size_t size = vagData.size();
    VagHeader header;
    const bool hasVagMagic = size >= 4 && std::memcmp(data, "VAGp", 4) == 0;
    if (hasVagMagic && !parseVagHeader(data, size, header)) return false;
    if (hasVagMagic) {
        const std::size_t declaredSize = readBigEndian32(data + 12);
        if (header.dataOffset > size || declaredSize < kVagBlockBytes ||
            declaredSize % kVagBlockBytes != 0 || declaredSize > size - header.dataOffset) return false;
        data += header.dataOffset;
        size = declaredSize;
    }
    if (size < kVagBlockBytes || size % kVagBlockBytes != 0) return false;
    if (!hasVagMagic && !hasValidRawBlockHeaders(data, size)) return false;

    PcmBuffer decoded;
    decoded.samples = decodeVagStream(data, size);
    if (decoded.samples.empty()) return false;
    decoded.frames = decoded.samples.size();
    decoded.loop = true;
    out = std::move(decoded);
    return true;
}

bool loadVagFile(const std::filesystem::path& path, PcmBuffer& out) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;

    const std::vector<std::uint8_t> data(std::istreambuf_iterator<char>(file), {});
    if (file.bad()) return false;
    return loadVagToPcm(data, out);
}

}  // namespace rt::native::audio
