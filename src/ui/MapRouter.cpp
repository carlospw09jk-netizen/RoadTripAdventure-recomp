// Where the game's map goes (see MapRouter.h): the app's side (the decision is MapRoute.cpp).

#include "ui/MapRouter.h"

#include "game/GameStats.h"
#include "settings/Capabilities.h"
#include "settings/Settings.h"
#include "ui/SecondScreen.h"

#include "ps2_runtime.h"
#include "runtime/ps2_host_presenter.h"
#include "runtime/ps2_test_harness.h"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace rt::ui::maprouter
{
    namespace
    {
        State g_state;
        Inputs g_in;
        bool g_layer = false;
        std::string g_line;
    }

    void update(PS2Runtime &runtime)
    {
        ps2x::HostPresenter *presenter = runtime.presenter();
        if (!presenter)
            return;
        const auto &settings = rt::settings::current();
        const auto &caps = rt::settings::capabilities();
        ps2x::HostPresenter::MapInfo info;
        g_layer = caps.hardwareGs && presenter->mapInfo(info);
        int w = 0, h = 0;
        Inputs in;
        in.vblank = ps2_test::currentVblank();
        in.mapLayer = g_layer;
        in.demo = rt::game::attractDemo(runtime.memory().getRDRAM());
        in.secondScreenOn = settings.secondScreen && caps.secondDisplay;
        in.secondSwapchain = presenter->secondScreenSize(w, h);
        in.playerHandedAway = secondScreenHandsMapAway();
        in.enhanced = settings.map == rt::settings::MapMode::Enhanced;
        in.framesSeen = info.framesSeen;
        in.serial = info.serial;
        in.shown = info.shown;
        const Destination before = g_state.dest;
        const uint64_t barred = g_state.barredUntil;
        const Destination d = decide(g_state, in);
        g_in = in;
        presenter->setWantMap(d != Destination::Game);

        char line[320];
        std::snprintf(line, sizeof(line),
                      "dest %s layer %d demo %d second %d/%d away %d enhanced %d seen %llu serial %llu shown %d up %d latched %d kind %u",
                      name(d), in.mapLayer, in.demo, in.secondScreenOn, in.secondSwapchain, in.playerHandedAway, in.enhanced,
                      static_cast<unsigned long long>(in.framesSeen), static_cast<unsigned long long>(in.serial), in.shown,
                      mapRecent(g_state, in.vblank), info.latched, info.kind);
        g_line = line;
        // RT_SECOND_DEBUG=1: a line at each change of destination, with the game's state words.
        static const bool debug = [] { const char *e = std::getenv("RT_SECOND_DEBUG"); return e && *e == '1'; }();
        if (debug && (d != before || g_state.barredUntil != barred))
            std::fprintf(stderr, "[map] vblank %llu %s -> %s%s | %s | %s\n", static_cast<unsigned long long>(in.vblank), name(before), name(d),
                         g_state.barredUntil != barred ? " (published nothing: back to the game for a while)" : "", line,
                         rt::game::stateWords(runtime.memory().getRDRAM()).c_str());
    }

    Destination destination() { return g_state.dest; }
    bool mapUp() { return mapRecent(g_state, g_in.vblank); }
    bool mapLayerOk() { return g_layer; }
    const char *debugLine() { return g_line.c_str(); }
}
