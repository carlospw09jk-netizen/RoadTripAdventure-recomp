// Authored race-route zone/progress contract tests. No game data.
#include "../../src/native/gameplay/route.h"

#include <cstdio>

using namespace rt::native::gameplay;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static RaceRoute makeRoute(float minArrivalSpeed = 117.f) {
    return RaceRoute({
        {{{0.f, 0.f}, {0.f, 1.f}, {1.f, 1.f}, {1.f, 0.f}}},
        {{{1.f, 0.f}, {1.f, 1.f}, {2.f, 1.f}, {2.f, 0.f}}},
        {{{2.f, 0.f}, {2.f, 1.f}, {3.f, 1.f}, {3.f, 0.f}}},
    }, {}, minArrivalSpeed);
}

static void testZoneOf() {
    const RaceRoute route = makeRoute();
    CHECK(route.zoneOf({0.5f, 0.5f}) == 0);
    CHECK(route.zoneOf({1.5f, 0.5f}) == 1);
    CHECK(route.zoneOf({2.5f, 0.5f}) == 2);
    CHECK(route.zoneOf({3.5f, 0.5f}) == -1);
}

static void testAdvanceAndWrap() {
    const RaceRoute route = makeRoute();
    RaceRoute::CarProgress car;

    route.advance(car, {0.5f, 0.5f});
    CHECK(car.lap == 0 && car.zone == 0 && car.progress == 0.f);
    route.advance(car, {1.5f, 0.5f});
    CHECK(car.lap == 0 && car.zone == 1 && car.progress == 1.f);
    route.advance(car, {2.5f, 0.5f});
    CHECK(car.lap == 0 && car.zone == 2 && car.progress == 2.f);
    route.advance(car, {1.5f, 0.5f});
    CHECK(car.lap == 0 && car.zone == 2 && car.progress == 2.f);
    route.advance(car, {0.5f, 0.5f});
    CHECK(car.lap == 1 && car.zone == 0 && car.progress == 3.f);
}

static void testArrivalSpeedThreshold() {
    const RaceRoute route = makeRoute(117.f);
    RaceRoute::CarProgress car;
    route.advance(car, {0.5f, 0.5f});
    route.advance(car, {1.5f, 0.5f});

    CHECK(!route.hasArrived(car, {1.5f, 0.5f}, 116.f));
    CHECK(route.hasArrived(car, {1.5f, 0.5f}, 117.f));
    CHECK(!route.hasArrived(car, {2.5f, 0.5f}, 200.f));
}

int main() {
    testZoneOf();
    testAdvanceAndWrap();
    testArrivalSpeedThreshold();
    if (g_fail) { std::fprintf(stderr, "%d failure(s)\n", g_fail); return 1; }
    std::puts("route: all checks passed");
    return 0;
}
