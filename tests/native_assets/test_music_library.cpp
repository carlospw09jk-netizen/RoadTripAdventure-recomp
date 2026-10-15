// Unit tests for src/native/audio/music_library.*. The resolver maps the
// documented BGM file-table index to injected VAG paths; fixtures are
// synthetic and no game data is used.
#include "../../src/native/audio/engine.h"
#include "../../src/native/audio/music_library.h"
#include "../../src/native/audio/vag.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <vector>

using namespace rt::native::audio;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static std::filesystem::path writeSyntheticVag() {
    VagNibbles n;
    n.shift = 12;
    n.filter = 0;
    for (std::size_t i = 0; i < 28; ++i) n.nibbles[i] = static_cast<std::uint8_t>(i & 0x0f);

    std::uint8_t block[kVagBlockBytes];
    buildVagBlock(n, block);
    std::uint8_t terminator[kVagBlockBytes] = {};
    terminator[1] = kVagEndOfStream;

    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "rt_native_music_library_test.vag";
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(block), sizeof(block));
    file.write(reinterpret_cast<const char*>(terminator), sizeof(terminator));
    return path;
}

// Removing the resolver lookup, using the wrong BGM id byte, or failing to
// retain decoded PCM while the engine plays it makes this test fail.
static void testResolvesAndPlaysRegisteredTrack() {
    const std::filesystem::path vagPath = writeSyntheticVag();
    MusicLibrary music;
    music.addTrack(0x1c, vagPath);

    std::filesystem::path resolved;
    CHECK(music.resolve(0x1c, resolved));
    CHECK(resolved == vagPath);
    CHECK(!music.resolve(0x00, resolved));

    SoundEngine engine;
    CHECK(music.playBgm(engine, 0x0a1c0201u));
    CHECK(engine.activeVoiceCount() > 0);

    std::error_code removeError;
    std::filesystem::remove(vagPath, removeError);
    CHECK(!removeError);
}

// Removing the unknown-index guard would replace a BGM voice or claim success.
static void testUnknownTrackDoesNotChangeEngine() {
    MusicLibrary music;
    SoundEngine engine;
    CHECK(!music.playBgm(engine, 0x0a1c0201u));
    CHECK(engine.activeVoiceCount() == 0);
}

int main() {
    testResolvesAndPlaysRegisteredTrack();
    testUnknownTrackDoesNotChangeEngine();
    if (g_fail) { std::fprintf(stderr, "%d failure(s)\n", g_fail); return 1; }
    std::puts("music library: all checks passed");
    return 0;
}
