// Authored car state: plain C++, no guest RAM offsets.
#pragma once
#include <cstdint>

namespace rt::native::gameplay {

constexpr int kSteerLock = 0x20;
constexpr int kSteerCenterStep = 4;
constexpr int kBrakeRampMax = 32;
constexpr int kRpmMax = 10000;
constexpr int kGearMin = 1;
constexpr int kGearMax = 5;
constexpr int kGravityPerTick = 88;

enum SurfaceMaterial : std::uint32_t {
    kMaterialAsphalt = 0,
    kMaterialDirt = 1,
    kMaterialGrass = 3,
    kMaterialSnow = 4,
    kMaterialIce = 5,
    kMaterialWater = 6,
};

struct CarInput {
    int steer = 0;  // -1, 0, +1
    bool gas = false;
    bool brake = false;
    bool reverse = false;
};

struct CarState {
    int steeringAngle = 0;   // -kSteerLock..kSteerLock
    int brakeRamp = 0;       // 0..kBrakeRampMax
    int gear = kGearMin;     // 1..5
    int rpm = 0;             // 0..kRpmMax
    std::int32_t speed = 0;  // signed forward speed
    std::int32_t odometer = 0;
    bool airborne = false;
    std::int32_t fallSpeed[3] = {0, 0, 0};  // three body points
    std::uint32_t contactWord[3] = {0, 0, 0};  // material in bits 8-10
};

}  // namespace rt::native::gameplay
