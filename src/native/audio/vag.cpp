// PlayStation ADPCM ("VAG") decoder implementation. See vag.h.
//
// The predictor math is the same as tools/audio_extract.cpp::decodeAdpcm
// (the disc extraction path used by rt_audio_extract), rewritten as a
// reusable block/stream API for the mixer model. We treat each block
// independently and walk the stream in 16-byte steps, so the in/out
// predictor state is the only carry-over between blocks; this matches
// the layout no$psx documents and what the IOP's SNDMOD.IRX expects of
// the stream it is given.
#include "audio/vag.h"

#include <algorithm>
#include <cstring>
#include <string>

namespace rt::native::audio {
namespace {

// Canonical PS ADPCM predictor coefficient tables. Index = (block header
// nibble >> 4) capped at 4, so only values 0..4 are addressable. The
// extraction tool also caps at 4; we mirror that.
constexpr int kF0[5] = {0, 60, 115, 98, 122};
constexpr int kF1[5] = {0,  0, -52, -55, -60};

// Sign-extend a 4-bit value into a 16-bit signed integer, then arithmetic-
// shift right by `shift`. We use the exact form `int(int16_t(uint16_t(nibble
// << 12))) >> shift` so we are bit-identical to the extractor (which is the
// live oracle on disc data).
inline std::int32_t signedNibbleAsShifted(std::uint32_t nibble, std::uint32_t shift) {
    const std::uint32_t v = static_cast<std::uint32_t>(static_cast<std::uint16_t>(nibble << 12));
    std::int32_t s = static_cast<std::int32_t>(static_cast<std::int16_t>(v));
    s >>= shift;
    return s;
}

inline std::int32_t saturate16(std::int32_t s) {
    if (s >  32767) return  32767;
    if (s < -32768) return -32768;
    return s;
}

}  // namespace

namespace {

std::size_t decodeVagBlockImpl(const std::uint8_t* block,
                               std::int16_t* outSamples,
                               std::int32_t& s1, std::int32_t& s2,
                               bool stopAtEndFlag) {
    // byte 0 = shift (low nibble), filter index (high nibble capped at 4)
    const std::uint32_t shift = block[0] & 0x0F;
    const std::uint32_t filter = std::min<std::uint32_t>(block[0] >> 4, 4);
    const std::uint8_t flags = block[1];

    if (stopAtEndFlag && flags == kVagEndOfStream) return 0;

    const int f0 = kF0[filter];
    const int f1 = kF1[filter];
    for (std::size_t i = 0; i < kVagBlockSamples; ++i) {
        const std::uint32_t byte = block[2 + (i / 2)];
        const std::uint32_t nibble = (byte >> ((i & 1) * 4)) & 0x0F;
        std::int32_t s = signedNibbleAsShifted(nibble, shift);
        s += (s1 * f0 + s2 * f1 + 32) >> 6;
        s = saturate16(s);
        s2 = s1;
        s1 = s;
        outSamples[i] = static_cast<std::int16_t>(s);
    }
    return kVagBlockSamples;
}

}  // namespace

std::size_t decodeVagBlock(const std::uint8_t* block,
                            std::int16_t* outSamples,
                            std::int32_t& s1, std::int32_t& s2) {
    // End-of-stream block: pad bytes with the magic flag value 7. We
    // consume no samples and leave the predictor untouched so a caller
    // that loops back would carry the same state into the next block.
    return decodeVagBlockImpl(block, outSamples, s1, s2, true);
}

bool decodeVagBlocks(const std::uint8_t* data, std::size_t size,
                     std::vector<std::int16_t>& outSamples) {
    outSamples.clear();
    if (size == 0) return true;
    if (data == nullptr || size % kVagBlockBytes != 0) return false;
    outSamples.reserve((size / kVagBlockBytes) * kVagBlockSamples);

    std::int32_t s1 = 0, s2 = 0;
    std::int16_t blockOut[kVagBlockSamples];
    for (std::size_t off = 0; off < size; off += kVagBlockBytes) {
        const std::size_t got = decodeVagBlockImpl(data + off, blockOut, s1, s2, false);
        outSamples.insert(outSamples.end(), blockOut, blockOut + got);
    }
    return true;
}

std::vector<std::int16_t> decodeVagStream(const std::uint8_t* data,
                                           std::size_t size) {
    std::vector<std::int16_t> out;
    if (data == nullptr || size == 0) return out;
    out.reserve((size / kVagBlockBytes) * kVagBlockSamples);

    std::int32_t s1 = 0, s2 = 0;
    std::int16_t blockOut[kVagBlockSamples];
    for (std::size_t off = 0; off + kVagBlockBytes <= size; off += kVagBlockBytes) {
        const std::size_t got =
            decodeVagBlock(data + off, blockOut, s1, s2);
        if (got == 0) break;  // terminator: drop padding blocks past this
        out.insert(out.end(), blockOut, blockOut + got);
    }
    return out;
}

bool parseVagHeader(const std::uint8_t* data, std::size_t size, VagHeader& out) {
    out = VagHeader{};
    if (data == nullptr || size < kVagHeaderBytes) return false;
    if (std::memcmp(data, "VAGp", 4) != 0) return false;
    // Big-endian 32-bit at offset 16 (sample rate) and offset 12 (length).
    auto be32 = [](const std::uint8_t* p) -> std::uint32_t {
        return (std::uint32_t(p[0]) << 24) | (std::uint32_t(p[1]) << 16) |
               (std::uint32_t(p[2]) << 8) |  std::uint32_t(p[3]);
    };
    out.sampleRate = be32(data + 16);
    out.dataSize = be32(data + 12);
    // dataSize is the authored body length; clamp to whatever's left in the
    // file (the extraction tool does the same).
    const std::size_t left = size - kVagHeaderBytes;
    if (out.dataSize > left) out.dataSize = static_cast<std::uint32_t>(left);
    return true;
}

void buildVagBlock(const VagNibbles& n, std::uint8_t out[16]) {
    // Header byte: low nibble = shift, high nibble = filter (capped at 4
    // to match the decoder's table bound).
    const std::uint32_t filter = std::min<std::uint32_t>(n.filter, 4);
    out[0] = static_cast<std::uint8_t>((filter << 4) | (n.shift & 0x0F));
    out[1] = n.flags;
    for (std::size_t i = 0; i < 14; ++i) out[2 + i] = 0;
    // Pack nibbles: i=0 → low nibble of byte 2, i=1 → high nibble of byte 2,
    // i=2 → low nibble of byte 3, and so on (same as the decoder's read).
    for (std::size_t i = 0; i < 28; ++i) {
        const std::uint8_t nib = static_cast<std::uint8_t>(n.nibbles[i] & 0x0F);
        if ((i & 1) == 0) {
            out[2 + (i / 2)] |= static_cast<std::uint8_t>(nib);
        } else {
            out[2 + (i / 2)] |= static_cast<std::uint8_t>(nib << 4);
        }
    }
}

}  // namespace rt::native::audio
