// tests/native_assets/test_container.cpp
//
// Synthetic contract tests for the neutral asset container wire format.
// Frozen contract: .superpowers/sdd/2026-10-14-native-asset-fixture/task-1a-brief.md
// ("Frozen host wire contract for tests and the next worker").
//
// - Includes the production header src/native/assets/container.h (relative include;
//   a future CMake target can equally add `PRIVATE src` and include
//   "native/assets/container.h"). No duplicated types, no mock bodies for
//   serialize/deserialize/hasValidProvenance: those live in container.cpp, which is
//   intentionally absent for this task. This file is written to link against the
//   future implementation.
// - Every malformed input byte is assembled by the independent little-endian encoder
//   below, directly from the frozen wire contract. serialize() under test is only used
//   for the positive round-trip path. All fixture data is synthetic; no game bytes.
// - Failure helpers are plain counters and if-statements: enabled under NDEBUG
//   (no assert()).
//
// NOT RUN: compiling, linking and executing this file is prohibited for this task and
// container.cpp does not exist yet. See
// .superpowers/sdd/2026-10-14-native-asset-fixture/task-1a-tests-report.md.

#include "../../src/native/assets/container.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using rt::native::assets::ContainerError;
using rt::native::assets::ContainerResult;
using rt::native::assets::HostAssetContainer;
using rt::native::assets::ImageRecord;
using rt::native::assets::MaterialRef;
using rt::native::assets::MeshRecord;
using rt::native::assets::PixelFormat;
using rt::native::assets::Provenance;
using rt::native::assets::Vertex;
using rt::native::assets::deserialize;
using rt::native::assets::hasValidProvenance;
using rt::native::assets::kContainerVersion;
using rt::native::assets::serialize;

namespace {

// ---------------------------------------------------------------------------
// Failure accounting (NDEBUG-independent; no assert() anywhere in this file).
// ---------------------------------------------------------------------------
int g_checks = 0;
int g_failures = 0;

void check(bool cond, std::string what) {
    ++g_checks;
    if (!cond) {
        ++g_failures;
        std::cerr << "FAIL: " << what << '\n';
    }
}

} // namespace

namespace {

const char* err_name(ContainerError e) {
    switch (e) {
        case ContainerError::Ok: return "Ok";
        case ContainerError::BadMagic: return "BadMagic";
        case ContainerError::BadVersion: return "BadVersion";
        case ContainerError::Truncated: return "Truncated";
        case ContainerError::OffsetOverflow: return "OffsetOverflow";
        case ContainerError::CountOverflow: return "CountOverflow";
        case ContainerError::IndexOutOfBounds: return "IndexOutOfBounds";
        case ContainerError::MaterialImageOutOfRange: return "MaterialImageOutOfRange";
        case ContainerError::NonFiniteFloat: return "NonFiniteFloat";
        case ContainerError::BadImageSize: return "BadImageSize";
        case ContainerError::MissingProvenance: return "MissingProvenance";
        case ContainerError::TrailingData: return "TrailingData";
    }
    return "ContainerError(unknown)";
}

// deserialize must answer with structured errors; an exception is a real failure and
// is reported instead of terminating the whole run.
ContainerResult run_deserialize(const std::vector<std::uint8_t>& bytes, const std::string& what) {
    try {
        return deserialize(bytes);
    } catch (const std::exception& e) {
        check(false, what + ": deserialize threw an exception: " + e.what());
    } catch (...) {
        check(false, what + ": deserialize threw a non-std exception");
    }
    return ContainerResult{};
}

// A failed result must carry err == want and no partially accepted container
// (brief: "ContainerResult failure means err != Ok and no partially accepted
// container").
void expect_err(const std::vector<std::uint8_t>& bytes, ContainerError want, std::string what) {
    ContainerResult r = run_deserialize(bytes, what);
    check(r.err == want,
          what + ": expected " + err_name(want) + ", got " + err_name(r.err));
    check(r.value == HostAssetContainer{}, what + ": failed result carries no partially accepted container");
}

// serialize rejects invalid host records by throwing std::invalid_argument.
void check_serialize_throws(const HostAssetContainer& c, std::string what) {
    bool threw = false;
    try {
        (void)serialize(c);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, what + ": serialize must throw std::invalid_argument");
}

// ---------------------------------------------------------------------------
// Independent little-endian encoder (frozen wire contract; never raw struct
// layouts). Used only to author fixture bytes by hand.
// ---------------------------------------------------------------------------
void put_u32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    out.push_back(static_cast<std::uint8_t>(v & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFFu));
}

void put_u64(std::vector<std::uint8_t>& out, std::uint64_t v) {
    for (int shift = 0; shift < 64; shift += 8) {
        out.push_back(static_cast<std::uint8_t>((v >> shift) & 0xFFu));
    }
}

void put_f32(std::vector<std::uint8_t>& out, float v) {
    put_u32(out, std::bit_cast<std::uint32_t>(v));
}

void put_str(std::vector<std::uint8_t>& out, std::string_view s) {
    put_u64(out, static_cast<std::uint64_t>(s.size()));
    out.insert(out.end(), s.begin(), s.end());
}

void put_blob(std::vector<std::uint8_t>& out, const std::vector<std::uint8_t>& b) {
    put_u64(out, static_cast<std::uint64_t>(b.size()));
    out.insert(out.end(), b.begin(), b.end());
}

void put_vertex(std::vector<std::uint8_t>& out, const Vertex& v) {
    // eight f32 components (px py pz nx ny nz u v) then four color bytes = 36 bytes.
    put_f32(out, v.px);
    put_f32(out, v.py);
    put_f32(out, v.pz);
    put_f32(out, v.nx);
    put_f32(out, v.ny);
    put_f32(out, v.nz);
    put_f32(out, v.u);
    put_f32(out, v.v);
    out.push_back(v.r);
    out.push_back(v.g);
    out.push_back(v.b);
    out.push_back(v.a);
}

// Provenance on the wire: sourceFile string, u64 offset, u64 length, 32 digest bytes.
void encode_provenance(std::vector<std::uint8_t>& out, const Provenance& p) {
    put_str(out, p.sourceFile);
    put_u64(out, p.offset);
    put_u64(out, p.length);
    out.insert(out.end(), p.sha256.begin(), p.sha256.end());
}

// Image payload: u32 width, height, mipCount; u8 PixelFormat; sourceVariantId string;
// pixel blob (u64 length + bytes); provenance.
std::vector<std::uint8_t> encode_image_payload(const ImageRecord& img) {
    std::vector<std::uint8_t> out;
    put_u32(out, img.width);
    put_u32(out, img.height);
    put_u32(out, img.mipCount);
    out.push_back(static_cast<std::uint8_t>(img.format));
    put_str(out, img.sourceVariantId);
    put_blob(out, img.pixels);
    encode_provenance(out, img.provenance);
    return out;
}

// Mesh payload: u64 vertex count; vertices; u64 index count; u32 indices;
// u64 material count; materials (u64 firstIndex, u64 indexCount, u32 imageIndex);
// provenance.
std::vector<std::uint8_t> encode_mesh_payload(const MeshRecord& m) {
    std::vector<std::uint8_t> out;
    put_u64(out, static_cast<std::uint64_t>(m.vertices.size()));
    for (const Vertex& v : m.vertices) {
        put_vertex(out, v);
    }
    put_u64(out, static_cast<std::uint64_t>(m.indices.size()));
    for (std::uint32_t i : m.indices) {
        put_u32(out, i);
    }
    put_u64(out, static_cast<std::uint64_t>(m.materials.size()));
    for (const MaterialRef& mat : m.materials) {
        put_u64(out, mat.firstIndex);
        put_u64(out, mat.indexCount);
        put_u32(out, mat.imageIndex);
    }
    encode_provenance(out, m.provenance);
    return out;
}

// Header: [0..3] ASCII 'RTAC', [4..7] u32 version, [8..11] u32 image count,
// [12..15] u32 mesh count (16 bytes).
std::vector<std::uint8_t> encode_header(std::uint32_t version, std::size_t images, std::size_t meshes) {
    std::vector<std::uint8_t> out{'R', 'T', 'A', 'C'};
    put_u32(out, version);
    put_u32(out, static_cast<std::uint32_t>(images));
    put_u32(out, static_cast<std::uint32_t>(meshes));
    return out;
}

// Header + one length-prefixed blob per payload, images then meshes.
std::vector<std::uint8_t> container_with(const std::vector<std::vector<std::uint8_t>>& image_payloads,
                                          const std::vector<std::vector<std::uint8_t>>& mesh_payloads) {
    std::vector<std::uint8_t> out = encode_header(kContainerVersion, image_payloads.size(), mesh_payloads.size());
    for (const auto& p : image_payloads) {
        put_blob(out, p);
    }
    for (const auto& p : mesh_payloads) {
        put_blob(out, p);
    }
    return out;
}

std::vector<std::uint8_t> encode_container(const HostAssetContainer& c) {
    std::vector<std::uint8_t> out = encode_header(c.version, c.images.size(), c.meshes.size());
    for (const ImageRecord& img : c.images) {
        put_blob(out, encode_image_payload(img));
    }
    for (const MeshRecord& m : c.meshes) {
        put_blob(out, encode_mesh_payload(m));
    }
    return out;
}

// Defensive LE reader for the header-byte checks; returns a value that fails every
// expectation instead of reading out of bounds.
std::uint32_t read_u32le(const std::vector<std::uint8_t>& v, std::size_t pos) {
    if (pos + 4 > v.size()) {
        return 0xFFFFFFFFu;
    }
    return static_cast<std::uint32_t>(v[pos]) | (static_cast<std::uint32_t>(v[pos + 1]) << 8) |
           (static_cast<std::uint32_t>(v[pos + 2]) << 16) | (static_cast<std::uint32_t>(v[pos + 3]) << 24);
}

// ---------------------------------------------------------------------------
// Synthetic fixtures (independently authored; no game bytes).
// ---------------------------------------------------------------------------
const std::vector<std::uint8_t> kPixels2x2{
    0x10, 0x20, 0x30, 0xFF, 0x40, 0x50, 0x60, 0xFF,
    0x70, 0x80, 0x90, 0xFF, 0xA0, 0xB0, 0xC0, 0xFF,
}; // 2x2 RGBA8 = 4 pixels x 4 bytes = 16 bytes

Provenance make_provenance(std::string source, std::uint64_t offset, std::uint64_t length, std::uint8_t seed) {
    Provenance p{};
    p.sourceFile = std::move(source);
    p.offset = offset;
    p.length = length;
    for (std::size_t i = 0; i < p.sha256.size(); ++i) {
        p.sha256[i] = static_cast<std::uint8_t>(seed + i); // seed >= 1 => never zero
    }
    return p;
}

ImageRecord make_image_2x2() {
    return ImageRecord{
        .width = 2,
        .height = 2,
        .mipCount = 1,
        .format = PixelFormat::R8G8B8A8_UNORM,
        .pixels = kPixels2x2,
        .sourceVariantId = "synthetic-variant-0",
        .provenance = make_provenance("synthetic/fixture_image.bin", 4096, 16, 0x01),
    };
}

std::vector<Vertex> make_vertices_3() {
    // Every float component is nonzero and finite so a zero-filled or broken
    // round-trip cannot compare equal by accident.
    Vertex v0{};
    v0.px = 1.5f;   v0.py = -2.5f;  v0.pz = 3.75f;
    v0.nx = 0.5f;   v0.ny = 0.25f;  v0.nz = 0.875f;
    v0.u = 0.125f;  v0.v = 0.875f;
    v0.r = 9; v0.g = 88; v0.b = 177; v0.a = 244;

    Vertex v1{};
    v1.px = -4.0f;  v1.py = 5.5f;   v1.pz = 6.25f;
    v1.nx = 0.75f;  v1.ny = 0.625f; v1.nz = 0.375f;
    v1.u = 0.625f;  v1.v = 0.25f;
    v1.r = 200; v1.g = 150; v1.b = 100; v1.a = 128;

    Vertex v2{};
    v2.px = 7.125f; v2.py = 8.5f;   v2.pz = -9.25f;
    v2.nx = 0.33333334f; v2.ny = 0.6666667f; v2.nz = 0.9f;
    v2.u = 0.5f;    v2.v = 0.125f;
    v2.r = 1; v2.g = 2; v2.b = 3; v2.a = 4;

    return {v0, v1, v2};
}

MeshRecord make_mesh_3v() {
    return MeshRecord{
        .vertices = make_vertices_3(),
        .indices = {0, 1, 2},
        .materials = {MaterialRef{.firstIndex = 0, .indexCount = 3, .imageIndex = 0}},
        .provenance = make_provenance("synthetic/fixture_mesh.bin", 8192, 148, 0x90),
    };
}

// Valid fixture container: version 1 (not the struct default 0), one 2x2 image,
// one 3-vertex mesh.
HostAssetContainer make_valid_container() {
    return HostAssetContainer{
        .version = kContainerVersion,
        .images = {make_image_2x2()},
        .meshes = {make_mesh_3v()},
    };
}

// Image payload carrying an arbitrary (possibly invalid) provenance; all other image
// fields are the valid 2x2/1-mip/RGBA8/16-byte fixture.
std::vector<std::uint8_t> image_payload_with_provenance(const Provenance& p) {
    ImageRecord img = make_image_2x2();
    img.provenance = p;
    return encode_image_payload(img);
}

// Mesh payload carrying an arbitrary (possibly invalid) provenance. All other fields
// are the valid 3-vertex fixture. The material list is cleared: these payloads go
// into containers with zero images, and the fixture material's imageIndex 0 would be
// an out-of-range image reference there (MaterialImageOutOfRange), stealing the
// case's single intended error.
std::vector<std::uint8_t> mesh_payload_with_provenance(const Provenance& p) {
    MeshRecord m = make_mesh_3v();
    m.materials.clear();
    m.provenance = p;
    return encode_mesh_payload(m);
}

} // namespace

// ---------------------------------------------------------------------------
// test_roundtrip: serialize -> independent-encoder agreement -> deserialize,
// comparing every field with the defaulted == operators (plus explicit spot
// checks so a failure is diagnosable).
// ---------------------------------------------------------------------------
namespace {

void test_roundtrip() {
    const HostAssetContainer fixture = make_valid_container();
    check(fixture.version == kContainerVersion, "fixture uses kContainerVersion (1), not the struct default 0");

    // Fixture invariant: three finite, nonzero vertex attribute sets including UVs.
    bool verts_ok = fixture.meshes.size() == 1;
    if (verts_ok) {
        for (const Vertex& v : fixture.meshes[0].vertices) {
            const float fs[8] = {v.px, v.py, v.pz, v.nx, v.ny, v.nz, v.u, v.v};
            for (float f : fs) {
                if (f == 0.0f || f != f ||
                    f == std::numeric_limits<float>::infinity() ||
                    f == -std::numeric_limits<float>::infinity()) {
                    verts_ok = false;
                }
            }
        }
    }
    check(verts_ok, "fixture vertices are nonzero and finite on all 8 float components");
    check(kPixels2x2.size() == 16, "fixture image carries exactly 16 pixel bytes (2x2 RGBA8)");

    std::vector<std::uint8_t> bytes;
    bool serialized = false;
    try {
        bytes = serialize(fixture);
        serialized = true;
    } catch (const std::exception& e) {
        check(false, std::string("serialize(valid fixture) threw: ") + e.what());
    }
    if (!serialized) {
        return;
    }

    // Frozen header layout check against serialize's output.
    check(bytes.size() >= 16, "serialized container is at least the 16-byte header");
    if (bytes.size() >= 16) {
        check(bytes[0] == 'R' && bytes[1] == 'T' && bytes[2] == 'A' && bytes[3] == 'C',
              "serialized magic bytes are ASCII 'RTAC'");
        check(read_u32le(bytes, 4) == 1u, "serialized header version field == 1");
        check(read_u32le(bytes, 8) == 1u, "serialized header image count == 1");
        check(read_u32le(bytes, 12) == 1u, "serialized header mesh count == 1");
    }

    // serialize must agree byte-for-byte with the independent encoder built from the
    // frozen contract (explicit LE fields, no raw struct layout).
    const std::vector<std::uint8_t> expected = encode_container(fixture);
    check(bytes == expected, "serialize output matches the independent LE encoder byte-for-byte");

    ContainerResult r = run_deserialize(bytes, "roundtrip");
    check(r.err == ContainerError::Ok, std::string("roundtrip deserialize: expected Ok, got ") + err_name(r.err));
    if (r.err != ContainerError::Ok) {
        return;
    }
    check(r.value == fixture, "roundtrip container equals the input (defaulted ==, every field)");
    check(r.value.version == 1u, "roundtrip version == 1");

    if (r.value.images.size() != 1 || r.value.meshes.size() != 1) {
        check(false, "roundtrip has exactly 1 image and 1 mesh");
        return;
    }

    const ImageRecord& img = r.value.images[0];
    check(img.width == 2u && img.height == 2u && img.mipCount == 1u, "image width/height/mipCount survive");
    check(img.format == PixelFormat::R8G8B8A8_UNORM, "image PixelFormat survives");
    check(img.pixels == kPixels2x2, "all 16 authored pixel bytes survive");
    check(img.sourceVariantId == "synthetic-variant-0", "image sourceVariantId survives");
    check(img.provenance == fixture.images[0].provenance, "image provenance (offset/length/sha256) survives");
    check(img.provenance.sha256[0] != 0, "image digest is the nonzero 32-byte fixture digest");

    const MeshRecord& mesh = r.value.meshes[0];
    check(mesh.vertices.size() == 3, "mesh has 3 vertices after roundtrip");
    if (mesh.vertices.size() == 3) {
        const Vertex& w0 = mesh.vertices[0];
        check(w0.px == 1.5f && w0.py == -2.5f && w0.pz == 3.75f, "vertex0 px/py/pz survive");
        check(w0.nx == 0.5f && w0.ny == 0.25f && w0.nz == 0.875f, "vertex0 normals survive");
        check(w0.u == 0.125f && w0.v == 0.875f, "vertex0 UVs survive");
        check(w0.r == 9 && w0.g == 88 && w0.b == 177 && w0.a == 244, "vertex0 RGBA bytes survive");
        check(mesh.vertices[2].px == 7.125f && mesh.vertices[2].u == 0.5f,
              "vertex2 position/UV survive");
    }
    check(mesh.indices == std::vector<std::uint32_t>{0, 1, 2}, "indices are exactly {0,1,2}");
    check(mesh.materials.size() == 1, "mesh has 1 material");
    if (mesh.materials.size() == 1) {
        check(mesh.materials[0].firstIndex == 0u && mesh.materials[0].indexCount == 3u,
              "material firstIndex/indexCount survive");
        check(mesh.materials[0].imageIndex == 0u, "material imageIndex 0 (references image 0) survives");
    }
    check(mesh.provenance == fixture.meshes[0].provenance, "mesh provenance survives");
}

// ---------------------------------------------------------------------------
// Header-level error cases, hand-encoded.
// ---------------------------------------------------------------------------
void test_bad_magic_rejected() {
    // Correct version and counts, single magic byte flipped: only BadMagic applies.
    std::vector<std::uint8_t> bytes = encode_header(kContainerVersion, 0, 0);
    bytes[0] = 'X';
    expect_err(bytes, ContainerError::BadMagic, "bad magic (first byte 'X')");
}

void test_bad_version_rejected() {
    // Correct magic, version != 1 (struct default 0 and a future 2): BadVersion.
    expect_err(encode_header(0, 0, 0), ContainerError::BadVersion, "version 0");
    expect_err(encode_header(2, 0, 0), ContainerError::BadVersion, "version 2");
}

void test_truncated_rejected() {
    // (a) Header cut mid-field: 10 of 16 bytes present, magic and version intact.
    std::vector<std::uint8_t> cut = encode_header(kContainerVersion, 0, 0);
    cut.resize(10);
    expect_err(cut, ContainerError::Truncated, "header cut to 10 bytes");

    // (b) Blob length claim (u64 = 1000) larger than the bytes present (a complete
    // valid 147-byte image payload). A finite claimed range beyond the remaining
    // input => Truncated; this holds whether the length check runs before or after
    // payload parsing (the payload itself is valid, so only the claim is wrong).
    std::vector<std::uint8_t> bytes = encode_header(kContainerVersion, 1, 0);
    put_u64(bytes, 1000);
    const std::vector<std::uint8_t> payload = encode_image_payload(make_image_2x2());
    bytes.insert(bytes.end(), payload.begin(), payload.end());
    expect_err(bytes, ContainerError::Truncated, "blob claims 1000 bytes, 147 present");
}

void test_pointer_overflow_rejected() {
    // Provenance offset+length addition that overflows u64 => OffsetOverflow.
    // Every other provenance/image field is valid, so no other error applies.
    Provenance over = make_provenance("synthetic/overflow.bin", ~std::uint64_t(0) - 7, 16, 0x21);
    expect_err(container_with({image_payload_with_provenance(over)}, {}),
               ContainerError::OffsetOverflow, "image provenance offset+length overflows u64");

    Provenance over_mesh = make_provenance("synthetic/overflow_mesh.bin", ~std::uint64_t(0), 1, 0x31);
    // The mesh payload carries no materials (see mesh_payload_with_provenance), so in
    // this zero-image container no material image reference competes with the
    // intended single error.
    expect_err(container_with({}, {mesh_payload_with_provenance(over_mesh)}),
               ContainerError::OffsetOverflow, "mesh provenance offset+length overflows u64");
    // NOTE (reported, not tested): a string length field of u64::MAX inside a payload
    // makes both OffsetOverflow (position+length wraps) and Truncated (range beyond
    // remaining input) apply; the precedence between them is a decision for the
    // implementation reviewer, so no test asserts it.
}

void test_count_overflow_rejected() {
    // Hostile mesh vertex count 2^62: count * 36 = 9 * 2^64, which wraps u64
    // (to 0). The overflow must be detected as CountOverflow before any reserve or
    // allocation; the mesh payload carries only the 8-byte count, so a correct
    // implementation never reads further and never allocates.
    std::vector<std::uint8_t> payload;
    put_u64(payload, std::uint64_t(1) << 62);
    expect_err(container_with({}, {payload}), ContainerError::CountOverflow,
               "vertex count 2^62 overflows count*36");
}

void test_nonfinite_float_rejected() {
    // One vertex whose px is +inf (then NaN in the second case); indices and
    // materials are empty and provenance is valid, so NonFiniteFloat is the only
    // applicable error.
    auto payload_with_px = [](float bad_px) {
        Vertex v{};
        v.px = bad_px;
        v.py = 1.5f;
        v.pz = -2.25f;
        v.nx = 0.5f;
        v.ny = 0.25f;
        v.nz = 0.75f;
        v.u = 0.125f;
        v.v = 0.875f;
        v.r = 1; v.g = 2; v.b = 3; v.a = 4;
        std::vector<std::uint8_t> p;
        put_u64(p, 1); // vertex count
        put_vertex(p, v);
        put_u64(p, 0); // index count
        put_u64(p, 0); // material count
        encode_provenance(p, make_provenance("synthetic/nonfinite_mesh.bin", 16, 36, 0x41));
        return p;
    };
    expect_err(container_with({}, {payload_with_px(std::numeric_limits<float>::infinity())}),
               ContainerError::NonFiniteFloat, "vertex px = +inf");
    expect_err(container_with({}, {payload_with_px(std::numeric_limits<float>::quiet_NaN())}),
               ContainerError::NonFiniteFloat, "vertex px = NaN");
}

void test_bad_image_size_rejected() {
    // Each sub-case violates exactly one rule of the image contract, so no error
    // precedence decision is exercised:
    // (a) 2x2/1 mip/RGBA8 with 15 pixel bytes instead of 16 => BadImageSize.
    ImageRecord short_pixels = make_image_2x2();
    short_pixels.pixels.pop_back();
    expect_err(container_with({encode_image_payload(short_pixels)}, {}),
               ContainerError::BadImageSize, "15 pixel bytes for 2x2 RGBA8 (needs 16)");

    // (b) width = 0 with an empty pixel blob (blob matches 0*2*4), so only the
    // dimension rule is broken.
    ImageRecord zero_width = make_image_2x2();
    zero_width.width = 0;
    zero_width.pixels.clear();
    expect_err(container_with({encode_image_payload(zero_width)}, {}),
               ContainerError::BadImageSize, "width = 0");

    // (c) mipCount = 4 beyond the complete chain for 2x2 (levels (2,2),(1,1) = 2).
    // The blob holds the floor-halving sum for 4 levels (16+4+4+4 = 28), so the only
    // violated rule is the mip-count range (any reading of the sum still yields
    // BadImageSize).
    ImageRecord too_many_mips = make_image_2x2();
    too_many_mips.mipCount = 4;
    too_many_mips.pixels.assign(28, 0x5A);
    expect_err(container_with({encode_image_payload(too_many_mips)}, {}),
               ContainerError::BadImageSize, "mipCount 4 beyond the 2x2 chain (2 levels)");

    // (d) Wire format byte 0x07: not the declared R8G8B8A8_UNORM (0) byte. Written
    // as a raw byte, not as an invented PixelFormat variant.
    std::vector<std::uint8_t> payload;
    put_u32(payload, 2);
    put_u32(payload, 2);
    put_u32(payload, 1);
    payload.push_back(0x07);
    put_str(payload, "synthetic-variant-0");
    put_blob(payload, kPixels2x2);
    encode_provenance(payload, make_provenance("synthetic/fixture_image.bin", 4096, 16, 0x01));
    expect_err(container_with({payload}, {}), ContainerError::BadImageSize, "pixel format byte 0x07");
}

void test_missing_provenance_rejected() {
    const Provenance valid = make_provenance("synthetic/ok.bin", 1, 16, 0x01);
    check(hasValidProvenance(valid), "valid provenance accepted by hasValidProvenance");

    Provenance empty_file = valid;
    empty_file.sourceFile.clear();
    Provenance zero_length = valid;
    zero_length.length = 0;
    Provenance zero_digest = valid;
    zero_digest.sha256.fill(0);

    // Form 1: empty sourceFile. Form 2: zero length. Form 3: all-zero digest
    // (missing-provenance sentinel, not cryptographic verification).
    check(!hasValidProvenance(empty_file), "empty sourceFile => hasValidProvenance false");
    check(!hasValidProvenance(zero_length), "zero length => hasValidProvenance false");
    check(!hasValidProvenance(zero_digest), "all-zero digest => hasValidProvenance false");

    // serialize rejects each invalid record with std::invalid_argument: on an image...
    {
        HostAssetContainer c = make_valid_container();
        c.images[0].provenance = empty_file;
        check_serialize_throws(c, "image provenance with empty sourceFile");
    }
    {
        HostAssetContainer c = make_valid_container();
        c.images[0].provenance = zero_length;
        check_serialize_throws(c, "image provenance with zero length");
    }
    {
        HostAssetContainer c = make_valid_container();
        c.images[0].provenance = zero_digest;
        check_serialize_throws(c, "image provenance with all-zero digest");
    }
    // ...and on a mesh record too.
    {
        HostAssetContainer c = make_valid_container();
        c.meshes[0].provenance = zero_length;
        check_serialize_throws(c, "mesh provenance with zero length");
    }

    // Malformed byte input (payloads otherwise fully valid) => MissingProvenance,
    // one hand-encoded buffer per form. The mesh payload carries no materials, so
    // its zero-image container stays single-error (only the provenance form is
    // violated).
    expect_err(container_with({image_payload_with_provenance(empty_file)}, {}),
               ContainerError::MissingProvenance, "bytes: image provenance with empty sourceFile");
    expect_err(container_with({image_payload_with_provenance(zero_length)}, {}),
               ContainerError::MissingProvenance, "bytes: image provenance with zero length");
    expect_err(container_with({image_payload_with_provenance(zero_digest)}, {}),
               ContainerError::MissingProvenance, "bytes: image provenance with all-zero digest");
    expect_err(container_with({}, {mesh_payload_with_provenance(empty_file)}),
               ContainerError::MissingProvenance, "bytes: mesh provenance with empty sourceFile");
}

void test_index_out_of_bounds_rejected() {
    // (a) index >= vertex count: indices {0,1,3} against 3 vertices, no materials,
    // so IndexOutOfBounds is the only applicable error.
    auto payload_with_bad_index = [](std::uint32_t last_index) {
        std::vector<std::uint8_t> p;
        const std::vector<Vertex> verts = make_vertices_3();
        put_u64(p, 3);
        for (const Vertex& v : verts) {
            put_vertex(p, v);
        }
        put_u64(p, 3);
        put_u32(p, 0);
        put_u32(p, 1);
        put_u32(p, last_index);
        put_u64(p, 0); // material count
        encode_provenance(p, make_provenance("synthetic/bad_index.bin", 108, 12, 0x51));
        return p;
    };
    expect_err(container_with({}, {payload_with_bad_index(3)}), ContainerError::IndexOutOfBounds,
               "index 3 with 3 vertices");

    // (b) Material index range beyond the index vector: firstIndex 2 + indexCount 3
    // = 5 > 3 (indices size). The addition does not overflow, the material image is
    // the allowed unresolved sentinel, so IndexOutOfBounds is the only applicable
    // error.
    std::vector<std::uint8_t> p;
    const std::vector<Vertex> verts = make_vertices_3();
    put_u64(p, 3);
    for (const Vertex& v : verts) {
        put_vertex(p, v);
    }
    put_u64(p, 3);
    put_u32(p, 0);
    put_u32(p, 1);
    put_u32(p, 2);
    put_u64(p, 1); // material count
    put_u64(p, 2); // firstIndex
    put_u64(p, 3); // indexCount (2 + 3 = 5 > 3 indices)
    put_u32(p, 0xFFFFFFFFu); // unresolved sentinel image, always allowed
    encode_provenance(p, make_provenance("synthetic/bad_range.bin", 108, 20, 0x61));
    expect_err(container_with({}, {p}), ContainerError::IndexOutOfBounds,
               "material range 2+3 beyond 3 indices");
}

void test_material_image_out_of_range_rejected() {
    // Container has 0 images; material imageIndex 7 is neither the unresolved
    // sentinel nor < image count. The material range itself is valid
    // (0 + 3 <= 3 indices), so MaterialImageOutOfRange is the only applicable error.
    std::vector<std::uint8_t> p;
    const std::vector<Vertex> verts = make_vertices_3();
    put_u64(p, 3);
    for (const Vertex& v : verts) {
        put_vertex(p, v);
    }
    put_u64(p, 3);
    put_u32(p, 0);
    put_u32(p, 1);
    put_u32(p, 2);
    put_u64(p, 1); // material count
    put_u64(p, 0); // firstIndex
    put_u64(p, 3); // indexCount
    put_u32(p, 7); // imageIndex 7 with 0 images
    encode_provenance(p, make_provenance("synthetic/bad_matref.bin", 108, 20, 0x71));
    expect_err(container_with({}, {p}), ContainerError::MaterialImageOutOfRange,
               "material imageIndex 7 with 0 images");
}

void test_unresolved_image_sentinel_allowed() {
    // 0xFFFFFFFFu imageIndex is explicitly allowed for mesh-only conversion.
    MeshRecord mesh = make_mesh_3v();
    mesh.materials[0].imageIndex = 0xFFFFFFFFu;
    const HostAssetContainer fixture{
        .version = kContainerVersion,
        .images = {},
        .meshes = {mesh},
    };

    const std::vector<std::uint8_t> bytes = encode_container(fixture);
    ContainerResult r = run_deserialize(bytes, "sentinel");
    check(r.err == ContainerError::Ok, std::string("sentinel container: expected Ok, got ") + err_name(r.err));
    if (r.err == ContainerError::Ok) {
        check(r.value == fixture, "sentinel container round-trips through hand-encoded bytes");
        if (r.value.meshes.size() == 1 && r.value.meshes[0].materials.size() == 1) {
            check(r.value.meshes[0].materials[0].imageIndex == 0xFFFFFFFFu,
                  "material keeps the unresolved sentinel 0xFFFFFFFFu");
        } else {
            check(false, "sentinel container keeps 1 mesh with 1 material");
        }
    }

    // serialize must accept the sentinel record too.
    std::vector<std::uint8_t> rt_bytes;
    bool serialized = false;
    try {
        rt_bytes = serialize(fixture);
        serialized = true;
    } catch (const std::exception& e) {
        check(false, std::string("serialize(sentinel fixture) threw: ") + e.what());
    }
    if (serialized) {
        ContainerResult r2 = run_deserialize(rt_bytes, "sentinel roundtrip");
        check(r2.err == ContainerError::Ok,
              std::string("sentinel serialize->deserialize: expected Ok, got ") + err_name(r2.err));
        if (r2.err == ContainerError::Ok) {
            check(r2.value == fixture, "sentinel container survives serialize->deserialize");
        }
    }
}

void test_trailing_data_rejected() {
    // A complete valid hand-encoded container plus one stray byte: not silently
    // accepted, not misreported as BadImageSize => dedicated TrailingData.
    std::vector<std::uint8_t> bytes = encode_container(make_valid_container());
    bytes.push_back(0xAB);
    expect_err(bytes, ContainerError::TrailingData, "one trailing byte after a valid container");
}

void test_valid_multi_mip_image() {
    // 4x2 with the complete floor-halving chain (4,2)->(2,1)->(1,1):
    // 4*2*4 + 2*1*4 + 1*1*4 = 32 + 8 + 4 = 44 pixel bytes.
    ImageRecord img{
        .width = 4,
        .height = 2,
        .mipCount = 3,
        .format = PixelFormat::R8G8B8A8_UNORM,
        .pixels = std::vector<std::uint8_t>(44),
        .sourceVariantId = "synthetic-mip-chain",
        .provenance = make_provenance("synthetic/mip_image.bin", 256, 44, 0xB1),
    };
    for (std::size_t i = 0; i < img.pixels.size(); ++i) {
        img.pixels[i] = static_cast<std::uint8_t>(0xC0 + i * 3);
    }
    const HostAssetContainer fixture{
        .version = kContainerVersion,
        .images = {img},
        .meshes = {},
    };

    // Hand-encoded bytes deserialize to the full record...
    ContainerResult r = run_deserialize(encode_container(fixture), "multi-mip");
    check(r.err == ContainerError::Ok, std::string("multi-mip image: expected Ok, got ") + err_name(r.err));
    if (r.err == ContainerError::Ok) {
        check(r.value == fixture, "multi-mip image equals the fixture after deserialization");
        if (!r.value.images.empty()) {
            check(r.value.images[0].mipCount == 3u, "mipCount 3 preserved");
            check(r.value.images[0].pixels.size() == 44, "44 mip pixel bytes preserved");
        }
    }

    // ...and the same container survives serialize -> deserialize.
    std::vector<std::uint8_t> bytes;
    bool serialized = false;
    try {
        bytes = serialize(fixture);
        serialized = true;
    } catch (const std::exception& e) {
        check(false, std::string("serialize(multi-mip fixture) threw: ") + e.what());
    }
    if (serialized) {
        ContainerResult r2 = run_deserialize(bytes, "multi-mip roundtrip");
        check(r2.err == ContainerError::Ok,
              std::string("multi-mip roundtrip: expected Ok, got ") + err_name(r2.err));
        if (r2.err == ContainerError::Ok) {
            check(r2.value == fixture, "multi-mip container round-trips all fields");
        }
    }
}

} // namespace

int main() {
    test_roundtrip();
    test_bad_magic_rejected();
    test_bad_version_rejected();
    test_truncated_rejected();
    test_pointer_overflow_rejected();
    test_count_overflow_rejected();
    test_nonfinite_float_rejected();
    test_bad_image_size_rejected();
    test_missing_provenance_rejected();
    test_index_out_of_bounds_rejected();
    test_material_image_out_of_range_rejected();
    test_unresolved_image_sentinel_allowed();
    test_trailing_data_rejected();
    test_valid_multi_mip_image();

    if (g_failures > 0) {
        std::cerr << "test_container: " << g_failures << " of " << g_checks << " checks FAILED\n";
        return EXIT_FAILURE;
    }
    std::cout << "test_container: " << g_checks << " checks passed\n";
    return EXIT_SUCCESS;
}
