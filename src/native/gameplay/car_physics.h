#pragma once
#include "car_state.h"

namespace rt::native::gameplay {

// One physics tick. `groundMaterial` is applied to the contact words when grounded.
void tickCar(CarState& car, const CarInput& in, std::uint32_t groundMaterial = kMaterialAsphalt);

// Contact-word helpers.
std::uint32_t makeContactWord(std::uint32_t material);
std::uint32_t contactMaterial(std::uint32_t word);

}  // namespace rt::native::gameplay
