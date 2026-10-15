// VIF/DMA chain decoder for RoadTripAdventure CAR model chains.
//
// This is a host-native, read-only decoder. It does NOT emulate the PS2 VIF/GIF/GS;
// it only understands the on-disk layout that tools/model_dump.cpp already decodes.
//
// Strictness: malformed, truncated, or count-overflowing chains return a structured
// error with NO partial batches. This is an intentional divergence from the lenient
// research dumper in tools/model_dump.cpp, which zero-fills missing bytes and keeps
// previously decoded batches.

#include "vif_chain.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <vector>

namespace rt::native::assets {
namespace {

// ----------------------------------------------------------------------------
// Little-endian integer/float helpers
// ----------------------------------------------------------------------------

std::uint32_t readU32LE(std::span<const std::uint8_t> b, std::size_t o) {
    return static_cast<std::uint32_t>(b[o]) |
           (static_cast<std::uint32_t>(b[o + 1]) << 8) |
           (static_cast<std::uint32_t>(b[o + 2]) << 16) |
           (static_cast<std::uint32_t>(b[o + 3]) << 24);
}

float u32ToFloat(std::uint32_t u) {
    float f = 0.0f;
    static_assert(sizeof(f) == sizeof(u));
    std::memcpy(&f, &u, sizeof(f));
    return f;
}

std::uint8_t floatToColour(std::uint32_t word) {
    const float f = u32ToFloat(word);
    if (!std::isfinite(f)) {
        return 0;
    }
    const float c = std::clamp(f, 0.0f, 255.0f);
    return static_cast<std::uint8_t>(std::floor(c + 0.5f));
}

// ----------------------------------------------------------------------------
// DMA tag walk
// ----------------------------------------------------------------------------

// Build the VIF code stream from a raw DMA chain. The stream is the concatenation
// of every tag's upper 8 bytes followed by its payload, exactly as
// tools/model_dump.cpp:84-151 builds it. Base is 0 for file-relative parsing.
struct DmaWalkResult {
    bool ok = false;
    VifChainError::Code code = VifChainError::Code::Ok;
    std::vector<std::uint8_t> stream;
};

DmaWalkResult walkDmaTags(std::span<const std::uint8_t> chain) {
    DmaWalkResult result;
    const std::size_t n = chain.size();
    std::size_t o = 0;
    std::vector<std::size_t> stack;

    constexpr int kMaxTags = 4096;
    for (int tags = 0; tags < kMaxTags; ++tags) {
        if (o + 16 > n) {
            result.code = VifChainError::Code::Truncated;
            return result;
        }

        const std::uint64_t lo = static_cast<std::uint64_t>(readU32LE(chain, o)) |
                                 (static_cast<std::uint64_t>(readU32LE(chain, o + 4)) << 32);
        const std::uint32_t qwc = static_cast<std::uint32_t>(lo & 0xFFFFu);
        const std::uint32_t id = static_cast<std::uint32_t>((lo >> 28) & 7u);
        const std::uint32_t addr = static_cast<std::uint32_t>((lo >> 32) & 0x7FFFFFFFu);

        const std::uint64_t payloadBytes = static_cast<std::uint64_t>(qwc) * 16u;

        auto append = [&](std::size_t from, std::uint64_t len) -> bool {
            if (len > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
                return false;
            }
            const std::size_t l = static_cast<std::size_t>(len);
            if (from > n || l > n - from) {
                return false;
            }
            result.stream.insert(result.stream.end(), chain.begin() + from, chain.begin() + from + l);
            return true;
        };

        // Every tag contributes its upper 8 bytes (the VIF code qword) to the stream.
        if (!append(o + 8, 8)) {
            result.code = VifChainError::Code::Truncated;
            return result;
        }

        const std::size_t target = static_cast<std::size_t>(addr); // base == 0 for file chains

        switch (id) {
        case 1: { // CNT
            if (!append(o + 16, payloadBytes)) {
                result.code = VifChainError::Code::Truncated;
                return result;
            }
            const std::uint64_t advance = 16u + payloadBytes;
            if (advance > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()) - o) {
                result.code = VifChainError::Code::Overflow;
                return result;
            }
            o += static_cast<std::size_t>(advance);
            break;
        }
        case 3:  // REF
        case 4:  // REFS
            if (!append(target, payloadBytes)) {
                result.code = VifChainError::Code::Truncated;
                return result;
            }
            o += 16;
            break;
        case 0: { // REFE
            if (!append(target, payloadBytes)) {
                result.code = VifChainError::Code::Truncated;
                return result;
            }
            result.ok = true;
            return result;
        }
        case 2: { // NEXT
            if (!append(o + 16, payloadBytes)) {
                result.code = VifChainError::Code::Truncated;
                return result;
            }
            o = target;
            break;
        }
        case 5: { // CALL
            if (!append(o + 16, payloadBytes)) {
                result.code = VifChainError::Code::Truncated;
                return result;
            }
            const std::uint64_t ret = static_cast<std::uint64_t>(o) + 16u + payloadBytes;
            if (ret > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
                result.code = VifChainError::Code::Overflow;
                return result;
            }
            stack.push_back(static_cast<std::size_t>(ret));
            o = target;
            break;
        }
        case 6: { // RET
            if (!append(o + 16, payloadBytes)) {
                result.code = VifChainError::Code::Truncated;
                return result;
            }
            if (stack.empty()) {
                result.ok = true;
                return result;
            }
            o = stack.back();
            stack.pop_back();
            break;
        }
        default: { // END (id 7)
            if (!append(o + 16, payloadBytes)) {
                result.code = VifChainError::Code::Truncated;
                return result;
            }
            result.ok = true;
            return result;
        }
        }
    }

    // Exceeded the tag cap without hitting a terminator.
    result.code = VifChainError::Code::Overflow;
    return result;
}

// ----------------------------------------------------------------------------
// VIF UNPACK / code helpers
// ----------------------------------------------------------------------------

struct BatchState {
    std::uint32_t hdr[4] = {};
    bool hasHdr = false;
    std::vector<std::uint32_t> words;
    std::uint32_t vecs = 0;
};

void reset(BatchState& b) {
    b = BatchState();
}

// UNPACK component-count and bit-width tables (model_dump.cpp:181-183).
struct UnpackTables {
    static constexpr int comps[16] = {1, 1, 1, 0, 2, 2, 2, 0, 3, 3, 3, 0, 4, 4, 4, 4};
    static constexpr int bits[16] = {32, 16, 8, 0, 32, 16, 8, 0, 32, 16, 8, 0, 32, 32, 32, 5};
};

constexpr int UnpackTables::comps[16];
constexpr int UnpackTables::bits[16];

std::size_t unpackSize(std::uint32_t vnvl, std::uint32_t n) {
    const std::uint64_t raw = static_cast<std::uint64_t>(UnpackTables::comps[vnvl & 15]) *
                              static_cast<std::uint64_t>(UnpackTables::bits[vnvl & 15]) *
                              static_cast<std::uint64_t>(n);
    return static_cast<std::size_t>((raw + 31u) / 32u * 4u);
}

// ----------------------------------------------------------------------------
// Vertex assembly
// ----------------------------------------------------------------------------

enum class VertexLayoutResult { Ok, Unsupported, Overflow };

VertexLayoutResult buildVertices(int entry,
                                 const std::vector<std::uint32_t>& words,
                                 std::uint32_t nloop,
                                 std::vector<Vertex>& out) {
    std::uint32_t stride = 0;
    std::uint32_t first = 0;
    std::uint32_t posVec = 0;
    std::uint32_t normalVec = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t colVec = 0;
    std::uint32_t stVec = 0;

    switch (entry) {
    case 2:
    case 5:
        stride = 4;
        posVec = 0;
        normalVec = 1;
        colVec = 2;
        stVec = 3;
        break;
    case 4:
        stride = 5;
        posVec = 0;
        colVec = 1;
        stVec = 4;
        break;
    case 6:
        stride = 3;
        posVec = 0;
        colVec = 1;
        stVec = 2;
        break;
    case 3:
        stride = 5;
        first = 1;
        posVec = 0;
        colVec = 1;
        stVec = 4;
        break;
    default:
        return VertexLayoutResult::Unsupported;
    }

    const std::uint32_t vecs = static_cast<std::uint32_t>(words.size() / 3);
    if (nloop > 0 && first + stride * nloop > vecs) {
        return VertexLayoutResult::Overflow;
    }

    float centre[3] = {0.0f, 0.0f, 0.0f};
    if (entry == 3 && nloop > 0) {
        for (int k = 0; k < 3; ++k) {
            centre[k] = u32ToFloat(words[(posVec) * 3 + k]);
        }
    }

    out.reserve(out.size() + nloop);
    for (std::uint32_t i = 0; i < nloop; ++i) {
        const std::uint32_t base = first + i * stride;
        Vertex vtx{};

        for (int k = 0; k < 3; ++k) {
            vtx.px = u32ToFloat(words[(base + posVec) * 3 + 0]);
            vtx.py = u32ToFloat(words[(base + posVec) * 3 + 1]);
            vtx.pz = u32ToFloat(words[(base + posVec) * 3 + 2]);
        }

        if (entry == 3) {
            vtx.px += centre[0];
            vtx.py += centre[1];
            vtx.pz += centre[2];
        }

        if (normalVec != std::numeric_limits<std::uint32_t>::max()) {
            vtx.nx = u32ToFloat(words[(base + normalVec) * 3 + 0]);
            vtx.ny = u32ToFloat(words[(base + normalVec) * 3 + 1]);
            vtx.nz = u32ToFloat(words[(base + normalVec) * 3 + 2]);
        }

        vtx.r = floatToColour(words[(base + colVec) * 3 + 0]);
        vtx.g = floatToColour(words[(base + colVec) * 3 + 1]);
        vtx.b = floatToColour(words[(base + colVec) * 3 + 2]);
        vtx.a = 255;

        float s = u32ToFloat(words[(base + stVec) * 3 + 0]);
        float t = u32ToFloat(words[(base + stVec) * 3 + 1]);
        const float q = u32ToFloat(words[(base + stVec) * 3 + 2]);
        if (entry != 2 && q != 0.0f && std::isfinite(q)) {
            s /= q;
            t /= q;
        }
        vtx.u = s;
        vtx.v = t;

        out.push_back(vtx);
    }

    return VertexLayoutResult::Ok;
}

void buildIndices(std::uint8_t prim, std::uint32_t nloop, std::vector<std::uint32_t>& out) {
    out.clear();
    const std::uint8_t type = prim & 7u;

    if (nloop < 3) {
        return;
    }

    switch (type) {
    case 3: // triangle list
        for (std::uint32_t i = 0; i + 2 < nloop; i += 3) {
            out.push_back(i);
            out.push_back(i + 1);
            out.push_back(i + 2);
        }
        break;
    case 5: // triangle fan
        for (std::uint32_t i = 0; i + 2 < nloop; ++i) {
            out.push_back(0);
            out.push_back(i + 1);
            out.push_back(i + 2);
        }
        break;
    default: // triangle strip (and any other PRIM type)
        for (std::uint32_t i = 0; i + 2 < nloop; ++i) {
            std::uint32_t a = i;
            std::uint32_t b = i + 1;
            std::uint32_t c = i + 2;
            if (i & 1u) {
                std::swap(b, c);
            }
            out.push_back(a);
            out.push_back(b);
            out.push_back(c);
        }
        break;
    }
}

// ----------------------------------------------------------------------------
// Main VIF decode over the walked stream
// ----------------------------------------------------------------------------

VifChainResult decodeStream(std::vector<std::uint8_t> stream) {
    VifChainResult result;
    const std::size_t total = stream.size();
    std::size_t o = 0;

    BatchState cur;
    int material = -1;

    auto readStreamU32 = [&](std::size_t at) -> std::uint32_t {
        if (at + 4 > total) {
            return 0;
        }
        return readU32LE(std::span<const std::uint8_t>(stream.data(), total), at);
    };

    while (o + 4 <= total) {
        const std::uint32_t code = readStreamU32(o);
        const std::uint32_t cmd = (code >> 24) & 0x7Fu;
        const std::uint32_t num = (code >> 16) & 0xFFu;
        const std::uint32_t imm = code & 0xFFFFu;
        o += 4;

        if (cmd >= 0x60) { // UNPACK family
            const std::uint32_t vnvl = cmd & 0xFu;
            const std::uint32_t n = num ? num : 256;
            const std::uint32_t addr = imm & 0x3FFu;
            const std::size_t size = unpackSize(vnvl, n);

            if (o + size > total) {
                result.error = VifChainError::Code::Truncated;
                result.batches.clear();
                return result;
            }

            if (vnvl == 0xC && n == 1 && addr == 0) {
                // Output GIF tag: NLOOP, PRIM, etc.
                for (int i = 0; i < 4; ++i) {
                    cur.hdr[i] = readStreamU32(o + static_cast<std::size_t>(i) * 4);
                }
                cur.hasHdr = true;
            } else if (vnvl == 0x8 && addr == 0x192) {
                // Material A+D block: vector 0 is the GIF tag, the rest are (data64, reg).
                // We only need the sticky material index; the register list is not retained.
                ++material;
            } else if (vnvl == 0x8) {
                // Vertex block: raw V3-32 vectors.
                cur.words.assign(static_cast<std::size_t>(n) * 3, 0);
                for (std::uint32_t i = 0; i < n * 3; ++i) {
                    cur.words[i] = readStreamU32(o + static_cast<std::size_t>(i) * 4);
                }
                cur.vecs = n;

                if (addr == 0 && !cur.hasHdr) {
                    // Sky/ELF chains store the GIF tag as the first V3 vector.
                    if (n < 1) {
                        result.error = VifChainError::Code::BadTag;
                        result.batches.clear();
                        return result;
                    }
                    cur.hdr[0] = cur.words[0];
                    cur.hdr[1] = cur.words[1];
                    cur.hdr[2] = cur.words[2];
                    cur.hasHdr = true;
                    cur.words.erase(cur.words.begin(), cur.words.begin() + 3);
                    cur.vecs -= 1;
                }
            }

            o += size;
            continue;
        }

        switch (cmd) {
        case 0x00: case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: case 0x06: case 0x07:
        case 0x10: case 0x11: case 0x13:
            // NOP, STCYCL, OFFSET, BASE, ITOP, STMOD, MSKPATH3, MARK, FLUSHE, FLUSH, FLUSHA
            break;

        case 0x14: // MSCAL
        case 0x15: // MSCALF
        case 0x17: { // MSCNT
            if (!cur.hasHdr) {
                result.error = VifChainError::Code::BadTag;
                result.batches.clear();
                return result;
            }

            const int entry = (cmd == 0x17) ? -2 : static_cast<int>(imm * 8 / 0x10);
            const std::uint32_t nloop = cur.hdr[0] & 0x7FFFu;
            const std::uint32_t primRaw = (cur.hdr[1] >> 15) & 0x7FFu;
            const std::uint8_t primType = static_cast<std::uint8_t>(primRaw & 7u);

            DecodedBatch batch;
            batch.nloop = nloop;
            batch.prim = primType;
            batch.materialImage = (material >= 0) ? static_cast<std::uint32_t>(material)
                                                  : 0xFFFFFFFFu;

            const auto layoutResult = buildVertices(entry, cur.words, nloop, batch.vertices);
            if (layoutResult == VertexLayoutResult::Unsupported) {
                result.error = VifChainError::Code::BadTag;
                result.batches.clear();
                return result;
            }
            if (layoutResult == VertexLayoutResult::Overflow) {
                result.error = VifChainError::Code::Overflow;
                result.batches.clear();
                return result;
            }
            buildIndices(primType, nloop, batch.indices);

            result.batches.push_back(std::move(batch));
            reset(cur);
            break;
        }

        case 0x20: { // STMASK
            constexpr std::size_t skip = 4;
            if (o + skip > total) {
                result.error = VifChainError::Code::Truncated;
                result.batches.clear();
                return result;
            }
            o += skip;
            break;
        }

        case 0x30: // STROW
        case 0x31: { // STCOL
            constexpr std::size_t skip = 16;
            if (o + skip > total) {
                result.error = VifChainError::Code::Truncated;
                result.batches.clear();
                return result;
            }
            o += skip;
            break;
        }

        case 0x4A: { // MPG
            const std::uint32_t count = num ? num : 256;
            const std::uint64_t skip64 = static_cast<std::uint64_t>(count) * 8u;
            if (skip64 > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
                result.error = VifChainError::Code::Overflow;
                result.batches.clear();
                return result;
            }
            const std::size_t skip = static_cast<std::size_t>(skip64);
            if (o + skip > total) {
                result.error = VifChainError::Code::Truncated;
                result.batches.clear();
                return result;
            }
            o += skip;
            break;
        }

        case 0x50: // DIRECT
        case 0x51: { // DIRECTHL
            const std::uint32_t count = imm ? imm : 65536;
            const std::uint64_t skip64 = static_cast<std::uint64_t>(count) * 16u;
            if (skip64 > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
                result.error = VifChainError::Code::Overflow;
                result.batches.clear();
                return result;
            }
            const std::size_t skip = static_cast<std::size_t>(skip64);
            if (o + skip > total) {
                result.error = VifChainError::Code::Truncated;
                result.batches.clear();
                return result;
            }
            o += skip;
            break;
        }

        default:
            result.error = VifChainError::Code::BadTag;
            result.batches.clear();
            return result;
        }
    }

    return result;
}

} // namespace

// ----------------------------------------------------------------------------
// Public entry point
// ----------------------------------------------------------------------------

VifChainResult decodeVifChain(std::span<const std::uint8_t> chain) {
    DmaWalkResult walk = walkDmaTags(chain);
    if (!walk.ok) {
        VifChainResult result;
        result.error = walk.code;
        return result;
    }
    return decodeStream(std::move(walk.stream));
}

} // namespace rt::native::assets
