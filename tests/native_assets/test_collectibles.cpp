// Authored collectible pickup contract tests. Synthetic SaveData only - no game data.
#include "../../src/native/gameplay/collectibles.h"

#include <cstdio>
#include <vector>

using namespace rt::native::gameplay;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

namespace {

void testStampPickupUsesGroundPlaneRadius() {
    SaveData save;
    Collectible stamp{CollectibleKind::STAMP, {0.f, 0.f}, 5.f, 7};

    CHECK(tryCollect(stamp, {3.f, 0.f}, save));
    CHECK(stamp.collected);
    CHECK(hasStamp(save, 7));

    Collectible distantStamp{CollectibleKind::STAMP, {0.f, 0.f}, 5.f, 8};
    CHECK(!tryCollect(distantStamp, {10.f, 0.f}, save));
    CHECK(!distantStamp.collected);
    CHECK(!hasStamp(save, 8));
}

void testCoinPickupClearsMissingBit() {
    SaveData save;
    save.coinsMissing.fill(0xFF);
    Collectible coin{CollectibleKind::COIN, {0.f, 0.f}, 5.f, 9};

    CHECK(tryCollect(coin, {3.f, 0.f}, save));
    CHECK(coin.collected);
    CHECK(hasCoin(save, 9));
}

void testCollectedPickupDoesNotCollectTwice() {
    SaveData save;
    save.coinsMissing.fill(0xFF);
    Collectible coin{CollectibleKind::COIN, {0.f, 0.f}, 5.f, 9};

    CHECK(tryCollect(coin, {0.f, 0.f}, save));
    CHECK(!tryCollect(coin, {0.f, 0.f}, save));
    CHECK(coinsFound(save) == 1);
}

void testOutOfRangeIndexDoesNotCollect() {
    SaveData save;
    save.coinsMissing.fill(0xFF);
    Collectible invalidStamp{CollectibleKind::STAMP, {0.f, 0.f}, 5.f, 100};
    Collectible invalidCoin{CollectibleKind::COIN, {0.f, 0.f}, 5.f, -1};

    CHECK(!tryCollect(invalidStamp, {0.f, 0.f}, save));
    CHECK(!invalidStamp.collected);
    CHECK(!tryCollect(invalidCoin, {0.f, 0.f}, save));
    CHECK(!invalidCoin.collected);
    CHECK(coinsFound(save) == 0);
}

void testCollectedCount() {
    const std::vector<Collectible> collectibles{
        {CollectibleKind::STAMP, {}, 1.f, 0, true},
        {CollectibleKind::COIN, {}, 1.f, 1, false},
        {CollectibleKind::COIN, {}, 1.f, 2, true},
    };
    CHECK(collectedCount(collectibles) == 2);
}

}  // namespace

int main() {
    testStampPickupUsesGroundPlaneRadius();
    testCoinPickupClearsMissingBit();
    testCollectedPickupDoesNotCollectTwice();
    testOutOfRangeIndexDoesNotCollect();
    testCollectedCount();
    if (g_fail) { std::fprintf(stderr, "%d failure(s)\n", g_fail); return 1; }
    std::puts("collectibles: all checks passed");
    return 0;
}
