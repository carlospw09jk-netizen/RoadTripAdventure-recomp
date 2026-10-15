#pragma once

// Bounded, provisional TSQ sequencer for diagnostics and offline PCM renders.
// This is deliberately separate from the device-facing audio engine.
//
// The shipped SNDMOD.IRX driver (see docs/native-reference/sndmod-irx.md)
// proves kTsqTickHz=60 (one tick per WaitVblankEnd at NTSC) and the pitch base:
// freqtbl[48]=4096, i.e. key 48 is the TVB sample's natural 44100-Hz pitch.
// E4/E5/EA values are signed LE 16-bit linear pitch multipliers, value / 4096
// (0 = disabled), matching the driver's freq*mult>>12 path.

#include <cstdint>
#include <span>
#include <vector>

namespace rt::native::assets {

inline constexpr double kTsqTickHz = 60.0;  // Proven: thread_sddr ticks per Vblank (NTSC).
inline constexpr int kTsqNaturalKey = 48;   // Proven: freqtbl[48] = 4096 = natural pitch.

enum class TsqNoteKind : std::uint8_t {
    NoteOn,
    NoteOff,
    Program,
    Volume,
    Pan,
    Pitch,
    Tempo,     // E3: u16 master tempo scale (IRX: tempo() consumes 2 bytes)
    Reverb,    // E6 mode / E7 depth / E8 on / E9 off
    Priority,  // F1: voice priority byte (IRX: pri_chg)
    ExtChan,   // F9: external channel alloc, u16 (IRX: ext_chan)
    Loop,
    End,
    Unknown,
};

struct TsqDumpNote {
    std::size_t trackIndex = 0;
    std::uint32_t tick = 0;
    TsqNoteKind kind = TsqNoteKind::Unknown;
    std::uint8_t key = 0;
    std::int32_t value = 0;
};

struct TsqDecodeStats {
    std::size_t consumedBytes = 0;
    std::size_t unknownBytes = 0;
};

// Decode high-confidence TSQ events into a diagnostic event list. Unknown
// bytes are represented individually as Unknown records and are skipped.
std::vector<TsqDumpNote> tsqDumpNotes(std::span<const std::uint8_t> tsqData);

// Accounting for the same bounded decode used by tsqDumpNotes. `consumedBytes`
// includes recognised status bytes and their operands, including loop visits.
TsqDecodeStats tsqDecodeStats(std::span<const std::uint8_t> tsqData);

// Render TSQ tracks through their selected TVB samples to interleaved stereo
// int16 PCM. No device output or shared engine state is involved. One voice is
// active per track; a new note releases the preceding one. F8 loop jumps are
// bounded per track so malformed streams cannot run indefinitely.
void seqRenderMusic(std::span<const std::uint8_t> tsqData,
                    std::span<const std::uint8_t> tvbData,
                    double seconds,
                    std::vector<std::int16_t>& stereoOut,
                    std::uint32_t sampleRate = 44100);

}  // namespace rt::native::assets
