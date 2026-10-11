// The game's map as we draw it (see MapView.h).

#include "ui/MapView.h"

#include "game/GameStats.h"
#include "ui/MapRouter.h"
#include "ui/Theme.h"

#include "ps2_runtime.h"
#include "runtime/ps2_host_presenter.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

namespace rt::ui::mapview
{
    namespace th = theme;

    namespace
    {
        constexpr ImU32 kOutline = IM_COL32(0x00, 0x08, 0x20, 0xFF), kWhite = IM_COL32(0xFF, 0xFF, 0xFF, 0xFF);
        constexpr ImU32 kInk = IM_COL32(0x1A, 0x3C, 0x8C, 0xFF);
        // Under the map on the second screen: the layer is transparent where the game's map is
        // (its window is translucent over the picture), so the second screen shows it on navy.
        constexpr ImU32 kMapGround = IM_COL32(0x0B, 0x12, 0x40, 0xFF);
    }

    void outlined(ImDrawList *dl, ImVec2 pos, float size, ImU32 colour, const char *s, float o)
    {
        o = o > 0 ? o : std::max(1.0f, th::px(3));
        static const ImVec2 dirs[8] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}, {-0.7f, -0.7f}, {0.7f, -0.7f}, {-0.7f, 0.7f}, {0.7f, 0.7f}};
        for (const ImVec2 &d : dirs)
            dl->AddText(th::font(), size, ImVec2(pos.x + d.x * o, pos.y + d.y * o), kOutline, s);
        dl->AddText(th::font(), size, pos, colour, s);
    }

    ImVec2 measure(float size, const char *s) { return th::font()->CalcTextSizeA(size, FLT_MAX, 0, s); }

    // The game's own map (drawn apart from the picture by the hardware GS, without its bezel), as
    // large as fits (its layer is drawn at 4x, sampled smoothly), in a gold plate rim.
    bool drawMap(ImDrawList *dl, ImVec2 min, ImVec2 max, ps2x::HostPresenter &presenter, bool centre, ImVec2 *placed, bool inset,
                 Placement *where)
    {
        float uv[4], aspect = 1.0f;
        if (!presenter.mapRegion(uv, aspect) || aspect <= 0.0f)
            return false;
        // Widescreen draws the HUD (the map with it) narrower by (4:3) / the display's shape,
        // to look right once stretched: undo that here.
        aspect /= std::min(1.0f, (4.0f / 3.0f) / std::max(presenter.displayAspect(), 0.1f));
        if (!inset) // the margin below widens the shown part
        {
            const float bw = std::max(1.0f, (uv[2] - uv[0]) * 640.0f), bh = std::max(1.0f, (uv[3] - uv[1]) * 224.0f);
            aspect *= ((bw + 4.0f) / bw) / ((bh + 2.0f) / bh);
        }
        const float rim = th::px(6);
        const float w = max.x - min.x - rim * 2, h = max.y - min.y - rim * 2;
        const float dw = std::floor(std::min(w, h * aspect)), dh = std::floor(dw / aspect);
        const ImVec2 a(centre ? std::floor(min.x + rim + (w - dw) * 0.5f) : min.x + rim, std::floor(min.y + rim + (h - dh) * 0.5f));
        const ImVec2 b(a.x + dw, a.y + dh);
        dl->AddRectFilled(ImVec2(a.x - rim, a.y - rim), ImVec2(b.x + rim, b.y + rim), IM_COL32(0xE8, 0xC8, 0x30, 0xFF), th::px(16));
        dl->AddRect(ImVec2(a.x - rim * 0.5f, a.y - rim * 0.5f), ImVec2(b.x + rim * 0.5f, b.y + rim * 0.5f), IM_COL32(0xFF, 0xF0, 0xA0, 0xFF),
                    th::px(12), 0, th::px(3));
        // Inside the rim's rounded corners; the town map a field pixel in from its box (the
        // game's bezel and scissor edge show there; a race map's box is its own extent).
        // A race map's box is the track's extent plus 4 GS pixels (fixed for the race by the
        // GS); a little more of the layer's background keeps the track off the rim.
        const float du = inset ? 1.0f / 640.0f : -2.0f / 640.0f, dv = inset ? 1.0f / 224.0f : -1.0f / 224.0f;
        dl->AddRectFilled(a, b, kMapGround, th::px(12));
        dl->AddImageRounded(static_cast<ImTextureID>(ps2x::HostPresenter::kMapTexture), a, b, ImVec2(uv[0] + du, uv[1] + dv),
                            ImVec2(uv[2] - du, uv[3] - dv), IM_COL32_WHITE, th::px(12));
        if (placed)
            *placed = ImVec2(b.x + rim, b.y + rim);
        if (where)
            *where = {a, b};
        return true;
    }

    // Projected like the game's minimap: the town map's box is its scissor, 46..158 x 157..207
    // shown, whatever widescreen did to it. One look for every mark (a navy outer stroke, as the
    // game's HUD outlines its pieces), placed most important first: Q's Factory, the shops, the
    // Quick-Pic Shops, then the houses; a mark that would land on one already placed is left out
    // (cameras and houses) or moved clear of it (a shop next to the factory). Marks over our car
    // (the game's red arrow) fade and shrink, so the map still says where you are.
    void drawPlaces(ImDrawList *dl, const Placement &m, const rt::game::Stats &st, const std::vector<rt::game::Place> &places, float r)
    {
        using K = rt::game::Place::Kind;
        constexpr float kX0 = 46, kX1 = 158, kY0 = 157, kY1 = 207;
        auto toScreen = [&](float fx, float fy) {
            return ImVec2(m.a.x + (fx - kX0) / (kX1 - kX0) * (m.b.x - m.a.x), m.a.y + (fy - kY0) / (kY1 - kY0) * (m.b.y - m.a.y));
        };
        float cx, cy;
        rt::game::mapPoint(st, st.carX, st.carZ, cx, cy);
        const ImVec2 car = toScreen(cx, cy);
        auto dist = [](ImVec2 p, ImVec2 q) { return std::hypot(p.x - q.x, p.y - q.y); };

        struct Mark
        {
            K kind;
            char letter;
            ImVec2 c;
        };
        std::vector<Mark> marks;
        for (int pass = 0; pass < 4; ++pass)
        {
            const K want = pass == 0 ? K::Factory : pass == 1 ? K::Shop : pass == 2 ? K::PhotoBooth : K::House;
            for (const auto &pl : places)
            {
                if (pl.kind != want || ((pl.kind == K::PhotoBooth || pl.kind == K::House) && pl.done))
                    continue;
                float fx, fy;
                rt::game::mapPoint(st, pl.x, pl.z, fx, fy);
                if (fx < kX0 - 4 || fx > kX1 + 4 || fy < kY0 - 4 || fy > kY1 + 4)
                    continue;
                ImVec2 c = toScreen(fx, fy);
                bool keep = true;
                for (const Mark &o : marks)
                {
                    const float d = dist(c, o.c);
                    if (want == K::Shop && o.kind == K::Factory && d < 2.3f * r)
                    {
                        // Moved clear of the factory, away from its centre.
                        const ImVec2 dir = d > 0.01f ? ImVec2((c.x - o.c.x) / d, (c.y - o.c.y) / d) : ImVec2(1, 0);
                        c = ImVec2(o.c.x + dir.x * 2.3f * r, o.c.y + dir.y * 2.3f * r);
                    }
                    else if ((want == K::PhotoBooth && d < 1.6f * r) || (want == K::House && d < 1.6f * r))
                        keep = false;
                }
                if (keep)
                    marks.push_back({want, pl.letter, c});
            }
        }

        dl->PushClipRect(m.a, m.b, true);
        const float stroke = std::max(1.5f, r * 0.18f), tag = r * 1.5f;
        for (const Mark &k : marks)
        {
            // Over the car: faint and smaller.
            const bool overCar = dist(k.c, car) < 2.2f * r;
            const float s = overCar ? 0.7f : 1.0f;
            const int alpha = overCar ? 0x59 : 0xFF;
            auto col = [&](ImU32 c) { return (c & ~IM_COL32_A_MASK) | (static_cast<ImU32>(alpha) << IM_COL32_A_SHIFT); };
            const ImU32 navy = col(kMapGround), white = col(kWhite);
            const ImVec2 c = k.c;
            switch (k.kind)
            {
            case K::Factory:
            {
                const float rr = 1.2f * r * s;
                dl->AddCircleFilled(c, rr + stroke, navy, 24);
                dl->AddCircleFilled(c, rr, col(IM_COL32(0xFF, 0xC0, 0x40, 0xFF)), 24);
                dl->AddCircleFilled(c, rr - std::max(1.0f, r * 0.1f), col(IM_COL32(0xF0, 0x7A, 0x10, 0xFF)), 24);
                const ImVec2 sz = measure(tag * s, "Q");
                outlined(dl, ImVec2(c.x - sz.x * 0.5f, c.y - sz.y * 0.5f + r * 0.05f), tag * s, white, "Q", std::max(1.0f, stroke * 0.6f));
                break;
            }
            case K::Shop:
            {
                const float rr = r * s;
                const char t[2] = {k.letter, 0};
                dl->AddRectFilled(ImVec2(c.x - rr - stroke, c.y - rr - stroke), ImVec2(c.x + rr + stroke, c.y + rr + stroke), navy, rr * 0.5f);
                dl->AddRectFilled(ImVec2(c.x - rr, c.y - rr), ImVec2(c.x + rr, c.y + rr), col(IM_COL32(0x1C, 0xA1, 0xE6, 0xFF)), rr * 0.45f);
                const ImVec2 sz = measure(tag * s, t);
                outlined(dl, ImVec2(c.x - sz.x * 0.5f, c.y - sz.y * 0.5f + r * 0.05f), tag * s, white, t, std::max(1.0f, stroke * 0.6f));
                break;
            }
            case K::PhotoBooth:
            {
                // A little camera: body, lens, flash.
                const float rr = 0.85f * r * s;
                const ImU32 pink = col(IM_COL32(0xFF, 0x5F, 0xB8, 0xFF));
                dl->AddRectFilled(ImVec2(c.x - rr - stroke, c.y - rr * 0.7f - stroke), ImVec2(c.x + rr + stroke, c.y + rr * 0.75f + stroke), navy,
                                  rr * 0.4f);
                dl->AddRectFilled(ImVec2(c.x - rr * 0.4f - stroke, c.y - rr - stroke), ImVec2(c.x + rr * 0.2f + stroke, c.y - rr * 0.6f), navy,
                                  rr * 0.2f);
                dl->AddRectFilled(ImVec2(c.x - rr, c.y - rr * 0.7f), ImVec2(c.x + rr, c.y + rr * 0.75f), pink, rr * 0.3f);
                dl->AddRectFilled(ImVec2(c.x - rr * 0.4f, c.y - rr), ImVec2(c.x + rr * 0.2f, c.y - rr * 0.6f), pink, rr * 0.15f);
                dl->AddCircleFilled(ImVec2(c.x, c.y + rr * 0.05f), rr * 0.45f, white, 16);
                dl->AddCircleFilled(ImVec2(c.x, c.y + rr * 0.05f), rr * 0.25f, col(kInk), 16);
                break;
            }
            default: // a house still to visit
            {
                const float rr = std::max(2.0f, 0.4f * r * s);
                dl->AddCircleFilled(c, rr + stroke, navy, 16);
                dl->AddCircleFilled(c, rr, white, 16);
                break;
            }
            }
        }
        dl->PopClipRect();
    }

    const std::vector<rt::game::Place> &placesFor(PS2Runtime &runtime, const rt::game::Stats &st, float now)
    {
        static int tile = -1;
        static std::vector<rt::game::Place> places;
        static float at = -10;
        if (st.inTown && (st.tile != tile || now - at > 1.0f || now < at))
        {
            places = rt::game::placesAround(runtime, st);
            tile = st.tile;
            at = now;
        }
        return places;
    }

    namespace
    {
        bool overlayMap(PS2Runtime &runtime, ps2x::HostPresenter::MapInfo &info, float picture[4])
        {
            ps2x::HostPresenter *presenter = runtime.presenter();
            if (!presenter || maprouter::destination() != maprouter::Destination::Overlay)
                return false;
            if (!presenter->mapInfo(info) || !info.shown || !presenter->pictureRect(picture))
                return false;
            // Loading and the game's 2D screens draw no map (the last one is taken away after a
            // few frames; until then it mustn't show over them).
            if (runtime.gsUnsynced().lastFrameWas2D())
                return false;
            return info.picture[2] > info.picture[0] && info.picture[3] > info.picture[1];
        }
    }

    bool mainOverlayWanted(PS2Runtime &runtime)
    {
        ps2x::HostPresenter::MapInfo info;
        float picture[4];
        return overlayMap(runtime, info, picture);
    }

    // The game's map from the layer, where the game drew it: the published outer box (with the
    // bezel) is in the picture's coordinates, already narrowed and moved as widescreen and the HUD
    // position put it, so the picture's rectangle on screen places it at the game's size. The
    // layer is premultiplied, so its translucent window shows the world behind as the game's does.
    void drawMainOverlay(PS2Runtime &runtime)
    {
        ps2x::HostPresenter::MapInfo info;
        float pr[4];
        if (!overlayMap(runtime, info, pr))
            return;
        const ImVec2 ds = ImGui::GetIO().DisplaySize;
        const ImVec2 p0(pr[0] * ds.x, pr[1] * ds.y), ps(pr[2] * ds.x, pr[3] * ds.y);
        auto at = [&](float u, float v) { return ImVec2(p0.x + u * ps.x, p0.y + v * ps.y); };
        const ImVec2 a = at(info.picture[0], info.picture[1]), b = at(info.picture[2], info.picture[3]);
        ImDrawList *dl = ImGui::GetBackgroundDrawList();
        const float *o = info.outerUv;
        dl->AddImage(static_cast<ImTextureID>(ps2x::HostPresenter::kMapTexture), a, b, ImVec2(o[0], o[1]), ImVec2(o[2], o[3]));
        // Town and field maps: our marks on the map itself (its box inside the outer one).
        if (info.kind != 1 || o[2] <= o[0] || o[3] <= o[1])
            return;
        const rt::game::Stats st = rt::game::readStats(runtime);
        if (!st.inTown)
            return;
        auto inside = [&](float u, float v) {
            return ImVec2(a.x + (u - o[0]) / (o[2] - o[0]) * (b.x - a.x), a.y + (v - o[1]) / (o[3] - o[1]) * (b.y - a.y));
        };
        const Placement m{inside(info.uv[0], info.uv[1]), inside(info.uv[2], info.uv[3])};
        drawPlaces(dl, m, st, placesFor(runtime, st, static_cast<float>(ImGui::GetTime())), std::max(3.0f, (m.b.y - m.a.y) * 0.038f));
    }
}
