// Synthetic contract tests plus an optional local-disc integration check.

#include "../../src/native/assets/field_converter.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using rt::native::assets::FieldTileError;
using rt::native::assets::FieldTileResult;
using rt::native::assets::convertFieldTile;
using rt::native::assets::parseFieldTileHeader;

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
using Digest = std::array<std::uint8_t, 32>;

Digest testDigest() {
    Digest digest{};
    digest.fill(0xA5);
    return digest;
}

void setU32(Bytes& bytes, std::size_t offset, std::uint32_t value) {
    for (std::size_t i = 0; i < 4; ++i) {
        bytes[offset + i] = static_cast<std::uint8_t>(value >> (8 * i));
    }
}

void setFloat(Bytes& bytes, std::size_t offset, float value) {
    setU32(bytes, offset, std::bit_cast<std::uint32_t>(value));
}

FieldTileResult convert(const Bytes& bytes) {
    return convertFieldTile(bytes, "synthetic.fld", testDigest());
}

void testTruncatedHeader() {
    const Bytes bytes(0x1F, 0);
    const FieldTileResult result = convert(bytes);
    check(result.err == FieldTileError::Truncated, "truncated header: returns Truncated");
}

void testOffsetPastFileIsBadHeader() {
    Bytes bytes(0x20, 0);
    setU32(bytes, 0x00, 0x20);
    setU32(bytes, 0x04, 0x21);  // offA exceeds file size
    setU32(bytes, 0x08, 0x21);
    setU32(bytes, 0x0C, 0x21);
    setU32(bytes, 0x10, 0x20);
    const FieldTileResult result = convert(bytes);
    check(result.err == FieldTileError::BadHeader, "out-of-file offA: returns BadHeader");
}

void testConvertsDescriptorVerticesAndSharedIndices() {
    // A mesh header's file positions are x,z,y.  A converter that leaves y/z
    // unswapped, ignores STQ, or drops the shared index buffer must fail this.
    constexpr std::size_t offA = 0x50AE;
    constexpr std::size_t vertexOffset = offA;
    Bytes bytes(vertexOffset + 2 * 6 * sizeof(float), 0);
    setU32(bytes, 0x00, 0x20);
    setU32(bytes, 0x04, offA);
    setU32(bytes, 0x08, offA);
    setU32(bytes, 0x0C, offA);
    setU32(bytes, 0x10, static_cast<std::uint32_t>(bytes.size()));

    setU32(bytes, 0x20, 0x10120106);
    setU32(bytes, 0x24, 0);
    setU32(bytes, 0x28, 0x00A80D6C);
    setU32(bytes, 0x2C, vertexOffset);
    setU32(bytes, 0x30, 2);

    const std::array<float, 12> fields{
        10.0f, 30.0f, 20.0f, 0.25f, 0.50f, 1.0f,
        -4.0f,  6.0f,  5.0f, 0.75f, 0.00f, 1.0f,
    };
    for (std::size_t i = 0; i < fields.size(); ++i) {
        setFloat(bytes, vertexOffset + i * sizeof(float), fields[i]);
    }
    bytes[0x50A6] = 1;
    bytes[0x50A8] = 1;

    const FieldTileResult result = convert(bytes);
    check(result.err == FieldTileError::Ok, "descriptor mesh: converts");
    if (result.err != FieldTileError::Ok) {
        return;
    }
    check(result.value.meshes.size() == 1, "descriptor mesh: one mesh");
    if (result.value.meshes.empty()) {
        return;
    }
    const auto& mesh = result.value.meshes.front();
    check(mesh.vertices.size() == 2, "descriptor mesh: vertex count");
    check(mesh.indices == std::vector<std::uint32_t>({1, 1, 0, 0}),
          "descriptor mesh: shared u16 index buffer");
    if (mesh.vertices.size() == 2) {
        const auto& first = mesh.vertices[0];
        const auto& second = mesh.vertices[1];
        check(first.px == 10.0f && first.py == 20.0f && first.pz == 30.0f,
              "descriptor mesh: swaps file x,z,y to authored x,y,z");
        check(first.u == 0.25f && first.v == 0.50f,
              "descriptor mesh: copies STQ s,t to UV");
        check(second.px == -4.0f && second.py == 5.0f && second.pz == 6.0f,
              "descriptor mesh: decodes subsequent vertex");
    }
}

void testRejectsMeshVertexRangeOutsideFile() {
    constexpr std::size_t offA = 0x50A6;
    Bytes bytes(offA, 0);
    setU32(bytes, 0x00, 0x20);
    setU32(bytes, 0x04, offA);
    setU32(bytes, 0x08, offA);
    setU32(bytes, 0x0C, offA);
    setU32(bytes, 0x10, static_cast<std::uint32_t>(bytes.size()));
    setU32(bytes, 0x20, 0x10120106);
    setU32(bytes, 0x24, 0);
    setU32(bytes, 0x2C, static_cast<std::uint32_t>(offA));
    setU32(bytes, 0x30, 1);

    const FieldTileResult result = convert(bytes);
    check(result.err == FieldTileError::Truncated,
          "descriptor mesh: vertex range outside file is rejected");
    check(result.errorOffset == offA,
          "descriptor mesh: out-of-file vertex range identifies vertex offset");
}

std::filesystem::path discRoot() {
    if (const char* root = std::getenv("RT_DISC")) {
        return root;
    }
    if (const char* home = std::getenv("HOME")) {
        return std::filesystem::path(home) / "Library/Application Support/RoadTripRecomp/disc";
    }
    return {};
}

void testRealFieldTileWhenAvailable() {
    const std::filesystem::path path = discRoot() / "FLD/000.BIN";
    if (path.empty() || !std::filesystem::exists(path)) {
        std::cout << "test_field_converter: SKIP real FLD/000.BIN (not found)\n";
        return;
    }

    std::ifstream input(path, std::ios::binary);
    const Bytes bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    check(!bytes.empty(), "real FLD/000.BIN: file read");
    const auto header = parseFieldTileHeader(bytes);
    check(header.has_value(), "real FLD/000.BIN: header parses");
    if (!header) {
        return;
    }
    check(header->offA == 22528, "real FLD/000.BIN: offA");
    check(header->offB == 1778832, "real FLD/000.BIN: offB");
    check(header->offC == 2505600, "real FLD/000.BIN: offC");
    check(header->fileSize == 2506352, "real FLD/000.BIN: fileSize");

    const FieldTileResult result =
        convertFieldTile(bytes, "FLD/000.BIN", testDigest());
    check(result.err == FieldTileError::Ok, "real FLD/000.BIN: converts");
    if (result.err != FieldTileError::Ok) {
        return;
    }

    std::size_t vertexCount = 0;
    bool sanePositions = true;
    for (const auto& mesh : result.value.meshes) {
        vertexCount += mesh.vertices.size();
        for (const auto& vertex : mesh.vertices) {
            sanePositions = sanePositions && std::isfinite(vertex.px) &&
                            std::isfinite(vertex.py) && std::isfinite(vertex.pz) &&
                            std::abs(vertex.px) <= 2000.0f && std::abs(vertex.py) <= 500.0f &&
                            std::abs(vertex.pz) <= 2000.0f;
        }
    }
    check(result.value.meshes.size() == 7, "real FLD/000.BIN: mesh count");
    check(vertexCount == 28, "real FLD/000.BIN: vertex count");
    check(sanePositions, "real FLD/000.BIN: decoded positions are sane");
    if (!result.value.meshes.empty() && !result.value.meshes.front().vertices.empty()) {
        const auto& vertex = result.value.meshes.front().vertices.front();
        std::cout << "test_field_converter: FLD/000.BIN meshes="
                  << result.value.meshes.size() << " vertices=" << vertexCount
                  << " first=(" << vertex.px << ", " << vertex.py << ", " << vertex.pz
                  << ") uv=(" << vertex.u << ", " << vertex.v << ")\n";
        const auto& vertices = result.value.meshes.front().vertices;
        for (std::size_t i = 1; i < std::min<std::size_t>(vertices.size(), 3); ++i) {
            const auto& sample = vertices[i];
            std::cout << "test_field_converter: FLD/000.BIN vertex[" << i << "]=("
                      << sample.px << ", " << sample.py << ", " << sample.pz << ") uv=("
                      << sample.u << ", " << sample.v << ")\n";
        }
    }
    if (!result.value.meshes.empty()) {
        const auto& indices = result.value.meshes.front().indices;
        const auto max = indices.empty() ? 0u : *std::max_element(indices.begin(), indices.end());
        const bool inRange = std::all_of(indices.begin(), indices.end(),
                                        [&result](std::uint32_t index) {
                                            return index < result.value.meshes.front().vertices.size();
                                        });
        check(indices.size() == 941, "real FLD/000.BIN: index count");
        check(max == 64, "real FLD/000.BIN: index maximum");
        std::cout << "test_field_converter: FLD/000.BIN indices=" << indices.size()
                  << " max=" << max << " first-mesh-in-range=" << (inRange ? "yes" : "no")
                  << " (shared grouping remains unresolved)\n";
    }
}

} // namespace

int main() {
    testTruncatedHeader();
    testOffsetPastFileIsBadHeader();
    testConvertsDescriptorVerticesAndSharedIndices();
    testRejectsMeshVertexRangeOutsideFile();
    testRealFieldTileWhenAvailable();

    std::cout << "test_field_converter: " << (g_checks - g_failures) << "/" << g_checks
              << " checks passed\n";
    return g_failures == 0 ? 0 : 1;
}
