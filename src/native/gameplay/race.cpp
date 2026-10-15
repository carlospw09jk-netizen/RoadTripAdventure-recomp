#include "gameplay/race.h"

#include <algorithm>

namespace rt::native::gameplay {

int getRaceTimeRank(std::uint32_t elapsedFrames) {
    if (elapsedFrames <= 58500u) return 0;
    if (elapsedFrames <= 144000u) return 1;
    if (elapsedFrames <= 162000u) return 2;
    return 0;
}

void rankCarsByProgress(std::vector<CarProgress>& cars) {
    const auto firstInactive = std::stable_partition(
        cars.begin(), cars.end(), [](const CarProgress& car) { return car.active; });

    std::stable_sort(cars.begin(), firstInactive, [](const CarProgress& a, const CarProgress& b) {
        const int aLap = a.zone < 2 ? -1 : a.lap;
        const int bLap = b.zone < 2 ? -1 : b.lap;
        if (aLap != bLap) return aLap > bLap;
        if (a.zone != b.zone) return a.zone > b.zone;
        if (a.flag != b.flag) return a.flag > b.flag;
        return a.progress < b.progress;
    });

    int rank = 0;
    for (auto it = cars.begin(); it != firstInactive; ++it) it->rank = rank++;
    for (auto it = firstInactive; it != cars.end(); ++it) it->rank = -1;
}

void RaceTracker::tick() {
    ++elapsedFrames_;
    ++currentLapFrames_;
}

void RaceTracker::completeLap() {
    lapTimes_.push_back(currentLapFrames_);
    if (currentLapFrames_ < bestLap_) {
        bestLap_ = currentLapFrames_;
        newBestLap_ = true;
    }
    currentLapFrames_ = 0;
    ++lapCount_;
}

void RaceTracker::finishRace() {
    totalFrames_ = elapsedFrames_;
    if (totalFrames_ < bestTotal_) {
        bestTotal_ = totalFrames_;
        newBestTotal_ = true;
    }
}

bool RaceTracker::takeNewBestLap() {
    const bool value = newBestLap_;
    newBestLap_ = false;
    return value;
}

bool RaceTracker::takeNewBestTotal() {
    const bool value = newBestTotal_;
    newBestTotal_ = false;
    return value;
}

void RaceTracker::writeToSave(SaveData& save) const {
    if (courseIndex_ >= kBestTimeEntries) return;
    save.bestLap[courseIndex_] = bestLap_;
    save.bestTotal[courseIndex_] = bestTotal_;
}

void RaceTracker::loadFromSave(const SaveData& save) {
    newBestLap_ = false;
    newBestTotal_ = false;
    if (courseIndex_ >= kBestTimeEntries ||
        (courseIndex_ < kRaceResultCount && save.raceResults[courseIndex_] == kResultNotRaced)) {
        bestLap_ = kNoBestTime;
        bestTotal_ = kNoBestTime;
        return;
    }
    bestLap_ = save.bestLap[courseIndex_];
    bestTotal_ = save.bestTotal[courseIndex_];
}

}  // namespace rt::native::gameplay
