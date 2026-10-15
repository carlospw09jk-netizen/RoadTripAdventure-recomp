// Authored post-race result and persistent-record contract tests. No game data.
#include "../../src/native/gameplay/results.h"

#include <cstdio>

using namespace rt::native::gameplay;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

namespace {

SaveData freshSave() {
    SaveData save;
    save.raceResults.fill(kResultNotRaced);
    save.bestLap.fill(kBestTimeNone);
    save.bestTotal.fill(kBestTimeNone);
    return save;
}

RaceTracker runRace(const SaveData& save, int course, int firstLapFrames, int totalFrames) {
    RaceTracker tracker(course);
    tracker.loadFromSave(save);
    for (int frame = 0; frame < firstLapFrames; ++frame) tracker.tick();
    tracker.completeLap();
    for (int frame = firstLapFrames; frame < totalFrames; ++frame) tracker.tick();
    tracker.finishRace();
    return tracker;
}

void testRecordsFirstRunAndRejectsSlowerRun() {
    constexpr int kCourse = 4;
    SaveData save = freshSave();

    const RaceTracker firstTracker = runRace(save, kCourse, 300, 1000);
    const RaceResult first = buildRaceResult(firstTracker, kCourse);
    CHECK(first.rank == getRaceTimeRank(1000u));
    CHECK(first.totalFrames == 1000u);
    CHECK(first.bestLap == 300u);
    CHECK(first.newBestLap);
    CHECK(first.newBestTotal);
    CHECK(rankFromResult(first) == first.rank);

    recordRaceResult(save, kCourse, first);
    CHECK(save.raceResults[kCourse] == 0u);
    CHECK(save.bestLap[kCourse] == 300u);
    CHECK(save.bestTotal[kCourse] == 1000u);

    const RaceTracker slowerTracker = runRace(save, kCourse, 500, 2000);
    const RaceResult slower = buildRaceResult(slowerTracker, kCourse);
    CHECK(slower.totalFrames == 2000u);
    CHECK(slower.bestLap == 300u);
    CHECK(!slower.newBestLap);
    CHECK(!slower.newBestTotal);

    recordRaceResult(save, kCourse, slower);
    CHECK(save.bestLap[kCourse] == 300u);
    CHECK(save.bestTotal[kCourse] == 1000u);
}

}  // namespace

int main() {
    testRecordsFirstRunAndRejectsSlowerRun();
    if (g_fail) { std::fprintf(stderr, "%d failure(s)\n", g_fail); return 1; }
    std::puts("results: all checks passed");
    return 0;
}
