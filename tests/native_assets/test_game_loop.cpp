// Pins the authored GameLoop simulation frame (gameMainLoop 0x2003B8 per-frame
// update). No game data.
#include "../../src/native/gameplay/game_loop.h"

#include <cmath>
#include <cstdio>
#include <cstring>

using namespace rt::native::gameplay;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static bool same(const FrameSnapshot& a, const FrameSnapshot& b) {
    return a.carPosition.x == b.carPosition.x && a.carPosition.z == b.carPosition.z &&
           a.heading == b.heading && a.waypointIndex == b.waypointIndex && a.lapCount == b.lapCount &&
           a.frame == b.frame && a.visibleCells.size() == b.visibleCells.size() &&
           std::memcmp(a.view, b.view, sizeof a.view) == 0 && std::memcmp(a.proj, b.proj, sizeof a.proj) == 0;
}

static void testResetAndFirstTick() {
    GameLoop g;
    g.reset();
    const Vec2 start = g.world().roadWaypoints[0];
    const MenuInput mi;
    const FrameSnapshot s = g.tick(1.f / 60.f, mi);
    CHECK(s.frame == 1);
    CHECK(std::isfinite(s.heading));
    const float moved = std::sqrt(lengthSq(s.carPosition - start));
    CHECK(moved > 0.f && moved < 20.f);
    CHECK(s.carPosition.x > start.x);  // starts toward +x (waypoint 1)
    CHECK(g.tasks().liveCount() == 1);
    const FrameSnapshot s2 = g.tick(1.f / 60.f, mi);
    CHECK(s2.frame == 2 && s2.carPosition.x > s.carPosition.x);

    g.reset();
    const FrameSnapshot r = g.tick(1.f / 60.f, mi);
    CHECK(same(r, s));  // reset reproduces the first frame
}

static void testLapCorridorCamera() {
    GameLoop g;
    const MenuInput mi;
    const float half = g.world().roadHalfWidth;
    float maxDev = 0.f, maxCam = 0.f;
    FrameSnapshot s;
    for (int i = 0; i < 130; ++i) {  // one lap completes ~frame 110
        s = g.tick(1.f / 60.f, mi);
        maxDev = std::fmax(maxDev, g.roadDistance(s.carPosition));
        const float dx = s.cameraEye.x - s.carPosition.x * g.params().renderScale;
        const float dz = s.cameraEye.z - s.carPosition.z * g.params().renderScale;
        const float dy = s.cameraEye.y;
        maxCam = std::fmax(maxCam, std::sqrt(dx * dx + dy * dy + dz * dz));
        CHECK(!s.visibleCells.empty());
        CHECK(s.carPosition.x >= 0.f && s.carPosition.x < kWorldSize && s.carPosition.z >= 0.f &&
              s.carPosition.z < kWorldSize);
    }
    std::printf("maxDev=%g laps=%d waypointsPassed=%d maxCam=%g\n", maxDev, s.lapCount, s.waypointsPassed, maxCam);
    CHECK(maxDev < half);
    CHECK(s.lapCount >= 1);
    CHECK(s.waypointsPassed >= 4);
    CHECK(maxCam < 25.f && maxCam > 1.f);
    // View matrix translation (column-major [12..14]) = -R * eye: camera origin maps to eye.
    // Transform the eye through view: must land at the origin.
    const float ex = s.cameraEye.x, ey = s.cameraEye.y, ez = s.cameraEye.z;
    for (int r = 0; r < 3; ++r) {
        const float o = s.view[r] * ex + s.view[4 + r] * ey + s.view[8 + r] * ez + s.view[12 + r];
        CHECK(std::fabs(o) < 1e-3f);
    }
}

static void testCull() {
    GameLoop g;
    const MenuInput mi;
    const FrameSnapshot s = g.tick(0.f, mi);
    const auto expect = g.world().visibleCells(s.carPosition, g.params().viewDistance);
    CHECK(s.visibleCells.size() == expect.size());
    CHECK(!s.visibleCells.empty() && s.visibleCells.size() <= static_cast<std::size_t>(kCellCount));
    // Car near (400,400): the (100,100) corner cell centre is ~424 away (in), (1500,1500) ~1555 (out).
    CHECK(s.visibleCells.size() < static_cast<std::size_t>(kCellCount));
    for (const Cell& c : s.visibleCells)
        CHECK(std::sqrt(lengthSq(c.center() - s.carPosition)) <= g.params().viewDistance + 1e-3f);
}

static void testDeterminism() {
    GameLoop a, b;
    const MenuInput mi;
    for (int i = 0; i < 60; ++i) CHECK(same(a.tick(1.f / 60.f, mi), b.tick(1.f / 60.f, mi)));
}

int main() {
    testResetAndFirstTick();
    testLapCorridorCamera();
    testCull();
    testDeterminism();
    if (g_fail) { std::fprintf(stderr, "%d failure(s)\n", g_fail); return 1; }
    std::puts("game_loop tests passed");
    return 0;
}
