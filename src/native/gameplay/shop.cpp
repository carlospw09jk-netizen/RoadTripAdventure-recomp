// Authored item-shop operations over SaveData money and host-owned stock.

#include "gameplay/shop.h"

#include "gameplay/progression.h"

#include <algorithm>
#include <limits>

namespace rt::native::gameplay {

bool Shop::buy(SaveData& save, std::size_t index) {
    if (index >= items.size() || items[index].quantity <= 0) return false;
    if (!spendMoney(save, items[index].price)) return false;
    --items[index].quantity;
    return true;
}

bool Shop::sell(SaveData& save, std::size_t index) {
    if (index >= items.size() || items[index].quantity == std::numeric_limits<int>::max()) return false;

    ShopItem& item = items[index];
    ++item.quantity;
    const std::uint32_t sellPrice = item.price / 2;
    save.money = std::min(kMoneyCap, save.money >= kMoneyCap ? kMoneyCap : save.money + sellPrice);
    return true;
}

void Shop::moveSelection(int delta) {
    if (items.empty()) return;

    const auto count = static_cast<long long>(items.size());
    const auto offset = static_cast<long long>(delta);
    const auto current = static_cast<long long>(selection % items.size());
    selection = static_cast<std::size_t>((current + offset % count + count) % count);
}

ShopItem* Shop::selected() {
    if (items.empty()) return nullptr;
    selection %= items.size();
    return &items[selection];
}

int Shop::countAffordable(const SaveData& save) const {
    return static_cast<int>(std::count_if(items.begin(), items.end(), [&save](const ShopItem& item) {
        return item.quantity > 0 && item.price <= save.money;
    }));
}

}  // namespace rt::native::gameplay
