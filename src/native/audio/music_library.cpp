#include "audio/music_library.h"

#include "audio/engine.h"
#include "audio/vag_loader.h"

namespace rt::native::audio {

void MusicLibrary::addTrack(std::uint8_t fileIndex, std::filesystem::path vagPath) {
    tracks_[fileIndex] = std::move(vagPath);
}

bool MusicLibrary::resolve(std::uint8_t fileIndex, std::filesystem::path& out) const {
    const auto track = tracks_.find(fileIndex);
    if (track == tracks_.end()) return false;
    out = track->second;
    return true;
}

bool MusicLibrary::playBgm(SoundEngine& engine, std::uint32_t packedId, float volume) {
    const BgmIdParts id = unpackBgmId(packedId);
    std::filesystem::path vagPath;
    const auto fileIndex = static_cast<std::uint8_t>(id.fileIndex);
    if (!resolve(fileIndex, vagPath)) return false;

    PcmBuffer pcm;
    if (!loadVagFile(vagPath, pcm)) return false;

    loadedTracks_.push_back(std::move(pcm));
    const PcmBuffer& source = loadedTracks_.back();
    const int slot = static_cast<int>(fileIndex % kBgmSlots);
    engine.playBgm(slot, packedId,
                   PcmSourceView{source.samples.data(), source.frames, source.loop}, volume);
    return true;
}

}  // namespace rt::native::audio
