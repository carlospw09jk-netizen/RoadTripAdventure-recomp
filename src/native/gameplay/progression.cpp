// Authored progression mutations over the documented save bitmaps and counters.

#include "gameplay/progression.h"

#include <algorithm>
#include <bit>

namespace rt::native::gameplay {
namespace {

constexpr int kCollectibleCount = 100;
constexpr std::uint8_t kMailReadBit = 0x80;

bool validCollectibleIndex(int index) {
    return index >= 0 && index < kCollectibleCount;
}

}  // namespace

bool spendMoney(SaveData& save, std::uint32_t amount) {
    if (amount > save.money) return false;
    save.money = std::min(save.money - amount, kMoneyCap);
    return true;
}

bool receiveMail(SaveData& save, std::uint8_t mailId) {
    if (save.mailCount >= save.mailInbox.size()) return false;
    for (std::uint8_t& entry : save.mailInbox) {
        if (entry == 0) {
            entry = mailId;
            ++save.mailCount;
            return true;
        }
    }
    return false;
}

bool markMailRead(SaveData& save, std::size_t index) {
    if (index >= save.mailInbox.size()) return false;
    save.mailInbox[index] |= kMailReadBit;
    return true;
}

void collectStamp(SaveData& save, int stampIndex) {
    if (!validCollectibleIndex(stampIndex)) return;
    const std::size_t word = static_cast<std::size_t>(stampIndex) / 64;
    const unsigned bit = static_cast<unsigned>(stampIndex) % 64;
    save.stampsEarned[word] |= std::uint64_t{1} << bit;
}

bool hasStamp(const SaveData& save, int stampIndex) {
    if (!validCollectibleIndex(stampIndex)) return false;
    const std::size_t word = static_cast<std::size_t>(stampIndex) / 64;
    const unsigned bit = static_cast<unsigned>(stampIndex) % 64;
    return (save.stampsEarned[word] & (std::uint64_t{1} << bit)) != 0;
}

void collectCoin(SaveData& save, int coinIndex) {
    if (!validCollectibleIndex(coinIndex)) return;
    const std::size_t byte = static_cast<std::size_t>(coinIndex) / 8;
    const unsigned bit = static_cast<unsigned>(coinIndex) % 8;
    save.coinsMissing[byte] &= static_cast<std::uint8_t>(~(std::uint8_t{1} << bit));
}

bool hasCoin(const SaveData& save, int coinIndex) {
    if (!validCollectibleIndex(coinIndex)) return false;
    const std::size_t byte = static_cast<std::size_t>(coinIndex) / 8;
    const unsigned bit = static_cast<unsigned>(coinIndex) % 8;
    return (save.coinsMissing[byte] & (std::uint8_t{1} << bit)) == 0;
}

int coinsFound(const SaveData& save) {
    int missing = 0;
    for (std::size_t byte = 0; byte < 12; ++byte)
        missing += std::popcount(save.coinsMissing[byte]);
    missing += std::popcount(static_cast<unsigned>(save.coinsMissing[12] & 0x0F));
    return kCollectibleCount - missing;
}

}  // namespace rt::native::gameplay
