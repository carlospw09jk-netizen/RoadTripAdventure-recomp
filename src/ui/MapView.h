#pragma once

// The game's map as we draw it (both screens): the hardware GS's map layer (drawn at 4x apart
// from the picture, HostPresenter::kMapTexture), the town's places marked on it, and text in the
// game's outlined style. The second screen enlarges it in a gold plate; Map = Enhanced puts it on
// the main screen in the game's own spot (drawMainOverlay).

#include "imgui.h"

#include <vector>

class PS2Runtime;

namespace ps2x
{
    class HostPresenter;
}

namespace rt::game
{
    struct Stats;
    struct Place;
}

namespace rt::ui::mapview
{
    // Text with the game's navy outline (o: the outline's width, 0 = the theme's).
    void outlined(ImDrawList *dl, ImVec2 pos, float size, ImU32 colour, const char *s, float o = 0);
    ImVec2 measure(float size, const char *s);

    // Where a map image went (its corners), for the marks.
    struct Placement
    {
        ImVec2 a, b;
    };

    // The map enlarged into min..max in a gold plate rim (the second screen): as large as fits,
    // centred or at the left; `inset`: the town window's box (a field pixel in, past its scissor
    // edge). False when no map is on hand.
    bool drawMap(ImDrawList *dl, ImVec2 min, ImVec2 max, ps2x::HostPresenter &presenter, bool centre, ImVec2 *placed = nullptr,
                 bool inset = false, Placement *where = nullptr);

    // Marks on the town map (Q's Factory, the shops, Quick-Pic Shops not used yet, houses not
    // visited yet) where the game's minimap puts them; `r`: a mark's radius in pixels.
    void drawPlaces(ImDrawList *dl, const Placement &m, const rt::game::Stats &st, const std::vector<rt::game::Place> &places, float r);

    // The town's places, read again once per tile or second (done-ness changes as you play).
    const std::vector<rt::game::Place> &placesFor(PS2Runtime &runtime, const rt::game::Stats &st, float now);

    // Map = Enhanced, main screen: our map where the game drew its own (the router sends it there).
    bool mainOverlayWanted(PS2Runtime &runtime); // ... in this host frame (it needs an ImGui frame)
    void drawMainOverlay(PS2Runtime &runtime);   // inside the main ImGui frame, under the windows
}
