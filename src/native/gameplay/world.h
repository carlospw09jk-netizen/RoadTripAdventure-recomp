// Authored world/field model: plain C++, no guest RAM offsets, no PS2 types,
// no game data.
//
// Mirrors the documented original field rendering (docs/game-mechanics/
// rendering-pipeline.md, docs/native-render.md): drawFieldTiles (0x21FB38)
// draws, per loaded tile quadrant, an 8 x 8 grid of 200 x 200-unit cells (one
// 1600-unit tile; a fieldCellTables 0x177AB20 slot keeps 64 cell chains),
// each culled by cellVisible (0x21FA38) — a frustum test of the 200-unit
// cell *centre*. The authored model keeps that layout: one 8 x 8 grid of
// 200-unit cells spanning a 1600 x 1600 field, cell centres on the +100
// lattice (100, 300, ..., 1500). With no view orientation in scope, the cull
// keeps its radial form: a cell is visible when its centre lies within the
// view distance of the camera (default 800 — the far end of the fog-far ramp
// written to fogFarParam 0x177F738/+4, i.e. 0x177F73C; ramps ~300-800), and
// cells are returned in fixed grid order, like the original's table walk (no
// depth sorting; the GS Z buffer did that).
//
// The road path (a waypoint polyline) and the terrain typing are authored;
// no game data ships here.
#pragma once

#include "car_state.h"
#include "gameplay/vec.h"

#include <array>
#include <cstdint>
#include <vector>

namespace rt::native::gameplay {

// ---------------------------------------------------------------- surfaces

// Cell type bitmask. The road bit marks cells the road corridor *reaches*
// (rendering granularity — the 200-unit cells); the surface under a point is
// resolved finer by World::surfaceAt (the corridor test), like the original's
// mesh intersection queries.
enum CellSurface : std::uint32_t {
    kSurfaceNone = 0,
    kSurfaceRoad = 1u << 0,
    kSurfaceGrass = 1u << 1,
    kSurfaceDirt = 1u << 2,
    kSurfaceSand = 1u << 3,
    kSurfaceSnow = 1u << 4,
    kSurfaceIce = 1u << 5,
    kSurfaceWater = 1u << 6,
};

inline bool hasSurface(std::uint32_t bits, std::uint32_t flag) { return (bits & flag) != 0; }

// Bitmask -> the car-physics ground material (car_state.h). The road wins;
// then hazards (water/ice/snow), then loose ground (dirt/sand); bare ground
// reads as grass.
SurfaceMaterial surfaceMaterial(std::uint32_t surface);

// ---------------------------------------------------------------- cells

// One terrain cell: a 200 x 200-unit AABB, its type bitmask, and its grid
// indices (gx east, gz north, each 0..kGridDim-1).
struct Cell {
    Aabb2 bounds;
    std::uint32_t surface = kSurfaceGrass;
    std::uint8_t gx = 0;
    std::uint8_t gz = 0;

    // The +100 lattice: cell (gx, gz)'s centre is (gx*200 + 100, gz*200 + 100).
    Vec2 center() const {
        return {(bounds.min.x + bounds.max.x) * 0.5f, (bounds.min.z + bounds.max.z) * 0.5f};
    }
};

// ---------------------------------------------------------------- world

// Layout constants, from the documented original (drawFieldTiles 0x21FB38:
// an 8 x 8 grid of 200-unit cells per tile quadrant = one 1600-unit tile).
inline constexpr int kCellSize = 200;
inline constexpr int kGridDim = 8;
inline constexpr int kCellCount = kGridDim * kGridDim;  // 64 cells (a fieldCellTables slot's 64 chains)
inline constexpr float kWorldSize = static_cast<float>(kGridDim * kCellSize);  // 1600 units

// Default view distance: the far end of the fog-far ramp (fogFarParam +4,
// 0x177F73C; distance ramps ~300-800).
inline constexpr float kDefaultViewDistance = 800.f;

// Default road corridor half width (a 120-unit road inside 200-unit cells).
inline constexpr float kDefaultRoadHalfWidth = 60.f;

struct World {
    // 64 cells, row-major over z: cells[gz * kGridDim + gx]. Built by the
    // constructors; mutate the road fields only through a new World.
    std::array<Cell, kCellCount> cells;

    // Road waypoints: a drivable polyline in world units. `roadClosed` joins
    // the last waypoint back to the first (a circuit). A single waypoint is
    // a degenerate point corridor; zero waypoints leave the field roadless.
    std::vector<Vec2> roadWaypoints;
    bool roadClosed = true;
    float roadHalfWidth = kDefaultRoadHalfWidth;

    // The authored default course: a closed square circuit of 4 waypoints
    // (400,400) -> (1200,400) -> (1200,1200) -> (400,1200) around the field
    // centre, road half width 60, grass everywhere else.
    World();

    // A custom course over the same 8 x 8 grid: grass base terrain, road
    // stamped along the corridor of the given polyline.
    World(std::vector<Vec2> waypoints, bool closed,
          float halfWidth = kDefaultRoadHalfWidth);

    // Cell lookup by world position. Half-open cell bounds over
    // [0, kWorldSize); outside the field returns nullptr.
    const Cell* cellAt(float x, float z) const;

    // Cells whose centre lies within `viewDistance` of `cameraPos` — the
    // radial form of the original's cellVisible centre test — in fixed grid
    // order (the original walks the 64 chains in table order and lets the
    // Z buffer sort).
    std::vector<Cell> visibleCells(Vec2 cameraPos,
                                   float viewDistance = kDefaultViewDistance) const;

    // Surface bitmask under a point: kSurfaceRoad when the point lies in the
    // road corridor (within roadHalfWidth of the path); otherwise the
    // containing cell's base terrain with the road bit cleared (a road-stamped
    // cell is not all road). Outside the field: kSurfaceNone.
    std::uint32_t surfaceAt(float x, float z) const;

    // The car-physics ground material under a point (tickCar's groundMaterial);
    // out-of-field ground reads as grass, like the original's open fields.
    SurfaceMaterial surfaceMaterialAt(float x, float z) const;

    // Pure road-corridor test: within roadHalfWidth of the road polyline,
    // independent of the cell grid and the field bounds (path adherence).
    bool isOnRoad(float x, float z) const;
};

}  // namespace rt::native::gameplay
