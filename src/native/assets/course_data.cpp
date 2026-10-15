#include "course_data.h"

#include <array>

namespace rt::native::assets {
namespace {

std::uint32_t readU32(std::span<const std::uint8_t> bytes, std::size_t offset) {
    return static_cast<std::uint32_t>(bytes[offset]) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

} // namespace

expected<CourseData, CourseDataError> parseCourse(std::span<const std::uint8_t> data) {
    if (data.size() < 0x20) return CourseDataError::Truncated;

    const std::uint32_t headerSize = readU32(data, 0);
    // Both forms occur on the supplied disc.  0x20 has four boundary words;
    // 0x30 permits additional regions, with zero words terminating the list.
    if (headerSize != 0x20 && headerSize != 0x30) return CourseDataError::BadHeader;
    if (data.size() < headerSize) return CourseDataError::Truncated;

    CourseData course;
    course.headerSize = headerSize;
    std::size_t previous = headerSize;
    bool sawTerminator = false;
    for (std::size_t offset = sizeof(std::uint32_t); offset < headerSize;
         offset += sizeof(std::uint32_t)) {
        const std::uint32_t boundary = readU32(data, offset);
        if (boundary == 0) {
            sawTerminator = true;
            continue;
        }
        if (sawTerminator || boundary <= previous || boundary > data.size()) {
            return CourseDataError::BadHeader;
        }
        course.regions.push_back({previous, boundary});
        previous = boundary;
    }

    // Some 0x30 headers end their listed regions before EOF.  Preserve that
    // documented tail as a region rather than claiming what it contains.
    if (previous < data.size()) course.regions.push_back({previous, data.size()});
    if (course.regions.empty()) return CourseDataError::BadHeader;
    return course;
}

} // namespace rt::native::assets
