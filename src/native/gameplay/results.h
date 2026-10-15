// Authored post-race result snapshot and persistent-record update model.
#pragma once

#include "gameplay/race.h"

#include <cstdint>

namespace rt::native::gameplay {

struct RaceResult {
    // Zero-based place, matching SaveData::raceResults (place - 1).
    int rank;
    std::uint32_t totalFrames;
    std::uint32_t bestLap;
    bool newBestLap;
    bool newBestTotal;
};

// Snapshots a completed tracker. The tracker must have loaded the course's
// existing records before the race; its record notifications identify whether
// this run set either record.
RaceResult buildRaceResult(const RaceTracker& tracker, int courseIndex);

// Records the zero-based place and lowers per-course time records only when
// the result beats a recorded time (or the recorded time is kBestTimeNone).
void recordRaceResult(SaveData& save, int courseIndex, const RaceResult& result);

// RaceResult::rank is the coarse GetRaceTimeRank value captured at build time,
// so this returns the already-computed zero-based place without remapping it.
int rankFromResult(const RaceResult& result);

}  // namespace rt::native::gameplay
