// Authored mini-game scoring predicate contract tests. No game data.
#include "../../src/native/gameplay/minigames.h"

#include <cstdio>
#include <vector>

using namespace rt::native::gameplay;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static const std::vector<Vec2> kUnitSquare = {
    {0.f, 0.f}, {0.f, 1.f}, {1.f, 1.f}, {1.f, 0.f},
};

static void testBalloonPopThreshold() {
    CHECK(balloonPopResult(333.f, 333.f) == MinigameResult::GOAL);
    CHECK(balloonPopResult(332.999f, 333.f) == MinigameResult::NONE);
}

static void testGoalZoneResultMapping() {
    ArrivalCheck arrival;
    CHECK(goalZoneResult({0.5f, 0.5f}, 169.f, 111.f, kUnitSquare, arrival)
          == MinigameResult::GOAL);
    CHECK(goalZoneResult({0.5f, 0.5f}, 171.f, 110.f, kUnitSquare, arrival)
          == MinigameResult::MISS);
    CHECK(goalZoneResult({0.5f, 0.5f}, 171.f, 111.f, kUnitSquare, arrival)
          == MinigameResult::NONE);
}

static void testScorerAccumulatesGoalsOnly() {
    MinigameScorer scorer;
    scorer.add(MinigameResult::NONE);
    scorer.add(MinigameResult::GOAL);
    scorer.add(MinigameResult::MISS);
    scorer.add(MinigameResult::GOAL);
    CHECK(scorer.score() == 2);
}

int main() {
    testBalloonPopThreshold();
    testGoalZoneResultMapping();
    testScorerAccumulatesGoalsOnly();
    if (g_fail) { std::fprintf(stderr, "%d failure(s)\n", g_fail); return 1; }
    std::puts("minigames: all checks passed");
    return 0;
}
