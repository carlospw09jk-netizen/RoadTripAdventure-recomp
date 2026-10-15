// Pins the live-verified driving feel (tests/regression/test_car_physics.py). No game data.
#include "../../src/native/gameplay/car_physics.h"

#include <cstdio>
#include <cstdlib>

using namespace rt::native::gameplay;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static void testSteering() {
    CarState c;
    CarInput in; in.steer = 1;
    for (int i = 0; i < 32; ++i) { tickCar(c, in); CHECK(c.steeringAngle == i + 1); }
    CHECK(c.steeringAngle == 0x20);
    tickCar(c, in); CHECK(c.steeringAngle == 0x20);
    in.steer = 0;
    for (int i = 0; i < 5; ++i) { tickCar(c, in); CHECK(c.steeringAngle == 0x20); }  // standstill holds
    c.speed = 1000;
    int prev = c.steeringAngle;
    for (int i = 0; i < 8; ++i) { tickCar(c, in); CHECK(c.steeringAngle == prev - 4); prev = c.steeringAngle; }
    CHECK(c.steeringAngle == 0);
    in.steer = -1;
    for (int i = 0; i < 40; ++i) tickCar(c, in);
    CHECK(c.steeringAngle == -0x20);
}

static void testBrake() {
    CarState c; c.speed = 15000;
    CarInput in; in.brake = true;
    for (int i = 0; i < 40; ++i) { tickCar(c, in); CHECK(c.brakeRamp == (i + 1 < 32 ? i + 1 : 32)); }
    CHECK(c.brakeRamp == 32);
    in.brake = false; tickCar(c, in);
    CHECK(c.brakeRamp == 0);
}

static void testGearsRpmOdometer() {
    CarState c; CarInput in; in.gas = true;
    int maxRpm = 0, maxGear = 1, shifts = 0; std::int32_t odo = 0;
    int firstRpm = -1;
    for (int i = 0; i < 1500; ++i) {
        int g = c.gear; int rpmBefore = c.rpm;
        tickCar(c, in);
        if (firstRpm < 0) firstRpm = c.rpm;
        if (c.gear != g) { CHECK(c.gear == g + 1); ++shifts; CHECK(rpmBefore > 6000); }
        CHECK(c.rpm <= kRpmMax);
        if (c.speed > 500) CHECK(c.rpm > 0);
        CHECK(c.odometer >= odo); odo = c.odometer;
        if (c.rpm > maxRpm) maxRpm = c.rpm;
        if (c.gear > maxGear) maxGear = c.gear;
    }
    CHECK(maxGear >= 4);
    CHECK(shifts >= 3);
    CHECK(maxRpm <= 10000);
    CHECK(odo > 0);
}

static void testAirborne() {
    CarState c; c.airborne = true;
    CarInput in;
    std::int32_t prev = 0, firstStep = 0;
    for (int i = 0; i < 30; ++i) {
        tickCar(c, in);
        CHECK(c.fallSpeed[0] > prev);
        if (i == 0) { firstStep = c.fallSpeed[0]; CHECK(firstStep == 88); }
        prev = c.fallSpeed[0];
    }
    CHECK(c.fallSpeed[0] == c.fallSpeed[1] && c.fallSpeed[1] == c.fallSpeed[2]);
    c.airborne = false; tickCar(c, in, kMaterialGrass);
    CHECK(c.fallSpeed[0] == 0);
    CHECK(contactMaterial(c.contactWord[0]) == 3);
    tickCar(c, in, kMaterialAsphalt);
    CHECK(contactMaterial(c.contactWord[1]) == 0);
}

int main() {
    testSteering(); testBrake(); testGearsRpmOdometer(); testAirborne();
    if (g_fail) { std::fprintf(stderr, "%d failure(s)\n", g_fail); return 1; }
    std::puts("car_physics: all checks passed");
    return 0;
}
