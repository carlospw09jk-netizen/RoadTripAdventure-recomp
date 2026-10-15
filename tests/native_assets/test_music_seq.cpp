// Contract tests for Road Trip's TVB voice-bank and TSQ sequence containers.
// Synthetic fixtures cover wire validation; a present local disc adds a
// read-only integration check without putting game data in the repository.
#include "../../src/native/assets/music_seq.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace rt::native::assets;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static void put16(std::vector<std::uint8_t>& bytes, std::size_t at, std::uint16_t value) {
    bytes[at] = static_cast<std::uint8_t>(value);
    bytes[at + 1] = static_cast<std::uint8_t>(value >> 8);
}
static void put32(std::vector<std::uint8_t>& bytes, std::size_t at, std::uint32_t value) {
    for (unsigned i = 0; i != 4; ++i) bytes[at + i] = static_cast<std::uint8_t>(value >> (i * 8));
}

// Removing the fixed 256-entry table or its monotonic/alignment checks lets a
// malformed voice bank masquerade as a sample pool.
static void testParsesTvbSampleOffsets() {
    std::vector<std::uint8_t> bytes(0x500, 0);
    put32(bytes, 2 * 4, 0x400);
    put32(bytes, 3 * 4, 0x420);
    put32(bytes, 4 * 4, 0x420);
    put32(bytes, 5 * 4, 0x440);
    for (std::size_t i = 6; i < kTvbProgramCount; ++i) put32(bytes, i * 4, 0x440);
    const auto parsed = parseTvb(bytes);
    CHECK(parsed);
    CHECK(parsed->programs.size() == kTvbProgramCount);
    CHECK(parsed->samplePoolOffset == 0x400);
    CHECK(parsed->samplePoolEnd == 0x440);
    CHECK(parsed->programs[2].sampleOffset == 0x400);
    CHECK(parsed->programs[3].sampleOffset == 0x420);
    CHECK(!parsed->programs[0].hasSample());

    put32(bytes, 4 * 4, 0x111);
    CHECK(!parseTvb(bytes));
}

// Removing the pointer-table boundary check would accept an offset outside the
// file, or would let a raw span cover header bytes.
static void testParsesTsqTrackSpans() {
    std::vector<std::uint8_t> bytes(0x50, 0);
    // Entries are IOP-style {segment, file-offset} word pairs.  Data begins
    // at the lowest target, leaving three table entries at byte 4.
    put16(bytes, 4, 0x90); put16(bytes, 6, 0x10);
    put16(bytes, 8, 0x90); put16(bytes, 10, 0x30);
    put16(bytes, 12, 0x90); put16(bytes, 14, 0x10);
    bytes[0x10] = 0xf0; bytes[0x11] = 0xff;
    bytes[0x30] = 0xe9; bytes[0x31] = 0xf0;
    const auto parsed = parseTsq(bytes);
    CHECK(parsed);
    CHECK(parsed->headerBytes == 0x10);
    CHECK(parsed->tracks.size() == 3);
    CHECK(parsed->tracks[0].offset == 0x10);
    CHECK(parsed->events[parsed->tracks[0].eventIndex].bytes.size() == 0x20);
    CHECK(parsed->tracks[1].offset == 0x30);
    CHECK(parsed->events[parsed->tracks[1].eventIndex].bytes.size() == 0x20);
    CHECK(parsed->events[parsed->tracks[2].eventIndex].bytes.size() == 0x20);

    put16(bytes, 10, 0x51);
    CHECK(!parseTsq(bytes));
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
        std::puts("music sequence: disc not present; integration check skipped");
        return;
    }
    const auto tvb = parseTvb(readFile(sound / "BGM.TVB"));
    CHECK(tvb);
    if (tvb) std::printf("BGM.TVB: programs=%zu pool=[0x%x,0x%x)\n",
                         tvb->programs.size(), tvb->samplePoolOffset, tvb->samplePoolEnd);

    for (const auto& entry : std::filesystem::directory_iterator(sound)) {
        if (entry.path().extension() != ".TSQ") continue;
        const std::string name = entry.path().filename().string();
        const auto sequence = parseTsq(readFile(entry.path()));
        CHECK(sequence);
        if (!sequence) continue;
        CHECK(!sequence->tracks.empty());
        std::size_t eventBytes = 0;
        for (const auto& event : sequence->events) eventBytes += event.bytes.size();
        CHECK(eventBytes > 0);
        std::printf("%s: tracks=%zu raw-event-bytes=%zu", name.c_str(), sequence->tracks.size(), eventBytes);
        for (std::size_t i = 0; i < sequence->tracks.size() && i < 3; ++i) {
            const auto& track = sequence->tracks[i];
            const auto& event = sequence->events[track.eventIndex];
            std::printf(" track%zu@0x%x=%02x%02x", i, track.offset,
                        event.bytes.empty() ? 0 : event.bytes[0],
                        event.bytes.size() < 2 ? 0 : event.bytes[1]);
        }
        std::puts("");
    }
}

int main() {
    testParsesTvbSampleOffsets();
    testParsesTsqTrackSpans();
    testRealDiscWhenPresent();
    if (g_fail) { std::fprintf(stderr, "%d failure(s)\n", g_fail); return 1; }
    std::puts("music sequence: all checks passed");
    return 0;
}
