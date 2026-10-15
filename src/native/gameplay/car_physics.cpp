#include "car_physics.h"

#include <cstdlib>

namespace rt::native::gameplay {
namespace {

constexpr int kAccel = 40;
constexpr int kReverseMax = -4000;
constexpr int kSpeedMax = 20000;
constexpr int kBrakeDecel = 8;      // per ramp step
constexpr int kCoast = 4;
constexpr int kGearSpeed = 3600;    // speed span per gear
constexpr int kUpshiftRpm = 8500;   // "near the top of the range"
constexpr int kFallDragDiv = 1 << 20;

int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

void steer(CarState& c, const CarInput& in) {
    if (in.steer != 0) {
        c.steeringAngle = clampi(c.steeringAngle + (in.steer > 0 ? 1 : -1), -kSteerLock, kSteerLock);
    } else if (c.speed != 0) {  // self-centre only while rolling
        int a = c.steeringAngle;
        if (a > 0) a = a > kSteerCenterStep ? a - kSteerCenterStep : 0;
        else if (a < 0) a = a < -kSteerCenterStep ? a + kSteerCenterStep : 0;
        c.steeringAngle = a;
    }
}

void drive(CarState& c, const CarInput& in) {
    c.brakeRamp = in.brake ? (c.brakeRamp < kBrakeRampMax ? c.brakeRamp + 1 : kBrakeRampMax) : 0;
    int v = c.speed;
    if (in.brake && c.brakeRamp > 0) {
        int d = kBrakeDecel * c.brakeRamp / 4 + 1;
        v = v > 0 ? (v > d ? v - d : 0) : (v < -d ? v + d : 0);
    } else if (in.reverse) {
        v = v > kReverseMax ? v - kAccel : v;
    } else if (in.gas) {
        v = v < kSpeedMax ? v + kAccel : v;
    } else {
        v = v > 0 ? (v > kCoast ? v - kCoast : 0) : (v < -kCoast ? v + kCoast : 0);
    }
    c.speed = v;

    int av = std::abs(v);
    // Gear/rpm: rpm climbs through each gear's speed span, single-step upshifts at the top.
    int inGear = av - (c.gear - 1) * kGearSpeed;
    int rpm = av == 0 ? (in.gas ? 800 : 0) : 1000 + inGear * (kRpmMax - 1000) / kGearSpeed;
    if (c.gear < kGearMax && rpm >= kUpshiftRpm) {
        ++c.gear;
        inGear = av - (c.gear - 1) * kGearSpeed;
        rpm = 1000 + (inGear > 0 ? inGear : 0) * (kRpmMax - 1000) / kGearSpeed + 3000;
    } else if (c.gear > kGearMin && av < (c.gear - 1) * kGearSpeed - 1000) {
        --c.gear;
    }
    if (av != 0 && rpm < 1) rpm = 1;
    c.rpm = clampi(rpm, 0, kRpmMax);

    c.odometer += av;
}

void fall(CarState& c, std::uint32_t groundMaterial) {
    if (!c.airborne) {
        for (int i = 0; i < 3; ++i) {
            c.fallSpeed[i] = 0;
            c.contactWord[i] = makeContactWord(groundMaterial);
        }
        return;
    }
    for (int i = 0; i < 3; ++i) {
        std::int64_t f = c.fallSpeed[i];
        f += kGravityPerTick - (f * f) / kFallDragDiv;  // drag tapers the gain
        c.fallSpeed[i] = static_cast<std::int32_t>(f);
        c.contactWord[i] = 0;  // no ground contact in the air
    }
}

}  // namespace

std::uint32_t makeContactWord(std::uint32_t material) { return (material & 7u) << 8; }
std::uint32_t contactMaterial(std::uint32_t word) { return (word >> 8) & 7u; }

void tickCar(CarState& car, const CarInput& in, std::uint32_t groundMaterial) {
    steer(car, in);
    drive(car, in);
    fall(car, groundMaterial);
}

}  // namespace rt::native::gameplay
