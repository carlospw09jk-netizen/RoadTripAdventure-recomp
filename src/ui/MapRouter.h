#pragma once

// Where the game's map goes (town, field, highway and race maps): the only place that asks the GS
// for it (HostPresenter::setWantMap). Run once per host frame, before the UI.
//
//   - the second screen (Android dual-screen devices), unless the player moved Map off it;
//   - else our own map on the main screen ([display] map = "enhanced"), in the game's spot;
//   - else the game's own, in the picture (always in the attract demo, and with a GS that can't
//     draw the map apart).
//
// "A map is up" is the GS's own signal (game frames that drew the map), not the game's state
// flags, so field areas and highways count; times are guest vblanks. The decision itself is a pure
// function (decide) so it can be tested on its own (map_router_test).

#include <cstdint>

class PS2Runtime;

namespace rt::ui::maprouter
{
    enum class Destination : uint8_t { Game, SecondScreen, Overlay };
    const char *name(Destination d);

    struct Inputs
    {
        uint64_t vblank = 0;           // guest vblanks so far
        bool mapLayer = false;         // the GS can draw the map apart (the hardware GS)
        bool demo = false;             // the attract demo
        bool secondScreenOn = false;   // the second screen is wanted (the setting) and exists
        bool secondSwapchain = false;  // ... and the presenter has its swapchain now
        bool playerHandedAway = false; // the player chose another page over the map
        bool enhanced = false;         // [display] map = "enhanced"
        uint64_t framesSeen = 0;       // GS: game frames that drew a map (MapInfo::framesSeen)
        uint64_t serial = 0;           // GS: maps published (MapInfo::serial)
        bool shown = false;            // GS: a published map is on hand
    };

    struct State
    {
        Destination dest = Destination::Game;
        uint64_t lastVblank = 0;
        uint64_t secondLostAt = 0;      // when the second screen's swapchain went away
        bool secondLost = false;
        uint64_t lastSeen = 0, lastSeenAt = 0;     // framesSeen and when it last moved
        uint64_t lastSerial = 0, lastSerialAt = 0; // serial and when it last moved
        uint64_t sentSince = 0;         // the map has been asked for since this vblank
        uint64_t barredUntil = 0;       // a map asked for but never published: back to the game till then
        bool started = false;
        bool seen = false;              // the game has drawn a map since the clocks started
    };

    // Grace for a second screen without a swapchain (a surface rebuild, out of date) before the
    // map goes elsewhere; how long a map can go unpublished while asked for and drawn; and how
    // long the game keeps its map after that.
    constexpr uint64_t kSecondGrace = 60, kUnpublished = 30, kBar = 600;
    // A map drawn in the last kRecent vblanks is "up" (the second screen's Map page).
    constexpr uint64_t kRecent = 45;

    // One step: the destination for these inputs (updates `s`).
    Destination decide(State &s, const Inputs &in);
    // The game drew a map within kRecent vblanks.
    bool mapRecent(const State &s, uint64_t vblank);

    // The app's router: reads the inputs, decides, tells the presenter (main loop, every host frame).
    void update(PS2Runtime &runtime);
    Destination destination();
    bool mapUp();      // the game draws a map now (mapRecent)
    bool mapLayerOk(); // the GS can draw the map apart
    // For the test socket: {"cmd":"map"}.
    const char *debugLine();
}
