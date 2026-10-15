// Authored race timing, standings, and save-record contract tests. No game data.
#include "../../src/native/gameplay/race.h"

#include <cstdio>

using namespace rt::native::gameplay;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static void testRaceTimeRankBoundaries() {
    CHECK(getRaceTimeRank(58500u) == 0);
    CHECK(getRaceTimeRank(58501u) == 1);
    CHECK(getRaceTimeRank(81000u) == 1);
    CHECK(getRaceTimeRank(81001u) == 1);
    CHECK(getRaceTimeRank(144000u) == 1);
    CHECK(getRaceTimeRank(144001u) == 2);
    CHECK(getRaceTimeRank(162000u) == 2);
    CHECK(getRaceTimeRank(162001u) == 0);
}

static void testRankCarsByProgress() {
    std::vector<CarProgress> cars = {
        {1, 8, 0, 0.30f, -1},  // fifth: lower completed lap
        {2, 3, 9, 0.01f, -1},  // fourth: lower zone loses to zone 5
        {2, 5, 2, 0.90f, -1},  // first: higher flag wins within a zone
        {2, 5, 1, 0.40f, -1},  // third: same flag, higher progress loses
        {2, 5, 1, 0.10f, -1},  // second: lower progress is ahead
        {8, 1, 0, 0.01f, -1},  // sixth: zone < 2 substitutes lap -1
        {9, 9, 0, 0.01f, -1, false},  // inactive, unranked
    };

    rankCarsByProgress(cars);
    CHECK(cars[0].lap == 2 && cars[0].zone == 5 && cars[0].flag == 2 && cars[0].rank == 0);
    CHECK(cars[1].lap == 2 && cars[1].flag == 1 && cars[1].progress == 0.10f && cars[1].rank == 1);
    CHECK(cars[2].lap == 2 && cars[2].flag == 1 && cars[2].progress == 0.40f && cars[2].rank == 2);
    CHECK(cars[3].lap == 2 && cars[3].zone == 3 && cars[3].rank == 3);
    CHECK(cars[4].lap == 1 && cars[4].rank == 4);
    CHECK(cars[5].lap == 8 && cars[5].zone == 1 && cars[5].rank == 5);
    CHECK(!cars[6].active && cars[6].rank == -1);
}

static void testRaceTrackerAndSave() {
    RaceTracker tracker(4);
    CHECK(tracker.bestLap() == kBestTimeNone);
    CHECK(tracker.bestTotal() == kBestTimeNone);

    for (int i = 0; i < 10; ++i) tracker.tick();
    tracker.completeLap();
    CHECK(tracker.elapsedFrames() == 10u && tracker.currentLapFrames() == 0u);
    CHECK(tracker.lapCount() == 1u && tracker.lapTimes()[0] == 10u);
    CHECK(tracker.bestLap() == 10u && tracker.takeNewBestLap());
    CHECK(!tracker.takeNewBestLap());

    for (int i = 0; i < 12; ++i) tracker.tick();
    tracker.completeLap();
    CHECK(tracker.bestLap() == 10u && !tracker.takeNewBestLap());

    for (int i = 0; i < 8; ++i) tracker.tick();
    tracker.completeLap();
    CHECK(tracker.lapCount() == 3u && tracker.lapTimes()[2] == 8u);
    CHECK(tracker.bestLap() == 8u && tracker.takeNewBestLap());

    tracker.finishRace();
    CHECK(tracker.totalFrames() == 30u && tracker.bestTotal() == 30u);
    CHECK(tracker.takeNewBestTotal() && !tracker.takeNewBestTotal());

    SaveData save;
    save.raceResults[4] = 0;
    save.bestLap[4] = kBestTimeNone;
    save.bestTotal[4] = kBestTimeNone;
    tracker.writeToSave(save);
    CHECK(save.bestLap[4] == 8u && save.bestTotal[4] == 30u);

    RaceTracker restored(4);
    restored.loadFromSave(save);
    CHECK(restored.bestLap() == 8u && restored.bestTotal() == 30u);

    SaveData noBest;
    noBest.raceResults[5] = 0;
    noBest.bestLap[5] = kBestTimeNone;
    noBest.bestTotal[5] = kBestTimeNone;
    RaceTracker sentinel(5);
    sentinel.loadFromSave(noBest);
    CHECK(sentinel.bestLap() == kBestTimeNone && sentinel.bestTotal() == kBestTimeNone);

    noBest.raceResults[5] = kResultNotRaced;
    noBest.bestLap[5] = 7u;
    noBest.bestTotal[5] = 21u;
    sentinel.loadFromSave(noBest);
    CHECK(sentinel.bestLap() == kBestTimeNone && sentinel.bestTotal() == kBestTimeNone);
}

int main() {
    testRaceTimeRankBoundaries();
    testRankCarsByProgress();
    testRaceTrackerAndSave();
    if (g_fail) { std::fprintf(stderr, "%d failure(s)\n", g_fail); return 1; }
    std::puts("race: all checks passed");
    return 0;
}
