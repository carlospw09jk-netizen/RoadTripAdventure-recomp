#include "world.h"

namespace rt::native::gameplay {
namespace {

// ---------------------------------------------------------------- geometry
// Exact 2D helpers for the corridor raster and the surface tests. All the
// authored coordinates are small integers, so the boundary cases (a corridor
// exactly halfWidth from a cell, a cell centre exactly viewDistance away)
// resolve exactly.

float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

// Squared distance from p to the segment [a, b] (a degenerate segment is a
// point).
float distSqPointSegment(Vec2 p, Vec2 a, Vec2 b) {
    const Vec2 ab = b - a;
    const float lenSq = lengthSq(ab);
    float t = 0.f;
    if (lenSq > 0.f) t = clampf(dot(p - a, ab) / lenSq, 0.f, 1.f);
    const Vec2 q = a + t * ab;
    return lengthSq(p - q);
}

// Squared distance from p to the closest point of the box (0 when inside).
float distSqPointAabb(Vec2 p, const Aabb2& box) {
    const float dx = p.x < box.min.x ? box.min.x - p.x
                                    : (p.x > box.max.x ? p.x - box.max.x : 0.f);
    const float dz = p.z < box.min.z ? box.min.z - p.z
                                    : (p.z > box.max.z ? p.z - box.max.z : 0.f);
    return dx * dx + dz * dz;
}

// Slab test: does the segment [a, b] meet the (closed) box?
bool segmentIntersectsAabb(Vec2 a, Vec2 b, const Aabb2& box) {
    const Vec2 d = b - a;
    float t0 = 0.f, t1 = 1.f;
    const float pa[2] = {a.x, a.z}, pd[2] = {d.x, d.z};
    const float lo[2] = {box.min.x, box.min.z}, hi[2] = {box.max.x, box.max.z};
    for (int i = 0; i < 2; ++i) {
        if (pd[i] == 0.f) {
            if (pa[i] < lo[i] || pa[i] > hi[i]) return false;  // parallel, outside
            continue;
        }
        float ta = (lo[i] - pa[i]) / pd[i];
        float tb = (hi[i] - pa[i]) / pd[i];
        if (ta > tb) {
            const float tmp = ta;
            ta = tb;
            tb = tmp;
        }
        t0 = t0 > ta ? t0 : ta;
        t1 = t1 < tb ? t1 : tb;
        if (t0 > t1) return false;
    }
    return true;
}

// Squared distance from the segment [a, b] to the box: 0 when they meet,
// otherwise the closest feature pair. Between disjoint convex sets in 2D the
// closest pair always involves a vertex, so the endpoint->box and
// corner->segment candidates cover it exactly.
float distSqSegmentAabb(Vec2 a, Vec2 b, const Aabb2& box) {
    if (segmentIntersectsAabb(a, b, box)) return 0.f;
    float best = distSqPointAabb(a, box);
    const float db = distSqPointAabb(b, box);
    if (db < best) best = db;
    const Vec2 corners[4] = {{box.min.x, box.min.z}, {box.max.x, box.min.z},
                             {box.min.x, box.max.z}, {box.max.x, box.max.z}};
    for (const Vec2 c : corners) {
        const float dc = distSqPointSegment(c, a, b);
        if (dc < best) best = dc;
    }
    return best;
}

// ---------------------------------------------------------------- road path
// The road polyline as segments: a closed circuit wraps last -> first; an
// open path stops at the last waypoint; a single waypoint is a degenerate
// point corridor; no waypoints -> no road.

template <typename Fn>
void forEachRoadSegment(const World& w, Fn&& fn) {
    const auto& wps = w.roadWaypoints;
    const std::size_t n = wps.size();
    if (n == 0) return;
    if (n == 1) {
        fn(wps[0], wps[0]);
        return;
    }
    const std::size_t count = w.roadClosed ? n : n - 1;
    for (std::size_t i = 0; i < count; ++i) fn(wps[i], wps[(i + 1) % n]);
}

// Pure corridor test: within roadHalfWidth of the road polyline.
bool inRoadCorridor(const World& w, Vec2 p) {
    const float rSq = w.roadHalfWidth * w.roadHalfWidth;
    bool hit = false;
    forEachRoadSegment(w, [&](Vec2 a, Vec2 b) {
        if (!hit && distSqPointSegment(p, a, b) <= rSq) hit = true;
    });
    return hit;
}

// ---------------------------------------------------------------- build
// Lay out the 8 x 8 grid (base terrain grass), then stamp the road bit on
// every cell the corridor reaches — boundary contact counts, so a road
// running along a cell edge marks both sides, matching the original's
// neighbour tiles sharing a boundary.
void buildCellGrid(World& w) {
    for (int gz = 0; gz < kGridDim; ++gz) {
        for (int gx = 0; gx < kGridDim; ++gx) {
            Cell& c = w.cells[static_cast<std::size_t>(gz * kGridDim + gx)];
            c.gx = static_cast<std::uint8_t>(gx);
            c.gz = static_cast<std::uint8_t>(gz);
            c.bounds.min = {static_cast<float>(gx * kCellSize), static_cast<float>(gz * kCellSize)};
            c.bounds.max = {static_cast<float>((gx + 1) * kCellSize),
                            static_cast<float>((gz + 1) * kCellSize)};
            c.surface = kSurfaceGrass;
        }
    }
    const float rSq = w.roadHalfWidth * w.roadHalfWidth;
    for (Cell& c : w.cells) {
        bool road = false;
        forEachRoadSegment(w, [&](Vec2 a, Vec2 b) {
            if (!road && distSqSegmentAabb(a, b, c.bounds) <= rSq) road = true;
        });
        if (road) c.surface |= kSurfaceRoad;
    }
}

}  // namespace

World::World() : World({{400.f, 400.f}, {1200.f, 400.f}, {1200.f, 1200.f}, {400.f, 1200.f}},
                       /*closed=*/true) {}

World::World(std::vector<Vec2> waypoints, bool closed, float halfWidth)
    : roadWaypoints(std::move(waypoints)), roadClosed(closed), roadHalfWidth(halfWidth) {
    buildCellGrid(*this);
}

const Cell* World::cellAt(float x, float z) const {
    // Half-open over [0, kWorldSize); the range check also keeps the int
    // casts below in range (and rejects NaNs).
    if (!(x >= 0.f) || !(x < kWorldSize) || !(z >= 0.f) || !(z < kWorldSize)) return nullptr;
    const int gx = static_cast<int>(x) / kCellSize;  // trunc == floor for x >= 0
    const int gz = static_cast<int>(z) / kCellSize;
    return &cells[static_cast<std::size_t>(gz * kGridDim + gx)];
}

std::vector<Cell> World::visibleCells(Vec2 cameraPos, float viewDistance) const {
    std::vector<Cell> out;
    const float rSq = viewDistance * viewDistance;
    for (const Cell& c : cells) {  // fixed grid order, no depth sorting
        const Vec2 d = c.center() - cameraPos;
        if (lengthSq(d) <= rSq) out.push_back(c);
    }
    return out;
}

std::uint32_t World::surfaceAt(float x, float z) const {
    const Cell* c = cellAt(x, z);
    if (c == nullptr) return kSurfaceNone;
    if (inRoadCorridor(*this, {x, z})) return kSurfaceRoad;
    return c->surface & ~kSurfaceRoad;  // the cell's base terrain
}

SurfaceMaterial World::surfaceMaterialAt(float x, float z) const {
    return surfaceMaterial(surfaceAt(x, z));
}

bool World::isOnRoad(float x, float z) const { return inRoadCorridor(*this, {x, z}); }

SurfaceMaterial surfaceMaterial(std::uint32_t surface) {
    if (hasSurface(surface, kSurfaceRoad)) return kMaterialAsphalt;
    if (hasSurface(surface, kSurfaceWater)) return kMaterialWater;
    if (hasSurface(surface, kSurfaceIce)) return kMaterialIce;
    if (hasSurface(surface, kSurfaceSnow)) return kMaterialSnow;
    if (hasSurface(surface, kSurfaceDirt) || hasSurface(surface, kSurfaceSand)) return kMaterialDirt;
    return kMaterialGrass;  // grass and bare ground
}

}  // namespace rt::native::gameplay
