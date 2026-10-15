#ifndef RT_NATIVE_GAMEPLAY_GEOMETRY_H
#define RT_NATIVE_GAMEPLAY_GEOMETRY_H

#include "gameplay/vec.h"

#include <vector>

namespace rt::native::gameplay {

bool pointInConvexPolygon(Vec2 p, const std::vector<Vec2>& verts);
bool pointInConvexQuad(Vec2 p, Vec2 v0, Vec2 v1, Vec2 v2, Vec2 v3);

struct ArrivalCheck {
    float yBandLow = 169.f;
    float yBandHigh = 171.f;
    float minSpeed = 110.f;
};

enum class ZoneResult {
    NONE,
    ARRIVED,
    FAILED,
};

ZoneResult checkGoalZone(Vec2 p, float playerY, float speed,
                         const std::vector<Vec2>& quad, const ArrivalCheck& arrival);

}  // namespace rt::native::gameplay

#endif  // RT_NATIVE_GAMEPLAY_GEOMETRY_H
