// Authored ground-plane zone geometry contract tests. No game data.
#include "../../src/native/gameplay/geometry.h"

#include <cstdio>
#include <vector>

using namespace rt::native::gameplay;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static const std::vector<Vec2> kUnitSquare = {
    {0.f, 0.f}, {0.f, 1.f}, {1.f, 1.f}, {1.f, 0.f},
};

static void testPointInConvexPolygon() {
    CHECK(pointInConvexPolygon({0.5f, 0.5f}, kUnitSquare));
    CHECK(!pointInConvexPolygon({1.5f, 0.5f}, kUnitSquare));
    CHECK(!pointInConvexPolygon({0.5f, -0.5f}, kUnitSquare));
    CHECK(!pointInConvexPolygon({0.5f, 0.f}, kUnitSquare));
    CHECK(!pointInConvexPolygon({0.5f, 0.5f}, {{0.f, 0.f}, {1.f, 0.f}}));
}

static void testPointInConvexQuad() {
    CHECK(pointInConvexQuad({0.5f, 0.5f}, {0.f, 0.f}, {0.f, 1.f}, {1.f, 1.f}, {1.f, 0.f}));
}

static void testGoalZoneBoundaries() {
    ArrivalCheck arrival;
    CHECK(checkGoalZone({0.5f, 0.5f}, 169.f, 111.f, kUnitSquare, arrival) == ZoneResult::ARRIVED);
    CHECK(checkGoalZone({0.5f, 0.5f}, 171.f, 111.f, kUnitSquare, arrival) == ZoneResult::NONE);
    CHECK(checkGoalZone({0.5f, 0.5f}, 171.f, 110.f, kUnitSquare, arrival) == ZoneResult::FAILED);
    CHECK(checkGoalZone({0.5f, 0.5f}, 171.f, 111.f, kUnitSquare, arrival) != ZoneResult::FAILED);
    CHECK(checkGoalZone({0.5f, 0.5f}, 100.f, 111.f, kUnitSquare, arrival) == ZoneResult::FAILED);
}

int main() {
    testPointInConvexPolygon();
    testPointInConvexQuad();
    testGoalZoneBoundaries();
    if (g_fail) { std::fprintf(stderr, "%d failure(s)\n", g_fail); return 1; }
    std::puts("geometry: all checks passed");
    return 0;
}
