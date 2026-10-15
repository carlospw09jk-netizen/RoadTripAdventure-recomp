// Authored Adventure session flow replacing the original mode-dispatch glue:
// explore -> race -> results -> explore. It owns simulation and persistent
// progress but leaves memory-card I/O to explicit save/load calls.
#pragma once

#include "gameplay/game_loop.h"
#include "gameplay/memcard.h"
#include "gameplay/save.h"

namespace rt::native::gameplay {

enum class SessionState { EXPLORE, RACE, RESULTS };

class GameSession {
public:
    // A single lap keeps the authored host loop concise while GameLoop remains
    // configurable for callers that want longer races.
    explicit GameSession(int totalLaps = 1);

    [[nodiscard]] SessionState state() const { return state_; }
    void update(const MenuInput& input);

    void startRace();
    void continueFromResults();

    [[nodiscard]] const SaveData& progress() const { return progress_; }
    [[nodiscard]] const FrameSnapshot& lastSnapshot() const { return lastSnapshot_; }

    bool save(MemoryCard& card, int slot) const;
    bool load(const MemoryCard& card, int slot);

private:
    SaveData progress_;
    GameLoop gameLoop_;
    RaceTracker raceTracker_{0};
    FrameSnapshot lastSnapshot_;
    SessionState state_ = SessionState::EXPLORE;
    int recordedLaps_ = 0;

    void awardRaceResult();
};

}  // namespace rt::native::gameplay
