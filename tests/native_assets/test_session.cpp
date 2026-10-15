// Pins the authored Adventure session loop. Synthetic simulation and temporary
// save files only; no game data.
#include "../../src/native/gameplay/session.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

using namespace rt::native::gameplay;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

namespace {

class TempDirectory {
public:
    TempDirectory() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path = std::filesystem::temp_directory_path() / ("rt-native-session-" + std::to_string(stamp));
        std::filesystem::create_directories(path);
    }
    ~TempDirectory() { std::error_code ec; std::filesystem::remove_all(path, ec); }

    std::filesystem::path path;
};

void testAdventureLoopAwardsFinishAndReturnsToExplore() {
    GameSession session(1);
    CHECK(session.state() == SessionState::EXPLORE);
    CHECK(session.progress().money == kMoneyNewGame);

    session.startRace();
    CHECK(session.state() == SessionState::RACE);
    CHECK(session.lastSnapshot().frame == 0);

    for (int i = 0; i < 130 && session.state() == SessionState::RACE; ++i)
        session.update({});

    CHECK(session.state() == SessionState::RESULTS);
    CHECK(session.lastSnapshot().finished);
    const std::uint32_t reward = session.lastSnapshot().rank == 0 ? 1000u :
                                 session.lastSnapshot().rank == 1 ? 500u : 250u;
    CHECK(session.progress().money == kMoneyNewGame + reward);
    CHECK(session.progress().raceResults[0] == static_cast<std::uint8_t>(session.lastSnapshot().rank));
    CHECK(session.progress().bestLap[0] != kBestTimeNone);
    CHECK(session.progress().bestTotal[0] != kBestTimeNone);

    session.continueFromResults();
    CHECK(session.state() == SessionState::EXPLORE);
}

void testConfirmDrivesExploreAndResultsTransitions() {
    GameSession session(1);
    session.update({.confirm = true});
    CHECK(session.state() == SessionState::RACE);

    for (int i = 0; i < 130 && session.state() == SessionState::RACE; ++i)
        session.update({});
    CHECK(session.state() == SessionState::RESULTS);

    session.update({.confirm = true});
    CHECK(session.state() == SessionState::EXPLORE);
}

void testProgressSaveLoadRoundTrip() {
    TempDirectory temp;
    MemoryCard card(temp.path);
    GameSession saved(1);
    saved.startRace();
    for (int i = 0; i < 130 && saved.state() == SessionState::RACE; ++i)
        saved.update({});
    CHECK(saved.state() == SessionState::RESULTS);
    CHECK(saved.save(card, 0));

    GameSession loaded(1);
    CHECK(loaded.load(card, 0));
    CHECK(loaded.progress().money == saved.progress().money);
}

}  // namespace

int main() {
    testAdventureLoopAwardsFinishAndReturnsToExplore();
    testConfirmDrivesExploreAndResultsTransitions();
    testProgressSaveLoadRoundTrip();
    if (g_fail != 0) return EXIT_FAILURE;
    std::puts("session: all checks passed");
    return EXIT_SUCCESS;
}
