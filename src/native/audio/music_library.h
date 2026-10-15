// Injected BGM file-table resolver. The original's gs_music_file_table holds
// 12-byte file records indexed by packed BGM id bits 16..23; this host-side
// replacement accepts the corresponding extracted-disc paths at startup.
#pragma once

#include "audio/vag_loader.h"

#include <cstdint>
#include <deque>
#include <filesystem>
#include <map>

namespace rt::native::audio {

class SoundEngine;

class MusicLibrary {
public:
    // Register or replace the extracted VAG path for one original file-table
    // index. Paths are deliberately injected rather than hardcoded game data.
    void addTrack(std::uint8_t fileIndex, std::filesystem::path vagPath);

    // Return the registered VAG path for `fileIndex`, if any.
    bool resolve(std::uint8_t fileIndex, std::filesystem::path& out) const;

    // Decode and start the registered VAG selected by packedId. BGM slot is
    // `fileIndex % kBgmSlots`, giving the original indexed files a stable slot
    // in this four-slot host engine. The complete packed id is forwarded to
    // SoundEngine, preserving its param and low-id fields for that boundary.
    // Decoded buffers remain owned here for the playback lifetime.
    bool playBgm(SoundEngine& engine, std::uint32_t packedId, float volume = 1.f);

private:
    std::map<std::uint8_t, std::filesystem::path> tracks_;
    std::deque<PcmBuffer> loadedTracks_;
};

}  // namespace rt::native::audio
