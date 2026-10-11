// Unit test of the map router's decision (src/ui/MapRouter.h): where the game's map goes for
// each state (second screen on/off, the player's choice, the Enhanced setting, the attract demo,
// a lost second-screen surface, a map asked for but never published).

#include "ui/MapRouter.h"

#include <cstdio>

using namespace rt::ui::maprouter;

static int g_failed = 0;
#define CHECK(c)                                                                 \
    do                                                                           \
    {                                                                            \
        if (!(c))                                                                \
        {                                                                        \
            std::fprintf(stderr, "%s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #c); \
            ++g_failed;                                                          \
        }                                                                        \
    } while (0)

namespace
{
    // A game that draws a map every frame and a GS that publishes it while asked for.
    struct Sim
    {
        State s;
        Inputs in;
        bool publishes = true;
        Sim()
        {
            in.mapLayer = true;
            in.vblank = 1000;
        }
        Destination step(int vblanks = 1, bool drawsMap = true)
        {
            Destination d = s.dest;
            for (int i = 0; i < vblanks; ++i)
            {
                ++in.vblank;
                if (drawsMap)
                {
                    ++in.framesSeen;
                    if (publishes && d != Destination::Game)
                        ++in.serial;
                }
                d = decide(s, in);
            }
            return d;
        }
    };
}

int main()
{
    // No GS that can draw the map apart (paraLLEl-GS): the game keeps it, whatever else.
    {
        Sim m;
        m.in.mapLayer = false;
        m.in.secondScreenOn = m.in.secondSwapchain = true;
        m.in.enhanced = true;
        CHECK(m.step() == Destination::Game);
    }
    // The attract demo: always the game's.
    {
        Sim m;
        m.in.demo = true;
        m.in.secondScreenOn = m.in.secondSwapchain = true;
        CHECK(m.step() == Destination::Game);
        m.in.enhanced = true, m.in.secondScreenOn = false;
        CHECK(m.step() == Destination::Game);
    }
    // Second screen on: there; the player moved Map away: Enhanced on the main screen, else the game.
    {
        Sim m;
        m.in.secondScreenOn = m.in.secondSwapchain = true;
        CHECK(m.step(5) == Destination::SecondScreen);
        m.in.playerHandedAway = true;
        CHECK(m.step() == Destination::Game);
        m.in.enhanced = true;
        CHECK(m.step() == Destination::Overlay);
        m.in.playerHandedAway = false;
        CHECK(m.step() == Destination::SecondScreen);
    }
    // No second screen: Enhanced shows ours, else the game's.
    {
        Sim m;
        CHECK(m.step() == Destination::Game);
        m.in.enhanced = true;
        CHECK(m.step(120) == Destination::Overlay);
        m.in.enhanced = false;
        CHECK(m.step() == Destination::Game);
    }
    // A second screen's surface rebuilt (no swapchain for a moment): the map stays there, then
    // goes to the main screen (Enhanced) or the game after kSecondGrace.
    {
        Sim m;
        m.in.secondScreenOn = m.in.secondSwapchain = true;
        m.in.enhanced = true;
        CHECK(m.step(10) == Destination::SecondScreen);
        m.in.secondSwapchain = false;
        CHECK(m.step(static_cast<int>(kSecondGrace) - 2) == Destination::SecondScreen);
        CHECK(m.step(5) == Destination::Overlay);
        m.in.secondSwapchain = true;
        CHECK(m.step() == Destination::SecondScreen);
        // back again: a fresh grace
        m.in.secondSwapchain = false;
        CHECK(m.step(static_cast<int>(kSecondGrace) - 2) == Destination::SecondScreen);
        m.in.enhanced = false;
        CHECK(m.step(5) == Destination::Game);
    }
    // Asked for, drawn, never published (the map would be on neither screen): back to the game for
    // kBar vblanks, then tried again.
    {
        Sim m;
        m.in.enhanced = true;
        m.publishes = false;
        CHECK(m.step(static_cast<int>(kUnpublished)) == Destination::Overlay);
        CHECK(m.step(3) == Destination::Game);
        CHECK(m.step(static_cast<int>(kBar) - 10) == Destination::Game);
        m.publishes = true;
        CHECK(m.step(20) == Destination::Overlay);
        CHECK(m.step(200) == Destination::Overlay);
    }
    // Asked for while no map is drawn (a building): nothing to publish, no reason to give up.
    {
        Sim m;
        m.in.secondScreenOn = m.in.secondSwapchain = true;
        m.publishes = false;
        CHECK(m.step(300, false) == Destination::SecondScreen);
        CHECK(!mapRecent(m.s, m.in.vblank));
        m.publishes = true;
        CHECK(m.step(1) == Destination::SecondScreen);
        CHECK(mapRecent(m.s, m.in.vblank));
        // the map's "up" holds for kRecent vblanks after the last frame that drew it
        m.step(static_cast<int>(kRecent) - 1, false);
        CHECK(mapRecent(m.s, m.in.vblank));
        m.step(2, false);
        CHECK(!mapRecent(m.s, m.in.vblank));
    }
    // Guest time going back (a state load) restarts the clocks: no bar from stale times.
    {
        Sim m;
        m.in.enhanced = true;
        CHECK(m.step(100) == Destination::Overlay);
        m.in.vblank = 10;
        CHECK(m.step(50) == Destination::Overlay);
    }
    if (g_failed)
        std::fprintf(stderr, "map_router_test: %d failed\n", g_failed);
    else
        std::printf("map_router_test: ok\n");
    return g_failed ? 1 : 0;
}
