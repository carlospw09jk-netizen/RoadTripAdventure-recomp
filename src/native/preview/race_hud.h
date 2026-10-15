#pragma once

#include "preview/preview.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace rt::native::preview {

// Pure, deterministic HUD normalisation. Invalid maxima intentionally produce
// an empty bar; the compile-time checks below keep its [0, 1] contract honest.
constexpr float raceHudFraction(float value, float maximum) {
    return maximum <= 0.f ? 0.f : value <= 0.f ? 0.f : value >= maximum ? 1.f : value / maximum;
}

static_assert(raceHudFraction(-1.f, 20000.f) == 0.f);
static_assert(raceHudFraction(10000.f, 20000.f) == .5f);
static_assert(raceHudFraction(30000.f, 20000.f) == 1.f);
static_assert(raceHudFraction(1.f, 0.f) == 0.f);

// Builds untextured screen-space race indicators. The speed bar is normalised
// to 20,000 car-physics speed units; elapsed time is normalised to 240 frames
// per configured lap. The caller renders these over its scene with
// previewRender2D().
inline std::vector<ScreenQuad> raceHudQuads(const gameplay::FrameSnapshot& snapshot, int totalLaps,
                                            int width, int height) {
    using Color = std::array<float, 4>;
    const auto rect = [](float x, float y, float w, float h, Color color) {
        return ScreenQuad{x, y, w, h, color};
    };
    const float scale = std::max(1.f, std::min(static_cast<float>(width) / 800.f,
                                                 static_cast<float>(height) / 600.f));
    const float margin = 20.f * scale;
    const float square = 14.f * scale;
    const float gap = 5.f * scale;
    const float barWidth = 180.f * scale;
    const float barHeight = 14.f * scale;
    const Color panel{.015f, .025f, .055f, .72f};
    const Color complete{.18f, .90f, .42f, .95f};
    const Color current{1.f, .76f, .12f, .95f};
    const Color pending{.22f, .29f, .38f, .75f};
    const Color speed{.18f, .72f, 1.f, .95f};
    const Color elapsed{.92f, .36f, .18f, .95f};
    std::vector<ScreenQuad> quads;

    const int laps = std::max(totalLaps, 1);
    const float lapPanelWidth = laps * (square + gap) + margin;
    quads.push_back(rect(margin - 8.f * scale, margin - 8.f * scale, lapPanelWidth, square + 16.f * scale, panel));
    for (int lap = 0; lap < laps; ++lap) {
        Color color = pending;
        if (lap < snapshot.lap) color = complete;
        else if (!snapshot.finished && lap == std::min(snapshot.lap, laps - 1)) color = current;
        quads.push_back(rect(margin + lap * (square + gap), margin, square, square, color));
    }

    const float speedY = margin + square + 24.f * scale;
    quads.push_back(rect(margin, speedY, barWidth, barHeight, panel));
    quads.push_back(rect(margin, speedY, barWidth * raceHudFraction(snapshot.speed, 20000.f), barHeight, speed));

    const float timeY = speedY + barHeight + 12.f * scale;
    quads.push_back(rect(margin, timeY, barWidth, barHeight, panel));
    const float timeBudget = static_cast<float>(laps) * 240.f;
    quads.push_back(rect(margin, timeY, barWidth * raceHudFraction(static_cast<float>(snapshot.elapsedFrames), timeBudget),
                         barHeight, elapsed));

    if (snapshot.finished) {
        const std::array<Color, 4> finishColors{{
            {.22f, .95f, .48f, .78f}, {.25f, .66f, 1.f, .78f},
            {1.f, .62f, .16f, .78f}, {.82f, .34f, .92f, .78f},
        }};
        const int colorIndex = std::clamp(snapshot.rank, 0, static_cast<int>(finishColors.size()) - 1);
        quads.push_back(rect(0.f, static_cast<float>(height) - 18.f * scale,
                             static_cast<float>(width), 18.f * scale, finishColors[colorIndex]));
    }
    return quads;
}

}  // namespace rt::native::preview
