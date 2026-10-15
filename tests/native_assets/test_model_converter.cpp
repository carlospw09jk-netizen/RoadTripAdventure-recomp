// tests/native_assets/test_model_converter.cpp
//
// Synthetic contract tests for convertCarModels (CAR model converter).
// Every input is hand-built here: a CAR header (six model starts, region
// length, file size), model headers, and DMA/VIF chains. No game bytes, no
// mocks, and no duplicated production types.
//
// - Includes the production headers src/native/assets/model_converter.h and
//   container.h via relative paths.
// - Failure accounting is plain counters and if-statements (no assert()), so
//   behaviour is NDEBUG-independent. Exit status is nonzero on any failure.
//
// NOT RUN: this file has not been compiled or executed.

#include "../../src/native/assets/container.h"
#include "../../src/native/assets/model_converter.h"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

using rt::native::assets::CarModelError;
using rt::native::assets::CarModelResult;
using rt::native::assets::MeshRecord;
using rt::native::assets::convertCarModels;
using rt::native::assets::kContainerVersion;

namespace {

int g_checks = 0;
int g_failures = 0;

void check(bool cond, const std::string& what) {
    ++g_checks;
    if (!cond) {
        ++g_failures;
        std::cerr << "FAIL: " << what << '\n';
    }
}

using Bytes = std::vector<std::uint8_t>;
using Digest = std::array<std::uint8_t, 32>;

const std::string kSourceFile = "synthetic.car";

Digest testDigest() {
    Digest d{};
    d.fill(0xAB);
    return d;
}

void putU32(Bytes& b, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) {
        b.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
    }
}

void putU64(Bytes& b, std::uint64_t v) {
    for (int i = 0; i < 8; ++i) {
        b.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
    }
}

void putFloat(Bytes& b, float f) {
    putU32(b, std::bit_cast<std::uint32_t>(f));
}

void setU32(Bytes& b, std::size_t off, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) {
        b[off + static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(v >> (8 * i));
    }
}

std::uint32_t getU32(const Bytes& b, std::size_t off) {
    return static_cast<std::uint32_t>(b[off]) |
           (static_cast<std::uint32_t>(b[off + 1]) << 8) |
           (static_cast<std::uint32_t>(b[off + 2]) << 16) |
           (static_cast<std::uint32_t>(b[off + 3]) << 24);
}

// Wraps a VIF code stream in a CNT(qwc=Q) + END DMA chain. The stream is
// zero-padded (NOP) to 16 + 16Q bytes. The CNT tag's upper qword carries
// stream[0..8), its payload carries stream[8..8+16Q), and the END tag's upper
// qword carries the final 8 stream bytes (see walkDmaTags in vif_chain.cpp).
Bytes dmaChain(Bytes stream) {
    std::size_t q = 0;
    if (stream.size() > 16) {
        q = (stream.size() - 16 + 15) / 16;
    }
    stream.resize(16 + 16 * q, 0);

    Bytes chain;
    putU64(chain, static_cast<std::uint64_t>(q) | (1ull << 28));  // CNT, qwc = q
    chain.insert(chain.end(), stream.begin(), stream.begin() + 8);
    chain.insert(chain.end(), stream.begin() + 8, stream.begin() + 8 + static_cast<std::ptrdiff_t>(16 * q));
    putU64(chain, 7ull << 28);  // END, qwc = 0
    chain.insert(chain.end(), stream.begin() + 8 + static_cast<std::ptrdiff_t>(16 * q), stream.end());
    return chain;
}

// VIF stream for one triangle batch, decoded through the entry-6 vertex
// layout (stride 3: position, colour, st) with a triangle-list primitive:
//   GIF tag UNPACK (V4-32, num 1, addr 0): NLOOP = 3, PRIM = triangle list
//   V3-32 UNPACK (num 9, addr 0): 3 vertices x {position, colour, st}
//   MSCAL imm 12 (entry = 12*8/16 = 6)
Bytes triangleStream() {
    Bytes s;
    putU32(s, 0x6C010000u);
    putU32(s, 3u);            // hdr[0]: NLOOP = 3
    putU32(s, 3u << 15);      // hdr[1]: PRIM field = 3 (triangle list)
    putU32(s, 0u);
    putU32(s, 0u);
    putU32(s, 0x68090000u);

    const float pos[3][3] = {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}};
    const float col[3][3] = {{255.0f, 0.0f, 0.0f}, {0.0f, 255.0f, 0.0f}, {0.0f, 0.0f, 255.0f}};
    const float st[3][3] = {{0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 1.0f}};
    for (int i = 0; i < 3; ++i) {
        for (int k = 0; k < 3; ++k) putFloat(s, pos[i][k]);
        for (int k = 0; k < 3; ++k) putFloat(s, col[i][k]);
        for (int k = 0; k < 3; ++k) putFloat(s, st[i][k]);
    }
    putU32(s, 0x1400000Cu);
    return s;
}

// One model: 16-byte header {partA off, partB off, cntA, cntB} followed by the
// main chain. When partChain is given it is appended after the main chain and
// partA's offset points at it (model-relative, 16-aligned); partB stays closed.
Bytes buildModel(const Bytes& mainChain, const Bytes* partChain, std::uint32_t cntA) {
    Bytes m;
    const std::uint32_t partOff =
        partChain ? static_cast<std::uint32_t>(16 + mainChain.size()) : 0u;
    putU32(m, partOff);
    putU32(m, 0u);
    putU32(m, cntA);
    putU32(m, 0u);
    m.insert(m.end(), mainChain.begin(), mainChain.end());
    if (partChain) {
        m.insert(m.end(), partChain->begin(), partChain->end());
    }
    return m;
}

// CAR file from exactly six models: 0x20-byte header, models back to back.
// word[6] (region length) and word[7] (file size) both equal the file size,
// so the texture chain is empty and the size check passes.
Bytes buildCar(const std::vector<Bytes>& models) {
    Bytes car(0x20, 0);
    std::vector<std::uint32_t> starts;
    for (const Bytes& m : models) {
        starts.push_back(static_cast<std::uint32_t>(car.size()));
        car.insert(car.end(), m.begin(), m.end());
    }
    for (std::size_t k = 0; k < starts.size(); ++k) {
        setU32(car, 4 * k, starts[k]);
    }
    const auto size = static_cast<std::uint32_t>(car.size());
    setU32(car, 0x18, size);
    setU32(car, 0x1C, size);
    return car;
}

// Model 0 holds the triangle chain; models 1..5 hold empty chains.
Bytes makeValidCar() {
    const Bytes tri = dmaChain(triangleStream());
    const Bytes empty = dmaChain({});
    std::vector<Bytes> models;
    models.push_back(buildModel(tri, nullptr, 0));
    for (int k = 1; k < 6; ++k) {
        models.push_back(buildModel(empty, nullptr, 0));
    }
    return buildCar(models);
}

CarModelResult convert(const Bytes& car) {
    return convertCarModels(car, kSourceFile, testDigest());
}

void checkFailureShape(const CarModelResult& r, CarModelError want, const std::string& tag) {
    check(r.err == want, tag + ": error code");
    check(r.value.meshes.empty(), tag + ": value.meshes empty on error");
    check(r.value.images.empty(), tag + ": value.images empty on error");
    check(r.value.version == 0, tag + ": value.version default on error");
}

void testValidTriangleModel() {
    const CarModelResult r = convert(makeValidCar());
    check(r.err == CarModelError::Ok, "valid car: err Ok");
    check(r.value.version == kContainerVersion, "valid car: container version set");
    check(r.value.images.empty(), "valid car: images stay empty (image binding out of scope)");
    check(r.value.meshes.size() == 6, "valid car: one mesh per model main chain");
    if (r.value.meshes.size() != 6) {
        return;
    }

    const MeshRecord& m = r.value.meshes[0];
    check(m.vertices.size() == 3, "tri mesh: 3 vertices");
    check(m.indices == std::vector<std::uint32_t>({0, 1, 2}), "tri mesh: indices 0,1,2");
    check(m.materials.size() == 1, "tri mesh: one material");
    if (m.vertices.size() == 3) {
        check(m.vertices[0].px == 0.0f && m.vertices[0].py == 0.0f && m.vertices[0].pz == 0.0f,
              "tri mesh: vertex 0 position");
        check(m.vertices[1].px == 1.0f && m.vertices[1].py == 0.0f, "tri mesh: vertex 1 position");
        check(m.vertices[2].px == 0.0f && m.vertices[2].py == 1.0f, "tri mesh: vertex 2 position");
        check(m.vertices[0].r == 255 && m.vertices[0].g == 0 && m.vertices[0].b == 0 &&
                  m.vertices[0].a == 255,
              "tri mesh: vertex 0 colour");
        check(m.vertices[1].g == 255 && m.vertices[1].r == 0, "tri mesh: vertex 1 colour");
        check(m.vertices[2].b == 255 && m.vertices[2].r == 0, "tri mesh: vertex 2 colour");
        check(m.vertices[1].u == 1.0f && m.vertices[1].v == 0.0f, "tri mesh: vertex 1 uv");
        check(m.vertices[2].u == 0.0f && m.vertices[2].v == 1.0f, "tri mesh: vertex 2 uv");
    }
    if (m.materials.size() == 1) {
        check(m.materials[0].firstIndex == 0, "tri mesh: material firstIndex 0");
        check(m.materials[0].indexCount == 3, "tri mesh: material indexCount 3");
        check(m.materials[0].imageIndex == 0xFFFFFFFFu, "tri mesh: material imageIndex unresolved");
    }

    const Bytes tri = dmaChain(triangleStream());
    check(m.provenance.sourceFile == kSourceFile, "tri mesh: provenance sourceFile");
    check(m.provenance.offset == 0x20 + 0x10, "tri mesh: provenance offset = model + 0x10");
    check(m.provenance.length == tri.size(), "tri mesh: provenance length = chain span");
    check(m.provenance.sha256 == testDigest(), "tri mesh: provenance sha256");

    for (std::size_t k = 1; k < r.value.meshes.size(); ++k) {
        const MeshRecord& e = r.value.meshes[k];
        check(e.vertices.empty() && e.indices.empty() && e.materials.empty(),
              "empty model " + std::to_string(k) + ": empty mesh record");
    }
}

void testTruncatedHeader() {
    const Bytes shortFile(0x1F, 0);
    checkFailureShape(convert(shortFile), CarModelError::Truncated, "truncated header");
    const CarModelResult r = convert(shortFile);
    check(r.errorOffset == 0, "truncated header: errorOffset 0");
}

void testSizeMismatch() {
    Bytes car = makeValidCar();
    setU32(car, 0x1C, static_cast<std::uint32_t>(car.size()) + 1);
    const CarModelResult r = convert(car);
    checkFailureShape(r, CarModelError::BadHeader, "size mismatch");
    check(r.errorOffset == 0x1C, "size mismatch: errorOffset 0x1C");
}

void testMonotonicViolation() {
    Bytes car = makeValidCar();
    const std::uint32_t word1 = getU32(car, 4);
    setU32(car, 8, word1 - 16);  // word[2] < word[1]
    const CarModelResult r = convert(car);
    checkFailureShape(r, CarModelError::BadHeader, "monotonic violation");
    check(r.errorOffset == 8, "monotonic violation: errorOffset 8");
}

void testMissingProvenance() {
    const Bytes car = makeValidCar();
    const CarModelResult noName = convertCarModels(car, "", testDigest());
    check(noName.err == CarModelError::MissingProvenance, "empty sourceFile: MissingProvenance");
    check(noName.value.meshes.empty(), "empty sourceFile: value empty");

    const Digest zero{};
    const CarModelResult noDigest = convertCarModels(car, kSourceFile, zero);
    check(noDigest.err == CarModelError::MissingProvenance, "zero digest: MissingProvenance");
    check(noDigest.value.meshes.empty(), "zero digest: value empty");
}

void testPartGatingZeroCount() {
    // partA offset points at a real chain, but its count word is zero: the
    // part must not be converted, so the car yields exactly the six mains.
    const Bytes tri = dmaChain(triangleStream());
    const Bytes empty = dmaChain({});
    std::vector<Bytes> models;
    models.push_back(buildModel(empty, &tri, 0));
    for (int k = 1; k < 6; ++k) {
        models.push_back(buildModel(empty, nullptr, 0));
    }
    const CarModelResult r = convert(buildCar(models));
    check(r.err == CarModelError::Ok, "part gate zero: err Ok");
    check(r.value.meshes.size() == 6, "part gate zero: no part mesh emitted");
    for (const MeshRecord& m : r.value.meshes) {
        check(m.vertices.empty(), "part gate zero: no part geometry leaked into a mesh");
    }
}

void testPartGatingNonZeroCount() {
    // Same layout with partA count = 1: the part chain (the triangle) is
    // converted right after model 0's main chain.
    const Bytes tri = dmaChain(triangleStream());
    const Bytes empty = dmaChain({});
    std::vector<Bytes> models;
    models.push_back(buildModel(empty, &tri, 1));
    for (int k = 1; k < 6; ++k) {
        models.push_back(buildModel(empty, nullptr, 0));
    }
    const CarModelResult r = convert(buildCar(models));
    check(r.err == CarModelError::Ok, "part gate nonzero: err Ok");
    check(r.value.meshes.size() == 7, "part gate nonzero: six mains plus one part mesh");
    if (r.value.meshes.size() != 7) {
        return;
    }
    check(r.value.meshes[0].vertices.empty(), "part gate nonzero: model 0 main is empty");
    const MeshRecord& part = r.value.meshes[1];
    check(part.vertices.size() == 3, "part gate nonzero: part mesh has 3 vertices");
    check(part.indices == std::vector<std::uint32_t>({0, 1, 2}), "part gate nonzero: part indices");
    // model 0 starts at 0x20; part sits at model + (16 + empty main size).
    const std::uint64_t partStart = 0x20 + 16 + empty.size();
    check(part.provenance.offset == partStart, "part gate nonzero: part provenance offset");
    check(part.provenance.length == tri.size(), "part gate nonzero: part provenance length");
}

} // namespace

int main() {
    testValidTriangleModel();
    testTruncatedHeader();
    testSizeMismatch();
    testMonotonicViolation();
    testMissingProvenance();
    testPartGatingZeroCount();
    testPartGatingNonZeroCount();

    std::cout << "test_model_converter: " << (g_checks - g_failures) << "/" << g_checks
              << " checks passed\n";
    return g_failures == 0 ? 0 : 1;
}
