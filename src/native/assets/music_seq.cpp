#include "music_seq.h"

#include <algorithm>
#include <limits>

namespace rt::native::assets {
namespace {

std::uint16_t readLe16(std::span<const std::uint8_t> data, std::size_t offset) {
    return static_cast<std::uint16_t>(data[offset]) |
           (static_cast<std::uint16_t>(data[offset + 1]) << 8);
}

std::uint32_t readLe32(std::span<const std::uint8_t> data, std::size_t offset) {
    return static_cast<std::uint32_t>(data[offset]) |
           (static_cast<std::uint32_t>(data[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(data[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(data[offset + 3]) << 24);
}

}  // namespace

expected<TvbVoiceBank, MusicSeqError> parseTvb(std::span<const std::uint8_t> data) {
    constexpr std::size_t kTableBytes = kTvbProgramCount * sizeof(std::uint32_t);
    if (data.size() < kTableBytes) return MusicSeqError::Truncated;

    TvbVoiceBank bank;
    bank.programs.reserve(kTvbProgramCount);
    std::uint32_t previous = 0;
    std::uint32_t firstSample = 0;
    for (std::size_t i = 0; i < kTvbProgramCount; ++i) {
        const std::uint32_t offset = readLe32(data, i * sizeof(std::uint32_t));
        if ((offset & 0xfu) != 0 || offset > data.size() || offset < previous)
            return MusicSeqError::BadTvbOffset;
        if (offset != 0 && firstSample == 0) firstSample = offset;
        previous = offset;
        bank.programs.push_back({offset});
    }
    if (firstSample == 0 || previous <= firstSample) return MusicSeqError::BadTvbOffset;
    bank.samplePoolOffset = firstSample;
    bank.samplePoolEnd = previous;
    return bank;
}

expected<TsqSequence, MusicSeqError> parseTsq(std::span<const std::uint8_t> data) {
    // Each real file has four leading bytes followed by variable-count pointer
    // records.  A target is necessarily beyond the record that references it;
    // the lowest target consequently gives the exact end of this table.
    if (data.size() < 8) return MusicSeqError::Truncated;

    std::vector<std::uint32_t> offsets;
    std::size_t cursor = 4;
    std::size_t headerEnd = std::numeric_limits<std::size_t>::max();
    while (cursor < headerEnd) {
        if (cursor + 4 > data.size()) return MusicSeqError::Truncated;
        const std::uint16_t segment = readLe16(data, cursor);
        const std::uint16_t offset = readLe16(data, cursor + 2);
        if (segment != 0 && segment != 0x90) return MusicSeqError::BadTsqPointer;
        if (offset >= data.size() || offset < 4) return MusicSeqError::BadTsqPointer;
        headerEnd = std::min(headerEnd, static_cast<std::size_t>(offset));
        offsets.push_back(offset);
        cursor += 4;
    }
    if (offsets.empty() || headerEnd <= 4 || cursor != headerEnd)
        return MusicSeqError::BadTsqTrackTable;
    for (const std::uint32_t offset : offsets) {
        if (offset < headerEnd) return MusicSeqError::BadTsqTrackTable;
    }

    std::vector<std::uint32_t> boundaries = offsets;
    std::sort(boundaries.begin(), boundaries.end());
    boundaries.erase(std::unique(boundaries.begin(), boundaries.end()), boundaries.end());

    TsqSequence sequence;
    sequence.header.bytes = static_cast<std::uint32_t>(headerEnd);
    sequence.headerBytes = sequence.header.bytes;
    sequence.tracks.reserve(offsets.size());
    sequence.events.reserve(offsets.size());
    for (const std::uint32_t offset : offsets) {
        const auto next = std::upper_bound(boundaries.begin(), boundaries.end(), offset);
        const std::size_t end = next == boundaries.end() ? data.size() : *next;
        if (end <= offset) return MusicSeqError::BadTsqTrackTable;
        TsqRawEvent event;
        event.offset = offset;
        event.bytes.assign(data.begin() + offset, data.begin() + end);
        sequence.tracks.push_back({offset, sequence.events.size()});
        sequence.events.push_back(std::move(event));
    }
    return sequence;
}

}  // namespace rt::native::assets
