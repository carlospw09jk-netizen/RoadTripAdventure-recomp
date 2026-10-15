#pragma once
// Host-file replacement for the two PS2 memory-card Adventure save slots.
// Each slot is one raw 0x3448-byte SaveData image; icon.sys and choroQ.ico are
// intentionally not represented by the native app.

#include "gameplay/save.h"

#include <cstdint>
#include <filesystem>

namespace rt::native::gameplay {

struct SaveSlotInfo {
    bool present = false;
    std::uintmax_t size = 0;
    bool valid = false;  // Present and exactly kSaveSize bytes.
};

class MemoryCard {
public:
    explicit MemoryCard(std::filesystem::path baseDirectory);

    [[nodiscard]] SaveSlotInfo getInfo(int slot) const;
    bool loadSlot(int slot, SaveData& out) const;
    bool saveSlot(int slot, const SaveData& save) const;
    bool createSlot(int slot, const SaveData& fresh) const;

private:
    [[nodiscard]] std::filesystem::path slotPath(int slot) const;

    std::filesystem::path baseDirectory_;
};

// InitSaveSlotDefaults 0x2277B0 for the documented SaveData fields.
[[nodiscard]] SaveData makeDefaultSave();

}  // namespace rt::native::gameplay
