// Contract tests for the bounded, looped TSQ BGM player.
#include "../../src/native/audio/bgm_player.h"
#include "../../src/native/audio/vag.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <vector>

using rt::native::audio::BgmPlayer;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static void put16(std::vector<std::uint8_t>& bytes, std::size_t at, std::uint16_t value) {
    bytes[at] = static_cast<std::uint8_t>(value);
    bytes[at + 1] = static_cast<std::uint8_t>(value >> 8);
}

static void put32(std::vector<std::uint8_t>& bytes, std::size_t at, std::uint32_t value) {
    for (unsigned i = 0; i != 4; ++i) bytes[at + i] = static_cast<std::uint8_t>(value >> (i * 8));
}

static std::vector<std::uint8_t> makeTvb() {
    std::vector<std::uint8_t> tvb(0x420, 0);
    put32(tvb, 2 * 4, 0x400);
    put32(tvb, 3 * 4, 0x410);
    for (std::size_t i = 4; i < 256; ++i) put32(tvb, i * 4, 0x410);
    rt::native::audio::VagNibbles block;
    block.shift = 8;
    for (std::size_t i = 0; i < 28; ++i) block.nibbles[i] = (i & 1) ? 7 : 9;
    rt::native::audio::buildVagBlock(block, tvb.data() + 0x400);
    return tvb;
}

static std::vector<std::uint8_t> makeTsq() {
    std::vector<std::uint8_t> tsq(0x40, 0);
    put16(tsq, 4, 0x90); put16(tsq, 6, 0x10);
    put16(tsq, 8, 0x90); put16(tsq, 10, 0x10);
    put16(tsq, 12, 0x90); put16(tsq, 14, 0x10);
    const std::uint8_t events[] = {0xe1, 0x40, 0x40, 0xe0, 127, 0xe2, 0,
                                   0x80, 60, 0xf0, 0xff};
    std::copy(std::begin(events), std::end(events), tsq.begin() + 0x10);
    return tsq;
}

static std::filesystem::path discRoot() {
    if (const char* root = std::getenv("RT_DISC")) return root;
    if (const char* home = std::getenv("HOME"))
        return std::filesystem::path(home) / "Library/Application Support/RoadTripRecomp/disc";
    return {};
}

static void testSyntheticLoadAndPump() {
    BgmPlayer player;
    CHECK(player.load(makeTvb(), makeTsq()));
    CHECK(player.ready());
    std::vector<std::int16_t> pcm(2 * 44100);
    player.pump(pcm.data(), 44100);
    const auto peak = std::max_element(pcm.begin(), pcm.end(), [](std::int16_t a, std::int16_t b) {
        return std::abs(static_cast<int>(a)) < std::abs(static_cast<int>(b));
    });
    CHECK(std::all_of(pcm.begin(), pcm.end(), [](std::int16_t sample) {
        return std::isfinite(static_cast<double>(sample));
    }));
    CHECK(peak != pcm.end());
    CHECK(peak != pcm.end() && std::abs(static_cast<int>(*peak)) > 0);
    CHECK(peak != pcm.end() && std::abs(static_cast<int>(*peak)) <= 32767);
}

static void testRealDiscWhenPresent() {
    const auto root = discRoot();
    if (!std::filesystem::is_regular_file(root / "SOUND/BGM.TVB") ||
        !std::filesystem::is_regular_file(root / "SOUND/ACTION.TSQ")) {
        std::puts("bgm player: disc not present; integration check skipped");
        return;
    }
    BgmPlayer player;
    CHECK(player.load(root, "ACTION"));
    std::vector<std::int16_t> pcm(2 * 44100);
    player.pump(pcm.data(), 44100);
    const auto nonzero = std::count_if(pcm.begin(), pcm.end(), [](std::int16_t sample) { return sample != 0; });
    int peak = 0;
    for (const auto sample : pcm) {
        CHECK(std::isfinite(static_cast<double>(sample)));
        peak = std::max(peak, std::abs(static_cast<int>(sample)));
    }
    CHECK(static_cast<std::size_t>(nonzero) > pcm.size() / 100);
    CHECK(peak <= 32767);
}

int main() {
    testSyntheticLoadAndPump();
    testRealDiscWhenPresent();
    if (g_fail) { std::fprintf(stderr, "%d failure(s)\n", g_fail); return 1; }
    std::puts("bgm player: all checks passed");
    return 0;
}
