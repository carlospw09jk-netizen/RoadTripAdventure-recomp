#pragma once

// Authored item-shop state. This is a host-side model of the documented shop
// flow and does not read guest item tables or other game data.

#include "gameplay/save.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace rt::native::gameplay {

struct ShopItem {
    int id;
    std::string name;
    std::uint32_t price;
    int quantity;
};

class Shop {
public:
    std::vector<ShopItem> items;
    std::size_t selection = 0;

    // Buys one in-stock item when spendMoney() accepts its full listed price.
    bool buy(SaveData& save, std::size_t index);

    // Sells one catalog item back to the shop for price / 2, truncating odd
    // prices. The shop stock rises by one and money is capped at kMoneyCap.
    bool sell(SaveData& save, std::size_t index);

    // Moves the cursor by delta, wrapping within non-empty store stock.
    void moveSelection(int delta);
    ShopItem* selected();

    // Counts in-stock items whose listed price does not exceed save.money.
    int countAffordable(const SaveData& save) const;
};

}  // namespace rt::native::gameplay
