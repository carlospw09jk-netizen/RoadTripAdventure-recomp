#include "gameplay/route.h"

namespace rt::native::gameplay {

RaceRoute::RaceRoute(std::vector<RouteZone> zones, std::vector<Vec2> waypoints,
                     float minArrivalSpeed)
    : zones_(std::move(zones)),
      waypoints_(std::move(waypoints)),
      minArrivalSpeed_(minArrivalSpeed) {}

int RaceRoute::zoneOf(Vec2 pos) const {
    for (std::size_t i = 0; i < zones_.size(); ++i) {
        if (pointInConvexPolygon(pos, zones_[i].quad)) return static_cast<int>(i);
    }
    return -1;
}

void RaceRoute::advance(CarProgress& car, Vec2 pos) const {
    const int nextZone = zoneOf(pos);
    if (nextZone < 0 || zones_.empty()) return;

    const int lastZone = static_cast<int>(zones_.size()) - 1;
    const int expectedZone = (car.zone + 1) % static_cast<int>(zones_.size());
    if (nextZone != expectedZone) return;

    if (car.zone == lastZone) ++car.lap;

    car.zone = nextZone;
    car.progress = static_cast<float>(car.lap * static_cast<int>(zones_.size()) + car.zone);
}

bool RaceRoute::hasArrived(const CarProgress& car, Vec2 pos, float speed) const {
    return car.zone >= 0 && car.zone == zoneOf(pos) && speed >= minArrivalSpeed_;
}

}  // namespace rt::native::gameplay
