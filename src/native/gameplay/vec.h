#ifndef RT_NATIVE_GAMEPLAY_VEC_H
#define RT_NATIVE_GAMEPLAY_VEC_H

#include <cmath>

namespace rt::native::gameplay {

struct Vec2 {
    float x = 0.f;
    float z = 0.f;
};

inline Vec2 operator+(Vec2 a, Vec2 b) { return {a.x + b.x, a.z + b.z}; }
inline Vec2 operator-(Vec2 a, Vec2 b) { return {a.x - b.x, a.z - b.z}; }
inline Vec2 operator*(Vec2 a, float s) { return {a.x * s, a.z * s}; }
inline Vec2 operator*(float s, Vec2 a) { return a * s; }
inline float dot(Vec2 a, Vec2 b) { return a.x * b.x + a.z * b.z; }
inline float length(Vec2 a) { return std::sqrt(dot(a, a)); }
inline float lengthSq(Vec2 a) { return dot(a, a); }

struct Vec3 {
    float x = 0.f, y = 0.f, z = 0.f;
};

inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 operator*(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline Vec3 operator*(float s, Vec3 a) { return a * s; }
inline float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline float length(Vec3 a) { return std::sqrt(dot(a, a)); }
inline Vec3 normalized(Vec3 a) {
    const float l = length(a);
    return l > 1e-12f ? a * (1.f / l) : Vec3{0.f, 0.f, 0.f};
}

// Axis-aligned rectangle on the ground plane.
struct Aabb2 {
    Vec2 min;
    Vec2 max;

    // Half-open: [min.x, max.x) x [min.z, max.z) — the cell-grid convention,
    // so a point on a shared edge belongs to exactly one cell.
    bool contains(Vec2 p) const {
        return p.x >= min.x && p.x < max.x && p.z >= min.z && p.z < max.z;
    }
};

}  // namespace rt::native::gameplay

#endif  // RT_NATIVE_GAMEPLAY_VEC_H
