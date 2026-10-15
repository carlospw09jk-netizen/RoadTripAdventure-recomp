#pragma once

// Conservative parser for Road Trip Adventure COURSE/*.BIN containers.
//
// COURSE files use the same little-endian, offset-delimited container idiom as
// FLD files, but the regions after the container header have not yet yielded a
// route record whose semantics can be demonstrated from the bytes.  This API
// deliberately reports those regions without treating render/collision floats
// as race waypoints.

#include "field_converter.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace rt::native::assets {

enum class CourseDataError : std::uint8_t {
    Ok = 0,
    Truncated,
    BadHeader,
};

struct CourseRegion {
    std::size_t begin = 0;
    std::size_t end = 0;
};

struct CourseWaypoint {
    float x = 0.0f;
    float z = 0.0f;
};

struct CourseData {
    std::uint32_t headerSize = 0;
    // Header-derived byte spans. Their content semantics are intentionally
    // unspecified until a route/control table is grounded.
    std::vector<CourseRegion> regions;
    std::vector<CourseWaypoint> waypoints;
    std::optional<std::uint32_t> lapCount;
    std::optional<std::uint32_t> timeLimit;
};

expected<CourseData, CourseDataError> parseCourse(std::span<const std::uint8_t> data);

} // namespace rt::native::assets
