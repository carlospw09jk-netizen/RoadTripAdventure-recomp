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
