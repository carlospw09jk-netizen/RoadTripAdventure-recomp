// Authored race-route zone and per-car progress model.  Zone membership uses
// the ground-plane convex-polygon predicate documented for race arrivals.
#pragma once

#include "gameplay/geometry.h"

#include <vector>

namespace rt::native::gameplay {

struct RouteZone {
    std::vector<Vec2> quad;  // Four CCW vertices; polygon edges are exclusive.
};

class RaceRoute {
public:
    struct CarProgress {
        int lap = 0;
        int zone = -1;
        float progress = 0.f;
        bool arrived = false;
    };

    explicit RaceRoute(std::vector<RouteZone> zones = {},
                       std::vector<Vec2> waypoints = {},
                       float minArrivalSpeed = 117.f);

    // Returns the first containing zone, or -1 when pos is outside the route.
    [[nodiscard]] int zoneOf(Vec2 pos) const;

    // Advances only to the immediately next zone. Positions in every other
    // zone, including lower/backward or skipped zones, are ignored so progress
    // is monotonic. The ordered last-zone -> zone-0 transition increments lap.
    void advance(CarProgress& car, Vec2 pos) const;

    // Arrival requires that pos is in the car's current target zone and that
    // speed meets the configurable CheckCarArrivedAtZone-derived threshold.
    [[nodiscard]] bool hasArrived(const CarProgress& car, Vec2 pos, float speed) const;

    void setMinArrivalSpeed(float speed) { minArrivalSpeed_ = speed; }
    [[nodiscard]] float minArrivalSpeed() const { return minArrivalSpeed_; }
    [[nodiscard]] const std::vector<RouteZone>& zones() const { return zones_; }
    [[nodiscard]] const std::vector<Vec2>& waypoints() const { return waypoints_; }

private:
    std::vector<RouteZone> zones_;
    std::vector<Vec2> waypoints_;
    float minArrivalSpeed_;
};

}  // namespace rt::native::gameplay
