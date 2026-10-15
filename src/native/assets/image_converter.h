#pragma once

// Offline image/texture converter for RoadTripAdventure native asset containers.
//
// Authored host-side decoder for a disc texture upload chain (the CNT...END DMA
// region a CAR/FLD container points at). It walks the DMA chain, decodes the GIF
// A+D upload packets inside each CNT (BITBLTBUF / TRXPOS / TRXREG / TRXDIR plus
// the IMAGE data tags), applies the CLUT/palette (CSM1 index swizzle, 16-bit
// colour expansion <<3, TEXA AEM), and emits tightly-packed RGBA8 pixels in a
// container ImageRecord.
//
// Evidence (read-only, no guest code reused):
// - build/scratch/decomp-audit/m2-m3-container-format.md §2 (upload packet fields,
//   payload order, CLUT/palette rules, cited to src/textures/Extract.cpp) and
//   §8 (chain start, CNT/END walk, cited to the game's loadCarModel decompile).
// - src/textures/Extract.cpp remains the independent reference decoder and oracle;
//   this file re-implements its parse/CLUT/decode math against the chain bytes
//   directly, without GS memory emulation (single-upload round trip is linear,
//   spec §2.2) and without any PS2/guest/GS-memory types or headers.
//
// Failure contract: malformed, truncated, or unsupported input yields a
// default-constructed ImageRecord (width == height == mipCount == 0, empty
// pixels). loadImage() rejects such a record; no partial output is produced.

#include "container.h"

#include <cstdint>
#include <span>
#include <string>

namespace rt::native::assets {

// Decodes the chain's first texture with palette selection 0 (see
// convertImageBank for the selection order). On success: format is
// R8G8B8A8_UNORM, mipCount == 1, pixels.size() == width * height * 4,
// sourceVariantId == variantId, provenance == provenance.
ImageRecord convertImage(std::span<const std::uint8_t> uploadChain,
                         const Provenance& provenance,
                         const std::string& variantId);

// Decodes one palette selection ("CLUT bank") of the chain's first texture.
// Selections are ordered exactly as the reference decoder emits them:
// 1. every texture-state register (GIF A+D address 0x06/0x07) in the chain whose
//    texture base/format match the target and whose CSM is 0, in field-tuple
//    order; else
// 2. when the upload right after the target is palette-shaped: banks 0..15 for a
//    4-bit texture with a 16x16 palette (a 256-entry palette holds sixteen
//    16-colour banks), otherwise the single bank 0.
// A bank index >= convertImageBankCount() yields an empty (rejected) record.
ImageRecord convertImageBank(std::span<const std::uint8_t> uploadChain,
                             const Provenance& provenance,
                             const std::string& variantId,
                             std::uint32_t bank);

// Number of palette selections convertImageBank accepts for this chain; callers
// can emit one ImageRecord per selection, each with a distinct sourceVariantId.
// Returns 0 when the chain is malformed or holds no decodable texture.
std::uint32_t convertImageBankCount(std::span<const std::uint8_t> uploadChain);

} // namespace rt::native::assets
