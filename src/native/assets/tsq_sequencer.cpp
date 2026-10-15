#include "tsq_sequencer.h"

#include "music_seq.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace rt::native::assets {
namespace {

constexpr std::size_t kMaxTrackInstructions = 65536;
constexpr std::size_t kMaxTrackLoops = 1024;

std::uint16_t readLe16(std::span<const std::uint8_t> data, std::size_t offset) {
    return static_cast<std::uint16_t>(data[offset]) |
           (static_cast<std::uint16_t>(data[offset + 1]) << 8);
}

bool isEventByte(std::uint8_t byte) {
    return byte <= 0xdf || byte == 0xe0 || byte == 0xe1 || byte == 0xe2 ||
           byte == 0xe4 || byte == 0xe5 || byte == 0xea || byte == 0xf0 ||
           byte == 0xf8 || byte == 0xff;
}

// BGM outer spans sometimes point to another table whose segment marker is
// 0x80, not the 0x90 used by the top-level table. Recursively following only
// well-formed lists avoids mistaking ordinary event bytes for pointers.
std::size_t resolveEventStart(std::span<const std::uint8_t> data, std::size_t start,
                              std::size_t headerBytes, unsigned depth = 0) {
    if (depth == 3 || start + 8 > data.size()) return start;
    std::vector<std::size_t> targets;
    for (std::size_t cursor = start; cursor + 4 <= data.size() && targets.size() < 128;
         cursor += 4) {
        const auto segment = readLe16(data, cursor);
        const auto offset = readLe16(data, cursor + 2);
        if (segment != 0x80 && segment != 0x90 && segment != 0) break;
        if (offset < headerBytes || offset >= data.size()) break;
        targets.push_back(offset);
    }
    if (targets.size() < 2) return start;

    // An inner table may include targets below its own location. Prefer a
    // target after the table that begins with a grammar byte, then recurse.
    for (const auto target : targets) {
        if (target > start && isEventByte(data[target]))
            return resolveEventStart(data, target, headerBytes, depth + 1);
    }
    return start;
}

struct Voice {
    std::uint32_t startTick = 0;
    std::uint32_t endTick = 0;
    std::uint8_t key = 0;
    std::uint8_t program = 0;
    double gain = 1.0;
    double left = 1.0;
    double right = 1.0;
    double fineSemitones = 0.0;
};

struct DecodedTrack {
    std::vector<TsqDumpNote> events;
    std::vector<Voice> voices;
    TsqDecodeStats stats;
};

DecodedTrack decodeTrack(std::span<const std::uint8_t> data, std::size_t start,
                         std::size_t trackIndex) {
    DecodedTrack out;
    std::size_t pc = start;
    std::uint32_t tick = 0;
    std::uint8_t program = 0;
    double gain = 1.0;
    double left = 1.0;
    double right = 1.0;
    double fineSemitones = 0.0;
    bool active = false;
    Voice voice;
    std::size_t loops = 0;

    const auto emit = [&](TsqNoteKind kind, std::uint8_t key = 0, std::int32_t value = 0) {
        out.events.push_back({trackIndex, tick, kind, key, value});
    };
    const auto release = [&] {
        if (active) {
            voice.endTick = tick;
            out.voices.push_back(voice);
            active = false;
        }
    };

    for (std::size_t steps = 0; pc < data.size() && steps < kMaxTrackInstructions; ++steps) {
        const std::uint8_t status = data[pc++];
        ++out.stats.consumedBytes;
        if (status <= 0x7f) {
            tick += status;
        } else if (status <= 0xdf) {
            release();
            voice = {tick, tick, static_cast<std::uint8_t>(status & 0x7f), program,
                     gain, left, right, fineSemitones};
            active = true;
            emit(TsqNoteKind::NoteOn, voice.key);
        } else if (status == 0xf0) {
            emit(TsqNoteKind::NoteOff, active ? voice.key : 0);
            release();
        } else if (status == 0xff) {
            emit(TsqNoteKind::End);
            release();
            break;
        } else if (status == 0xe0 && pc < data.size()) {
            const auto value = data[pc++];
            ++out.stats.consumedBytes;
            gain = static_cast<double>(value) / 127.0;
            emit(TsqNoteKind::Volume, 0, value);
        } else if (status == 0xe1 && pc + 1 < data.size()) {
            const auto l = data[pc++];
            const auto r = data[pc++];
            out.stats.consumedBytes += 2;
            left = static_cast<double>(l) / 64.0;
            right = static_cast<double>(r) / 64.0;
            emit(TsqNoteKind::Pan, 0, static_cast<std::int32_t>(l) | (static_cast<std::int32_t>(r) << 8));
        } else if (status == 0xe2 && pc < data.size()) {
            program = data[pc++];
            ++out.stats.consumedBytes;
            emit(TsqNoteKind::Program, 0, program);
        } else if ((status == 0xe4 || status == 0xe5 || status == 0xea) && pc + 1 < data.size()) {
            const auto value = static_cast<std::int16_t>(readLe16(data, pc));
            pc += 2;
            out.stats.consumedBytes += 2;
            fineSemitones = static_cast<double>(value) / 4096.0;
            emit(TsqNoteKind::Pitch, 0, value);
        } else if (status == 0xf8 && pc + 1 < data.size()) {
            const auto relative = static_cast<std::int16_t>(readLe16(data, pc));
            pc += 2;
            out.stats.consumedBytes += 2;
            emit(TsqNoteKind::Loop, 0, relative);
            const auto target = static_cast<std::ptrdiff_t>(pc) + relative;
            if (loops++ < kMaxTrackLoops && target >= static_cast<std::ptrdiff_t>(start) &&
                target < static_cast<std::ptrdiff_t>(data.size())) {
                pc = static_cast<std::size_t>(target);
            }
        } else {
            emit(TsqNoteKind::Unknown, status);
            ++out.stats.unknownBytes;
        }
    }
    release();
    return out;
}

std::vector<DecodedTrack> decodeSequence(std::span<const std::uint8_t> data) {
    std::vector<DecodedTrack> tracks;
    const auto sequence = parseTsq(data);
    if (!sequence) return tracks;
    tracks.reserve(sequence->tracks.size());
    for (std::size_t i = 0; i < sequence->tracks.size(); ++i) {
        const auto start = resolveEventStart(data, sequence->tracks[i].offset, sequence->headerBytes);
        tracks.push_back(decodeTrack(data, start, i));
    }
    return tracks;
}

std::int16_t clamp16(double value) {
    // Use symmetric saturation so diagnostics can report absolute peak <=32767.
    value = std::clamp(value, -32767.0, 32767.0);
    return static_cast<std::int16_t>(std::lrint(value));
}

}  // namespace

std::vector<TsqDumpNote> tsqDumpNotes(std::span<const std::uint8_t> tsqData) {
    std::vector<TsqDumpNote> notes;
    for (auto& track : decodeSequence(tsqData))
        notes.insert(notes.end(), track.events.begin(), track.events.end());
    return notes;
}

TsqDecodeStats tsqDecodeStats(std::span<const std::uint8_t> tsqData) {
    TsqDecodeStats stats;
    for (const auto& track : decodeSequence(tsqData)) {
        stats.consumedBytes += track.stats.consumedBytes;
        stats.unknownBytes += track.stats.unknownBytes;
    }
    return stats;
}

void seqRenderMusic(std::span<const std::uint8_t> tsqData,
                    std::span<const std::uint8_t> tvbData,
                    double seconds,
                    std::vector<std::int16_t>& stereoOut,
                    std::uint32_t sampleRate) {
    stereoOut.clear();
    if (!std::isfinite(seconds) || seconds <= 0.0 || sampleRate == 0) return;
    const double requestedFrames = seconds * sampleRate;
    if (requestedFrames > static_cast<double>(std::numeric_limits<std::size_t>::max() / 2)) return;
    const std::size_t frames = static_cast<std::size_t>(requestedFrames);
    stereoOut.assign(frames * 2, 0);
    if (frames == 0) return;

    std::vector<double> mix(frames * 2, 0.0);
    std::array<std::vector<std::int16_t>, kTvbProgramCount - 2> samples;
    std::array<bool, kTvbProgramCount - 2> decoded{};
    for (const auto& track : decodeSequence(tsqData)) {
        for (const Voice& voice : track.voices) {
            if (voice.program >= samples.size()) continue;
            if (!decoded[voice.program]) {
                decoded[voice.program] = true;
                tvbDecodeSample(tvbData, voice.program, samples[voice.program]);
            }
            const auto& pcm = samples[voice.program];
            if (pcm.empty()) continue;
            const std::size_t first = std::min(frames, static_cast<std::size_t>(
                static_cast<double>(voice.startTick) * sampleRate / kTsqTickHz));
            const std::size_t last = std::min(frames, static_cast<std::size_t>(
                static_cast<double>(voice.endTick) * sampleRate / kTsqTickHz));
            const double ratio = std::exp2((static_cast<double>(voice.key) + voice.fineSemitones) / 12.0);
            for (std::size_t frame = first; frame < last; ++frame) {
                const double position = static_cast<double>(frame - first) * ratio;
                const auto index = static_cast<std::size_t>(position);
                if (index >= pcm.size()) break;
                const double fraction = position - index;
                const double a = pcm[index];
                const double b = pcm[std::min(index + 1, pcm.size() - 1)];
                const double sample = (a + (b - a) * fraction) * voice.gain;
                mix[frame * 2] += sample * voice.left;
                mix[frame * 2 + 1] += sample * voice.right;
            }
        }
    }
    for (std::size_t i = 0; i < stereoOut.size(); ++i) stereoOut[i] = clamp16(mix[i]);
}

}  // namespace rt::native::assets
