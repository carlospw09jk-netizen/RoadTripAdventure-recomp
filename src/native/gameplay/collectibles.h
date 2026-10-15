#pragma once

// Authored stamp and coin pickup model. Pure host C++; no guest/game-data input.

#include "gameplay/progression.h"
#include "gameplay/vec.h"

#include <vector>

namespace rt::native::gameplay {

enum class CollectibleKind { STAMP, COIN };

struct Collectible {
    CollectibleKind kind;
    Vec2 pos;
    float radius;
    int index;
    bool collected = false;
};

// Collects an uncollected, valid collectible when carPos is within its x/z radius.
bool tryCollect(Collectible& collectible, Vec2 carPos, SaveData& save);

int collectedCount(const std::vector<Collectible>& collectibles);

}  // namespace rt::native::gameplay
