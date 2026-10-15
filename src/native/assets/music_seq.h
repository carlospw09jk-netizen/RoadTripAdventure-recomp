#pragma once

// Road Trip SOUND music container parsing.  TVB is a 256-entry, little-endian
// voice/sample-offset table.  TSQ begins with encoded IOP pointers
// ({segment, file-offset} u16 pairs) followed by independent track streams.
//
// TSQ track stream opcodes are deliberately not named here: byte inspection
// proves stream boundaries, but not note/program/tempo semantics.  `events`
// therefore exposes one bounded raw span per track until that grammar is
// independently established.

#include "field_converter.h"  // expected<Value, Error>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace rt::native::assets {

enum class MusicSeqError : std::uint8_t {
    Ok = 0,
    Truncated,
    BadTvbOffset,
    BadTsqPointer,
    BadTsqTrackTable,
};

inline constexpr std::size_t kTvbProgramCount = 256;

struct TvbProgram {
    std::uint32_t sampleOffset = 0;  // file-relative; 0 and pool end mean empty
    bool hasSample() const noexcept { return sampleOffset != 0; }
};

struct TvbVoiceBank {
    std::vector<TvbProgram> programs;
    std::uint32_t samplePoolOffset = 0;
    std::uint32_t samplePoolEnd = 0;  // exclusive terminal offset from table
};

// Parse the first 256 u32s of a TVB.  Its non-zero offsets are monotonic,
// 16-byte VAG-block aligned file offsets.  The greatest offset is the sample
// pool's terminal boundary (it is repeated by empty trailing program slots).
expected<TvbVoiceBank, MusicSeqError> parseTvb(std::span<const std::uint8_t> data);

// Number of logical sample slots after TVB's two leading non-sample entries.
// Returns zero when the offset table is invalid or truncated.
std::size_t tvbSampleSlotCount(std::span<const std::uint8_t> data);

// Return the block-aligned headerless ADPCM byte range for one logical TVB
// sample slot.  Empty trailing slots are valid and produce an empty span.
expected<std::span<const std::uint8_t>, MusicSeqError>
tvbSampleByteRange(std::span<const std::uint8_t> data, std::size_t slot);

// Decode one TVB sample's headerless PS ADPCM blocks into PCM.  Every complete
// block in its byte range produces 28 samples, including blocks whose flags
// byte would terminate a standalone VAG stream.  Returns false on an invalid
// TVB table, slot, or block range; pcmOut is cleared in either case.
bool tvbDecodeSample(std::span<const std::uint8_t> data, std::size_t slot,
                     std::vector<std::int16_t>& pcmOut);

struct TsqHeader {
    // TSQ's pointer table has no explicit count.  Its byte extent is inferred
    // from the lowest valid target; each entry is {segment, fileOffset}.
    std::uint32_t bytes = 0;
};

struct TsqTrack {
    std::uint32_t offset = 0;
    std::size_t eventIndex = 0;  // one raw event span per unresolved track
};

struct TsqRawEvent {
    std::uint32_t offset = 0;
    std::vector<std::uint8_t> bytes;
};

struct TsqSequence {
    TsqHeader header;
    std::uint32_t headerBytes = 0;  // convenience mirror of header.bytes
    std::vector<TsqTrack> tracks;
    std::vector<TsqRawEvent> events;
};

// Parse the variable-size TSQ pointer table and create raw bounded spans for
// the track event streams.  Accepted pointer segments are 0x0090 (the common
// IOP segment marker) and 0 (seen in BGM's leading entry); every file offset
// must be in bounds and at/after the inferred header end.
expected<TsqSequence, MusicSeqError> parseTsq(std::span<const std::uint8_t> data);

}  // namespace rt::native::assets
