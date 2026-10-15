// Pins the authored day/night lighting contracts
// (build/scratch/decomp-audit/daynight-lighting.md; mirrors
// updateTimeOfDayLight 0x225988, UpdateTimeOfDayCounter 0x2114E0,
// setFixedLight 0x226248). No game data.
#include "../../src/native/gameplay/daynight.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace rt::native::gameplay;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static bool nearF(float a, float b, float eps = 1e-5f) { return std::fabs(a - b) <= eps; }
#define CHECK_NEAR(a, b) CHECK(nearF((a), (b)))

static DayNightOutput run(float fogAdjust, std::uint32_t clock) {
    DayNightOutput o;
    updateTimeOfDayLight(fogAdjust, clock, o);
    return o;
}

// ---------------------------------------------------------------- weights

static void testWeights() {
    // night (clock 0): x=0, y=0, z=1.1
    DayNightOutput o = run(0.f, 0u);
    CHECK(o.dayWeight == 0.f);
    CHECK(o.duskWeight == 0.f);
    CHECK(o.nightWeight == 1.1f);

    // night again past the dusk ramp (clock 166500 > 166499)
    o = run(0.f, 166500u);
    CHECK(o.nightWeight == 1.1f);
    CHECK(o.dayWeight == 0.f);
    CHECK(o.duskWeight == 0.f);

    // dawn start (49500): x=0, z=1.1
    o = run(0.f, 49500u);
    CHECK(o.dayWeight == 0.f);
    CHECK(o.nightWeight == 1.1f);

    // dawn mid (54000): x=(54000-49500)/9000=0.5, z=1.1-0.5=0.6
    o = run(0.f, 54000u);
    CHECK_NEAR(o.dayWeight, 0.5f);
    CHECK_NEAR(o.nightWeight, 0.6f);
    CHECK(o.duskWeight == 0.f);

    // day (58500): x=1.1, y=0, z=0
    o = run(0.f, 58500u);
    CHECK(o.dayWeight == 1.1f);
    CHECK(o.duskWeight == 0.f);
    CHECK(o.nightWeight == 0.f);

    // sunset mid (157500): y=(157500-153000)/9000=0.5, x=1.1-0.5=0.6
    o = run(0.f, 157500u);
    CHECK_NEAR(o.duskWeight, 0.5f);
    CHECK_NEAR(o.dayWeight, 0.6f);
    CHECK(o.nightWeight == 0.f);

    // dusk start (162000): z=0/4500+0.1=0.1, y=1.1-0.1=1.0, x=0
    o = run(0.f, 162000u);
    CHECK_NEAR(o.nightWeight, 0.1f);
    CHECK_NEAR(o.duskWeight, 1.0f);
    CHECK(o.dayWeight == 0.f);

    // dusk mid (164250): z=2250/4500+0.1=0.6, y=0.5
    o = run(0.f, 164250u);
    CHECK_NEAR(o.nightWeight, 0.6f);
    CHECK_NEAR(o.duskWeight, 0.5f);
}

// ---------------------------------------------------------------- fog

static void testFog() {
    // night: far 300, param 1.0
    DayNightOutput o = run(0.f, 0u);
    CHECK(o.fogFar == 300.f);
    CHECK(o.fogFarParam == 1.f);

    // day (fogAdjust 0): far 800, param 0.5
    o = run(0.f, 100000u);
    CHECK(o.fogFar == 800.f);
    CHECK(o.fogFarParam == 0.5f);

    // dawn midpoint (54000, x=0.5, z=0.6): far = 0.5*500+300 = 550,
    // param = 0.6*0.5+0.5 = 0.8
    o = run(0.f, 54000u);
    CHECK_NEAR(o.fogFar, 550.f);
    CHECK_NEAR(o.fogFarParam, 0.8f);

    // dusk start (162000, z=0.1): far = 0.1*500+300 = 350, param = 0.55
    o = run(0.f, 162000u);
    CHECK_NEAR(o.fogFar, 350.f);
    CHECK_NEAR(o.fogFarParam, 0.55f);

    // sunset shares the day fog: far 800, param 0.5
    o = run(0.f, 157500u);
    CHECK(o.fogFar == 800.f);
    CHECK(o.fogFarParam == 0.5f);
}

static void testFogAdjust() {
    // fogAdjust 500 is clamped to 300: day far = 800-300 = 500, same as
    // passing 300 directly.
    DayNightOutput hi = run(500.f, 100000u);
    DayNightOutput clamped = run(300.f, 100000u);
    CHECK(hi.fogFar == 500.f);
    CHECK(hi.fogFar == clamped.fogFar);

    // The clamp also feeds the dawn ramp: at 54000, 550-300 = 250 -> min 300.
    DayNightOutput dawn = run(500.f, 54000u);
    CHECK(dawn.fogFar == 300.f);

    // fogAdjust -3 selects the alternate short-fog branch: far 300, param 1.0.
    DayNightOutput o = run(-3.f, 100000u);
    CHECK(o.fogFar == 300.f);
    CHECK(o.fogFarParam == 1.f);
    o = run(-3.f, 54000u);  // transitional bands too
    CHECK(o.fogFar == 300.f);
    CHECK(o.fogFarParam == 1.f);

    // Short-fog colours. Day swaps in the dim yellow word 0x807F7F00.
    o = run(-3.f, 100000u);
    CHECK(o.fogColourRGBA == 0x807f7f00u);

    // Dawn dims to R = min(127, 254x), G = 127x, B = 0; at x=0.5 that is
    // R = min(127, 127) = 127, G = 63.5 -> 63, B = 0.
    o = run(-3.f, 54000u);
    CHECK(((o.fogColourRGBA >> 16) & 0xff) == 127u);
    CHECK(((o.fogColourRGBA >> 8) & 0xff) == 63u);
    CHECK((o.fogColourRGBA & 0xff) == 0u);

    // Night keeps its exact colours even in the short-fog branch.
    o = run(-3.f, 0u);
    CHECK(o.fogColourRGBA == 0x80080000u);
    CHECK(o.skyColour == 0x80000000u);
}

// ---------------------------------------------------------------- colours

static void testColours() {
    // night + day endpoint colours are exact.
    DayNightOutput o = run(0.f, 0u);
    CHECK(o.fogColourRGBA == 0x80080000u);
    CHECK(o.skyColour == 0x80000000u);
    o = run(0.f, 100000u);
    CHECK(o.fogColourRGBA == 0x80ffffffu);
    CHECK(o.skyColour == 0x80ffffffu);

    // Ramp bands keep A=0x80 and stay between the night and day endpoints
    // (exact ramp byte-packing is approximated; only sanity-checked here).
    // 49500 (x=0) is excluded: the dawn grey ramp legitimately starts black.
    const std::uint32_t clocks[] = {54000u, 157500u, 162000u, 164250u};
    for (std::uint32_t c : clocks) {
        o = run(0.f, c);
        CHECK((o.skyColour >> 24) == 0x80u);
        CHECK((o.fogColourRGBA >> 24) == 0x80u);
        CHECK((o.skyColour & 0xffffffu) != 0u);           // not fully black...
        CHECK((o.skyColour & 0xffffffu) != 0xffffffu);     // ...nor full white
    }

    // Dawn midpoint (54000, x=0.5): fog R = min(255, 510*0.5) = 255 with
    // G = B = 255x = 127; sky R = G = 127 with B = min(255, 510x) = 255.
    o = run(0.f, 54000u);
    CHECK(((o.fogColourRGBA >> 16) & 0xff) == 255u);
    CHECK(((o.fogColourRGBA >> 8) & 0xff) == 127u);
    CHECK((o.fogColourRGBA & 0xff) == 127u);
    CHECK(((o.skyColour >> 16) & 0xff) == 127u);
    CHECK(((o.skyColour >> 8) & 0xff) == 127u);
    CHECK((o.skyColour & 0xff) == 255u);

    // Dawn quarter (51750, x=0.25) exercises the 510x ramp before saturation:
    // fog red / sky blue = 510*0.25 = 127.5 -> 127; grey = 255*0.25 = 63.75 -> 63.
    o = run(0.f, 51750u);
    CHECK(((o.fogColourRGBA >> 16) & 0xff) == 127u);
    CHECK((o.skyColour & 0xff) == 127u);
    CHECK(((o.fogColourRGBA >> 8) & 0xff) == 63u);
    CHECK(((o.skyColour >> 16) & 0xff) == 63u);

    // Dusk warm ramp at z=0.6: r=153, g=96, b=76 (255z/161z/127z truncated).
    o = run(0.f, 164250u);
    CHECK(((o.skyColour >> 16) & 0xff) == 153u);
    CHECK(((o.skyColour >> 8) & 0xff) == 96u);
    CHECK((o.skyColour & 0xff) == 76u);
}

// ---------------------------------------------------------------- matrices

static void testLightMatrices() {
    // Pure day: blend = todLightDay * 1.1 (the day weight is 1.1 — the
    // intentional overbright at the extremes), w-column forced {0,0,0,128}.
    DayNightOutput o = run(0.f, 100000u);
    for (int i = 0; i < 16; ++i) {
        if (i % 4 == 3) continue;  // w-column is overridden, checked below
        CHECK_NEAR(o.lightMatrix[i], kTodLightDay[i] * 1.1f);
        CHECK_NEAR(o.lightMatrix2[i], kTodLight2Day[i] * 1.1f);
    }
    CHECK(o.lightMatrix[3] == 0.f && o.lightMatrix[7] == 0.f && o.lightMatrix[11] == 0.f);
    CHECK(o.lightMatrix[15] == 128.f);  // forced, NOT 128*1.1

    // Pure night: blend = todLightNight * 1.1 (todLight2Night == todLightNight).
    o = run(0.f, 0u);
    for (int i = 0; i < 16; ++i) {
        if (i % 4 == 3) continue;  // w-column is overridden, checked below
        CHECK_NEAR(o.lightMatrix[i], kTodLightNight[i] * 1.1f);
        CHECK_NEAR(o.lightMatrix2[i], kTodLight2Night[i] * 1.1f);
    }

    // A mixed band matches the elementwise day*x + dusk*y + night*z formula.
    o = run(0.f, 157500u);  // sunset mid: x=0.6, y=0.5, z=0
    const float x = o.dayWeight, y = o.duskWeight, z = o.nightWeight;
    for (int i = 0; i < 16; ++i) {
        if (i % 4 == 3) continue;  // w-column overridden below
        CHECK_NEAR(o.lightMatrix[i], kTodLightDay[i] * x + kTodLightDusk[i] * y + kTodLightNight[i] * z);
        CHECK_NEAR(o.lightMatrix2[i], kTodLight2Day[i] * x + kTodLight2Dusk[i] * y + kTodLight2Night[i] * z);
    }

    // w-column is {0,0,0,128} in EVERY band, both matrices.
    const std::uint32_t clocks[] = {0u, 49500u, 54000u, 58500u, 100000u, 153000u, 157500u, 162000u, 164250u, 166500u};
    for (std::uint32_t c : clocks) {
        o = run(0.f, c);
        CHECK(o.lightMatrix[3] == 0.f);
        CHECK(o.lightMatrix[7] == 0.f);
        CHECK(o.lightMatrix[11] == 0.f);
        CHECK(o.lightMatrix[15] == 128.f);
        CHECK(o.lightMatrix2[3] == 0.f);
        CHECK(o.lightMatrix2[7] == 0.f);
        CHECK(o.lightMatrix2[11] == 0.f);
        CHECK(o.lightMatrix2[15] == 128.f);
    }
}

// ---------------------------------------------------------------- counter

static void testAdvanceTimeOfDayCounter() {
    CHECK(advanceTimeOfDayCounter(0u) == 1u);
    CHECK(advanceTimeOfDayCounter(12345u) == 12346u);
    CHECK(advanceTimeOfDayCounter(216000u - 1u) == 0u);  // wraps at 0x34BC0
}

// ---------------------------------------------------------------- fixed light

static void testSetFixedLight() {
    FixedLight f = setFixedLight(0);
    CHECK(f.sky == 0x80f05800u);
    CHECK(f.fog == 0x80ffffffu);
    CHECK(f.fogFarParam == 0.5f);
    CHECK(f.fogFar == 800.f);

    f = setFixedLight(1);
    CHECK(f.sky == 0x80dee0dcu);
    CHECK(f.fog == 0x80404040u);
    CHECK(f.fogFarParam == 0.75f);
    CHECK(f.fogFar == 500.f);

    f = setFixedLight(4);
    CHECK(f.sky == 0x80f05800u);
    CHECK(f.fog == 0x80a5cbe9u);
    CHECK(f.fogFarParam == 1.0f);
    CHECK(f.fogFar == 400.f);

    // Out-of-range presets fall back to entry 0.
    FixedLight e0 = setFixedLight(0);
    f = setFixedLight(9);
    CHECK(f.sky == e0.sky && f.fog == e0.fog);
    CHECK(f.fogFarParam == e0.fogFarParam && f.fogFar == e0.fogFar);
    f = setFixedLight(-1);
    CHECK(f.sky == e0.sky && f.fogFar == e0.fogFar);
}

int main() {
    testWeights();
    testFog();
    testFogAdjust();
    testColours();
    testLightMatrices();
    testAdvanceTimeOfDayCounter();
    testSetFixedLight();
    if (g_fail) { std::fprintf(stderr, "%d failure(s)\n", g_fail); return 1; }
    std::puts("daynight: all checks passed");
    return 0;
}
