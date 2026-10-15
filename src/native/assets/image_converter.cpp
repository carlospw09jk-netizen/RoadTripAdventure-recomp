// Offline image/texture converter — implementation.
//
// Host-native, read-only, std-only. This is an AUTHORED decoder of the disc's
// texture upload chain: it re-implements the parse/CLUT/decode math of the
// reference decoder (src/textures/Extract.cpp) directly against the chain bytes.
// It does NOT emulate GS memory (no swizzle tables): for a single upload the
// write/read round trip is linear row-major payload order (container-format spec
// §2.2), so texel (x, y) is payload[y * w + x] and the decoder never needs the
// guest's block layout. No PS2/guest/GS-memory types appear in this file.
//
// Strictness: every read is bounds-checked; malformed, truncated, or unsupported
// chains reject with an empty ImageRecord (no partial output, no zero-fill).
// This is intentionally stricter than the reference decoder, which silently
// drops unparsable packets while scanning whole files.

#include "image_converter.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <tuple>
#include <vector>

namespace rt::native::assets {
namespace {

// ---------------------------------------------------------------------------
// Evidence constants
// ---------------------------------------------------------------------------

// Pixel storage modes (spec §2.1, citing src/textures/Extract.cpp:22-39).
// Authored constants; the shipped code carries no guest headers or types.
constexpr std::uint32_t kPsmCt32 = 0x00;
constexpr std::uint32_t kPsmCt24 = 0x01;
constexpr std::uint32_t kPsmCt16 = 0x02;
constexpr std::uint32_t kPsmCt16S = 0x0A;
constexpr std::uint32_t kPsmT8 = 0x13;
constexpr std::uint32_t kPsmT4 = 0x14;
constexpr std::uint32_t kPsmT8H = 0x1B;
constexpr std::uint32_t kPsmT4Hl = 0x24;
constexpr std::uint32_t kPsmT4Hh = 0x2C;

// GIF A+D register addresses (spec §2.1, Extract.cpp:248-253).
constexpr std::uint64_t kRegBlitBuf = 0x50;     // BITBLTBUF
constexpr std::uint64_t kRegTransferPos = 0x51; // TRXPOS
constexpr std::uint64_t kRegTransferReg = 0x52; // TRXREG
constexpr std::uint64_t kRegTransferDir = 0x53; // TRXDIR
constexpr std::uint64_t kRegTexState1 = 0x06;   // texture-state register, context 1
constexpr std::uint64_t kRegTexState2 = 0x07;   // texture-state register, context 2
constexpr std::uint64_t kMaxRegAddr = 0x63;     // addresses above this invalidate a packet

// TEXA defaults of the reference decoder (src/textures/Extract.h:20-21): the
// alpha of "opaque" 16-/24-bit texels is 0x80 and AEM is on (all-zero colour is
// transparent). The game sets TEXA globally; the disc chains do not carry it, so
// these values are fixed here exactly as the reference fixes them.
constexpr std::uint32_t kAlphaOpaque = 0x80;
constexpr bool kAem = true;

// DMA tag identity byte, as the game's own loader checks it (spec §8.2):
// byte 3 of the tag qword is 0x10 for CNT and 0x70 for END; anything else is
// fatal. qwc (payload qword count) is the u16 at tag+0.
constexpr std::uint8_t kDmaTagCnt = 0x10;
constexpr std::uint8_t kDmaTagEnd = 0x70;

// Walk cap, mirroring vif_chain.cpp's bound (defence in depth; the walk also
// always advances by >= 16 bytes, so termination does not rely on the cap).
constexpr int kMaxChainTags = 4096;

// ---------------------------------------------------------------------------
// Little-endian readers and bitfields
// ---------------------------------------------------------------------------

std::uint64_t readU64Le(std::span<const std::uint8_t> b, std::size_t o) {
    std::uint64_t v = 0;
    for (int i = 7; i >= 0; --i) {
        v = (v << 8) | static_cast<std::uint64_t>(b[o + static_cast<std::size_t>(i)]);
    }
    return v;
}

std::uint32_t readU32Le(std::span<const std::uint8_t> b, std::size_t o) {
    return static_cast<std::uint32_t>(b[o]) |
           (static_cast<std::uint32_t>(b[o + 1]) << 8) |
           (static_cast<std::uint32_t>(b[o + 2]) << 16) |
           (static_cast<std::uint32_t>(b[o + 3]) << 24);
}

std::uint16_t readU16Le(std::span<const std::uint8_t> b, std::size_t o) {
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(b[o]) | (static_cast<std::uint16_t>(b[o + 1]) << 8));
}

std::uint64_t field(std::uint64_t v, unsigned lo, unsigned n) {
    return (v >> lo) & ((1ull << n) - 1ull);
}

// ---------------------------------------------------------------------------
// Format predicates (Extract.cpp:28-42, 208-211)
// ---------------------------------------------------------------------------

std::uint32_t bitsPerPixel(std::uint32_t psm) {
    switch (psm) {
    case kPsmCt32: return 32;
    case kPsmCt24: return 24;
    case kPsmCt16:
    case kPsmCt16S: return 16;
    case kPsmT8:
    case kPsmT8H: return 8;
    case kPsmT4:
    case kPsmT4Hl:
    case kPsmT4Hh: return 4;
    default: return 0;
    }
}

bool isIndexed(std::uint32_t psm) {
    return psm == kPsmT8 || psm == kPsmT8H || psm == kPsmT4 || psm == kPsmT4Hl || psm == kPsmT4Hh;
}

bool isFourBit(std::uint32_t psm) {
    return psm == kPsmT4 || psm == kPsmT4Hl || psm == kPsmT4Hh;
}

bool isSixteenBitColour(std::uint32_t psm) {
    return psm == kPsmCt16 || psm == kPsmCt16S;
}

// A palette-shaped upload (32/16-bit colour, 8x2 / 16x2 / 16x16 and the like),
// exactly the reference's clutShaped() (Extract.cpp:208-211).
bool isClutShaped(std::uint32_t psm, std::uint32_t w, std::uint32_t h) {
    return (psm == kPsmCt32 || isSixteenBitColour(psm)) && w <= 16 &&
           (h <= 2 || (w == 16 && h == 16) || (w == 8 && h == 8));
}

// ---------------------------------------------------------------------------
// Parsed chain model
// ---------------------------------------------------------------------------

// One host-to-local upload and its pixel payload (spec §2.1; field positions
// from Extract.cpp:267-269).
struct UploadPacket {
    std::uint32_t dbp = 0;  // destination block pointer (file-relative; never rebased)
    std::uint32_t dbw = 0;  // destination buffer width
    std::uint32_t psm = 0;  // destination pixel storage mode
    std::uint32_t dx = 0;   // TRXPOS DSAX
    std::uint32_t dy = 0;   // TRXPOS DSAY
    std::uint32_t w = 0;    // TRXREG width
    std::uint32_t h = 0;    // TRXREG height
    std::vector<std::uint8_t> pixels; // row-major payload, w texels per row (spec §2.2)
};

// A texture-state register (GIF A+D address 0x06/0x07) the chain writes.
// Field positions from Extract.cpp:260-261. Named neutrally: the shipped code
// carries no guest register types.
struct TexState {
    std::uint32_t tbp = 0;  // texture base pointer
    std::uint32_t tbw = 0;  // texture buffer width
    std::uint32_t psm = 0;  // texture pixel storage mode
    std::uint32_t cbp = 0;  // CLUT base pointer
    std::uint32_t cpsm = 0; // CLUT pixel storage mode
    std::uint32_t csm = 0;  // CLUT storage mode (0 = CSM1)
    std::uint32_t csa = 0;  // CLUT start address (bank)

    bool operator<(const TexState& o) const {
        return std::tie(tbp, tbw, psm, cbp, cpsm, csm, csa) <
               std::tie(o.tbp, o.tbw, o.psm, o.cbp, o.cpsm, o.csm, o.csa);
    }
    bool operator==(const TexState& o) const {
        return std::tie(tbp, tbw, psm, cbp, cpsm, csm, csa) ==
               std::tie(o.tbp, o.tbw, o.psm, o.cbp, o.cpsm, o.csm, o.csa);
    }
};

// One palette the reference decoder would emit the target texture with
// (Extract.cpp:376-394): a CLUT base pointer, storage mode and start address.
struct PaletteChoice {
    std::uint32_t cbp = 0;
    std::uint32_t cpsm = 0;
    std::uint32_t csa = 0;
};

struct ParsedChain {
    std::vector<UploadPacket> uploads; // chain order
    std::vector<TexState> texStates;   // sorted, unique (reference uses std::set)
    std::size_t targetIndex = 0;       // first emittable texture upload
    bool hasTarget = false;
};

// ---------------------------------------------------------------------------
// Texel access (linear payload model, spec §2.2)
// ---------------------------------------------------------------------------

// Texel i of a row-major payload in the given storage mode. The caller has
// validated pixels.size() >= ceil(count * bpp / 8), so every index below is
// in bounds. 4-bit order is low nibble first (Extract.cpp:82-84); CT24 is
// 3 bytes little-endian (Extract.cpp:77); CT16/CT16S is 16-bit little-endian.
// The reference treats CT16S and the "H" indexed modes exactly like CT16/T4/T8;
// this decoder mirrors the reference, not the console's exact behaviour.
std::uint32_t texelValue(std::uint32_t psm, std::span<const std::uint8_t> pixels, std::size_t i) {
    switch (psm) {
    case kPsmCt32:
        return readU32Le(pixels, i * 4);
    case kPsmCt24:
        return static_cast<std::uint32_t>(pixels[i * 3]) |
               (static_cast<std::uint32_t>(pixels[i * 3 + 1]) << 8) |
               (static_cast<std::uint32_t>(pixels[i * 3 + 2]) << 16);
    case kPsmCt16:
    case kPsmCt16S:
        return readU16Le(pixels, i * 2);
    case kPsmT8:
    case kPsmT8H:
        return pixels[i];
    case kPsmT4:
    case kPsmT4Hl:
    case kPsmT4Hh:
        return (pixels[i / 2] >> ((i & 1) * 4)) & 0xFu;
    default:
        return 0;
    }
}

// The palette upload's texel at (x, y), under the round-trip model: the upload
// wrote texel i at (i % w, i / w) with dx = dy = 0, and the reference's CLUT
// read-back uses the same base pointer with buffer width 1 (Extract.cpp:140),
// so positions the upload never wrote read back as zero (fresh memory).
std::uint32_t readPaletteTexel(const UploadPacket& pal, std::uint32_t x, std::uint32_t y) {
    if (x >= pal.w) {
        return 0;
    }
    const std::size_t idx = static_cast<std::size_t>(y) * pal.w + x;
    if (idx >= static_cast<std::size_t>(pal.w) * pal.h) {
        return 0;
    }
    return texelValue(pal.psm, std::span<const std::uint8_t>(pal.pixels), idx);
}

// ---------------------------------------------------------------------------
// Colour expansion (Extract.cpp:110-127)
// ---------------------------------------------------------------------------

std::uint32_t packRgba(int r, int g, int b, int a) {
    const auto c = [](int v) { return static_cast<std::uint32_t>(std::clamp(v, 0, 255)); };
    return c(r) | (c(g) << 8) | (c(b) << 16) | (c(a) << 24);
}

// 16-bit colour: R bits 0..4, G bits 5..9, B bits 10..14, A bit 15; each
// channel expands with <<3; the alpha of an opaque texel comes from TEXA and
// AEM makes an all-zero entry transparent. The reference selects ta1/ta0 on
// the alpha bit; both defaults are 0x80 (Extract.h:20), so bit 15 cannot
// change the result and a single constant is used.
std::uint32_t rgba16(std::uint32_t c) {
    const int r = static_cast<int>((c & 0x1Fu) << 3);
    const int g = static_cast<int>(((c >> 5) & 0x1Fu) << 3);
    const int b = static_cast<int>(((c >> 10) & 0x1Fu) << 3);
    const bool empty = kAem && (c & 0xFFFFu) == 0;
    const int a = empty ? 0 : static_cast<int>(kAlphaOpaque);
    return packRgba(r, g, b, a);
}

// 24-bit colour with TEXA alpha / AEM (Extract.cpp:123-127).
std::uint32_t rgb24(std::uint32_t c) {
    const bool empty = kAem && (c & 0xFFFFFFu) == 0;
    return (c & 0xFFFFFFu) | (static_cast<std::uint32_t>(empty ? 0 : kAlphaOpaque) << 24);
}

// ---------------------------------------------------------------------------
// CLUT load (Extract.cpp:131-151)
// ---------------------------------------------------------------------------

// The CLUT buffer after a CSM1 load: 512 half-words. 32-bit entries split into
// a low half at dst and a high half at dst + 256, exactly as the reference
// stores them. Returns false when the palette upload cannot back this load
// under the linear model (see the header comment in the .h for the contract).
bool loadClut(const UploadPacket& pal, std::uint32_t texPsm, std::uint32_t cpsm, std::uint32_t csa,
              std::array<std::uint16_t, 512>& clut) {
    // Linear-model preconditions (documented deviations; the reference reads
    // real GS blocks, this decoder refuses what it cannot represent):
    // - the CLUT read-back uses buffer width 1, so the upload must have dbw 1;
    // - partial destination offsets are not modelled;
    // - the read format must equal the write format (a byte-level reinterpretation
    //   would need the guest's block layout).
    if (pal.dbw != 1 || pal.dx != 0 || pal.dy != 0 || pal.psm != cpsm) {
        return false;
    }
    if (cpsm != kPsmCt32 && cpsm != kPsmCt24 && !isSixteenBitColour(cpsm)) {
        return false;
    }

    clut.fill(0);
    const bool sixteen = isSixteenBitColour(cpsm);
    const std::uint32_t entries = isFourBit(texPsm) ? 16u : 256u;
    const std::uint32_t base = (csa & (sixteen ? 0x1Fu : 0x0Fu)) << 4;
    for (std::uint32_t e = 0; e < entries; ++e) {
        // CSM1: bits 3 and 4 of the entry index are swapped (Extract.cpp:139).
        const std::uint32_t s = (e & ~0x18u) | ((e & 0x08u) << 1) | ((e & 0x10u) >> 1);
        const std::uint32_t raw = readPaletteTexel(pal, s & 0x0Fu, s >> 4);
        const std::uint32_t dst = (base + e) & (sixteen ? 0x1FFu : 0xFFu);
        if (sixteen) {
            clut[dst] = static_cast<std::uint16_t>(raw);
        } else {
            clut[dst] = static_cast<std::uint16_t>(raw & 0xFFFFu);
            clut[dst + 256] = static_cast<std::uint16_t>(raw >> 16);
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Texture decode (Extract.cpp:153-188)
// ---------------------------------------------------------------------------

// Decodes the target upload to RGBA8. `clut` must be non-null for indexed
// formats. Returns false on internal inconsistency (an indexed texture without
// a CLUT, or an unknown storage mode) so the caller rejects instead of
// emitting zeros.
bool decodeTexture(const UploadPacket& u, const std::array<std::uint16_t, 512>* clut,
                   std::uint32_t cpsm, std::uint32_t csa, std::vector<std::uint8_t>& rgba) {
    if (isIndexed(u.psm) && clut == nullptr) {
        return false;
    }
    if (bitsPerPixel(u.psm) == 0) {
        return false;
    }

    const std::size_t texels = static_cast<std::size_t>(u.w) * u.h;
    rgba.resize(texels * 4);
    const bool sixteen = isSixteenBitColour(cpsm);
    const std::span<const std::uint8_t> pixels(u.pixels);

    for (std::size_t i = 0; i < texels; ++i) {
        const std::uint32_t v = texelValue(u.psm, pixels, i);
        std::uint32_t c = 0;
        if (isIndexed(u.psm)) {
            // CLUT index (Extract.cpp:163-177): offset by the bank, 8-bit
            // indices clamped into the last bank, then half-word recombination
            // for 32/24-bit CLUTs.
            std::uint32_t idx = (csa * 16u + v) & 0x1FFu;
            if (!isFourBit(u.psm)) {
                idx = std::min(idx >> 4, sixteen ? 31u : 15u) * 16u + (idx & 15u);
            }
            if (sixteen) {
                c = rgba16((*clut)[idx & 0x1FFu]);
            } else {
                idx &= 0xFFu;
                c = static_cast<std::uint32_t>((*clut)[idx]) |
                    (static_cast<std::uint32_t>((*clut)[idx + 256]) << 16);
                if (cpsm == kPsmCt24) {
                    c = rgb24(c);
                }
            }
        } else if (u.psm == kPsmCt32) {
            c = v; // raw u32 is the logical RGBA (spec §2.2, Extract.cpp:179-180)
        } else if (u.psm == kPsmCt24) {
            c = rgb24(v);
        } else if (isSixteenBitColour(u.psm)) {
            c = rgba16(v);
        } else {
            return false;
        }

        std::uint8_t* out = &rgba[i * 4];
        out[0] = static_cast<std::uint8_t>(c & 0xFFu);
        out[1] = static_cast<std::uint8_t>((c >> 8) & 0xFFu);
        out[2] = static_cast<std::uint8_t>((c >> 16) & 0xFFu);
        // GS alpha is 0..128 (0x80 = opaque); the container stores host UNORM
        // 0..255, so scale exactly as the reference export does
        // (min(255, a * 255 / 128), Extract.cpp:346-347).
        const std::uint32_t a = (c >> 24) & 0xFFu;
        out[3] = static_cast<std::uint8_t>(std::min(255u, a * 255u / 128u));
    }
    return true;
}

// ---------------------------------------------------------------------------
// DMA chain walk (spec §8.2/§8.3, mirroring the game loader's checks)
// ---------------------------------------------------------------------------

// Splits the CNT...END chain into its CNT payloads. Returns false on any
// malformed structure: a tag that is neither CNT nor END, a payload running
// past the span, or the walk cap being exceeded without an END.
bool walkChain(std::span<const std::uint8_t> chain,
               std::vector<std::span<const std::uint8_t>>& payloads) {
    const std::size_t n = chain.size();
    std::size_t o = 0;
    for (int tags = 0; tags < kMaxChainTags; ++tags) {
        if (o + 16 > n) {
            return false; // truncated tag
        }
        const std::uint8_t id = chain[o + 3];
        if (id == kDmaTagEnd) {
            return true; // END terminates the walk; its payload is not entered
        }
        if (id != kDmaTagCnt) {
            return false; // not a CNT...END texture chain
        }
        const std::uint64_t qwc = readU16Le(chain, o);
        const std::uint64_t payloadBytes = qwc * 16u;
        if (payloadBytes > n - (o + 16)) {
            return false; // payload past the end of the span
        }
        payloads.emplace_back(chain.subspan(o + 16, static_cast<std::size_t>(payloadBytes)));
        o += 16u + static_cast<std::size_t>(payloadBytes);
    }
    return false; // no END within the cap
}

// ---------------------------------------------------------------------------
// GIF stream parse inside one CNT payload (spec §2.1, Extract.cpp:221-296)
// ---------------------------------------------------------------------------

// Parses a CNT payload as a sequence of GIF packets. Accepts A+D packets
// (FLG == 0, NREG == 1, REGS low nibble 0xE, 1 <= NLOOP <= 64; a register
// address above 0x63 invalidates the packet) and the IMAGE packets that carry
// an upload's pixels. Anything else inside the texture chain is malformed.
bool parseUploadPayload(std::span<const std::uint8_t> payload, ParsedChain& out) {
    const std::size_t size = payload.size();
    std::size_t pos = 0;

    while (pos < size) {
        if (pos + 16 > size) {
            return false; // dangling partial tag
        }
        const std::uint64_t tag = readU64Le(payload, pos);
        const std::uint32_t flg = static_cast<std::uint32_t>(field(tag, 58, 2));
        const std::uint32_t nloop = static_cast<std::uint32_t>(tag & 0x7FFFu);

        if (flg == 2) {
            // IMAGE data only ever follows an A+D upload packet, which consumes
            // it inline; a stray IMAGE tag is malformed.
            return false;
        }
        if (flg != 0) {
            return false; // REGLIST / reserved packets do not occur in upload chains
        }

        const std::uint64_t regs = readU64Le(payload, pos + 8);
        const std::uint32_t nreg = static_cast<std::uint32_t>(field(tag, 60, 4));
        if (nreg != 1 || (regs & 0xFu) != 0xE || nloop == 0 || nloop > 64) {
            return false; // not a well-formed A+D packet
        }
        const std::size_t packetEnd = pos + 16u * (static_cast<std::size_t>(nloop) + 1u);
        if (packetEnd > size) {
            return false; // register data truncated
        }

        std::uint64_t blitBuf = 0;
        std::uint64_t transferPos = 0;
        std::uint64_t transferReg = 0;
        std::uint64_t transferDir = 0;
        bool haveBlit = false, havePos = false, haveReg = false, haveDir = false;
        std::vector<std::uint64_t> texStates;

        for (std::uint32_t i = 0; i < nloop; ++i) {
            const std::size_t at = pos + 16u * (static_cast<std::size_t>(i) + 1u);
            const std::uint64_t value = readU64Le(payload, at);
            const std::uint64_t addr = readU64Le(payload, at + 8) & 0xFFu; // A+D: low byte
            if (addr > kMaxRegAddr) {
                return false; // invalidates the packet (reference drops it; we reject)
            }
            switch (addr) {
            case kRegBlitBuf: blitBuf = value; haveBlit = true; break;
            case kRegTransferPos: transferPos = value; havePos = true; break;
            case kRegTransferReg: transferReg = value; haveReg = true; break;
            case kRegTransferDir: transferDir = value & 3u; haveDir = true; break;
            case kRegTexState1:
            case kRegTexState2: texStates.push_back(value); break;
            default: break; // unknown registers are ignored (Extract.cpp:253)
            }
        }

        for (std::uint64_t t : texStates) {
            TexState x;
            x.tbp = static_cast<std::uint32_t>(field(t, 0, 14));
            x.tbw = static_cast<std::uint32_t>(field(t, 14, 6));
            x.psm = static_cast<std::uint32_t>(field(t, 20, 6));
            x.cbp = static_cast<std::uint32_t>(field(t, 37, 14));
            x.cpsm = static_cast<std::uint32_t>(field(t, 51, 4));
            x.csm = static_cast<std::uint32_t>(field(t, 55, 1));
            x.csa = static_cast<std::uint32_t>(field(t, 56, 5));
            if (isIndexed(x.psm)) { // the reference only keeps indexed-format entries
                out.texStates.push_back(x);
            }
        }

        if (!(haveBlit && havePos && haveReg && haveDir && transferDir == 0)) {
            pos = packetEnd; // not a host-to-local upload; nothing follows it here
            continue;
        }

        UploadPacket u;
        u.dbp = static_cast<std::uint32_t>(field(blitBuf, 32, 14));
        u.dbw = static_cast<std::uint32_t>(field(blitBuf, 48, 6));
        u.psm = static_cast<std::uint32_t>(field(blitBuf, 56, 6));
        u.dx = static_cast<std::uint32_t>(field(transferPos, 32, 11));
        u.dy = static_cast<std::uint32_t>(field(transferPos, 48, 11));
        u.w = static_cast<std::uint32_t>(field(transferReg, 0, 12));
        u.h = static_cast<std::uint32_t>(field(transferReg, 32, 12));

        const std::uint32_t bpp = bitsPerPixel(u.psm);
        if (bpp == 0 || u.w == 0 || u.h == 0 || u.dbw == 0) {
            return false; // unusable upload descriptor inside a texture chain
        }
        const std::size_t texels = static_cast<std::size_t>(u.w) * u.h;
        const std::size_t bytes = (texels * bpp + 7u) / 8u;

        // Consume the IMAGE packets that carry the pixels (Extract.cpp:276-291).
        std::size_t at = packetEnd;
        while (u.pixels.size() < bytes) {
            if (at + 16 > size) {
                return false; // truncated IMAGE tag
            }
            const std::uint64_t t = readU64Le(payload, at);
            if (field(t, 58, 2) != 2) {
                return false; // upload without its IMAGE data
            }
            const std::size_t chunk = static_cast<std::size_t>(t & 0x7FFFu) * 16u;
            at += 16;
            if (chunk > size - at) {
                return false; // IMAGE payload past the end of the CNT
            }
            u.pixels.insert(u.pixels.end(), payload.begin() + static_cast<std::ptrdiff_t>(at),
                            payload.begin() + static_cast<std::ptrdiff_t>(at + chunk));
            at += chunk;
        }
        u.pixels.resize(bytes); // a final IMAGE tag may overshoot (reference truncates)
        out.uploads.push_back(std::move(u));
        pos = at;
    }
    return true;
}

// Walks the chain and parses every CNT payload. Returns false on any malformed
// input; on success also selects the target texture: the first upload the
// reference decoder would emit (full-destination, and either indexed or not
// palette-shaped; Extract.cpp:361-373).
bool parseChain(std::span<const std::uint8_t> uploadChain, ParsedChain& out) {
    std::vector<std::span<const std::uint8_t>> payloads;
    if (!walkChain(uploadChain, payloads)) {
        return false;
    }
    for (const std::span<const std::uint8_t>& payload : payloads) {
        if (!parseUploadPayload(payload, out)) {
            return false;
        }
    }
    std::sort(out.texStates.begin(), out.texStates.end());
    out.texStates.erase(std::unique(out.texStates.begin(), out.texStates.end()),
                        out.texStates.end());

    for (std::size_t i = 0; i < out.uploads.size(); ++i) {
        const UploadPacket& u = out.uploads[i];
        if (u.dx != 0 || u.dy != 0) {
            continue; // partial uploads are never decoded (Extract.cpp:366)
        }
        if (isIndexed(u.psm) || !isClutShaped(u.psm, u.w, u.h)) {
            out.targetIndex = i;
            out.hasTarget = true;
            break;
        }
    }
    return out.hasTarget;
}

// ---------------------------------------------------------------------------
// Palette selection (Extract.cpp:374-402)
// ---------------------------------------------------------------------------

// Builds the ordered palette selections for the target texture, mirroring the
// reference's emit order. For a direct-colour target there is exactly one
// (unused) selection. Returns false when an indexed target has no palette at
// all — the reference emits nothing then, so the chain has no decodable image.
bool buildPaletteChoices(const ParsedChain& chain, std::vector<PaletteChoice>& out) {
    const UploadPacket& target = chain.uploads[chain.targetIndex];
    if (!isIndexed(target.psm)) {
        out.push_back({0, 0, 0});
        return true;
    }

    for (const TexState& t : chain.texStates) {
        if (t.tbp == target.dbp && t.psm == target.psm && t.csm == 0) {
            out.push_back({t.cbp, t.cpsm, t.csa});
        }
    }

    // The next-upload rule: the upload right after the texture is its palette
    // when it is palette-shaped (Extract.cpp:381). It is only consulted when no
    // texture-state register in the chain names a palette.
    const std::size_t next = chain.targetIndex + 1;
    if (out.empty() && next < chain.uploads.size()) {
        const UploadPacket& c = chain.uploads[next];
        if (isClutShaped(c.psm, c.w, c.h)) {
            // A 256-colour (16x16) palette holds sixteen 16-colour banks for a
            // 4-bit texture; the reference emits all of them (allBanks default).
            // The 16x2 "second palette at DBP+1" probe the reference writes to
            // its palette-variants folder is an exploratory guess and is not
            // reproduced here (spec §2.3 marks the real bank UNKNOWN).
            const std::uint32_t banks =
                (isFourBit(target.psm) && c.w == 16 && c.h == 16) ? 16u : 1u;
            for (std::uint32_t b = 0; b < banks; ++b) {
                out.push_back({c.dbp, c.psm, b});
            }
        }
    }
    return !out.empty();
}

// Finds the upload that stores the CLUT at `cbp`: the latest upload up to and
// including the one right after the target (the reference's write schedule at
// the emit point) whose destination equals cbp in the same format. Without GS
// block arithmetic only an exact base-pointer match is representable; anything
// else rejects.
const UploadPacket* findPaletteSource(const ParsedChain& chain, std::uint32_t cbp,
                                      std::uint32_t cpsm) {
    const std::size_t limit = std::min(chain.targetIndex + 1, chain.uploads.size() - 1);
    const UploadPacket* found = nullptr;
    for (std::size_t j = 0; j <= limit; ++j) {
        const UploadPacket& c = chain.uploads[j];
        if (c.dbp == cbp && c.psm == cpsm) {
            found = &c; // latest write wins, matching the replay order
        }
    }
    return found;
}

// ---------------------------------------------------------------------------
// Record assembly
// ---------------------------------------------------------------------------

ImageRecord makeRecord(const UploadPacket& target, std::vector<std::uint8_t> rgba,
                       const Provenance& provenance, const std::string& variantId) {
    ImageRecord record;
    record.width = target.w;
    record.height = target.h;
    record.mipCount = 1; // upload chains carry a single level; no mip registers
    record.format = PixelFormat::R8G8B8A8_UNORM;
    record.pixels = std::move(rgba);
    record.sourceVariantId = variantId;
    record.provenance = provenance;
    return record;
}

// Shared implementation: plan the decode once, then render one palette bank.
struct DecodePlan {
    ParsedChain chain;
    std::vector<PaletteChoice> choices;
    bool valid = false;
};

DecodePlan planDecode(std::span<const std::uint8_t> uploadChain) {
    DecodePlan plan;
    if (!parseChain(uploadChain, plan.chain)) {
        return plan;
    }
    if (!buildPaletteChoices(plan.chain, plan.choices)) {
        return plan;
    }
    plan.valid = true;
    return plan;
}

ImageRecord renderBank(const DecodePlan& plan, const Provenance& provenance,
                       const std::string& variantId, std::uint32_t bank) {
    if (!plan.valid || bank >= plan.choices.size()) {
        return {};
    }
    const ParsedChain& chain = plan.chain;
    const UploadPacket& target = chain.uploads[chain.targetIndex];
    const PaletteChoice choice = plan.choices[bank];

    std::vector<std::uint8_t> rgba;
    if (!isIndexed(target.psm)) {
        if (!decodeTexture(target, nullptr, 0, 0, rgba)) {
            return {};
        }
        return makeRecord(target, std::move(rgba), provenance, variantId);
    }

    const UploadPacket* source = findPaletteSource(chain, choice.cbp, choice.cpsm);
    if (source == nullptr) {
        return {}; // CLUT base does not match any upload this decoder can model
    }

    // A 16x16 palette right after a 4-bit texture is a 256-entry palette: load
    // all 256 entries (as 8-bit does) with start address 0 and let the decode
    // bank offset pick the 16-colour group (Extract.cpp:398-400). The reference
    // tests the next upload (its clutUp), not whichever upload matched cbp.
    const std::size_t next = chain.targetIndex + 1;
    const bool bankOf256 =
        isFourBit(target.psm) && next < chain.uploads.size() &&
        isClutShaped(chain.uploads[next].psm, chain.uploads[next].w, chain.uploads[next].h) &&
        chain.uploads[next].w == 16 && chain.uploads[next].h == 16 &&
        choice.cbp == chain.uploads[next].dbp;

    std::array<std::uint16_t, 512> clut{};
    if (!loadClut(*source, bankOf256 ? kPsmT8 : target.psm, choice.cpsm,
                  bankOf256 ? 0u : choice.csa, clut)) {
        return {};
    }
    if (!decodeTexture(target, &clut, choice.cpsm, choice.csa, rgba)) {
        return {};
    }
    return makeRecord(target, std::move(rgba), provenance, variantId);
}

} // namespace

// ---------------------------------------------------------------------------
// Public entry points
// ---------------------------------------------------------------------------

ImageRecord convertImage(std::span<const std::uint8_t> uploadChain,
                         const Provenance& provenance, const std::string& variantId) {
    return convertImageBank(uploadChain, provenance, variantId, 0);
}

ImageRecord convertImageBank(std::span<const std::uint8_t> uploadChain,
                             const Provenance& provenance, const std::string& variantId,
                             std::uint32_t bank) {
    return renderBank(planDecode(uploadChain), provenance, variantId, bank);
}

std::uint32_t convertImageBankCount(std::span<const std::uint8_t> uploadChain) {
    const DecodePlan plan = planDecode(uploadChain);
    return plan.valid ? static_cast<std::uint32_t>(plan.choices.size()) : 0u;
}

} // namespace rt::native::assets
