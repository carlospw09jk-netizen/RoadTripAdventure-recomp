// Authored day/night lighting cycle: plain C++, no PS2/guest types; guest
// addresses appear in comments only.
//
// Primary spec: build/scratch/decomp-audit/daynight-lighting.md (Ghidra
// decompile of updateTimeOfDayLight 0x225988 plus raw memory reads); narrative
// context in docs/game-mechanics/sound-and-daynight.md §2. The clock counts
// vblanks since midnight and wraps at 216000 (0x34BC0, UpdateTimeOfDayCounter
// 0x2114E0). Five phase bands (night / dawn / day / sunset / dusk) blend three
// weights (x=day, y=dusk, z=night), fog far + fogFarParam, ARGB sky/fog
// colours and two 4x4 light matrices. Race mode bypasses the clock entirely
// with the 5-entry fixed-light table of setFixedLight 0x226248 (0x29EA10).
//
// The scripted per-area step machine (ApplyTimeOfDayLighting 0x26CA48 /
// AdvanceDayCycleStep 0x26D1C0) is intentionally NOT modelled in this slice.
#pragma once

#include <cstdint>

namespace rt::native::gameplay {

// One full in-game day in clock ticks (original wrap constant 0x34BC0).
inline constexpr std::uint32_t kDayTicks = 216000u;

// Bundle of everything updateTimeOfDayLight computes. The original scatters
// these across the scene-params block: weights x/y/z at 0x177F720/24/28,
// fogFarParam 0x177F738, fogFar 0x177F73C, fogColourRGBA 0x177F750 (also
// mirrored to the caller's out+0x50 — the struct field is the equivalent),
// skyColour 0x177F754, blended light matrices at 0x177F620 / 0x177F660.
struct DayNightOutput {
    float dayWeight = 0.f;    // x
    float duskWeight = 0.f;   // y
    float nightWeight = 0.f;  // z
    float fogFar = 300.f;       // 300 (night) .. 800 (day)
    float fogFarParam = 1.f;    // 1.0 night / 0.5 day
    std::uint32_t fogColourRGBA = 0x80080000u;  // ARGB, A=0x80
    std::uint32_t skyColour = 0x80000000u;      // ARGB, A=0x80
    float lightMatrix[16] = {};   // blend of kTodLightDay/Dusk/Night
    float lightMatrix2[16] = {};  // blend of kTodLight2Day/Dusk/Night
};

// The six 4x4 light presets, row-major, verbatim from the audit
// (originals todLightDay/Dusk/Night at 0x29D500/0x29D540/0x29D580 and
// todLight2Day/Dusk/Night at 0x29D5C0/0x29D600/0x29D640). The 4th column of
// the blend is forced to {0,0,0,128} after weighting, so the presets' own
// 4th-column values only matter through that override.
inline constexpr float kTodLightDay[16] = {
    0.6f, 0.6f, 0.6f, 0.f,    0.25f, 0.25f, 0.25f, 0.f,
    0.6f, 0.6f, 0.6f, 0.f,    0.4f, 0.4f, 0.4f, 128.f};
inline constexpr float kTodLightDusk[16] = {
    0.6f, 0.38f, 0.2f, 0.f,   0.3f, 0.19f, 0.1f, 0.f,
    0.6f, 0.5f, 0.2f, 0.f,    0.5f, 0.4f, 0.4f, 128.f};
inline constexpr float kTodLightNight[16] = {
    0.3f, 0.3f, 0.3f, 0.f,    0.0f, 0.0f, 0.0f, 0.f,
    0.4f, 0.4f, 0.5f, 0.f,    0.4f, 0.4f, 0.5f, 128.f};
inline constexpr float kTodLight2Day[16] = {
    0.4f, 0.4f, 0.4f, 0.f,    0.25f, 0.25f, 0.25f, 0.f,
    0.2f, 0.2f, 0.2f, 0.f,    0.5f, 0.5f, 0.5f, 128.f};
inline constexpr float kTodLight2Dusk[16] = {
    0.36f, 0.2f, 0.1f, 0.f,   0.3f, 0.19f, 0.1f, 0.f,
    0.12f, 0.12f, 0.12f, 0.f, 0.5f, 0.4f, 0.4f, 128.f};
inline constexpr float kTodLight2Night[16] = {
    0.3f, 0.3f, 0.3f, 0.f,    0.0f, 0.0f, 0.0f, 0.f,
    0.4f, 0.4f, 0.5f, 0.f,    0.4f, 0.4f, 0.5f, 128.f};

// Mirrors updateTimeOfDayLight 0x225988: given the fog-distance adjust
// (clamped to min(fogAdjust, 300); <= -2.5 selects the alternate short-fog
// branch) and the time-of-day clock, fills every field of `out`.
void updateTimeOfDayLight(float fogAdjust, std::uint32_t clock, DayNightOutput& out);

// Mirrors UpdateTimeOfDayCounter 0x2114E0: (clock + 1) % 216000.
std::uint32_t advanceTimeOfDayCounter(std::uint32_t clock);

// Race-mode fixed lighting, mirroring one 16-byte entry of setFixedLight
// 0x226248 (table 0x29EA10): {sky RGBA, fog RGBA, fogFarParam, fogFar}.
struct FixedLight {
    std::uint32_t sky = 0x80f05800u;
    std::uint32_t fog = 0x80ffffffu;
    float fogFarParam = 0.5f;
    float fogFar = 800.f;
};

// Returns preset 0..4 from the fixed-light table; out-of-range -> entry 0.
FixedLight setFixedLight(int preset);

}  // namespace rt::native::gameplay
