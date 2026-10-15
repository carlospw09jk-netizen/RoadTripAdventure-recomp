#include "bgm_player.h"

#include "assets/tsq_sequencer.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <string>

namespace rt::native::audio {
namespace {

constexpr std::uint32_t kSampleRate = 44100;
constexpr double kMaxPreRenderSeconds = 60.0;
constexpr std::size_t kStereoChannels = 2;

std::vector<std::uint8_t> readFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return {};
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::filesystem::path tsqPath(const std::filesystem::path& discDir,
                              std::string_view bgmName) {
    std::filesystem::path name{bgmName};
    if (name.is_relative()) {
        std::string normalized = name.string();
        for (char& character : normalized)
            character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
        name = normalized;
        if (name.extension().empty()) name += ".TSQ";
        const auto soundPath = discDir / "SOUND" / name;
        if (std::filesystem::is_regular_file(soundPath)) return soundPath;
        return discDir / name;
    }
    return name;
}

}  // namespace

bool BgmPlayer::load(const std::filesystem::path& discDir, std::string_view bgmName) {
    const std::filesystem::path requested{bgmName};
    if (!requested.extension().empty() && requested.extension() != ".TSQ" &&
        requested.extension() != ".tsq") return false;
    const auto tvbPath = std::filesystem::is_regular_file(discDir / "SOUND/BGM.TVB")
        ? discDir / "SOUND/BGM.TVB" : discDir / "BGM.TVB";
    return load(readFile(tvbPath), readFile(tsqPath(discDir, bgmName)));
}

bool BgmPlayer::load(std::span<const std::uint8_t> tvbData,
                     std::span<const std::uint8_t> tsqData) {
    pcm_.clear();
    cursorFrames_ = 0;
    if (tvbData.empty() || tsqData.empty()) return false;

    assets::seqRenderMusic(tsqData, tvbData, kMaxPreRenderSeconds, pcm_, kSampleRate);
    if (pcm_.empty() || pcm_.size() % kStereoChannels != 0) {
        pcm_.clear();
        return false;
    }
    return true;
}

bool BgmPlayer::ready() const noexcept {
    return !pcm_.empty();
}

void BgmPlayer::pump(std::int16_t* out, std::size_t frames) {
    if (!out || frames == 0) return;
    if (!ready()) {
        std::fill_n(out, frames * kStereoChannels, std::int16_t{0});
        return;
    }

    const std::size_t totalFrames = pcm_.size() / kStereoChannels;
    std::size_t written = 0;
    while (written < frames) {
        const std::size_t copiedFrames = std::min(frames - written, totalFrames - cursorFrames_);
        const auto* source = pcm_.data() + cursorFrames_ * kStereoChannels;
        auto* destination = out + written * kStereoChannels;
        std::copy_n(source, copiedFrames * kStereoChannels, destination);
        written += copiedFrames;
        cursorFrames_ = (cursorFrames_ + copiedFrames) % totalFrames;
    }
}

}  // namespace rt::native::audio
