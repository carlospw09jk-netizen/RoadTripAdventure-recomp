#include "gameplay/geometry.h"

namespace rt::native::gameplay {

bool pointInConvexPolygon(Vec2 p, const std::vector<Vec2>& verts) {
    if (verts.size() < 3) return false;

    for (size_t i = 0; i < verts.size(); ++i) {
        const Vec2 v0 = verts[i];
        const Vec2 v1 = verts[(i + 1) % verts.size()];
        const float cross = (v1.z - v0.z) * (p.x - v1.x)
                          - (v1.x - v0.x) * (p.z - v1.z);
        if (cross <= 0.f) return false;
    }
    return true;
}

bool pointInConvexQuad(Vec2 p, Vec2 v0, Vec2 v1, Vec2 v2, Vec2 v3) {
    return pointInConvexPolygon(p, {v0, v1, v2, v3});
}

ZoneResult checkGoalZone(Vec2 p, float playerY, float speed,
                         const std::vector<Vec2>& quad, const ArrivalCheck& arrival) {
    if (playerY >= arrival.yBandLow && playerY < arrival.yBandHigh
        && pointInConvexPolygon(p, quad)) {
        return ZoneResult::ARRIVED;
    }
    if (playerY <= 100.f || speed <= arrival.minSpeed) return ZoneResult::FAILED;
    return ZoneResult::NONE;
}

}  // namespace rt::native::gameplay
