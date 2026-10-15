// Authored day/night lighting cycle — implementation.
// See daynight.h and build/scratch/decomp-audit/daynight-lighting.md (the
// authoritative extraction from updateTimeOfDayLight 0x225988). No guest
// types; addresses in comments only. No game data.
#include "gameplay/daynight.h"

#include <algorithm>

namespace rt::native::gameplay {
namespace {

// Phase constants (original 0x332DA8, nine floats). Note the weights peak at
// 1.1, not 1.0 — intentional overbright at the day/night extremes.
constexpr float kNightZ = 1.1f;         // DAT_00332DA8: night z
constexpr float kDawnDiv = 9000.f;      // dawn ramp divisor
constexpr float kDawnZOff = 1.1f;       // dawn z = 1.1 - x
constexpr float kDayX = 1.1f;           // day x
constexpr float kSunsetDiv = 9000.f;    // sunset ramp divisor
constexpr float kSunsetXOff = 1.1f;     // sunset x = 1.1 - y
constexpr float kDuskDiv = 4500.f;      // dusk ramp divisor
constexpr float kDuskZOff = 0.1f;       // dusk z base offset
constexpr float kDuskYOff = 1.1f;       // dusk y = 1.1 - z

// Band edges in clock ticks (vblanks since midnight, wraps at 216000).
constexpr std::uint32_t kDawnStart = 49500u;    // 0xC15C
constexpr std::uint32_t kDayStart = 58500u;     // 0xE484
constexpr std::uint32_t kSunsetStart = 153000u; // 0x255A8
constexpr std::uint32_t kDuskStart = 162000u;   // 0x278D0
constexpr std::uint32_t kNightResume = 166500u; // night is < 49500 or > 166499

// ARGB byte pack (A << 24 | R << 16 | G << 8 | B), matching the u32 colour
// words the original writes (e.g. day white 0x80FFFFFF).
std::uint32_t packARGB(std::uint32_t a, std::uint32_t r, std::uint32_t g, std::uint32_t b) {
    return (a << 24) | (r << 16) | (g << 8) | b;
}

// Float channel -> byte: truncating cast + clamp, mirroring the original's
// float->u8 conversions feeding the byte-level CONCAT packs.
std::uint32_t chanByte(float v) {
    return static_cast<std::uint32_t>(std::clamp(static_cast<int>(v), 0, 255));
}

// Phase band selector, needed because the short-fog branch swaps colours
// per band after the main if-chain.
enum class Band { Night, Dawn, Day, Sunset, Dusk };

}  // namespace

void updateTimeOfDayLight(float fogAdjust, std::uint32_t clock, DayNightOutput& out) {
    // The original clamps the adjust to at most 300 before use.
    const float adj = std::min(fogAdjust, 300.f);

    float x = 0.f;  // day
    float y = 0.f;  // dusk
    float z = 0.f;  // night
    float far = 300.f;
    float param = 1.f;
    std::uint32_t fog = 0x80080000u;  // exact night fog
    std::uint32_t sky = 0x80000000u;  // exact night sky
    Band band = Band::Night;

    if (clock < kDawnStart || clock >= kNightResume) {
        // ---- night: constant preset, near-black fog, black sky (both exact).
        band = Band::Night;
        z = kNightZ;
        far = 300.f;
        param = 1.f;
        fog = 0x80080000u;
        sky = 0x80000000u;
    } else if (clock < kDayStart) {
        // ---- dawn: x ramps 0->1 over 9000 ticks, z = 1.1 - x.
        band = Band::Dawn;
        x = static_cast<float>(clock - kDawnStart) / kDawnDiv;
        z = kDawnZOff - x;
        far = std::max(300.f, x * 500.f + 300.f - adj);
        param = z * 0.5f + 0.5f;
        // Per the 0x225988 decompile the two dawn words differ: the FOG colour
        // is R = min(255, 510x), G = B = 255x; the SKY colour is R = G = 255x,
        // B = min(255, 510x). The original builds these with byte-level CONCAT;
        // channel floats truncate to bytes here.
        const std::uint32_t grey = chanByte(255.f * x);
        const std::uint32_t fast = chanByte(std::min(255.f, 510.f * x));
        fog = packARGB(0x80u, fast, grey, grey);
        sky = packARGB(0x80u, grey, grey, fast);
    } else if (clock < kSunsetStart) {
        // ---- day: full day weight, white fog+sky (both exact 0x80FFFFFF).
        band = Band::Day;
        x = kDayX;
        far = 800.f - adj;
        param = 0.5f;
        fog = 0x80ffffffu;
        sky = 0x80ffffffu;
    } else if (clock < kDuskStart) {
        // ---- sunset: y ramps 0->1 over 9000 ticks, x = 1.1 - y.
        band = Band::Sunset;
        y = static_cast<float>(clock - kSunsetStart) / kSunsetDiv;
        x = kSunsetXOff - y;
        far = 800.f - adj;
        param = 0.5f;
        // White->warm ramp (r=255-94y, g=255-128y, b=255); byte-packing
        // approximated (truncating float->byte).
        sky = packARGB(0x80u, chanByte(255.f - 94.f * y), chanByte(255.f - 128.f * y), 255u);
        fog = sky;
    } else {
        // ---- dusk->night: z ramps in over 4500 ticks (+0.1 base), y = 1.1 - z.
        band = Band::Dusk;
        z = static_cast<float>(clock - kDuskStart) / kDuskDiv + kDuskZOff;
        y = kDuskYOff - z;
        far = std::max(300.f, z * 500.f + 300.f - adj);
        param = z * 0.5f + 0.5f;
        // Darkening ramp (r=255z, g=161z, b=127z); byte-packing approximated.
        sky = packARGB(0x80u, chanByte(255.f * z), chanByte(161.f * z), chanByte(127.f * z));
        fog = sky;
    }

    // Alternate short-fog branch (weather/overcast switch): when the clamped
    // adjust is <= -2.5 the original pins far 300 / fogFarParam 1.0 in every
    // band and swaps in this dimmer colour set (extracted from the 0x225988
    // decompile). The original packs alpha byte 0x7f in the dawn/dusk
    // branches; this module keeps its A=0x80 convention. The extraction
    // documents these as the fog words; sky mirrors them (a separate sky
    // handling for this branch is not documented). Night keeps its exact
    // colours.
    if (adj <= -2.5f) {
        far = 300.f;
        param = 1.f;
        switch (band) {
        case Band::Dawn: {
            // R = min(127, 254x), G = 127x, B = 0.
            fog = packARGB(0x80u, chanByte(std::min(127.f, 254.f * x)), chanByte(127.f * x), 0u);
            sky = fog;
            break;
        }
        case Band::Day:
            fog = 0x807f7f00u;
            sky = fog;
            break;
        case Band::Sunset: {
            // R = 127 - 64y, G = 127 - 47y, B = 0.
            fog = packARGB(0x80u, chanByte(127.f - 64.f * y), chanByte(127.f - 47.f * y), 0u);
            sky = fog;
            break;
        }
        case Band::Dusk: {
            // R = 63z, G = 80z, B = 0.
            fog = packARGB(0x80u, chanByte(63.f * z), chanByte(80.f * z), 0u);
            sky = fog;
            break;
        }
        case Band::Night:
            break;
        }
    }

    out.dayWeight = x;
    out.duskWeight = y;
    out.nightWeight = z;
    out.fogFar = far;
    out.fogFarParam = param;
    out.fogColourRGBA = fog;  // original also mirrors this word to *(out+0x50)
    out.skyColour = sky;

    // Blend both 4x4 light presets row-wise (row-major, elementwise):
    // blend[i] = day[i]*x + dusk[i]*y + night[i]*z, then force the 4th column
    // to {0,0,0,128} (original DAT_0177F65C / DAT_0177F69C).
    for (int i = 0; i < 16; ++i) {
        out.lightMatrix[i] = kTodLightDay[i] * x + kTodLightDusk[i] * y + kTodLightNight[i] * z;
        out.lightMatrix2[i] = kTodLight2Day[i] * x + kTodLight2Dusk[i] * y + kTodLight2Night[i] * z;
    }
    out.lightMatrix[3] = 0.f;
    out.lightMatrix[7] = 0.f;
    out.lightMatrix[11] = 0.f;
    out.lightMatrix[15] = 128.f;
    out.lightMatrix2[3] = 0.f;
    out.lightMatrix2[7] = 0.f;
    out.lightMatrix2[11] = 0.f;
    out.lightMatrix2[15] = 128.f;
}

std::uint32_t advanceTimeOfDayCounter(std::uint32_t clock) {
    // Original: counter + 1, wrapping at 0x34BC0 (216000 vblanks = one day).
    return (clock + 1u) % kDayTicks;
}

FixedLight setFixedLight(int preset) {
    // Original table 0x29EA10: five 16-byte {sky, fog, fogFarParam, fogFar}
    // entries copied wholesale; anything out of range falls back to entry 0.
    static constexpr FixedLight kTable[5] = {
        {0x80f05800u, 0x80ffffffu, 0.5f, 800.f},
        {0x80dee0dcu, 0x80404040u, 0.75f, 500.f},
        {0x8066688bu, 0x80285ad2u, 0.5f, 800.f},
        {0x80f05800u, 0x80ffffffu, 0.5f, 800.f},
        {0x80f05800u, 0x80a5cbe9u, 1.0f, 400.f},
    };
    if (preset < 0 || preset >= 5) preset = 0;
    return kTable[preset];
}

}  // namespace rt::native::gameplay
