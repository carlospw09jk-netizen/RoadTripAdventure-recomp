#pragma once

// src/native/assets/model_converter.h
//
// CAR container (CARn/Qnn.BIN) model conversion: whole-file bytes -> a
// mesh-only HostAssetContainer. Image binding is out of scope (a separate
// group owns it): the container's images vector stays empty and every
// MaterialRef keeps the unresolved-imageIndex sentinel 0xFFFFFFFFu.
//
// CAR header, eight little-endian u32 words (reconciled evidence:
// build/scratch/decomp-audit/m2-m3-container-format.md §8.4, CONFIRMED
// loader loadCarModel 0x20C7B8):
//   +0x00..+0x14  six file-relative model starts; roles body near, body
//                 far, variant a, variant b, extra a, extra b
//   +0x18         model-region length == texture-chain start; the loader
//                 fatals above 0x34000
//   +0x1C         file size (the loader never reads it; it equals the real
//                 size in all 30 pinned files and is enforced here)
//   +0x20..0x2F   zero padding; never validated (purpose unknown)
// The game reads the file verbatim, so file-relative offsets need no runtime
// base (§8.5): reading these words straight from the file equals the
// loader's *(u32*)(dest + 4*i) + dest pointer math on the copy.
//
// Model k starts at word_k and spans [word_k, word_{k+1}) (word_6 for k = 5;
// span INFERRED from the monotonic starts, §8.4):
//   +0x00 u32 partA offset (model-relative, dereferenced floor16)
//   +0x04 u32 partB offset
//   +0x08 u32 partA count (a draw gate only; never a batch count)
//   +0x0C u32 partB count
//   +0x10 main VIF/DMA chain (drawCar calls model + 0x10)
//
// Chains converted per model (header-driven locating,
// build/scratch/decomp-audit/m2-model-chain-spec.md §2.3): the main chain at
// model + 0x10 always, plus the partA / partB chains at
// model + floor16(offset) when the matching count word is non-zero. One
// MeshRecord per converted chain, emitted in order: model 0 main, model 0
// partA, model 0 partB, model 1 main, ... A chain that decodes with zero
// batches yields an empty MeshRecord (provenance only): emptiness falls out
// of the decode.
//
// Strict policy: a malformed header, or any chain that decodeVifChain
// rejects, fails the whole conversion (structured error, empty value).
// Partial flagged output is impossible: HostAssetContainer has no notes
// channel, so model_dump's emit-and-flag behaviour cannot be mirrored here.

#include "container.h"

#include <array>
#include <cstdint>
#include <span>
#include <string>

namespace rt::native::assets {

// Conversion failure codes.
enum class CarModelError : std::uint8_t {
    Ok = 0,
    // Input shorter than a structure requires: file under 0x20 bytes, or a
    // model header / chain span that cannot hold even one DMA tag inside its
    // model region.
    Truncated,
    // A header word violates the CAR invariants: non-monotonic model starts,
    // word@+0x18 above 0x34000 or beyond the file, word@+0x1C not equal to
    // the file size, word@+0x18 not pointing at a CNT tag, or a model-header
    // part offset that escapes its model's region.
    BadHeader,
    // decodeVifChain rejected a located chain (malformed / truncated /
    // overflow VIF or DMA data).
    ChainDecode,
    // A decoded batch cannot become a valid MeshRecord: an index outside its
    // own batch's vertices, a merged vertex count beyond u32 indexability,
    // or a non-finite vertex float (serialize() would reject the container).
    BadBatch,
    // The caller supplied an empty sourceFile or an all-zero sha256 digest.
    MissingProvenance,
};

// Result of converting one CAR file. err != Ok means value is default-empty;
// err == Ok means value holds a complete mesh-only container. errorOffset is
// the file offset of the header word or located chain being validated when
// the error was found; 0 when not applicable (MissingProvenance, or a file
// too short to hold the header).
struct CarModelResult {
    CarModelError err = CarModelError::Ok;
    HostAssetContainer value;       // meshes only; valid iff err == Ok
    std::uint64_t errorOffset = 0;
};

// Converts the six CAR models of one Qnn.BIN file (main chains plus
// count-gated part chains) into mesh records. sourceFile and sha256 identify
// the source blob for every mesh's provenance; each mesh's offset/length is
// set to the exact byte span handed to decodeVifChain. The texture chain at
// word@+0x18 is validated as a header claim only, never converted.
CarModelResult convertCarModels(std::span<const std::uint8_t> carFile,
                                const std::string& sourceFile,
                                const std::array<std::uint8_t, 32>& sha256);

} // namespace rt::native::assets
