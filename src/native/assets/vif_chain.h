#pragma once

// VIF/DMA chain decoding types for RoadTriAdventure asset batches.
// Depends only on the frozen Vertex type; no implementation here.

#include "container.h"

#include <cstdint>
#include <span>
#include <vector>

namespace rt::native::assets {

// One decoded VIF block (a vertex batch) pulled from a DMA/VIF chain.
struct DecodedBatch {
    std::vector<Vertex> vertices;   // decoded vertices, in input order
    std::vector<std::uint32_t> indices; // index list derived from prim + nloop
    std::uint32_t nloop = 0;        // primitive count for this batch
    std::uint8_t prim = 0;          // primitive type (PRIM & 7)
    std::uint32_t materialImage = 0xFFFFFFFFu; // sticky, unresolved until image binding
};

// Strict decode outcome; empty enum-style code kept deliberately simple.
struct VifChainError {
    enum class Code { Ok, Truncated, Overflow, BadTag } code = Code::Ok; // result code
    bool ok() const { return code == Code::Ok; } // true when code == Ok
};

// Result of decoding a whole VIF/DMA chain, or the failure that prevented it.
struct VifChainResult {
    VifChainError::Code error = VifChainError::Code::Ok; // overall result code
    std::vector<DecodedBatch> batches;                    // decoded batches, empty on error
};

// Walk a DMA/VIF chain (CNT...RET) and decode the V3-32/V4-32 vertex blocks.
// Strict: returns error (no partial batches) on malformed/truncated/overflow input.
VifChainResult decodeVifChain(std::span<const std::uint8_t> chain);

} // namespace rt::native::assets
