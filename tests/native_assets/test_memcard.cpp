// Host-file memory-card contract tests. Synthetic saves and temporary files only.
#include "../../src/native/gameplay/memcard.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace rt::native::gameplay;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

namespace {

class TempDirectory {
public:
    TempDirectory() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path = std::filesystem::temp_directory_path() / ("rt-native-memcard-" + std::to_string(stamp));
        std::filesystem::create_directories(path);
    }
    ~TempDirectory() { std::error_code ec; std::filesystem::remove_all(path, ec); }

    std::filesystem::path path;
};

void writeShortFile(const std::filesystem::path& path) {
    std::ofstream output(path, std::ios::binary);
    output.write("short", 5);
}

void testDefaultSave() {
    const SaveData save = makeDefaultSave();
    const std::array<std::uint16_t, kControllerActionCount> expected = {
        0x0040, 0x0080, 0x0020, 0x0002, 0x0004,
        0x0001, 0x0008, 0x0010, 0x0008, 0x0100,
    };
    const std::array<std::uint32_t, kLocationCount> expectedDoors = {
        0u, 0x3DFFFu, 0x7FBFFu, 0x1FFFFFu, 0xFBFu,
        0xFu, 0xFFFFFu, 0x3FFFFu, 0x17Fu, 0x7FFFDFu,
    };
    CHECK(save.money == kMoneyNewGame);
    CHECK(save.playerName == "PLAYER 1");
    CHECK(save.controllerRemap == expected);
    CHECK(save.unvisitedDoors == expectedDoors);
    CHECK(save.raceResults[0] == kResultNotRaced && save.raceResults.back() == kResultNotRaced);
    CHECK(save.bestLap[0] == kBestTimeNone && save.bestTotal.back() == kBestTimeNone);
    CHECK(save.townsUnvisited == kTownsAllUnvisited);
}

void testCreateInfoAndRoundTrip() {
    TempDirectory temp;
    MemoryCard card(temp.path);
    CHECK(!card.getInfo(0).present);

    SaveData original = makeDefaultSave();
    original.money = 123456;
    original.raw[0x0010] = 0xA5;  // unknown bytes must survive save/load.
    CHECK(card.createSlot(0, original));
    const SaveSlotInfo info = card.getInfo(0);
    CHECK(info.present && info.valid && info.size == kSaveSize);

    SaveData loaded;
    CHECK(card.loadSlot(0, loaded));
    CHECK(serializeSave(loaded) == serializeSave(original));
}

void testMissingShortAndCorruptFilesRejected() {
    TempDirectory temp;
    MemoryCard card(temp.path);
    SaveData out;
    CHECK(!card.loadSlot(1, out));
    CHECK(!card.getInfo(1).present);

    writeShortFile(temp.path / "slot1.sav");
    const SaveSlotInfo shortInfo = card.getInfo(1);
    CHECK(shortInfo.present && !shortInfo.valid && shortInfo.size == 5u);
    CHECK(!card.loadSlot(1, out));

    std::ofstream corrupt(temp.path / "slot0.sav", std::ios::binary);
    std::vector<std::uint8_t> oversized(kSaveSize + 1, 0);
    corrupt.write(reinterpret_cast<const char*>(oversized.data()), static_cast<std::streamsize>(oversized.size()));
    corrupt.close();
    CHECK(card.getInfo(0).present && !card.getInfo(0).valid);
    CHECK(!card.loadSlot(0, out));
}

void testSlotsAreIndependent() {
    TempDirectory temp;
    MemoryCard card(temp.path);
    SaveData zero = makeDefaultSave();
    SaveData one = makeDefaultSave();
    zero.money = 111;
    one.money = 222;
    CHECK(card.saveSlot(0, zero));
    CHECK(card.saveSlot(1, one));

    SaveData loadedZero;
    SaveData loadedOne;
    CHECK(card.loadSlot(0, loadedZero));
    CHECK(card.loadSlot(1, loadedOne));
    CHECK(loadedZero.money == 111u);
    CHECK(loadedOne.money == 222u);
    CHECK(!card.saveSlot(-1, zero));
    CHECK(!card.loadSlot(2, loadedZero));
}

}  // namespace

int main() {
    testDefaultSave();
    testCreateInfoAndRoundTrip();
    testMissingShortAndCorruptFilesRejected();
    testSlotsAreIndependent();
    if (g_fail != 0) return EXIT_FAILURE;
    std::puts("memcard: all checks passed");
    return EXIT_SUCCESS;
}
