// Bounded TSQ BGM playback source. It renders an extracted TSQ/TVB pair once
// into a fixed-duration stereo PCM buffer, then loops that buffer in pump().
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string_view>
#include <vector>

namespace rt::native::audio {

class BgmPlayer {
public:
    // `discDir` is the extracted-disc root containing SOUND/. `bgmName` may be
    // a bare name (ACTION, BGM_03) or a .TSQ file name.
    bool load(const std::filesystem::path& discDir, std::string_view bgmName);

    // In-memory entry point used by host-side tests and tools. The supplied
    // buffers are consumed during load; no game data is retained by the player.
    bool load(std::span<const std::uint8_t> tvbData,
              std::span<const std::uint8_t> tsqData);

    bool ready() const noexcept;

    // Fill `frames` interleaved stereo frames, wrapping at the end of the
    // bounded pre-render. A non-ready player writes silence.
    void pump(std::int16_t* out, std::size_t frames);

private:
    std::vector<std::int16_t> pcm_;
    std::size_t cursorFrames_ = 0;
};

}  // namespace rt::native::audio
