// COURSE container header parser contracts plus an optional local-disc report.

#include "../../src/native/assets/course_data.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

using rt::native::assets::CourseDataError;
using rt::native::assets::parseCourse;

namespace {

int g_checks = 0;
int g_failures = 0;

void check(bool condition, const std::string& description) {
    ++g_checks;
    if (!condition) {
        ++g_failures;
        std::cerr << "FAIL: " << description << '\n';
    }
}

using Bytes = std::vector<std::uint8_t>;

void setU32(Bytes& bytes, std::size_t offset, std::uint32_t value) {
    for (std::size_t i = 0; i < 4; ++i) {
        bytes[offset + i] = static_cast<std::uint8_t>(value >> (8 * i));
    }
}

void testRejectsTruncatedAndInvalidHeaders() {
    check(parseCourse(Bytes(0x1f)).error() == CourseDataError::Truncated,
          "short header is truncated");

    Bytes badMagic(0x20, 0);
    setU32(badMagic, 0, 0x24);
    check(parseCourse(badMagic).error() == CourseDataError::BadHeader,
          "unsupported header size is rejected");

    Bytes unordered(0x40, 0);
    setU32(unordered, 0, 0x20);
    setU32(unordered, 4, 0x30);
    setU32(unordered, 8, 0x28);
    check(parseCourse(unordered).error() == CourseDataError::BadHeader,
          "unordered region boundaries are rejected");
}

void testParsesHeaderRegionsWithoutInventingRouteSemantics() {
    Bytes bytes(0x80, 0);
    setU32(bytes, 0, 0x20);
    setU32(bytes, 4, 0x40);
    setU32(bytes, 8, 0x60);
    setU32(bytes, 12, 0x70);
    setU32(bytes, 16, 0x80);

    const auto course = parseCourse(bytes);
    check(course.has_value(), "well-formed course header parses");
    if (!course) return;
    check(course->headerSize == 0x20, "header size retained");
    check(course->regions.size() == 4, "regions include header-to-boundary spans");
    check(course->regions[0].begin == 0x20 && course->regions[0].end == 0x40,
          "first region boundaries retained");
    check(course->regions[3].begin == 0x70 && course->regions[3].end == 0x80,
          "last region boundaries retained");
    check(course->waypoints.empty(), "header alone does not fabricate waypoints");
    check(!course->lapCount.has_value() && !course->timeLimit.has_value(),
          "header alone does not fabricate race scalars");
}

std::filesystem::path discRoot() {
    if (const char* root = std::getenv("RT_DISC")) return root;
    if (const char* home = std::getenv("HOME")) {
        return std::filesystem::path(home) / "Library/Application Support/RoadTripRecomp/disc";
    }
    return {};
}

void testRealCoursesWhenAvailable() {
    const auto directory = discRoot() / "COURSE";
    if (directory.empty() || !std::filesystem::exists(directory)) {
        std::cout << "test_course_data: SKIP real COURSE directory (not found)\n";
        return;
    }

    std::vector<std::filesystem::directory_entry> entries;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        entries.push_back(entry);
    }
    std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
        return a.path() < b.path();
    });
    for (const auto& entry : entries) {
        if (!entry.is_regular_file() || entry.path().extension() != ".BIN" ||
            entry.path().filename().string().front() != 'C') continue;
        std::ifstream input(entry.path(), std::ios::binary);
        const Bytes bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        const auto course = parseCourse(bytes);
        check(course.has_value(), entry.path().filename().string() + ": header parses");
        if (!course) continue;

        std::cout << "test_course_data: " << entry.path().filename().string() << " regions=";
        for (const auto& region : course->regions) {
            std::cout << " [0x" << std::hex << region.begin << ",0x" << region.end << ')';
        }
        std::cout << std::dec << " waypoints=" << course->waypoints.size();
        if (!course->waypoints.empty()) {
            float minX = std::numeric_limits<float>::infinity();
            float maxX = -minX;
            float minZ = minX;
            float maxZ = -minX;
            float pathLength = 0.0f;
            bool sane = true;
            for (std::size_t i = 0; i < course->waypoints.size(); ++i) {
                const auto& point = course->waypoints[i];
                minX = std::min(minX, point.x); maxX = std::max(maxX, point.x);
                minZ = std::min(minZ, point.z); maxZ = std::max(maxZ, point.z);
                sane = sane && std::abs(point.x) < 40000.0f && std::abs(point.z) < 40000.0f;
                if (i != 0) {
                    const auto& previous = course->waypoints[i - 1];
                    const float step = std::hypot(point.x - previous.x, point.z - previous.z);
                    pathLength += step;
                    sane = sane && step >= 1.0f && step <= 2000.0f;
                }
            }
            check(sane, entry.path().filename().string() + ": waypoint bounds and steps");
            std::cout << " first3=";
            for (std::size_t i = 0; i < std::min<std::size_t>(3, course->waypoints.size()); ++i) {
                std::cout << " (" << course->waypoints[i].x << ',' << course->waypoints[i].z << ')';
            }
            std::cout << " length=" << pathLength << " bbox=(" << minX << ',' << minZ
                      << ")-(" << maxX << ',' << maxZ << ')';
        }
        std::cout << '\n';
    }
}

} // namespace

int main() {
    testRejectsTruncatedAndInvalidHeaders();
    testParsesHeaderRegionsWithoutInventingRouteSemantics();
    testRealCoursesWhenAvailable();
    std::cout << "test_course_data: " << (g_checks - g_failures) << '/' << g_checks
              << " checks passed\n";
    return g_failures == 0 ? 0 : 1;
}
