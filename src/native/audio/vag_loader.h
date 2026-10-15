// Host-side VAG file loading: container/raw-stream selection plus PCM storage.
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace rt::native::audio {

// Owns a decoded mono PCM stream. `frames` is the count of mono int16 frames
// in `samples`; VAG BGM streams are loopable by default when fed to the engine.
struct PcmBuffer {
    std::vector<std::int16_t> samples;
    std::size_t frames = 0;
    bool loop = true;
};

// Decode an in-memory VAGp container or raw 16-byte-block VAG stream. VAGp
// streams use the header's bounded body extent; raw streams begin at byte zero.
// Returns false and clears `out` if there is no complete, decodable audio block.
bool loadVagToPcm(const std::vector<std::uint8_t>& vagData, PcmBuffer& out);

// Read and decode a VAGp container or raw VAG stream from disk.
bool loadVagFile(const std::filesystem::path& path, PcmBuffer& out);

}  // namespace rt::native::audio
