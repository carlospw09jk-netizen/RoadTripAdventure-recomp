// PlayStation ADPCM ("VAG") decoder. Pure host C++; no PS2/guest code.
//
// VAG is Sony's 4-bit adaptive-difference ADPCM used across PlayStation games.
// Each 16-byte block carries 28 mono int16 samples: a 1-byte shift/filter
// header, a 1-byte flags slot, and 14 bytes of packed 4-bit nibbles (28 total).
// Per-nibble reconstruction:
//
//   d     = (sign_extend_nibble(nibble << 12)) >> shift
//   out   = clamp16(d + ((s1 * f0[filter] + s2 * f1[filter] + 32) >> 6))
//   s2,s1 = out, s1          (two-sample predictor state)
//
// with the canonical coefficient tables {0, 60, 115, 98, 122} / {0, 0, -52,
// -55, -60}. The model's reference is tools/audio_extract.cpp::decodeAdpcm
// (the disc-extraction path used by rt_audio_extract); the public ADPCM
// algorithm is the same one no$psx documents. Block flags:
//
//   bit 0  loop region start
//   bit 1  loop region end
//   7      end-of-stream  (any other flag pattern is informational)
//
// VAG ("VAGp") files have a 48-byte header; raw streams start at byte 0.
// The stream decoder models the documented IOP SNDMOD semantics for sample
// playback (see docs/game-mechanics/sound-and-daynight.md §1): looping is
// handled by the stream, not the block. Channels, pan and volume live in the
// mixer (mixer.h). TSQ sequence playback is outside this decoder; the TVB
// container parser feeds its headerless blocks through decodeVagBlocks().
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace rt::native::audio {

// Block shape is fixed: 16 bytes in, 28 samples out, 2-byte header + 14-byte
// payload. Documented in CLAUDE.md ("VAG format notes") and matching the disc
// extraction tool's decode loop (tools/audio_extract.cpp).
inline constexpr std::size_t kVagBlockBytes = 16;   // one ADPCM block on the wire
inline constexpr std::size_t kVagBlockSamples = 28; // samples decoded per block
inline constexpr std::size_t kVagHeaderBytes = 48;   // standard ".VAG" container
inline constexpr std::uint8_t kVagEndOfStream = 7;   // flags byte marking the last block
inline constexpr std::uint8_t kVagFlagLoopStart = 0x01;
inline constexpr std::uint8_t kVagFlagLoopEnd   = 0x02;

// Result of probing a VAG ("VAGp") container: header sample rate plus data
// extent inside the file. Stream decoding needs the rate as the source's own
// rate (e.g. 12000 Hz for Road Trip's SOUND/1CH-3CH streams).
struct VagHeader {
    std::uint32_t sampleRate = 0;       // big-endian at offset 16 (VAGp "rate")
    std::uint32_t dataSize = 0;         // body bytes; bounded by file length
    std::uint32_t dataOffset = kVagHeaderBytes;
};

// Decode one 16-byte block into 28 int16 samples, returning how many samples
// were produced. `s1`/`s2` are the two-sample predictor state (in/out). If
// the block's flags byte equals kVagEndOfStream (7) the block is the silent
// terminator and the function returns 0 without advancing the buffer.
std::size_t decodeVagBlock(const std::uint8_t* block,
                           std::int16_t* outSamples,
                           std::int32_t& s1, std::int32_t& s2);

// Whole-stream decode: every byte in [data, data+size) is consumed in
// 16-byte block steps, producing a flat interleaved-mono int16 vector. If
// the stream ends with a kVagEndOfStream block, decoding stops there (no
// zero-padding beyond it). A truncated tail (last block < 16 bytes) is
// dropped. All output samples are clamped to int16 range.
std::vector<std::int16_t> decodeVagStream(const std::uint8_t* data,
                                           std::size_t size);

// Decode a headerless, block-aligned PS ADPCM buffer.  Unlike
// decodeVagStream(), this treats every block as sample data and does not give
// the VAG stream end flag special meaning; container formats such as TVB use
// that byte as in-band metadata.  Returns false when data is null for a
// non-empty input or when size is not a whole number of blocks.  `outSamples`
// is cleared before decoding.
bool decodeVagBlocks(const std::uint8_t* data, std::size_t size,
                     std::vector<std::int16_t>& outSamples);

// Parse a VAG ("VAGp") container header. Returns true on a 48-byte file that
// starts with "VAGp"; on false, the out parameter is left zeroed. Used by the
// disc extraction path; the raw decoder above is what the mixer feeds.
bool parseVagHeader(const std::uint8_t* data, std::size_t size, VagHeader& out);

// Test helper: build one 16-byte ADPCM block from a chosen shift+filter and
// exactly 28 nibbles. The bytes are written in the on-stream layout (the
// same layout the decoder consumes), so a stream built from these bytes
// can be decoded with `decodeVagStream` and compared against a manual
// reconstruction of the standard PS ADPCM formula. Used by the unit
// tests in tests/native_assets/test_audio.cpp to round-trip against a
// known PCM reference.
struct VagNibbles {
    std::uint32_t shift = 0;        // 0..12 (low nibble of byte 0)
    std::uint32_t filter = 0;       // 0..4 (high nibble of byte 0; capped)
    std::uint8_t flags = 0;         // block[1]; 7 = end-of-stream
    std::uint8_t nibbles[28] = {};  // 28x 4-bit values packed into 14 bytes
};
void buildVagBlock(const VagNibbles& n, std::uint8_t out[16]);

}  // namespace rt::native::audio
