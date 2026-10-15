// Authored mini-game scoring predicates. No game data.
#pragma once

#include "gameplay/geometry.h"

#include <vector>

namespace rt::native::gameplay {

enum class MinigameResult {
    NONE,
    GOAL,
    MISS,
};

[[nodiscard]] MinigameResult balloonPopResult(float playerY, float threshold);
[[nodiscard]] MinigameResult goalZoneResult(Vec2 p, float playerY, float speed,
                                             const std::vector<Vec2>& quad,
                                             const ArrivalCheck& arrival);

class MinigameScorer {
public:
    void add(MinigameResult result);
    [[nodiscard]] int score() const { return score_; }

private:
    int score_ = 0;
};

}  // namespace rt::native::gameplay
