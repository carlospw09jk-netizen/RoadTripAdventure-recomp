// Contract tests for the bounded TSQ event decoder and offline PCM renderer.
#include "../../src/native/assets/tsq_sequencer.h"
#include "../../src/native/assets/music_seq.h"
#include "../../src/native/audio/vag.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace rt::native::assets;
namespace audio = rt::native::audio;

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
    for (std::size_t i = 4; i < kTvbProgramCount; ++i) put32(tvb, i * 4, 0x410);
    audio::VagNibbles block;
    block.shift = 8;
    block.filter = 0;
    for (std::size_t i = 0; i < 28; ++i) block.nibbles[i] = (i & 1) ? 7 : 9;
    audio::buildVagBlock(block, tvb.data() + 0x400);
    return tvb;
}

static std::vector<std::uint8_t> makeTsq() {
    std::vector<std::uint8_t> tsq(0x40, 0);
    put16(tsq, 4, 0x90); put16(tsq, 6, 0x10);
    put16(tsq, 8, 0x90); put16(tsq, 10, 0x10);
    put16(tsq, 12, 0x90); put16(tsq, 14, 0x10);
    // Centre, full volume, slot zero, key 0, wait one tick, release, end.
    const std::uint8_t events[] = {0xe1, 0x40, 0x40, 0xe0, 127, 0xe2, 0,
                                   0x80, 1, 0xf0, 0xff};
    std::copy(std::begin(events), std::end(events), tsq.begin() + 0x10);
    return tsq;
}

static void testSyntheticDumpAndRender() {
    const auto tsq = makeTsq();
    const auto notes = tsqDumpNotes(tsq);
    CHECK(notes.size() == 18);
    if (notes.size() >= 6) {
        CHECK(notes[0].kind == TsqNoteKind::Pan);
        CHECK(notes[1].kind == TsqNoteKind::Volume);
        CHECK(notes[2].kind == TsqNoteKind::Program);
        CHECK(notes[3].kind == TsqNoteKind::NoteOn && notes[3].key == 0);
        CHECK(notes[4].kind == TsqNoteKind::NoteOff && notes[4].tick == 1);
        CHECK(notes[5].kind == TsqNoteKind::End);
    }
    std::vector<std::int16_t> out;
    seqRenderMusic(tsq, makeTvb(), 0.1, out);
    CHECK(out.size() == 2 * 4410);
    const auto nonzero = std::count_if(out.begin(), out.end(), [](std::int16_t s) { return s != 0; });
    CHECK(nonzero > 0);
    CHECK(*std::max_element(out.begin(), out.end()) > 0);
}

static std::vector<std::uint8_t> readFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
static std::filesystem::path discRoot() {
    if (const char* root = std::getenv("RT_DISC")) return root;
    if (const char* home = std::getenv("HOME"))
        return std::filesystem::path(home) / "Library/Application Support/RoadTripRecomp/disc";
    return {};
}

static void testRealDiscWhenPresent() {
    const auto sound = discRoot() / "SOUND";
    if (!std::filesystem::is_directory(sound)) {
        std::puts("tsq sequencer: disc not present; integration check skipped");
        return;
    }
    const auto tvb = readFile(sound / "BGM.TVB");
    for (const char* name : {"BGM_01.TSQ", "ACTION.TSQ"}) {
        const auto tsq = readFile(sound / name);
        std::vector<std::int16_t> out;
        seqRenderMusic(tsq, tvb, 3.0, out);
        const auto notes = tsqDumpNotes(tsq);
        const auto stats = tsqDecodeStats(tsq);
        const auto noteOns = std::count_if(notes.begin(), notes.end(), [](const TsqDumpNote& n) {
            return n.kind == TsqNoteKind::NoteOn;
        });
        const auto nonzero = std::count_if(out.begin(), out.end(), [](std::int16_t s) { return s != 0; });
        int peak = 0;
        for (const auto sample : out) peak = std::max(peak, std::abs(static_cast<int>(sample)));
        std::printf("%s: note-ons=%zu unknown-bytes=%zu/%zu (%.2f%%) output=%zu nonzero=%zu peak=%d\n",
                    name, static_cast<std::size_t>(noteOns), stats.unknownBytes, stats.consumedBytes,
                    stats.consumedBytes ? 100.0 * stats.unknownBytes / stats.consumedBytes : 0.0,
                    out.size(), nonzero, peak);
        CHECK(out.size() == 2 * 3 * 44100);
        CHECK(static_cast<std::size_t>(nonzero) > out.size() / 100);
        CHECK(peak <= 32767);
        if (std::string(name) == "ACTION.TSQ")
            CHECK(stats.unknownBytes * 100 < stats.consumedBytes * 2);
    }
}

int main() {
    testSyntheticDumpAndRender();
    testRealDiscWhenPresent();
    if (g_fail) { std::fprintf(stderr, "%d failure(s)\n", g_fail); return 1; }
    std::puts("tsq sequencer: all checks passed");
    return 0;
}
