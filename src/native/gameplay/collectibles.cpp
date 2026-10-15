// Authored stamp and coin pickup model. Pure host C++; no guest/game-data input.

#include "gameplay/collectibles.h"

namespace rt::native::gameplay {
namespace {

constexpr int kCollectibleCount = 100;

bool validCollectibleIndex(int index) {
    return index >= 0 && index < kCollectibleCount;
}

}  // namespace

bool tryCollect(Collectible& collectible, Vec2 carPos, SaveData& save) {
    if (collectible.collected || !validCollectibleIndex(collectible.index) ||
        collectible.radius < 0.f) {
        return false;
    }

    const Vec2 offset = carPos - collectible.pos;
    if (lengthSq(offset) > collectible.radius * collectible.radius) return false;

    collectible.collected = true;
    if (collectible.kind == CollectibleKind::STAMP)
        collectStamp(save, collectible.index);
    else
        collectCoin(save, collectible.index);
    return true;
}

int collectedCount(const std::vector<Collectible>& collectibles) {
    int count = 0;
    for (const Collectible& collectible : collectibles)
        if (collectible.collected) ++count;
    return count;
}

}  // namespace rt::native::gameplay
