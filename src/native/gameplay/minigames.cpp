#include "gameplay/minigames.h"

namespace rt::native::gameplay {

MinigameResult balloonPopResult(float playerY, float threshold) {
    return playerY >= threshold ? MinigameResult::GOAL : MinigameResult::NONE;
}

MinigameResult goalZoneResult(Vec2 p, float playerY, float speed,
                              const std::vector<Vec2>& quad,
                              const ArrivalCheck& arrival) {
    switch (checkGoalZone(p, playerY, speed, quad, arrival)) {
    case ZoneResult::ARRIVED:
        return MinigameResult::GOAL;
    case ZoneResult::FAILED:
        return MinigameResult::MISS;
    case ZoneResult::NONE:
        return MinigameResult::NONE;
    }
    return MinigameResult::NONE;
}

void MinigameScorer::add(MinigameResult result) {
    if (result == MinigameResult::GOAL) ++score_;
}

}  // namespace rt::native::gameplay
