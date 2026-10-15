#pragma once

// Authored mutations for documented Adventure-progress save fields. Pure host
// C++; these functions mutate SaveData only and have no guest/game-data input.

#include "gameplay/save.h"

#include <cstddef>
#include <cstdint>

namespace rt::native::gameplay {

// Deducts money when affordable. Successful deductions are capped at kMoneyCap.
bool spendMoney(SaveData& save, std::uint32_t amount);

// Adds an unread mail ID to the first free inbox slot. mailInbox stores an ID
// with bit 0x80 set once read; a zero entry is a free slot.
bool receiveMail(SaveData& save, std::uint8_t mailId);

// Marks the selected mailbox entry read by setting its 0x80 convention bit.
bool markMailRead(SaveData& save, std::size_t index);

// The stampsEarned bitmap has 100 valid bits, indexed 0 through 99.
void collectStamp(SaveData& save, int stampIndex);
bool hasStamp(const SaveData& save, int stampIndex);

// The coinsMissing bitmap has 100 valid bits: a clear bit means found.
void collectCoin(SaveData& save, int coinIndex);
bool hasCoin(const SaveData& save, int coinIndex);
int coinsFound(const SaveData& save);

}  // namespace rt::native::gameplay
