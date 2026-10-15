// Authored world/field model contract tests (src/native/gameplay/world.*):
// the 8x8-cell layout of 200-unit cells (a 1600-unit field, centres on the
// +100 lattice), the cellAt half-open lookup, the centre-distance cull, and
// the road/grass surface queries over the road waypoint path. Concrete
// numeric assertions; no game data, no framework.
#include "../../src/native/gameplay/world.h"
#include "../../src/native/gameplay/car_physics.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace rt::native::gameplay;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

// Squared distance from a cell centre to a camera position (the cull rule).
static float centerDistSq(const Cell& c, Vec2 p) {
    const Vec2 d = c.center() - p;
    return lengthSq(d);
}

// ---------------------------------------------------------------- layout

// 8x8 cells of 200 units: 64 cells, one 1600-unit field, every cell an
// aligned 200x200 AABB with its centre on the +100 lattice (100, 300, ...).
static void testCellLayout() {
    CHECK(kCellSize == 200);
    CHECK(kGridDim == 8);
    CHECK(kCellCount == 64);
    CHECK(kWorldSize == 1600.f);
    CHECK(kDefaultViewDistance == 800.f);
    CHECK(kDefaultRoadHalfWidth == 60.f);

    World w;
    CHECK(w.cells.size() == 64u);
    for (int gz = 0; gz < kGridDim; ++gz) {
        for (int gx = 0; gx < kGridDim; ++gx) {
            const Cell& c = w.cells[static_cast<std::size_t>(gz * kGridDim + gx)];
            CHECK(c.gx == gx);
            CHECK(c.gz == gz);
            CHECK(c.bounds.min.x == static_cast<float>(gx * 200));
            CHECK(c.bounds.min.z == static_cast<float>(gz * 200));
            CHECK(c.bounds.max.x == static_cast<float>((gx + 1) * 200));
            CHECK(c.bounds.max.z == static_cast<float>((gz + 1) * 200));
            // The +100 lattice: centres at 100, 300, ..., 1500.
            CHECK(c.center().x == static_cast<float>(gx * 200 + 100));
            CHECK(c.center().z == static_cast<float>(gz * 200 + 100));
            // Authored base terrain: grass under everything.
            CHECK(hasSurface(c.surface, kSurfaceGrass));
        }
    }
    // The grid spans exactly [0, 1600) x [0, 1600).
    CHECK(w.cells[0].bounds.min.x == 0.f && w.cells[0].bounds.min.z == 0.f);
    CHECK(w.cells[63].bounds.max.x == kWorldSize && w.cells[63].bounds.max.z == kWorldSize);

    // Half-open cell bounds: an edge point belongs to exactly one cell.
    CHECK(w.cells[0].bounds.contains({0.f, 0.f}));
    CHECK(w.cells[0].bounds.contains({199.9f, 199.9f}));
    CHECK(!w.cells[0].bounds.contains({200.f, 0.f}));
    CHECK(w.cells[63].bounds.contains({1599.9f, 1599.9f}));

    // The default course: a closed 4-waypoint square circuit, road half width 60.
    CHECK(w.roadWaypoints.size() == 4u);
    CHECK(w.roadClosed);
    CHECK(w.roadHalfWidth == 60.f);
    CHECK(w.roadWaypoints[0].x == 400.f && w.roadWaypoints[0].z == 400.f);
    CHECK(w.roadWaypoints[2].x == 1200.f && w.roadWaypoints[2].z == 1200.f);
}

// ---------------------------------------------------------------- cellAt

// Half-open lookup over [0, 1600): edge points, interior points, and the
// out-of-field cases (beyond the max edge, negative).
static void testCellAtEdges() {
    World w;
    CHECK(w.cellAt(0.f, 0.f) == &w.cells[0]);
    CHECK(w.cellAt(199.9f, 199.9f) == &w.cells[0]);       // last point of cell (0,0)
    CHECK(w.cellAt(200.f, 0.f) == &w.cells[1]);           // x edge -> next cell east
    CHECK(w.cellAt(0.f, 200.f) == &w.cells[8]);           // z edge -> next cell north
    CHECK(w.cellAt(799.9f, 799.9f) == &w.cells[27]);      // cell (3,3)
    CHECK(w.cellAt(800.f, 800.f) == &w.cells[36]);        // edge -> cell (4,4)
    CHECK(w.cellAt(1599.9f, 1599.9f) == &w.cells[63]);     // last cell (7,7)
    CHECK(w.cellAt(250.f, 250.f) == &w.cells[9]);         // cell (1,1)
    CHECK(w.cellAt(1210.f, 610.f) == &w.cells[30]);       // cell (6,3)

    const Cell* c = w.cellAt(1210.f, 610.f);
    CHECK(c != nullptr);
    CHECK(c->gx == 6 && c->gz == 3);
    CHECK(c->center().x == 1300.f && c->center().z == 700.f);

    CHECK(w.cellAt(1600.f, 100.f) == nullptr);   // at/after the max edge
    CHECK(w.cellAt(100.f, 1600.f) == nullptr);
    CHECK(w.cellAt(2400.f, -300.f) == nullptr);
    CHECK(w.cellAt(-0.1f, -0.1f) == nullptr);   // negative
    CHECK(w.cellAt(100.f, -5.f) == nullptr);
}

// ---------------------------------------------------------------- road cells

// The default circuit stamps an exact ring of road cells: the corridor of the
// square (400,400) -> (1200,400) -> (1200,1200) -> (400,1200) at half width
// 60 reaches rows 1,2,5,6 across columns 1..6 and rows 3,4 in columns
// 1,2,5,6 — 32 road cells of 64, grass everywhere else.
static bool expectedDefaultRoad(int gx, int gz) {
    if (gz == 0 || gz == 7) return false;  // the road never reaches the border rows
    if (gx == 0 || gx == 7) return false;  // ...or the border columns
    if (gz == 1 || gz == 2 || gz == 5 || gz == 6) return true;
    return gx == 1 || gx == 2 || gx == 5 || gx == 6;  // the two side columns, interior rows
}

static void testRoadStamping() {
    World w;
    int roadCells = 0;
    for (const Cell& c : w.cells) {
        const bool road = hasSurface(c.surface, kSurfaceRoad);
        CHECK(road == expectedDefaultRoad(c.gx, c.gz));
        if (road) {
            ++roadCells;
            // Road cells keep the grass base terrain under the road bit.
            CHECK(c.surface == (kSurfaceGrass | kSurfaceRoad));
        } else {
            CHECK(c.surface == kSurfaceGrass);
        }
    }
    CHECK(roadCells == 32);

    // Spot cells: corridor cells, corner-contact cells, interior grass.
    CHECK(hasSurface(w.cells[1 * 8 + 2].surface, kSurfaceRoad));   // (2,1) along the south road
    CHECK(hasSurface(w.cells[1 * 8 + 6].surface, kSurfaceRoad));   // (6,1) corner contact at (1200,400)
    CHECK(hasSurface(w.cells[2 * 8 + 1].surface, kSurfaceRoad));  // (1,2) along the west road
    CHECK(hasSurface(w.cells[3 * 8 + 1].surface, kSurfaceRoad));  // (1,3) west corridor, interior row
    CHECK(hasSurface(w.cells[4 * 8 + 5].surface, kSurfaceRoad));   // (5,4) east corridor
    CHECK(!hasSurface(w.cells[3 * 8 + 3].surface, kSurfaceRoad)); // (3,3) interior
    CHECK(!hasSurface(w.cells[0].surface, kSurfaceRoad));          // (0,0) far corner
    CHECK(!hasSurface(w.cells[2 * 8 + 7].surface, kSurfaceRoad));  // (7,2) just east of the road
    CHECK(!hasSurface(w.cells[7 * 8 + 7].surface, kSurfaceRoad)); // (7,7) far corner
}

// ---------------------------------------------------------------- surfaces

// surfaceAt resolves the road with the fine corridor test (half width 60,
// inclusive), falls back to the cell's base terrain, and reports no surface
// outside the field; isOnRoad is the pure corridor test; the bitmask maps to
// the car-physics ground materials.
static void testSurfaceQuery() {
    World w;
    // On the south road, at its centre-line and at the exact corridor edge.
    CHECK(w.surfaceAt(800.f, 400.f) == kSurfaceRoad);
    CHECK(w.surfaceAt(800.f, 340.f) == kSurfaceRoad);   // exactly halfWidth away: inclusive
    CHECK(w.surfaceAt(800.f, 339.f) == kSurfaceGrass); // 61 units off the road
    // The west road runs at x = 400.
    CHECK(w.surfaceAt(400.f, 800.f) == kSurfaceRoad);
    CHECK(w.surfaceAt(460.f, 800.f) == kSurfaceRoad);
    CHECK(w.surfaceAt(461.f, 800.f) == kSurfaceGrass);
    // Interior and border ground: grass.
    CHECK(w.surfaceAt(800.f, 800.f) == kSurfaceGrass);
    CHECK(w.surfaceAt(100.f, 100.f) == kSurfaceGrass);
    // A road-stamped cell is not all road: (250,250) sits in road-stamped cell
    // (1,1) but 212+ units from the path, so the ground under it is grass.
    CHECK(hasSurface(w.cellAt(250.f, 250.f)->surface, kSurfaceRoad));
    CHECK(w.surfaceAt(250.f, 250.f) == kSurfaceGrass);
    // Outside the field: no cell, no surface (material still reads as grass).
    CHECK(w.surfaceAt(1650.f, 800.f) == kSurfaceNone);
    CHECK(w.surfaceAt(-5.f, 800.f) == kSurfaceNone);

    // isOnRoad is the pure corridor test, matching surfaceAt in-field.
    CHECK(w.isOnRoad(800.f, 400.f));
    CHECK(w.isOnRoad(800.f, 340.f));
    CHECK(!w.isOnRoad(800.f, 339.f));
    CHECK(!w.isOnRoad(800.f, 800.f));
    CHECK(!w.isOnRoad(250.f, 250.f));

    // Bitmask -> physics material.
    CHECK(surfaceMaterial(kSurfaceRoad) == kMaterialAsphalt);
    CHECK(surfaceMaterial(kSurfaceGrass) == kMaterialGrass);
    CHECK(surfaceMaterial(kSurfaceDirt) == kMaterialDirt);
    CHECK(surfaceMaterial(kSurfaceSand) == kMaterialDirt);
    CHECK(surfaceMaterial(kSurfaceSnow) == kMaterialSnow);
    CHECK(surfaceMaterial(kSurfaceIce) == kMaterialIce);
    CHECK(surfaceMaterial(kSurfaceWater) == kMaterialWater);
    CHECK(surfaceMaterial(kSurfaceNone) == kMaterialGrass);
    CHECK(surfaceMaterial(kSurfaceRoad | kSurfaceGrass) == kMaterialAsphalt);  // road wins
    CHECK(surfaceMaterial(kSurfaceGrass | kSurfaceDirt) == kMaterialDirt);

    // The ground material feeds tickCar: it round-trips through the contact word.
    CHECK(w.surfaceMaterialAt(800.f, 400.f) == kMaterialAsphalt);
    CHECK(w.surfaceMaterialAt(800.f, 800.f) == kMaterialGrass);
    CHECK(w.surfaceMaterialAt(-10.f, 800.f) == kMaterialGrass);  // out of field reads as grass
    CarState car;
    tickCar(car, CarInput{0, true, false, false}, w.surfaceMaterialAt(800.f, 400.f));
    CHECK(contactMaterial(car.contactWord[0]) == kMaterialAsphalt);
    CHECK(car.speed > 0);  // one gas tick actually drove
}

// Custom courses: an open straight road stamps a two-column strip and is
// drivable along x = 800; a single waypoint is a point clearing; no waypoints
// leaves the field roadless.
static void testCustomCourses() {
    World straight({{800.f, 0.f}, {800.f, 1600.f}}, /*closed=*/false);
    CHECK(straight.roadWaypoints.size() == 2u);
    CHECK(!straight.roadClosed);
    CHECK(straight.isOnRoad(800.f, 800.f));
    CHECK(straight.isOnRoad(745.f, 300.f));   // 55 from the centre-line
    CHECK(!straight.isOnRoad(735.f, 300.f));  // 65 from the centre-line
    CHECK(!straight.isOnRoad(700.f, 800.f));
    // The corridor test is pure geometry: it holds beyond the field bounds too.
    CHECK(straight.isOnRoad(800.f, -10.f));
    CHECK(straight.surfaceAt(800.f, 800.f) == kSurfaceRoad);
    CHECK(straight.surfaceAt(700.f, 800.f) == kSurfaceGrass);
    CHECK(straight.surfaceAt(1700.f, 800.f) == kSurfaceNone);
    // Stamped cells: columns 3 and 4 (x 600..1000) across all 8 rows — 16 road cells.
    int roadCells = 0;
    for (const Cell& c : straight.cells) {
        const bool road = hasSurface(c.surface, kSurfaceRoad);
        CHECK(road == (c.gx == 3 || c.gx == 4));
        if (road) ++roadCells;
    }
    CHECK(roadCells == 16);
    CHECK(hasSurface(straight.cellAt(900.f, 100.f)->surface, kSurfaceRoad));   // cell (4,0)
    CHECK(!hasSurface(straight.cellAt(500.f, 100.f)->surface, kSurfaceRoad));  // cell (2,0)

    World clearing({{800.f, 800.f}}, /*closed=*/true);  // degenerate point corridor
    roadCells = 0;
    for (const Cell& c : clearing.cells)
        if (hasSurface(c.surface, kSurfaceRoad)) ++roadCells;
    CHECK(roadCells == 4);  // exactly the four cells meeting at (800,800)
    CHECK(clearing.isOnRoad(800.f, 800.f));
    CHECK(clearing.isOnRoad(850.f, 830.f));   // ~58.3 from the point
    CHECK(!clearing.isOnRoad(900.f, 800.f));  // 100 from the point

    World roadless({}, /*closed=*/true);  // no waypoints: no road anywhere
    roadCells = 0;
    for (const Cell& c : roadless.cells) {
        CHECK(c.surface == kSurfaceGrass);
        if (hasSurface(c.surface, kSurfaceRoad)) ++roadCells;
    }
    CHECK(roadCells == 0);
    CHECK(!roadless.isOnRoad(400.f, 400.f));
    CHECK(roadless.surfaceAt(400.f, 400.f) == kSurfaceGrass);
}

// ---------------------------------------------------------------- culling

// The cull keeps exactly the cells whose centre lies within viewDistance of
// the camera (inclusive), in fixed grid order. Centres sit at 100+200k, so
// from the field centre (800,800) the offsets are (100..700) diagonals:
// sqrt(20000) = 141.421... separates the central four from the rest, and at
// the default 800 only the twelve far-ring cells drop out (52 remain).
static void testVisibleCells() {
    World w;
    const Vec2 centre{800.f, 800.f};

    // Default view distance (fog far, 0x177F73C end of the ramp): 52 of 64.
    auto vis = w.visibleCells(centre);
    CHECK(vis.size() == 52u);
    CHECK(w.visibleCells(centre, kDefaultViewDistance).size() == 52u);

    // Membership matches the centre-distance rule exactly, both ways.
    bool seen[64] = {};
    for (const Cell& c : vis) seen[c.gz * kGridDim + c.gx] = true;
    for (const Cell& c : w.cells)
        CHECK(seen[c.gz * kGridDim + c.gx] ==
              (centerDistSq(c, centre) <= kDefaultViewDistance * kDefaultViewDistance));
    // The twelve culled cells are the border ones: the four far corners
    // (offsets (700,700) -> 989.9) plus the two cells in from each edge
    // ((700,500)/(500,700) -> 860.2). None of the road ring drops out.
    CHECK(!seen[0] && !seen[7] && !seen[56] && !seen[63]);
    CHECK(vis[0].gx == 2 && vis[0].gz == 0);  // first visible in grid order

    // Fixed grid order (the original's table walk, no depth sorting).
    int prev = -1;
    for (const Cell& c : vis) {
        const int idx = c.gz * kGridDim + c.gx;
        CHECK(idx > prev);
        prev = idx;
    }

    // The central four at exactly sqrt(20000): 141.4 culls all, 141.5 keeps four.
    CHECK(w.visibleCells(centre, 141.4f).empty());
    vis = w.visibleCells(centre, 141.5f);
    CHECK(vis.size() == 4u);
    CHECK(vis[0].gx == 3 && vis[0].gz == 3);
    CHECK(vis[1].gx == 4 && vis[1].gz == 3);
    CHECK(vis[2].gx == 3 && vis[2].gz == 4);
    CHECK(vis[3].gx == 4 && vis[3].gz == 4);

    // Inclusive boundary from a cell centre: neighbours at exactly 200 stay.
    const Vec2 sw{100.f, 100.f};  // centre of cell (0,0)
    vis = w.visibleCells(sw, 200.f);
    CHECK(vis.size() == 3u);  // (0,0), (1,0), (0,1); (1,1) is 282.8 away
    CHECK(vis[0].gx == 0 && vis[0].gz == 0);
    CHECK(vis[1].gx == 1 && vis[1].gz == 0);
    CHECK(vis[2].gx == 0 && vis[2].gz == 1);
    CHECK(w.visibleCells(sw, 199.9f).size() == 1u);  // only the host cell
    CHECK(w.visibleCells(sw, 0.f).size() == 1u);     // zero radius: the host cell only

    // Far enough to see the whole field: the farthest centre is 989.9 away.
    CHECK(w.visibleCells(centre, 1000.f).size() == 64u);
    // No cell centre is within 141.4 of the field centre (20000 > 19993.96).
    CHECK(w.visibleCells(centre, 100.f).empty());

    // Culling keeps road and grass cells alike (no per-type bias): every road
    // cell sits in the interior ring, so all 32 survive the default 800 cull
    // (the twelve culled cells are all border grass cells).
    int roadSeen = 0;
    for (const Cell& c : w.visibleCells(centre))
        if (hasSurface(c.surface, kSurfaceRoad)) ++roadSeen;
    CHECK(roadSeen == 32);
}

int main() {
    testCellLayout();
    testCellAtEdges();
    testRoadStamping();
    testSurfaceQuery();
    testCustomCourses();
    testVisibleCells();
    if (g_fail) { std::fprintf(stderr, "%d failure(s)\n", g_fail); return 1; }
    std::puts("world: all checks passed");
    return 0;
}
