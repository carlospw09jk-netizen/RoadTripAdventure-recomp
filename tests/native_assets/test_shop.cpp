// Authored shop model contract tests. Synthetic SaveData only - no game data.
#include "../../src/native/gameplay/shop.h"

#include <cstdio>

using namespace rt::native::gameplay;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

namespace {

Shop makeShop() {
    return Shop{{
        {1, "Oil", 100, 2},
        {2, "Tire", 250, 0},
        {3, "Horn", 500, 1},
    }};
}

void testBuyDeductsPriceAndStock() {
    SaveData save;
    save.money = 350;
    Shop shop = makeShop();

    CHECK(shop.buy(save, 0));
    CHECK(save.money == 250);
    CHECK(shop.items[0].quantity == 1);
}

void testBuyRejectsUnavailableOrUnaffordableItems() {
    SaveData save;
    Shop shop = makeShop();

    save.money = 1000;
    CHECK(!shop.buy(save, 1));
    CHECK(save.money == 1000);
    CHECK(shop.items[1].quantity == 0);

    save.money = 499;
    CHECK(!shop.buy(save, 2));
    CHECK(save.money == 499);
    CHECK(shop.items[2].quantity == 1);
}

void testSellCreditsHalfPriceAndClampsMoney() {
    SaveData save;
    Shop shop = makeShop();

    save.money = 100;
    CHECK(shop.sell(save, 0));
    CHECK(save.money == 150);
    CHECK(shop.items[0].quantity == 3);

    save.money = kMoneyCap - 10;
    CHECK(shop.sell(save, 2));
    CHECK(save.money == kMoneyCap);
}

void testMoveSelectionWraps() {
    Shop shop = makeShop();

    CHECK(shop.selected()->id == 1);
    shop.moveSelection(-1);
    CHECK(shop.selected()->id == 3);
    shop.moveSelection(2);
    CHECK(shop.selected()->id == 2);
}

void testCountAffordableIncludesOnlyStockedItems() {
    Shop shop = makeShop();
    SaveData save;

    save.money = 100;
    CHECK(shop.countAffordable(save) == 1);
    save.money = 500;
    CHECK(shop.countAffordable(save) == 2);
}

}  // namespace

int main() {
    testBuyDeductsPriceAndStock();
    testBuyRejectsUnavailableOrUnaffordableItems();
    testSellCreditsHalfPriceAndClampsMoney();
    testMoveSelectionWraps();
    testCountAffordableIncludesOnlyStockedItems();
    if (g_fail) { std::fprintf(stderr, "%d failure(s)\n", g_fail); return 1; }
    std::puts("shop: all checks passed");
    return 0;
}
