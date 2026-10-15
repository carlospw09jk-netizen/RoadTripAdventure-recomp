// Host-file replacement for the sceMc save-slot operations.  It writes only
// the documented raw Adventure progress image; PS2 icon files are not needed.

#include "gameplay/memcard.h"

#include "gameplay/menu.h"

#include <fstream>
#include <iterator>
#include <system_error>
#include <vector>

namespace rt::native::gameplay {
namespace {

constexpr std::array<std::uint32_t, kLocationCount> kInitialUnvisitedDoors = {
    0u, 0x3DFFFu, 0x7FBFFu, 0x1FFFFFu, 0xFBFu,
    0xFu, 0xFFFFFu, 0x3FFFFu, 0x17Fu, 0x7FFFDFu,
};

bool isSlotIndex(int slot) { return slot == 0 || slot == 1; }

}  // namespace

MemoryCard::MemoryCard(std::filesystem::path baseDirectory)
    : baseDirectory_(std::move(baseDirectory)) {}

std::filesystem::path MemoryCard::slotPath(int slot) const {
    if (!isSlotIndex(slot)) return {};
    return baseDirectory_ / ("slot" + std::to_string(slot) + ".sav");
}

SaveSlotInfo MemoryCard::getInfo(int slot) const {
    const std::filesystem::path path = slotPath(slot);
    if (path.empty()) return {};

    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec) || ec) return {};
    const std::uintmax_t size = std::filesystem::file_size(path, ec);
    if (ec) return {};
    return {.present = true, .size = size, .valid = size == kSaveSize};
}

bool MemoryCard::loadSlot(int slot, SaveData& out) const {
    const SaveSlotInfo info = getInfo(slot);
    if (!info.present) return false;

    std::ifstream input(slotPath(slot), std::ios::binary);
    if (!input) return false;
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(input)), {});
    if (bytes.size() != kSaveSize) return false;

    SaveResult parsed = parseSave(bytes);
    if (!parsed.ok) return false;
    out = std::move(parsed.data);
    return true;
}

bool MemoryCard::saveSlot(int slot, const SaveData& save) const {
    const std::filesystem::path path = slotPath(slot);
    if (path.empty()) return false;

    std::error_code ec;
    std::filesystem::create_directories(baseDirectory_, ec);
    if (ec) return false;

    const std::vector<std::uint8_t> bytes = serializeSave(save);
    if (bytes.size() != kSaveSize) return false;
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) return false;
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(output);
}

bool MemoryCard::createSlot(int slot, const SaveData& fresh) const {
    return saveSlot(slot, fresh);
}

SaveData makeDefaultSave() {
    SaveData save;
    save.money = kMoneyNewGame;
    save.playerName = "PLAYER 1";
    save.unvisitedDoors = kInitialUnvisitedDoors;
    save.raceResults.fill(kResultNotRaced);
    save.bestLap.fill(kBestTimeNone);
    save.bestTotal.fill(kBestTimeNone);
    save.townsUnvisited = kTownsAllUnvisited;

    ControllerConfig controls;
    controls.resetDefaults();
    save.controllerRemap = controls.saveWords();
    return save;
}

}  // namespace rt::native::gameplay
