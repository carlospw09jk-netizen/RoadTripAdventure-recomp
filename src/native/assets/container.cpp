// src/native/assets/container.cpp
//
// Implementation of the neutral host asset container serialization.
// Frozen contract: .superpowers/sdd/2026-10-14-native-asset-fixture/task-1a-brief.md
//
// Design notes (the header is frozen; these comments are for reviewers):
// - Provenance is syntactic only: an empty sourceFile, a zero length, or an
//   all-zero sha256 digest means "missing provenance". The all-zero digest is an
//   absence sentinel, not a cryptographic verification result.
// - MaterialRef::imageIndex == 0xFFFFFFFFu means "unresolved image" and is
//   explicitly allowed (e.g. for mesh-only conversion).
// - ContainerResult invariant: err != Ok => value is the default-empty
//   HostAssetContainer; err == Ok => value is a complete, validated container.
// - Wire layout written by serialize(): ASCII 'RTAC', u32 version, u32 image
//   count, u32 mesh count (16 bytes), then for each image and each mesh a u64
//   payload length followed by the exact little-endian payload. No padding.
// - serialize() throws std::invalid_argument for invalid host records;
//   deserialize() returns a structured ContainerError and never throws for
//   malformed input.
// - TrailingData is reported for non-empty bytes after the last parsed record
//   and for unparsed bytes inside a record blob (exact payload required).

#include "container.h"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rt::native::assets {

namespace {

// ---------------------------------------------------------------------------
// Wire constants
// ---------------------------------------------------------------------------
constexpr std::uint64_t kUnresolvedImage = 0xFFFFFFFFu;

constexpr std::size_t kVertexFloatCount = 8;
constexpr std::size_t kVertexColorBytes = 4;
constexpr std::size_t kVertexBytes = kVertexFloatCount * sizeof(std::uint32_t) + kVertexColorBytes; // 36
constexpr std::size_t kIndexBytes = sizeof(std::uint32_t); // 4
constexpr std::size_t kMaterialBytes = sizeof(std::uint64_t) + sizeof(std::uint64_t) + sizeof(std::uint32_t); // 20
constexpr std::size_t kSha256Bytes = 32;

// ---------------------------------------------------------------------------
// Little-endian append helpers (serialization)
// ---------------------------------------------------------------------------
void append_u8(std::vector<std::uint8_t>& out, std::uint8_t v) {
    out.push_back(v);
}

void append_u32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    append_u8(out, static_cast<std::uint8_t>(v & 0xFFu));
    append_u8(out, static_cast<std::uint8_t>((v >> 8) & 0xFFu));
    append_u8(out, static_cast<std::uint8_t>((v >> 16) & 0xFFu));
    append_u8(out, static_cast<std::uint8_t>((v >> 24) & 0xFFu));
}

void append_u64(std::vector<std::uint8_t>& out, std::uint64_t v) {
    for (int shift = 0; shift < 64; shift += 8) {
        append_u8(out, static_cast<std::uint8_t>((v >> shift) & 0xFFu));
    }
}

void append_f32(std::vector<std::uint8_t>& out, float v) {
    append_u32(out, std::bit_cast<std::uint32_t>(v));
}

void append_string(std::vector<std::uint8_t>& out, std::string_view s) {
    append_u64(out, static_cast<std::uint64_t>(s.size()));
    if (!s.empty()) {
        out.insert(out.end(),
                   reinterpret_cast<const std::uint8_t*>(s.data()),
                   reinterpret_cast<const std::uint8_t*>(s.data() + s.size()));
    }
}

void append_blob(std::vector<std::uint8_t>& out, std::span<const std::uint8_t> b) {
    append_u64(out, static_cast<std::uint64_t>(b.size()));
    if (!b.empty()) {
        out.insert(out.end(), b.begin(), b.end());
    }
}

void append_provenance(std::vector<std::uint8_t>& out, const Provenance& p) {
    append_string(out, p.sourceFile);
    append_u64(out, p.offset);
    append_u64(out, p.length);
    out.insert(out.end(), p.sha256.begin(), p.sha256.end());
}

// ---------------------------------------------------------------------------
// Host-record validation helpers (serialization path)
// ---------------------------------------------------------------------------
std::uint32_t completeMipLevels(std::uint32_t width, std::uint32_t height) {
    std::uint32_t levels = 1;
    std::uint32_t w = width;
    std::uint32_t h = height;
    while (w > 1 || h > 1) {
        w = (w > 1) ? w / 2 : 1;
        h = (h > 1) ? h / 2 : 1;
        ++levels;
    }
    return levels;
}

// Returns true if the tightly-packed RGBA8 mip chain size fits in a u64.
// width and height must be non-zero when called.
bool computeImageByteCount(std::uint32_t width,
                           std::uint32_t height,
                           std::uint32_t mipCount,
                           std::uint64_t& out) {
    std::uint64_t total = 0;
    std::uint64_t w = width;
    std::uint64_t h = height;
    for (std::uint32_t i = 0; i < mipCount; ++i) {
        if (w > std::numeric_limits<std::uint64_t>::max() / h) {
            return false;
        }
        std::uint64_t wh = w * h;
        if (wh > std::numeric_limits<std::uint64_t>::max() / 4) {
            return false;
        }
        std::uint64_t bytes = wh * 4;
        if (total > std::numeric_limits<std::uint64_t>::max() - bytes) {
            return false;
        }
        total += bytes;
        w = (w > 1) ? w / 2 : 1;
        h = (h > 1) ? h / 2 : 1;
    }
    out = total;
    return true;
}

bool isAllZeroDigest(const std::array<std::uint8_t, kSha256Bytes>& digest) {
    return std::all_of(digest.begin(), digest.end(),
                       [](std::uint8_t b) { return b == 0; });
}

bool isFiniteVertex(const Vertex& v) {
    const float fs[kVertexFloatCount] = {v.px, v.py, v.pz, v.nx, v.ny, v.nz, v.u, v.v};
    for (float f : fs) {
        if (!std::isfinite(f)) {
            return false;
        }
    }
    return true;
}

void validateImage(const ImageRecord& img) {
    if (!hasValidProvenance(img.provenance)) {
        throw std::invalid_argument("image has missing or invalid provenance");
    }
    if (img.provenance.offset > std::numeric_limits<std::uint64_t>::max() - img.provenance.length) {
        throw std::invalid_argument("image provenance offset+length overflows u64");
    }
    if (img.format != PixelFormat::R8G8B8A8_UNORM) {
        throw std::invalid_argument("image format must be R8G8B8A8_UNORM");
    }
    if (img.width == 0 || img.height == 0 || img.mipCount == 0) {
        throw std::invalid_argument("image width/height/mipCount must be positive");
    }
    if (img.mipCount > completeMipLevels(img.width, img.height)) {
        throw std::invalid_argument("image mipCount exceeds complete mip chain");
    }
    std::uint64_t expected = 0;
    if (!computeImageByteCount(img.width, img.height, img.mipCount, expected)) {
        throw std::invalid_argument("image pixel byte count overflows");
    }
    if (static_cast<std::uint64_t>(img.pixels.size()) != expected) {
        throw std::invalid_argument("image pixel blob size does not match expected mip chain");
    }
}

void validateMesh(const MeshRecord& mesh, std::size_t imageCount) {
    if (!hasValidProvenance(mesh.provenance)) {
        throw std::invalid_argument("mesh has missing or invalid provenance");
    }
    if (mesh.provenance.offset > std::numeric_limits<std::uint64_t>::max() - mesh.provenance.length) {
        throw std::invalid_argument("mesh provenance offset+length overflows u64");
    }
    for (const Vertex& v : mesh.vertices) {
        if (!isFiniteVertex(v)) {
            throw std::invalid_argument("mesh vertex contains a non-finite float");
        }
    }
    for (std::uint32_t idx : mesh.indices) {
        if (static_cast<std::uint64_t>(idx) >= mesh.vertices.size()) {
            throw std::invalid_argument("mesh index out of vertex range");
        }
    }
    for (const MaterialRef& mat : mesh.materials) {
        if (mat.firstIndex > std::numeric_limits<std::uint64_t>::max() - mat.indexCount) {
            throw std::invalid_argument("mesh material index range overflows");
        }
        if (mat.firstIndex + mat.indexCount > mesh.indices.size()) {
            throw std::invalid_argument("mesh material index range exceeds index vector");
        }
        if (mat.imageIndex != kUnresolvedImage &&
            static_cast<std::uint64_t>(mat.imageIndex) >= static_cast<std::uint64_t>(imageCount)) {
            throw std::invalid_argument("mesh material image index out of range");
        }
    }
}

// ---------------------------------------------------------------------------
// Payload encoders
// ---------------------------------------------------------------------------
std::vector<std::uint8_t> encodeImagePayload(const ImageRecord& img) {
    std::vector<std::uint8_t> out;
    append_u32(out, img.width);
    append_u32(out, img.height);
    append_u32(out, img.mipCount);
    append_u8(out, static_cast<std::uint8_t>(img.format));
    append_string(out, img.sourceVariantId);
    append_blob(out, img.pixels);
    append_provenance(out, img.provenance);
    return out;
}

std::vector<std::uint8_t> encodeMeshPayload(const MeshRecord& mesh) {
    std::vector<std::uint8_t> out;
    append_u64(out, static_cast<std::uint64_t>(mesh.vertices.size()));
    for (const Vertex& v : mesh.vertices) {
        append_f32(out, v.px);
        append_f32(out, v.py);
        append_f32(out, v.pz);
        append_f32(out, v.nx);
        append_f32(out, v.ny);
        append_f32(out, v.nz);
        append_f32(out, v.u);
        append_f32(out, v.v);
        append_u8(out, v.r);
        append_u8(out, v.g);
        append_u8(out, v.b);
        append_u8(out, v.a);
    }
    append_u64(out, static_cast<std::uint64_t>(mesh.indices.size()));
    for (std::uint32_t i : mesh.indices) {
        append_u32(out, i);
    }
    append_u64(out, static_cast<std::uint64_t>(mesh.materials.size()));
    for (const MaterialRef& mat : mesh.materials) {
        append_u64(out, mat.firstIndex);
        append_u64(out, mat.indexCount);
        append_u32(out, mat.imageIndex);
    }
    append_provenance(out, mesh.provenance);
    return out;
}

// ---------------------------------------------------------------------------
// Little-endian reader (deserialization path)
// ---------------------------------------------------------------------------
struct Reader {
    std::span<const std::uint8_t> data;
    std::size_t pos = 0;

    std::size_t size() const noexcept { return data.size(); }
    std::size_t remaining() const noexcept { return size() - pos; }
};

ContainerError read_u8(Reader& r, std::uint8_t& out) noexcept {
    if (r.pos >= r.size()) {
        return ContainerError::Truncated;
    }
    out = r.data[r.pos++];
    return ContainerError::Ok;
}

ContainerError read_u32(Reader& r, std::uint32_t& out) noexcept {
    if (r.pos + 4 > r.size()) {
        return ContainerError::Truncated;
    }
    out = static_cast<std::uint32_t>(r.data[r.pos]) |
          (static_cast<std::uint32_t>(r.data[r.pos + 1]) << 8) |
          (static_cast<std::uint32_t>(r.data[r.pos + 2]) << 16) |
          (static_cast<std::uint32_t>(r.data[r.pos + 3]) << 24);
    r.pos += 4;
    return ContainerError::Ok;
}

ContainerError read_u64(Reader& r, std::uint64_t& out) noexcept {
    if (r.pos + 8 > r.size()) {
        return ContainerError::Truncated;
    }
    out = 0;
    for (std::size_t i = 0; i < 8; ++i) {
        out |= static_cast<std::uint64_t>(r.data[r.pos + i]) << (8 * i);
    }
    r.pos += 8;
    return ContainerError::Ok;
}

ContainerError read_f32(Reader& r, float& out) noexcept {
    std::uint32_t u = 0;
    ContainerError e = read_u32(r, u);
    if (e != ContainerError::Ok) {
        return e;
    }
    out = std::bit_cast<float>(u);
    return ContainerError::Ok;
}

// Reads a length-prefixed byte span. The length itself is a u64.
// OffsetOverflow is reported before Truncated when pos+length wraps.
ContainerError read_blob(Reader& r, std::span<const std::uint8_t>& out) noexcept {
    std::uint64_t len = 0;
    ContainerError e = read_u64(r, len);
    if (e != ContainerError::Ok) {
        return e;
    }
    const std::uint64_t pos64 = r.pos;
    if (len > std::numeric_limits<std::uint64_t>::max() - pos64) {
        return ContainerError::OffsetOverflow;
    }
    const std::uint64_t end64 = pos64 + len;
    if (end64 > static_cast<std::uint64_t>(r.size())) {
        return ContainerError::Truncated;
    }
    out = r.data.subspan(static_cast<std::size_t>(pos64), static_cast<std::size_t>(len));
    r.pos = static_cast<std::size_t>(end64);
    return ContainerError::Ok;
}

ContainerError read_string(Reader& r, std::string& out) {
    std::span<const std::uint8_t> span;
    ContainerError e = read_blob(r, span);
    if (e != ContainerError::Ok) {
        return e;
    }
    out.assign(reinterpret_cast<const char*>(span.data()), span.size());
    return ContainerError::Ok;
}

ContainerError read_digest(Reader& r, std::array<std::uint8_t, kSha256Bytes>& out) noexcept {
    if (r.pos + kSha256Bytes > r.size()) {
        return ContainerError::Truncated;
    }
    for (std::size_t i = 0; i < kSha256Bytes; ++i) {
        out[i] = r.data[r.pos + i];
    }
    r.pos += kSha256Bytes;
    return ContainerError::Ok;
}

ContainerError parseProvenance(Reader& r, Provenance& p) {
    ContainerError e = read_string(r, p.sourceFile);
    if (e != ContainerError::Ok) {
        return e;
    }
    e = read_u64(r, p.offset);
    if (e != ContainerError::Ok) {
        return e;
    }
    e = read_u64(r, p.length);
    if (e != ContainerError::Ok) {
        return e;
    }
    e = read_digest(r, p.sha256);
    if (e != ContainerError::Ok) {
        return e;
    }

    // offset+length overflow takes precedence over missing-provenance checks
    // for the cases the tests isolate (valid sourceFile/length/digest with a
    // wrapping range). The contract does not order MissingProvenance against
    // OffsetOverflow, so this choice is documented in the report.
    if (p.offset > std::numeric_limits<std::uint64_t>::max() - p.length) {
        return ContainerError::OffsetOverflow;
    }
    if (p.sourceFile.empty() || p.length == 0 || isAllZeroDigest(p.sha256)) {
        return ContainerError::MissingProvenance;
    }
    return ContainerError::Ok;
}

ContainerError parseImage(Reader& r, ImageRecord& img) {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t mipCount = 0;
    std::uint8_t formatByte = 0;

    ContainerError e = read_u32(r, width);
    if (e != ContainerError::Ok) return e;
    e = read_u32(r, height);
    if (e != ContainerError::Ok) return e;
    e = read_u32(r, mipCount);
    if (e != ContainerError::Ok) return e;
    e = read_u8(r, formatByte);
    if (e != ContainerError::Ok) return e;

    if (formatByte != static_cast<std::uint8_t>(PixelFormat::R8G8B8A8_UNORM)) {
        return ContainerError::BadImageSize;
    }
    if (width == 0 || height == 0 || mipCount == 0) {
        return ContainerError::BadImageSize;
    }
    if (mipCount > completeMipLevels(width, height)) {
        return ContainerError::BadImageSize;
    }

    std::string variantId;
    e = read_string(r, variantId);
    if (e != ContainerError::Ok) return e;

    // CountOverflow for the mip-sum must be checked before the pixel blob
    // length is compared against the remaining buffer (F2).
    std::uint64_t expectedBytes = 0;
    if (!computeImageByteCount(width, height, mipCount, expectedBytes)) {
        return ContainerError::CountOverflow;
    }

    std::span<const std::uint8_t> pixels;
    e = read_blob(r, pixels);
    if (e != ContainerError::Ok) return e;

    Provenance prov;
    e = parseProvenance(r, prov);
    if (e != ContainerError::Ok) return e;

    if (static_cast<std::uint64_t>(pixels.size()) != expectedBytes) {
        return ContainerError::BadImageSize;
    }

    img.width = width;
    img.height = height;
    img.mipCount = mipCount;
    img.format = PixelFormat::R8G8B8A8_UNORM;
    img.pixels.assign(pixels.begin(), pixels.end());
    img.sourceVariantId = std::move(variantId);
    img.provenance = std::move(prov);

    // Exact payload: any unparsed bytes inside the image blob are trailing
    // data, reported with the same TrailingData code used for bytes after the
    // last record (Q3).
    if (r.pos != r.size()) {
        return ContainerError::TrailingData;
    }
    return ContainerError::Ok;
}

ContainerError parseMesh(Reader& r, MeshRecord& mesh, std::size_t imageCount) {
    std::uint64_t vertexCount = 0;
    ContainerError e = read_u64(r, vertexCount);
    if (e != ContainerError::Ok) return e;

    // CountOverflow before any allocation or remaining-bytes check.
    if (vertexCount > std::numeric_limits<std::uint64_t>::max() / kVertexBytes) {
        return ContainerError::CountOverflow;
    }
    const std::uint64_t vertexBytes = vertexCount * kVertexBytes;
    if (vertexBytes > static_cast<std::uint64_t>(r.remaining())) {
        return ContainerError::Truncated;
    }

    std::vector<Vertex> vertices;
    vertices.reserve(static_cast<std::size_t>(vertexCount));
    for (std::uint64_t i = 0; i < vertexCount; ++i) {
        Vertex v{};
        e = read_f32(r, v.px); if (e != ContainerError::Ok) return e;
        e = read_f32(r, v.py); if (e != ContainerError::Ok) return e;
        e = read_f32(r, v.pz); if (e != ContainerError::Ok) return e;
        e = read_f32(r, v.nx); if (e != ContainerError::Ok) return e;
        e = read_f32(r, v.ny); if (e != ContainerError::Ok) return e;
        e = read_f32(r, v.nz); if (e != ContainerError::Ok) return e;
        e = read_f32(r, v.u);  if (e != ContainerError::Ok) return e;
        e = read_f32(r, v.v);  if (e != ContainerError::Ok) return e;
        e = read_u8(r, v.r);   if (e != ContainerError::Ok) return e;
        e = read_u8(r, v.g);   if (e != ContainerError::Ok) return e;
        e = read_u8(r, v.b);   if (e != ContainerError::Ok) return e;
        e = read_u8(r, v.a);   if (e != ContainerError::Ok) return e;
        if (!isFiniteVertex(v)) {
            return ContainerError::NonFiniteFloat;
        }
        vertices.push_back(v);
    }

    std::uint64_t indexCount = 0;
    e = read_u64(r, indexCount);
    if (e != ContainerError::Ok) return e;
    if (indexCount > std::numeric_limits<std::uint64_t>::max() / kIndexBytes) {
        return ContainerError::CountOverflow;
    }
    const std::uint64_t indexBytes = indexCount * kIndexBytes;
    if (indexBytes > static_cast<std::uint64_t>(r.remaining())) {
        return ContainerError::Truncated;
    }
    std::vector<std::uint32_t> indices;
    indices.reserve(static_cast<std::size_t>(indexCount));
    for (std::uint64_t i = 0; i < indexCount; ++i) {
        std::uint32_t idx = 0;
        e = read_u32(r, idx);
        if (e != ContainerError::Ok) return e;
        indices.push_back(idx);
    }

    std::uint64_t materialCount = 0;
    e = read_u64(r, materialCount);
    if (e != ContainerError::Ok) return e;
    if (materialCount > std::numeric_limits<std::uint64_t>::max() / kMaterialBytes) {
        return ContainerError::CountOverflow;
    }
    const std::uint64_t materialBytes = materialCount * kMaterialBytes;
    if (materialBytes > static_cast<std::uint64_t>(r.remaining())) {
        return ContainerError::Truncated;
    }
    std::vector<MaterialRef> materials;
    materials.reserve(static_cast<std::size_t>(materialCount));
    for (std::uint64_t i = 0; i < materialCount; ++i) {
        MaterialRef mat{};
        e = read_u64(r, mat.firstIndex);  if (e != ContainerError::Ok) return e;
        e = read_u64(r, mat.indexCount);  if (e != ContainerError::Ok) return e;
        e = read_u32(r, mat.imageIndex);  if (e != ContainerError::Ok) return e;
        materials.push_back(mat);
    }

    Provenance prov;
    e = parseProvenance(r, prov);
    if (e != ContainerError::Ok) return e;

    // Index and material validation.
    for (std::uint32_t idx : indices) {
        if (static_cast<std::uint64_t>(idx) >= vertexCount) {
            return ContainerError::IndexOutOfBounds;
        }
    }
    for (const MaterialRef& mat : materials) {
        if (mat.firstIndex > std::numeric_limits<std::uint64_t>::max() - mat.indexCount) {
            return ContainerError::IndexOutOfBounds;
        }
        if (mat.firstIndex + mat.indexCount > indexCount) {
            return ContainerError::IndexOutOfBounds;
        }
        if (mat.imageIndex != kUnresolvedImage &&
            static_cast<std::uint64_t>(mat.imageIndex) >= static_cast<std::uint64_t>(imageCount)) {
            return ContainerError::MaterialImageOutOfRange;
        }
    }

    mesh.vertices = std::move(vertices);
    mesh.indices = std::move(indices);
    mesh.materials = std::move(materials);
    mesh.provenance = std::move(prov);

    if (r.pos != r.size()) {
        // Exact payload: any unparsed bytes inside the mesh blob are trailing
        // data, reported with TrailingData to match the test contract (Q3).
        return ContainerError::TrailingData;
    }
    return ContainerError::Ok;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
bool hasValidProvenance(const Provenance& provenance) {
    if (provenance.sourceFile.empty()) {
        return false;
    }
    if (provenance.length == 0) {
        return false;
    }
    if (isAllZeroDigest(provenance.sha256)) {
        return false;
    }
    return true;
}

std::vector<std::uint8_t> serialize(const HostAssetContainer& container) {
    // All serialize rejections use std::invalid_argument (N2). The version
    // check is not in the brief but is required because the wire format only
    // supports kContainerVersion.
    if (container.version != kContainerVersion) {
        throw std::invalid_argument("container version must be kContainerVersion");
    }
    if (container.images.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw std::invalid_argument("too many images for u32 count");
    }
    if (container.meshes.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw std::invalid_argument("too many meshes for u32 count");
    }

    for (const ImageRecord& img : container.images) {
        validateImage(img);
    }
    for (const MeshRecord& mesh : container.meshes) {
        validateMesh(mesh, container.images.size());
    }

    std::vector<std::uint8_t> out;

    out.push_back('R');
    out.push_back('T');
    out.push_back('A');
    out.push_back('C');
    append_u32(out, container.version);
    append_u32(out, static_cast<std::uint32_t>(container.images.size()));
    append_u32(out, static_cast<std::uint32_t>(container.meshes.size()));

    for (const ImageRecord& img : container.images) {
        std::vector<std::uint8_t> payload = encodeImagePayload(img);
        append_u64(out, static_cast<std::uint64_t>(payload.size()));
        out.insert(out.end(), payload.begin(), payload.end());
    }
    for (const MeshRecord& mesh : container.meshes) {
        std::vector<std::uint8_t> payload = encodeMeshPayload(mesh);
        append_u64(out, static_cast<std::uint64_t>(payload.size()));
        out.insert(out.end(), payload.begin(), payload.end());
    }

    return out;
}

ContainerResult deserialize(std::span<const std::uint8_t> bytes) {
    // Failure path: scratch vectors may be allocated while parsing, but every
    // count is validated against remaining bytes (or against its element-size
    // product for CountOverflow) before it drives an allocation. No allocation
    // is driven by an unchecked untrusted count. On any failure the function
    // returns early with result.value still default-empty (the ContainerResult
    // invariant). Only std::bad_alloc from genuine OOM may propagate; malformed
    // input never throws.
    Reader r{bytes};
    // ContainerResult default-initializes err to Ok and value to an empty
    // HostAssetContainer, matching the header invariant.
    ContainerResult result{};

    // A buffer shorter than the header is Truncated before magic is checked
    // (precedence: Truncated before BadMagic/BadVersion).
    if (r.size() < 16) {
        result.err = ContainerError::Truncated;
        return result;
    }

    if (r.data[0] != 'R' || r.data[1] != 'T' || r.data[2] != 'A' || r.data[3] != 'C') {
        result.err = ContainerError::BadMagic;
        return result;
    }
    r.pos = 4;

    std::uint32_t version = 0;
    ContainerError e = read_u32(r, version);
    if (e != ContainerError::Ok) {
        result.err = e;
        return result;
    }
    if (version != kContainerVersion) {
        result.err = ContainerError::BadVersion;
        return result;
    }

    std::uint32_t imageCount = 0;
    e = read_u32(r, imageCount);
    if (e != ContainerError::Ok) {
        result.err = e;
        return result;
    }
    std::uint32_t meshCount = 0;
    e = read_u32(r, meshCount);
    if (e != ContainerError::Ok) {
        result.err = e;
        return result;
    }

    if (static_cast<std::uint64_t>(imageCount) * 8 > static_cast<std::uint64_t>(r.remaining())) {
        result.err = ContainerError::Truncated;
        return result;
    }

    std::vector<ImageRecord> images;
    for (std::uint32_t i = 0; i < imageCount; ++i) {
        std::span<const std::uint8_t> payload;
        e = read_blob(r, payload);
        if (e != ContainerError::Ok) {
            result.err = e;
            return result;
        }
        Reader pr{payload};
        ImageRecord img;
        e = parseImage(pr, img);
        if (e != ContainerError::Ok) {
            result.err = e;
            return result;
        }
        images.push_back(std::move(img));
    }

    if (static_cast<std::uint64_t>(meshCount) * 8 > static_cast<std::uint64_t>(r.remaining())) {
        result.err = ContainerError::Truncated;
        return result;
    }

    std::vector<MeshRecord> meshes;
    for (std::uint32_t i = 0; i < meshCount; ++i) {
        std::span<const std::uint8_t> payload;
        e = read_blob(r, payload);
        if (e != ContainerError::Ok) {
            result.err = e;
            return result;
        }
        Reader pr{payload};
        MeshRecord mesh;
        e = parseMesh(pr, mesh, images.size());
        if (e != ContainerError::Ok) {
            result.err = e;
            return result;
        }
        meshes.push_back(std::move(mesh));
    }

    if (r.pos != r.size()) {
        result.err = ContainerError::TrailingData;
        return result;
    }

    result.value.version = kContainerVersion;
    result.value.images = std::move(images);
    result.value.meshes = std::move(meshes);
    return result;
}

} // namespace rt::native::assets
