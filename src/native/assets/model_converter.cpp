// src/native/assets/model_converter.cpp
//
// CAR model-chain conversion implementation. Format evidence:
// build/scratch/decomp-audit/m2-m3-container-format.md §8 (reconciled header
// and loader facts) and m2-model-chain-spec.md §2 (header-driven chain
// locating). Reviewer notes:
// - Every file read is bounds-checked before it happens: the eight header
//   words after a size >= 0x20 check, the 16-byte model header after a
//   region-fit check, the CNT sanity byte after a tag-fit check, and every
//   decodeVifChain span after a region-fit check. No offset is trusted
//   without a preceding range check.
// - Chain spans handed to decodeVifChain end at the model's region end. A
//   well-formed chain stops at its own RET tag (m2-model-chain-spec.md §8),
//   so span bytes past the RET are a bounds limit only, never consumed data.
//   A chain that outruns its region is reported as a decode failure instead
//   of silently reading the next model's bytes.
// - Only host-side byte parsing happens here: no PS2/guest types, and no
//   VIF/DMA stream semantics; those belong to the frozen vif_chain.h
//   contract, which this converter relies on and nothing else.

#include "model_converter.h"
#include "vif_chain.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace rt::native::assets {
namespace {

// ---------------------------------------------------------------------------
// CAR layout constants (m2-m3-container-format.md §8.4)
// ---------------------------------------------------------------------------
constexpr std::size_t kHeaderWords = 8;          // words +0x00..+0x1C
constexpr std::size_t kHeaderBytes = 0x20;       // eight u32 words
constexpr std::size_t kModelCount = 6;           // words +0x00..+0x14
constexpr std::size_t kPartCount = 2;            // partA / partB per model
constexpr std::size_t kModelHeaderBytes = 0x10;  // {partA off, partB off, cntA, cntB}
constexpr std::size_t kMainChainOffset = 0x10;   // main chain at model + 0x10
constexpr std::size_t kDmaTagBytes = 16;         // one DMA tag
constexpr std::size_t kDigestBytes = 32;

// Loader fatal bound on the region-length word (§8.2 step 2:
// "if (0x34000 < word@+0x18) fatal").
constexpr std::uint32_t kModelRegionMaxBytes = 0x34000u;

// The loader's texture-chain walk requires the first tag's byte 3 to be
// 0x10 (CNT) or it fatals (§8.2 step 5). Used as a word@+0x18 sanity check
// only; the chain itself is never converted here.
constexpr std::uint8_t kCntTagByte3 = 0x10u;

// Merged meshes must stay indexable with u32 vertex indices (container.h).
constexpr std::uint64_t kMaxIndexableVertices = 0xFFFFFFFFull;

constexpr std::uint32_t kUnresolvedImage = 0xFFFFFFFFu;

// Little-endian u32 at byte offset o. Callers pre-check o + 4 <= size (same
// convention as vif_chain.cpp's readU32LE).
std::uint32_t readU32LE(std::span<const std::uint8_t> b, std::size_t o) noexcept {
    return static_cast<std::uint32_t>(b[o]) |
           (static_cast<std::uint32_t>(b[o + 1]) << 8) |
           (static_cast<std::uint32_t>(b[o + 2]) << 16) |
           (static_cast<std::uint32_t>(b[o + 3]) << 24);
}

bool isAllZeroDigest(const std::array<std::uint8_t, kDigestBytes>& digest) noexcept {
    return std::all_of(digest.begin(), digest.end(),
                       [](std::uint8_t byte) { return byte == 0; });
}

// floor16 of a model-relative part offset: the runtime dereference
// "model + (off >> 4) * 4" equals "model + (off & ~15)" for every offset
// that stays non-negative as an int (m2-model-chain-spec.md §2.2). The
// masked form is computed in u64 and range-checked against the model region
// by the caller, so an offset with the sign bit set is rejected as
// out-of-region rather than wrapped.
std::uint64_t floor16Offset(std::uint32_t offset) noexcept {
    return static_cast<std::uint64_t>(offset & ~0xFu);
}

// Same finiteness rule serialize()/validateMesh applies (container.cpp);
// rejecting here keeps every Ok result serializable.
bool isFiniteVertex(const Vertex& v) noexcept {
    return std::isfinite(v.px) && std::isfinite(v.py) && std::isfinite(v.pz) &&
           std::isfinite(v.nx) && std::isfinite(v.ny) && std::isfinite(v.nz) &&
           std::isfinite(v.u) && std::isfinite(v.v);
}

// Appends one decoded chain's batches as one MeshRecord: batch-local
// vertices in decode order, indices rebased onto the running vertex base
// (batch numbering is batch-local, m2-model-chain-spec.md §6.2), and one
// MaterialRef per batch spanning that batch's index range with the
// unresolved image sentinel (image binding is a separate group's stage).
// BadBatch fires only if the decoder broke its own contract (an index
// outside its batch's vertices) or the merged mesh could not be serialized
// (vertex count past u32 indexability, non-finite float).
CarModelError assembleChainMesh(const VifChainResult& decoded,
                                 const std::string& sourceFile,
                                 const std::array<std::uint8_t, kDigestBytes>& sha256,
                                 std::uint64_t chainStart,
                                 std::uint64_t chainLength,
                                 MeshRecord& out) {
    out.provenance.sourceFile = sourceFile;
    out.provenance.offset = chainStart;
    out.provenance.length = chainLength;
    out.provenance.sha256 = sha256;

    std::uint64_t vertexBase = 0;  // running vertex count == rebase base
    std::uint64_t indexBase = 0;   // running index count == next firstIndex
    for (const DecodedBatch& batch : decoded.batches) {
        const std::uint64_t batchVertexCount =
            static_cast<std::uint64_t>(batch.vertices.size());
        const std::uint64_t batchIndexCount =
            static_cast<std::uint64_t>(batch.indices.size());

        // Rebased indices are u32 vertex indices, so the merged vertex
        // count must stay within u32 range (overflow-safe subtraction).
        if (vertexBase > kMaxIndexableVertices ||
            batchVertexCount > kMaxIndexableVertices - vertexBase) {
            return CarModelError::BadBatch;
        }
        for (const Vertex& v : batch.vertices) {
            if (!isFiniteVertex(v)) {
                return CarModelError::BadBatch;
            }
        }
        for (const std::uint32_t index : batch.indices) {
            // Decoder contract: indices address their own batch's vertices.
            if (static_cast<std::uint64_t>(index) >= batchVertexCount) {
                return CarModelError::BadBatch;
            }
            out.indices.push_back(static_cast<std::uint32_t>(vertexBase + index));
        }

        MaterialRef material{};
        material.firstIndex = indexBase;
        material.indexCount = batchIndexCount;
        material.imageIndex = kUnresolvedImage;
        out.materials.push_back(material);

        out.vertices.insert(out.vertices.end(), batch.vertices.begin(),
                            batch.vertices.end());

        vertexBase += batchVertexCount;
        indexBase += batchIndexCount;
    }
    return CarModelError::Ok;
}

// Decodes one chain span [chainStart, chainEnd) and appends its MeshRecord.
// Returns Ok, or the error that stopped the conversion; the caller attributes
// the failure to chainStart. Callers guarantee chainEnd <= file size, so the
// subspan is always in range.
CarModelError appendChainMesh(std::span<const std::uint8_t> carFile,
                              std::uint64_t chainStart,
                              std::uint64_t chainEnd,
                              const std::string& sourceFile,
                              const std::array<std::uint8_t, kDigestBytes>& sha256,
                              std::vector<MeshRecord>& meshes) {
    // A chain must hold at least one whole DMA tag.
    if (chainStart + kDmaTagBytes > chainEnd) {
        return CarModelError::Truncated;
    }
    const std::size_t len = static_cast<std::size_t>(chainEnd - chainStart);
    const VifChainResult decoded =
        decodeVifChain(carFile.subspan(static_cast<std::size_t>(chainStart), len));
    if (decoded.error != VifChainError::Code::Ok) {
        return CarModelError::ChainDecode;
    }

    MeshRecord mesh;
    const CarModelError assembled =
        assembleChainMesh(decoded, sourceFile, sha256, chainStart, len, mesh);
    if (assembled != CarModelError::Ok) {
        return assembled;
    }
    meshes.push_back(std::move(mesh));
    return CarModelError::Ok;
}

} // namespace

CarModelResult convertCarModels(std::span<const std::uint8_t> carFile,
                                const std::string& sourceFile,
                                const std::array<std::uint8_t, 32>& sha256) {
    CarModelResult result{};
    const std::uint64_t fileSize = static_cast<std::uint64_t>(carFile.size());

    // Every mesh's provenance is built from this identity, so it must be
    // complete before anything else (hasValidProvenance would reject the
    // whole container downstream).
    if (sourceFile.empty() || isAllZeroDigest(sha256)) {
        result.err = CarModelError::MissingProvenance;
        return result;
    }

    if (fileSize < kHeaderBytes) {
        result.err = CarModelError::Truncated;
        return result;
    }

    std::uint32_t word[kHeaderWords] = {};
    for (std::size_t i = 0; i < kHeaderWords; ++i) {
        word[i] = readU32LE(carFile, i * 4);
    }
    // word[0..5]: file-relative model starts. word[6]: model-region length
    // == texture-chain start. word[7]: file size.

    // --- header invariants ("reject malformed headers") --------------------

    // The size word must equal the real size (value CONFIRMED 30/30, §8.4);
    // this is what catches a truncated read of the file.
    if (static_cast<std::uint64_t>(word[7]) != fileSize) {
        result.err = CarModelError::BadHeader;
        result.errorOffset = 0x1C;
        return result;
    }
    // The loader fatals above this bound (§8.2 step 2).
    if (word[6] > kModelRegionMaxBytes) {
        result.err = CarModelError::BadHeader;
        result.errorOffset = 0x18;
        return result;
    }
    // No model region may overlap the eight header words (they occupy
    // [0, 0x20)); observed word[0] is 0x30 in all 30 pinned files, 0x20 is
    // the semantic minimum.
    if (word[0] < kHeaderBytes) {
        result.err = CarModelError::BadHeader;
        result.errorOffset = 0x00;
        return result;
    }
    // Monotonic non-decreasing words: model starts through the region
    // length, and the region length within the size word (hence within the
    // file). CONFIRMED monotonic in all 30 pinned files (§8.3).
    for (std::size_t i = 0; i + 1 < kHeaderWords; ++i) {
        if (word[i] > word[i + 1]) {
            result.err = CarModelError::BadHeader;
            result.errorOffset = 4 * (i + 1);
            return result;
        }
    }
    // When a texture chain is present, its first tag must be CNT: the
    // loader's own walk fatals on any other byte 3 (§8.2 step 5). An empty
    // chain (word[6] == file size) is not rejected: its legality is unknown
    // and the mesh conversion does not depend on it.
    if (static_cast<std::uint64_t>(word[6]) < fileSize) {
        const std::uint64_t textureChainStart = static_cast<std::uint64_t>(word[6]);
        if (textureChainStart + kDmaTagBytes > fileSize ||
            carFile[static_cast<std::size_t>(textureChainStart) + 3] != kCntTagByte3) {
            result.err = CarModelError::BadHeader;
            result.errorOffset = textureChainStart;
            return result;
        }
    }

    // --- models --------------------------------------------------------------
    std::vector<MeshRecord> meshes;

    for (std::size_t k = 0; k < kModelCount; ++k) {
        const std::uint64_t modelStart = word[k];
        const std::uint64_t regionEnd = word[k + 1];  // word[6] closes model 5
        const std::size_t modelStartZ = static_cast<std::size_t>(modelStart);

        // The 16-byte model header {partA off, partB off, cntA, cntB} must
        // sit inside the model's region (§7.2/§2.2). With the monotonic
        // check above this also rejects equal adjacent words (an empty
        // region cannot hold the header).
        if (modelStart + kModelHeaderBytes > regionEnd) {
            result.err = CarModelError::Truncated;
            result.errorOffset = modelStart;
            return result;
        }
        const std::uint32_t partAOffset = readU32LE(carFile, modelStartZ + 0);
        const std::uint32_t partBOffset = readU32LE(carFile, modelStartZ + 4);
        const std::uint32_t partACount = readU32LE(carFile, modelStartZ + 8);
        const std::uint32_t partBCount = readU32LE(carFile, modelStartZ + 12);

        // Main chain at model + 0x10 (drawCar calls model + 0x10, §7.3).
        const std::uint64_t mainStart = modelStart + kMainChainOffset;
        const CarModelError mainError =
            appendChainMesh(carFile, mainStart, regionEnd, sourceFile, sha256, meshes);
        if (mainError != CarModelError::Ok) {
            result.err = mainError;
            result.errorOffset = mainStart;
            return result;
        }

        // Part chains, gated by their count words: drawModelParts draws a
        // part iff count != 0 and its mask bit is set (§7.2); the mask is
        // runtime state, so count != 0 is the only file-side gate. The
        // counts are never batch counts (§7.2, value meaning unknown).
        const std::array<std::uint32_t, kPartCount> partOffsets{partAOffset, partBOffset};
        const std::array<std::uint32_t, kPartCount> partCounts{partACount, partBCount};
        for (std::size_t part = 0; part < kPartCount; ++part) {
            if (partCounts[part] == 0) {
                continue;  // gate closed: the runtime draws nothing here
            }
            const std::uint64_t partStart =
                modelStart + floor16Offset(partOffsets[part]);
            // A part chain cannot begin inside the 16-byte model header and
            // must hold at least one tag inside the model's region.
            if (partStart < modelStart + kModelHeaderBytes ||
                partStart + kDmaTagBytes > regionEnd) {
                result.err = CarModelError::BadHeader;
                result.errorOffset = modelStart + 4 * part;  // the part-offset word
                return result;
            }
            const CarModelError partError =
                appendChainMesh(carFile, partStart, regionEnd, sourceFile, sha256, meshes);
            if (partError != CarModelError::Ok) {
                result.err = partError;
                result.errorOffset = partStart;
                return result;
            }
        }
    }

    result.value.version = kContainerVersion;
    result.value.meshes = std::move(meshes);
    return result;
}

} // namespace rt::native::assets
