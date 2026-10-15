// Authored race timing and standings model.  The ordering comes from
// RankCarsByProgress (0x22D7B8); persistent best times use SaveData's
// documented per-course arrays.
#pragma once

#include "gameplay/save.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace rt::native::gameplay {

// GetRaceTimeRank (0x249670): elapsed frames map to the game's time rank.
int getRaceTimeRank(std::uint32_t elapsedFrames);

struct CarProgress {
    int lap;
    int zone;
    int flag;
    float progress;
    int rank;
    // Authored equivalent of the binary routine's car-state eligibility test.
    // Inactive cars are left after active cars and receive rank -1.
    bool active = true;
};

// Sorts active cars ahead-to-behind and assigns ranks 0, 1, 2, ... .
// The exact sort key is: (lap desc, where lap is -1 when zone < 2; zone asc;
// flag asc; progress asc).  Inactive cars are skipped.
void rankCarsByProgress(std::vector<CarProgress>& cars);

class RaceTracker {
public:
    explicit RaceTracker(std::size_t courseIndex) : courseIndex_(courseIndex) {}

    void tick();
    void completeLap();
    void finishRace();

    [[nodiscard]] std::uint32_t elapsedFrames() const { return elapsedFrames_; }
    [[nodiscard]] std::uint32_t currentLapFrames() const { return currentLapFrames_; }
    [[nodiscard]] std::uint32_t totalFrames() const { return totalFrames_; }
    [[nodiscard]] std::uint32_t lapCount() const { return lapCount_; }
    [[nodiscard]] std::uint32_t bestLap() const { return bestLap_; }
    [[nodiscard]] std::uint32_t bestTotal() const { return bestTotal_; }
    [[nodiscard]] const std::vector<std::uint32_t>& lapTimes() const { return lapTimes_; }

    bool takeNewBestLap();
    bool takeNewBestTotal();

    // Transfers the per-course records only. A 0xFF race-result entry means
    // that course has not been raced, so loading it yields no records.
    void writeToSave(SaveData& save) const;
    void loadFromSave(const SaveData& save);

private:
    static constexpr std::uint32_t kNoBestTime = kBestTimeNone;

    std::size_t courseIndex_;
    std::uint32_t elapsedFrames_ = 0;
    std::uint32_t currentLapFrames_ = 0;
    std::uint32_t totalFrames_ = 0;
    std::uint32_t lapCount_ = 0;
    std::uint32_t bestLap_ = kNoBestTime;
    std::uint32_t bestTotal_ = kNoBestTime;
    std::vector<std::uint32_t> lapTimes_;
    bool newBestLap_ = false;
    bool newBestTotal_ = false;
};

}  // namespace rt::native::gameplay
