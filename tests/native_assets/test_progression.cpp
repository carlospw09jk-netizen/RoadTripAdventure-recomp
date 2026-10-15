// Authored progression mutation contract tests. Synthetic SaveData only - no game data.
#include "../../src/native/gameplay/progression.h"

#include <cstdio>

using namespace rt::native::gameplay;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

namespace {

void testSpendMoney() {
    SaveData save;
    save.money = 500;
    CHECK(spendMoney(save, 200));
    CHECK(save.money == 300);

    save.money = 500;
    CHECK(!spendMoney(save, 1000));
    CHECK(save.money == 500);

    save.money = 2000000;
    CHECK(spendMoney(save, 1));
    CHECK(save.money == kMoneyCap);
}

void testReceiveMail() {
    SaveData save;
    CHECK(receiveMail(save, 0x15));
    CHECK(save.mailInbox[0] == 0x15);
    CHECK(save.mailCount == 1);

    save.mailInbox.fill(1);
    save.mailCount = 128;
    CHECK(!receiveMail(save, 0x16));
    CHECK(save.mailCount == 128);
}

void testMarkMailRead() {
    SaveData save;
    save.mailInbox[0] = 0x15;
    save.mailInbox[1] = 0x22;
    CHECK(markMailRead(save, 1));
    CHECK(save.mailInbox[0] == 0x15);
    CHECK(save.mailInbox[1] == 0xA2);
    CHECK(!markMailRead(save, save.mailInbox.size()));
}

void testStamps() {
    SaveData save;
    collectStamp(save, 0);
    collectStamp(save, 99);
    CHECK(hasStamp(save, 0));
    CHECK(hasStamp(save, 99));
    CHECK(!hasStamp(save, 1));
    collectStamp(save, -1);
    collectStamp(save, 100);
    CHECK(!hasStamp(save, -1));
    CHECK(!hasStamp(save, 100));
}

void testCoins() {
    SaveData save;
    save.coinsMissing.fill(0xFF);
    save.coinsMissing[12] = 0x0F;  // Only bits 0..99 are meaningful.
    collectCoin(save, 0);
    collectCoin(save, 99);
    CHECK(hasCoin(save, 0));
    CHECK(hasCoin(save, 99));
    CHECK(!hasCoin(save, 1));
    CHECK(coinsFound(save) == 2);
    collectCoin(save, -1);
    collectCoin(save, 100);
    CHECK(coinsFound(save) == 2);
}

}  // namespace

int main() {
    testSpendMoney();
    testReceiveMail();
    testMarkMailRead();
    testStamps();
    testCoins();
    if (g_fail) { std::fprintf(stderr, "%d failure(s)\n", g_fail); return 1; }
    std::puts("progression: all checks passed");
    return 0;
}
