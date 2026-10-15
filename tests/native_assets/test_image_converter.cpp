// tests/native_assets/test_image_converter.cpp
//
// Synthetic contract tests for the authored offline texture converter
// (src/native/assets/image_converter.{h,cpp}).
//
// - Includes only the production header (relative include). The implementation
//   lives in src/native/assets/image_converter.cpp; a future CMake target links
//   this executable against it. No mocks, no duplicated record types: the
//   frozen ImageRecord / Provenance / PixelFormat come from image_converter.h
//   (via container.h).
// - Every upload chain is hand-built synthetic bytes in the DMA CNT...END /
//   GIF A+D / IMAGE wire shapes the converter documents — no disc bytes
//   anywhere.
// - Expected pixel values are derived from the decode math the implementation
//   documents (re-implementing src/textures/Extract.cpp): linear row-major
//   payload, texel i at payload[i] (T8) or low-nibble-first (T4), the CSM1
//   palette-entry swizzle swapping bits 3 and 4, the 32-bit CLUT halfword
//   split (low half at dst, high half at dst+256), TEXA opaque alpha 0x80 with
//   AEM, and the reference export alpha scale min(255, a * 255 / 128).
// - Failure handling is plain counters and if-statements only (no assert();
//   identical behaviour under NDEBUG). Any failed check exits nonzero.
//
// NOT RUN by this task: compiling, linking and executing this file is left to
// the build worker.

#include "../../src/native/assets/image_converter.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iomanip>
#include <iostream>
#include <span>
#include <string>
#include <utility>
#include <vector>

using rt::native::assets::ImageRecord;
using rt::native::assets::PixelFormat;
using rt::native::assets::Provenance;
using rt::native::assets::convertImage;
using rt::native::assets::convertImageBank;
using rt::native::assets::convertImageBankCount;

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

// ---------------------------------------------------------------------------
// Independent little-endian writers, used only to author fixture bytes by hand.
// ---------------------------------------------------------------------------
void put_u16(std::vector<std::uint8_t>& out, std::uint16_t v) {
    out.push_back(static_cast<std::uint8_t>(v & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFFu));
}

void put_u64(std::vector<std::uint8_t>& out, std::uint64_t v) {
    for (int shift = 0; shift < 64; shift += 8) {
        out.push_back(static_cast<std::uint8_t>((v >> shift) & 0xFFu));
    }
}

// ---------------------------------------------------------------------------
// Converter wrappers. The documented failure contract is "empty record, no
// exception"; an exception is a real failure and is reported, not fatal.
// ---------------------------------------------------------------------------
ImageRecord run_convert_image(std::span<const std::uint8_t> chain,
                              const Provenance& provenance,
                              const std::string& variantId, const std::string& what) {
    try {
        return convertImage(chain, provenance, variantId);
    } catch (const std::exception& e) {
        check(false, what + ": convertImage threw an exception: " + e.what());
    } catch (...) {
        check(false, what + ": convertImage threw a non-std exception");
    }
    return ImageRecord{};
}

ImageRecord run_convert_bank(std::span<const std::uint8_t> chain,
                             const Provenance& provenance, const std::string& variantId,
                             std::uint32_t bank, const std::string& what) {
    try {
        return convertImageBank(chain, provenance, variantId, bank);
    } catch (const std::exception& e) {
        check(false, what + ": convertImageBank threw an exception: " + e.what());
    } catch (...) {
        check(false, what + ": convertImageBank threw a non-std exception");
    }
    return ImageRecord{};
}

std::uint32_t run_bank_count(std::span<const std::uint8_t> chain, const std::string& what) {
    try {
        return convertImageBankCount(chain);
    } catch (const std::exception& e) {
        check(false, what + ": convertImageBankCount threw an exception: " + e.what());
    } catch (...) {
        check(false, what + ": convertImageBankCount threw a non-std exception");
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Synthetic provenance (nonzero digest so a zero-filled copy can never compare
// equal by accident).
// ---------------------------------------------------------------------------
Provenance make_prov() {
    Provenance p;
    p.sourceFile = "synthetic/upload_chain.bin";
    p.offset = 0x4000;
    p.length = 0x1000;
    for (std::size_t i = 0; i < p.sha256.size(); ++i) {
        p.sha256[i] = static_cast<std::uint8_t>(i + 1);
    }
    return p;
}

// The malformed/truncated/oversized sentinel is a default-constructed record.
// Takes the chain by const vector reference so hand-built temporaries bind
// without any span-conversion pitfall.
void expect_empty(const std::vector<std::uint8_t>& chain, const std::string& what) {
    const ImageRecord record = run_convert_image(chain, make_prov(), "negative", what);
    check(record == ImageRecord{}, what + ": returns the empty-record sentinel");
    check(record.width == 0, what + ": sentinel width == 0 (loader-rejected)");
    check(record.pixels.empty(), what + ": sentinel carries no pixels");
    check(run_bank_count(chain, what) == 0, what + ": bank count is 0");
}

// ---------------------------------------------------------------------------
// DMA CNT...END tag builders (spec §8.2: qwc = u16 little-endian at tag+0,
// id byte at tag+3 — 0x10 CNT, 0x70 END).
// ---------------------------------------------------------------------------
constexpr std::uint8_t kDmaTagCnt = 0x10;
constexpr std::uint8_t kDmaTagEnd = 0x70;

void append_dma_tag(std::vector<std::uint8_t>& out, std::uint8_t id, std::uint64_t qwc) {
    put_u16(out, static_cast<std::uint16_t>(qwc));
    out.push_back(0x00);
    out.push_back(id);
    out.resize(out.size() + 12, 0x00); // remaining tag bytes are walker-ignored
}

// CNT tag + payload [+ END tag]. tagId == kDmaTagEnd and endTag==false support
// the negative fixtures.
std::vector<std::uint8_t> make_chain(const std::vector<std::uint8_t>& payload,
                                     std::uint8_t tagId = kDmaTagCnt,
                                     bool endTag = true) {
    std::vector<std::uint8_t> chain;
    append_dma_tag(chain, tagId, payload.size() / 16);
    chain.insert(chain.end(), payload.begin(), payload.end());
    if (endTag) {
        append_dma_tag(chain, kDmaTagEnd, 0);
    }
    return chain;
}

// ---------------------------------------------------------------------------
// GIF packet builders (spec §2.1: FLG==0 A+D packets with NREG==1 and A+D id
// 0xE in the REGS qword; FLG==2 IMAGE packets carry the pixel payload).
// ---------------------------------------------------------------------------
using RegWrite = std::pair<std::uint64_t, std::uint64_t>; // (register address, value)

// A+D packet tag qword, the single-write descriptor qword, one
// (value, address) qword pair per register write, then the IMAGE chunk that
// carries the upload's pixels.
void append_ad_packet(std::vector<std::uint8_t>& payload,
                     const std::vector<RegWrite>& writes,
                     const std::vector<std::uint8_t>& pixels) {
    put_u64(payload, static_cast<std::uint64_t>(writes.size()) | (1ull << 60)); // NLOOP | NREG=1
    put_u64(payload, 0xE); // REGS qword: low nibble = A+D
    for (const RegWrite& w : writes) {
        put_u64(payload, w.second); // value qword
        put_u64(payload, w.first);  // address qword (low byte is the A+D address)
    }
    if (!pixels.empty()) {
        const std::uint64_t qwords =
            (static_cast<std::uint64_t>(pixels.size()) + 15) / 16;
        put_u64(payload, qwords | (2ull << 58)); // IMAGE tag, FLG = 2
        // The disc chain keeps each IMAGE packet 16-byte aligned: an unused
        // padding qword follows the tag before the pixel data
        // (src/textures/Extract.cpp:276-291 advances by 16 after the tag).
        payload.insert(payload.end(), 8, 0x00);
        payload.insert(payload.end(), pixels.begin(), pixels.end());
        payload.resize(payload.size() + (qwords * 16 - pixels.size()), 0x00);
    }
}

// GIF A+D addresses used by upload chains (format spec §2.1).
constexpr std::uint64_t kAddrBitbltbuf = 0x50;
constexpr std::uint64_t kAddrTrxpos = 0x51;
constexpr std::uint64_t kAddrTrxreg = 0x52;
constexpr std::uint64_t kAddrTrxdir = 0x53;
constexpr std::uint64_t kAddrTex0_1 = 0x06; // texture-state register, context 1

// Pixel storage modes (values documented in the implementation notes).
constexpr std::uint32_t kPsmCt32 = 0x00;
constexpr std::uint32_t kPsmT8 = 0x13;
constexpr std::uint32_t kPsmT4 = 0x14;
constexpr std::uint32_t kPsmUnknown = 0x30;

// BITBLTBUF: DBP bits 32..45, DBW bits 48..53, PSM bits 56..61.
// TRXPOS: DSAX bits 32..42, DSAY bits 48..58.
// TRXREG: width bits 0..11, height bits 32..43. TRXDIR must be 0 for a
// host-to-local upload.
std::vector<RegWrite> make_upload_regs(std::uint32_t dbp, std::uint32_t dbw,
                                       std::uint32_t psm, std::uint32_t dx,
                                       std::uint32_t dy, std::uint32_t w,
                                       std::uint32_t h) {
    const std::uint64_t blitbuf = (static_cast<std::uint64_t>(dbp) << 32) |
                                  (static_cast<std::uint64_t>(dbw) << 48) |
                                  (static_cast<std::uint64_t>(psm) << 56);
    const std::uint64_t trxpos =
        (static_cast<std::uint64_t>(dx) << 32) | (static_cast<std::uint64_t>(dy) << 48);
    const std::uint64_t trxreg =
        static_cast<std::uint64_t>(w) | (static_cast<std::uint64_t>(h) << 32);
    return {
        {kAddrBitbltbuf, blitbuf},
        {kAddrTrxpos, trxpos},
        {kAddrTrxreg, trxreg},
        {kAddrTrxdir, 0},
    };
}

// TEX0-style texture-state register (fields from the implementation notes:
// TBP 0..13, TBW 14..19, PSM 20..25, CBP 37..50, CPSM 51..54, CSM 55, CSA 56..60).
RegWrite make_tex0(std::uint32_t tbp, std::uint32_t tbw, std::uint32_t psm,
                   std::uint32_t cbp, std::uint32_t cpsm, std::uint32_t csm,
                   std::uint32_t csa) {
    const std::uint64_t value = static_cast<std::uint64_t>(tbp) |
                                (static_cast<std::uint64_t>(tbw) << 14) |
                                (static_cast<std::uint64_t>(psm) << 20) |
                                (static_cast<std::uint64_t>(cbp) << 37) |
                                (static_cast<std::uint64_t>(cpsm) << 51) |
                                (static_cast<std::uint64_t>(csm) << 55) |
                                (static_cast<std::uint64_t>(csa) << 56);
    return {kAddrTex0_1, value};
}

// ---------------------------------------------------------------------------
// Texel / palette authors. Expected pixel values are computed from the decode
// math, not from game data.
// ---------------------------------------------------------------------------

// CSM1 load order: bits 3 and 4 of the 8-bit CLUT entry index are swapped
// (an involution). A 16x16 CT32 palette upload stores entry e at payload
// position swizzle(e).
std::uint32_t csm1_swizzle(std::uint32_t e) {
    return (e & ~0x18u) | ((e & 0x08u) << 1) | ((e & 0x10u) >> 1);
}

// Raw 32-bit palette entry for CLUT index e, so the decode math returns
// exactly this value as the logical RGBA byte pattern:
//   raw = r | g<<8 | b<<16 | a<<24 recombines as clut[e] | clut[e+256]<<16.
// Entry 0 is the all-zero AEM-transparent entry; every other entry is opaque
// with the TEXA alpha 0x80.
std::uint32_t pal_raw(std::uint32_t e) {
    if (e == 0) {
        return 0;
    }
    const std::uint32_t r = e & 0xFFu;
    const std::uint32_t g = (e * 3 + 11) & 0xFFu;
    const std::uint32_t b = (e * 7 + 5) & 0xFFu;
    const std::uint32_t a = 0x80u;
    return r | (g << 8) | (b << 16) | (a << 24);
}

// The 16x16 CT32 palette upload payload: 256 little-endian u32 entries. Because
// the loader swizzles, entry e's value must be stored at position swizzle(e).
std::vector<std::uint8_t> palette_payload_ct32_16x16() {
    std::vector<std::uint8_t> pixels(256 * 4, 0x00);
    for (std::uint32_t e = 0; e < 256; ++e) {
        const std::size_t pos = csm1_swizzle(e) * 4;
        const std::uint32_t raw = pal_raw(e);
        for (int i = 0; i < 4; ++i) {
            pixels[pos + static_cast<std::size_t>(i)] =
                static_cast<std::uint8_t>((raw >> (8 * i)) & 0xFFu);
        }
    }
    return pixels;
}

// T8 texture payload: texel i's byte value is i % 256 (identity texture).
std::vector<std::uint8_t> t8_texture_payload(std::size_t texels) {
    std::vector<std::uint8_t> pixels(texels, 0x00);
    for (std::size_t i = 0; i < texels; ++i) {
        pixels[i] = static_cast<std::uint8_t>(i % 256);
    }
    return pixels;
}

// T4 texture payload: texel i's nibble value is i % 16, low nibble first.
// Callers use an even texel count so the byte count is exact.
std::vector<std::uint8_t> t4_texture_payload(std::size_t texels) {
    std::vector<std::uint8_t> pixels(texels / 2, 0x00);
    for (std::size_t j = 0; j < pixels.size(); ++j) {
        const std::uint8_t lo = static_cast<std::uint8_t>((2 * j) % 16);
        const std::uint8_t hi = static_cast<std::uint8_t>((2 * j + 1) % 16);
        pixels[j] = static_cast<std::uint8_t>(lo | (hi << 4));
    }
    return pixels;
}

// ---------------------------------------------------------------------------
// Expected record contents, derived from the decode math:
// - decoded alpha is the export scale min(255, a * 255 / 128): TEXA 0x80 gives
//   exactly 255, an all-zero entry gives 0 (AEM).
// - the decoded logical RGBA of a T8/T4 texel selecting CLUT entry e is
//   pal_raw(e) recombined, whose bytes export as [r, g, b, scaled alpha].
// ---------------------------------------------------------------------------
void push_expected_pixel(std::vector<std::uint8_t>& out, std::uint32_t raw) {
    const std::uint32_t a = (raw >> 24) & 0xFFu;
    out.push_back(static_cast<std::uint8_t>(raw & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((raw >> 8) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((raw >> 16) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>(
        std::min(255u, a * 255u / 128u)));
}

std::vector<std::uint8_t> expected_t8_rgba() {
    std::vector<std::uint8_t> out;
    out.reserve(128 * 128 * 4);
    for (std::size_t i = 0; i < 128 * 128; ++i) {
        push_expected_pixel(out, pal_raw(i % 256));
    }
    return out;
}

std::vector<std::uint8_t> expected_t4_bank_rgba(std::uint32_t bank) {
    std::vector<std::uint8_t> out;
    out.reserve(16 * 16 * 4);
    for (std::size_t i = 0; i < 16 * 16; ++i) {
        push_expected_pixel(out, pal_raw(bank * 16 + i % 16));
    }
    return out;
}

ImageRecord expected_record(std::uint32_t w, std::uint32_t h,
                            const std::vector<std::uint8_t>& pixels,
                            const Provenance& provenance, const std::string& variantId) {
    ImageRecord r;
    r.width = w;
    r.height = h;
    r.mipCount = 1;
    r.format = PixelFormat::R8G8B8A8_UNORM;
    r.pixels = pixels;
    r.sourceVariantId = variantId;
    r.provenance = provenance;
    return r;
}

// A successful record must equal the expected record field-for-field (using the
// frozen defaulted ==) and carry the exact provenance/variant given.
void expect_record_matches(std::span<const std::uint8_t> chain, std::uint32_t bank,
                           std::uint32_t w, std::uint32_t h,
                           const std::vector<std::uint8_t>& expectedPixels,
                           const Provenance& provenance, const std::string& variantId,
                           const std::string& what) {
    const ImageRecord r = run_convert_bank(chain, provenance, variantId, bank, what);
    const ImageRecord want = expected_record(w, h, expectedPixels, provenance, variantId);
    check(r == want,
          what + ": record equals the decode-math expectation (defaulted ==, all fields)");
    if (r != want) {
        check(r.width == w && r.height == h && r.mipCount == 1,
              what + ": width/height/mipCount");
        check(r.format == PixelFormat::R8G8B8A8_UNORM, what + ": format is R8G8B8A8_UNORM");
        check(r.sourceVariantId == variantId, what + ": sourceVariantId propagated");
        check(r.provenance == provenance, what + ": provenance propagated");
        check(r.pixels == expectedPixels, what + ": pixel bytes");
    }
}

// ---------------------------------------------------------------------------
// Chain fixtures.
// ---------------------------------------------------------------------------

// Valid 8-bit chain: [T8 128x128 texture][CT32 16x16 palette], palette found by
// the next-upload rule (a 16x16 palette backs a 256-entry CLUT for one bank).
std::vector<std::uint8_t> chain_t8_ct32() {
    std::vector<std::uint8_t> payload;
    append_ad_packet(payload, make_upload_regs(100, 16, kPsmT8, 0, 0, 128, 128),
                     t8_texture_payload(128 * 128));
    append_ad_packet(payload, make_upload_regs(400, 1, kPsmCt32, 0, 0, 16, 16),
                     palette_payload_ct32_16x16());
    return make_chain(payload);
}

// Valid 4-bit chain: [T4 16x16 texture][CT32 16x16 palette]. A 256-entry
// palette next to a 4-bit texture holds sixteen 16-colour banks.
std::vector<std::uint8_t> chain_t4_ct32() {
    std::vector<std::uint8_t> payload;
    append_ad_packet(payload, make_upload_regs(100, 16, kPsmT4, 0, 0, 16, 16),
                     t4_texture_payload(16 * 16));
    append_ad_packet(payload, make_upload_regs(400, 1, kPsmCt32, 0, 0, 16, 16),
                     palette_payload_ct32_16x16());
    return make_chain(payload);
}

// Valid 4-bit chain whose palette is named by a TEX0 texture-state register
// instead of the next-upload rule (register route wins, so a single bank).
std::vector<std::uint8_t> chain_t4_texstate() {
    std::vector<std::uint8_t> payload;
    std::vector<RegWrite> writes = make_upload_regs(100, 16, kPsmT4, 0, 0, 16, 16);
    writes.push_back(make_tex0(100, 16, kPsmT4, 400, kPsmCt32, 0, 0));
    append_ad_packet(payload, writes, t4_texture_payload(16 * 16));
    append_ad_packet(payload, make_upload_regs(400, 1, kPsmCt32, 0, 0, 16, 16),
                     palette_payload_ct32_16x16());
    return make_chain(payload);
}

} // namespace

// ---------------------------------------------------------------------------
// Case A: valid 8-bit 128x128 + CT32 16x16 palette chain decodes to packed
// RGBA8 with the math-expected pixel values (single CLUT bank).
// ---------------------------------------------------------------------------
namespace {

void test_valid_8bit_128x128_ct32() {
    const std::vector<std::uint8_t> chain = chain_t8_ct32();
    std::cerr << "chainA size=" << chain.size() << "\n";
    for (std::size_t i = 16470; i < 16500 && i < chain.size(); ++i) {
        std::cerr << std::hex << (int)chain[i] << " ";
    }
    std::cerr << std::dec << "\n";
    const std::size_t texels = 128 * 128;
    check(chain.size() == 32 + 2 * (16 + 4 * 16 + 16) + texels + 256 * 4,
          "chain A: fixture size matches the hand-built shape");

    const std::vector<std::uint8_t> expectedPixels = expected_t8_rgba();
    check(expectedPixels.size() == texels * 4, "chain A: expected vector is 65536 bytes");

    const Provenance provenance = make_prov();
    const std::string variant = "valid-8bit";
    const ImageRecord r = run_convert_image(chain, provenance, variant, "chain A decode");
    const ImageRecord want = expected_record(128, 128, expectedPixels, provenance, variant);
    check(r == want,
          "chain A: decode equals the decode-math record (128x128, mips 1, UNORM, "
          "packed RGBA8, variant and provenance propagated)");
    if (r != want) {
        check(r.width == 128 && r.height == 128 && r.mipCount == 1,
              "chain A: dimensions/mipCount");
        check(r.format == PixelFormat::R8G8B8A8_UNORM, "chain A: format");
        check(r.pixels == expectedPixels, "chain A: pixels");
        check(r.sourceVariantId == variant, "chain A: variant");
        check(r.provenance == provenance, "chain A: provenance");
    }

    // Spot checks on the derived math: entry 0 is the AEM-transparent zero
    // entry, entry 1 is opaque with TEXA alpha 0x80 exported as 255, and the
    // last texel wraps to entry 255.
    const std::uint8_t* p0 = expectedPixels.data();
    const std::uint8_t* pLast =
        expectedPixels.data() + expectedPixels.size() - 4;
    check(p0[0] == 0 && p0[1] == 0 && p0[2] == 0 && p0[3] == 0,
          "chain A: texel 0 (entry 0) exports all-zero (AEM transparent)");
    check(p0[4] == 0x01 && p0[5] == 0x0E && p0[6] == 0x0C && p0[7] == 255,
          "chain A: texel 1 (entry 1) exports {1, 14, 12, 255}");
    check(pLast[0] == 255 && pLast[1] == 8 && pLast[2] == 254 && pLast[3] == 255,
          "chain A: texel 16383 (entry 255) exports wrapped channel values");

    check(run_bank_count(chain, "chain A banks") == 1,
          "chain A: bank count is 1 (single 16x16 palette, 8-bit texture)");
    const ImageRecord bank1 =
        run_convert_bank(chain, provenance, variant, 1, "chain A bank 1");
    check(bank1 == ImageRecord{},
          "chain A: bank >= bank count yields the empty-record sentinel");
    const ImageRecord bank0 = run_convert_bank(chain, provenance, variant, 0, "chain A bank 0");
    check(bank0 == r, "chain A: convertImage == convertImageBank(bank 0)");
}

} // namespace

// ---------------------------------------------------------------------------
// Case B: 4-bit + 16x16 palette produces 16 CLUT banks; each bank decodes with
// its own bank offset; convertImage == bank 0; banks beyond the count reject.
// ---------------------------------------------------------------------------
namespace {

void test_four_bit_sixteen_banks() {
    const std::vector<std::uint8_t> chain = chain_t4_ct32();
    const Provenance provenance = make_prov();

    const std::uint32_t banks = run_bank_count(chain, "chain B banks");
    check(banks == 16, "chain B: 4-bit + 16x16 palette yields 16 CLUT banks");

    // Every bank 0..15 decodes; a bank's texel i selects CLUT entry bank*16 + i%16.
    std::vector<std::vector<std::uint8_t>> expectedPerBank;
    for (std::uint32_t b = 0; b < banks; ++b) {
        expectedPerBank.push_back(expected_t4_bank_rgba(b));
        expect_record_matches(chain, b, 16, 16, expectedPerBank.back(), provenance,
                              "valid-4bit-bank", "chain B bank " + std::to_string(b));
    }

    // Bank distinction: banks differ from each other in every stored byte set.
    check(expectedPerBank[0] != expectedPerBank[1],
          "chain B: bank palette offsets produce distinct pixel sets");
    check(expectedPerBank[0][3] == 0, // pixel 0 (texel 0, entry 0) alpha: AEM zero entry
          "chain B: bank 0 texel 0 is the transparent zero entry");

    // convertImage == convertImageBank(0).
    const ImageRecord bank0 = run_convert_image(chain, provenance, "valid-4bit-bank", "chain B image");
    const ImageRecord want0 = expected_record(16, 16, expectedPerBank[0], provenance, "valid-4bit-bank");
    check(bank0 == want0, "chain B: convertImage decodes bank 0");

    // Past the bank count: empty-record sentinel.
    check(run_convert_bank(chain, provenance, "valid-4bit-bank", 16, "chain B bank 16") ==
              ImageRecord{},
          "chain B: bank 16 (== count) yields the empty-record sentinel");
    check(run_convert_bank(chain, provenance, "valid-4bit-bank", 999, "chain B bank 999") ==
              ImageRecord{},
          "chain B: far-out bank index yields the empty-record sentinel");
}

} // namespace

// ---------------------------------------------------------------------------
// Case C: a TEX0 texture-state register naming the palette takes precedence
// over the next-upload rule, so the same chain shape yields ONE bank (not the
// sixteen of the adjacency rule) and decodes with CSA 0.
// ---------------------------------------------------------------------------
namespace {

void test_texstate_register_palette_route() {
    const std::vector<std::uint8_t> chain = chain_t4_texstate();
    const Provenance provenance = make_prov();

    const std::uint32_t banks = run_bank_count(chain, "chain C banks");
    check(banks == 1,
          "chain C: TEX0-named palette overrides the 16-bank adjacency rule");

    expect_record_matches(chain, 0, 16, 16, expected_t4_bank_rgba(0), provenance,
                          "valid-texstate", "chain C bank 0");
    check(run_convert_bank(chain, provenance, "valid-texstate", 1, "chain C bank 1") ==
              ImageRecord{},
          "chain C: bank 1 yields the empty-record sentinel");
}

} // namespace

// ---------------------------------------------------------------------------
// Malformed / truncated / oversized chains: every one yields the empty-record
// sentinel (width == 0, no pixels) and a bank count of 0 — no partial output,
// no hang, no out-of-bounds read.
// ---------------------------------------------------------------------------
namespace {

void test_malformed_truncated_oversized_chains() {
    const std::vector<std::uint8_t> tex4 = t4_texture_payload(16 * 16);
    const std::vector<std::uint8_t> pal = palette_payload_ct32_16x16();

    // (a) Empty span.
    expect_empty({}, "empty span");

    // (b) END tag first (no CNT payload).
    {
        std::vector<std::uint8_t> chain;
        append_dma_tag(chain, kDmaTagEnd, 0);
        expect_empty(chain, "END-only chain");
    }

    // (c) DMA tag id that is neither CNT nor END.
    {
        std::vector<std::uint8_t> chain;
        append_dma_tag(chain, 0x00, 0);
        append_dma_tag(chain, kDmaTagEnd, 0);
        expect_empty(chain, "DMA tag id 0x00 is not a texture chain");

        chain.clear();
        append_dma_tag(chain, kDmaTagCnt, 0); // valid CNT with empty payload first
        append_dma_tag(chain, 0x11, 0);
        append_dma_tag(chain, kDmaTagEnd, 0);
        expect_empty(chain, "DMA tag id 0x11 is not a texture chain");
    }

    // (d) QWC claims more payload bytes than the span holds (oversized claim).
    {
        std::vector<std::uint8_t> chain;
        append_dma_tag(chain, kDmaTagCnt, 4); // claims 64 payload bytes
        chain.insert(chain.end(), 16, 0x00);  // only 16 present
        expect_empty(chain, "CNT qwc claims beyond the span");

        chain.clear();
        append_dma_tag(chain, kDmaTagCnt, 0xFFFF); // claims ~1 MB, chain is tiny
        append_dma_tag(chain, kDmaTagEnd, 0);
        expect_empty(chain, "CNT qwc 0xFFFF against a 32-byte chain");
    }

    // (e) Valid payload but no END tag anywhere.
    {
        std::vector<std::uint8_t> payload;
        append_ad_packet(payload, make_upload_regs(100, 16, kPsmT4, 0, 0, 16, 16), tex4);
        append_ad_packet(payload, make_upload_regs(400, 1, kPsmCt32, 0, 0, 16, 16), pal);
        expect_empty(make_chain(payload, kDmaTagCnt, false), "chain without an END tag");
    }

    // (f) Walk cap: more than 4096 CNT tags without an END never terminates.
    {
        std::vector<std::uint8_t> chain;
        for (int i = 0; i < 4100; ++i) {
            append_dma_tag(chain, kDmaTagCnt, 0);
        }
        expect_empty(chain, "4100 CNT tags with no END exceeds the walk cap");
    }

    // (g) REGLIST (FLG=1) packet inside a texture chain.
    {
        std::vector<std::uint8_t> payload;
        put_u64(payload, (1ull << 60) | (1ull << 58)); // NLOOP=1, NREG=1, FLG=1
        put_u64(payload, 0xE);
        expect_empty(make_chain(payload), "REGLIST packet in a texture chain");
    }

    // (h) A+D packet with NLOOP=0 (no upload descriptor follows).
    {
        std::vector<std::uint8_t> payload;
        put_u64(payload, 1ull << 60); // NLOOP=0, NREG=1, FLG=0
        put_u64(payload, 0xE);
        expect_empty(make_chain(payload), "empty NLOOP=0 A+D packet");
    }

    // (i) Stray IMAGE tag with no preceding A+D upload (qword-aligned 16 bytes).
    {
        std::vector<std::uint8_t> payload;
        put_u64(payload, (2ull << 58)); // FLG=2 immediately
        put_u64(payload, 0x00);
        expect_empty(make_chain(payload), "stray IMAGE tag first in packet");
    }

    // (j) A+D register address above the valid range invalidates the packet.
    {
        std::vector<std::uint8_t> payload;
        put_u64(payload, 1ull | (1ull << 60)); // NLOOP=1
        put_u64(payload, 0xE);
        put_u64(payload, 0);       // value
        put_u64(payload, 0x64);    // address 0x64 > the 0x63 cap
        expect_empty(make_chain(payload), "A+D register address 0x64");
    }

    // (k) Host-to-local upload without its IMAGE data (payload ends first).
    {
        std::vector<std::uint8_t> payload;
        append_ad_packet(payload, make_upload_regs(100, 16, kPsmT4, 0, 0, 16, 16), {});
        expect_empty(make_chain(payload), "upload missing its IMAGE data");
    }

    // (l) Descriptor larger than its pixel data (oversized TRXREG vs payload):
    // the claimed 128x128 T8 upload needs 16384 bytes, only 8192 are present.
    {
        std::vector<std::uint8_t> payload;
        const std::vector<RegWrite> regs =
            make_upload_regs(100, 16, kPsmT8, 0, 0, 128, 128);
        put_u64(payload, regs.size() | (1ull << 60));
        put_u64(payload, 0xE);
        for (const RegWrite& w : regs) {
            put_u64(payload, w.second);
            put_u64(payload, w.first);
        }
        put_u64(payload, ((8192 + 15) / 16) | (2ull << 58)); // half the IMAGE data
        payload.insert(payload.end(), 8192, 0x00);
        expect_empty(make_chain(payload), "oversized descriptor vs pixel payload");
    }

    // (m) Upload descriptor with an unusable field: DBW = 0.
    {
        std::vector<std::uint8_t> payload;
        append_ad_packet(payload, make_upload_regs(100, 0, kPsmT4, 0, 0, 16, 16), {});
        expect_empty(make_chain(payload), "upload descriptor with DBW 0");
    }

    // (n) Zero dimensions in TRXREG.
    {
        std::vector<std::uint8_t> payload;
        append_ad_packet(payload, make_upload_regs(100, 16, kPsmT4, 0, 0, 0, 0), {});
        expect_empty(make_chain(payload), "upload descriptor with 0x0 dimensions");
    }

    // (o) Unknown pixel storage mode in BITBLTBUF (bpp == 0).
    {
        std::vector<std::uint8_t> payload;
        append_ad_packet(payload, make_upload_regs(100, 16, kPsmUnknown, 0, 0, 16, 16), {});
        expect_empty(make_chain(payload), "upload with unknown PSM 0x30");
    }

    // (p) TRXDIR != 0 (not a host-to-local upload, so nothing decodable).
    {
        std::vector<std::uint8_t> payload;
        std::vector<RegWrite> regs = make_upload_regs(100, 16, kPsmT4, 0, 0, 16, 16);
        regs.back().second = 1; // TRXDIR = 1
        append_ad_packet(payload, regs, tex4);
        append_ad_packet(payload, make_upload_regs(400, 1, kPsmCt32, 0, 0, 16, 16), pal);
        expect_empty(make_chain(payload), "TRXDIR=1 is not an upload");
    }

    // (q) Partial-destination upload (DSAX != 0): partial uploads are never
    // decoded, and an otherwise valid chain with only partial uploads has no
    // emittable texture.
    {
        std::vector<std::uint8_t> payload;
        append_ad_packet(payload, make_upload_regs(100, 16, kPsmT4, 1, 0, 16, 16), tex4);
        append_ad_packet(payload, make_upload_regs(400, 1, kPsmCt32, 0, 0, 16, 16), pal);
        expect_empty(make_chain(payload), "DSAX!=0 partial upload only");
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Indexed target without any usable palette: the reference emits nothing, so
// the chain has no decodable image.
// ---------------------------------------------------------------------------
namespace {

void test_indexed_without_palette() {
    const std::vector<std::uint8_t> tex4 = t4_texture_payload(16 * 16);

    // (a) Texture alone: no palette to select.
    {
        std::vector<std::uint8_t> payload;
        append_ad_packet(payload, make_upload_regs(100, 16, kPsmT4, 0, 0, 16, 16), tex4);
        expect_empty(make_chain(payload), "4-bit texture with no palette upload");
    }

    // (b) Texture followed by a non-palette-shaped upload (32x2 CT32 is wider
    // than the 16-texel palette bound): no palette choice applies.
    {
        std::vector<std::uint8_t> payload;
        append_ad_packet(payload, make_upload_regs(100, 16, kPsmT4, 0, 0, 16, 16), tex4);
        append_ad_packet(payload, make_upload_regs(400, 32, kPsmCt32, 0, 0, 32, 2),
                         std::vector<std::uint8_t>(32 * 2 * 4, 0x00));
        expect_empty(make_chain(payload),
                     "adjacent upload is not palette-shaped (32x2)");
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Bounds: every strict prefix of a valid chain is malformed (no END in reach),
// so all truncations must give the sentinel or — if a walk is lucky enough to
// still find a complete upload — a record pixel-identical to a correct decode.
// Nothing may crash or produce garbage. Short chains are scanned prefix by
// prefix; the long chain is sampled.
// ---------------------------------------------------------------------------
namespace {

void test_truncated_prefixes_stay_in_bounds() {
    const Provenance provenance = make_prov();
    const std::vector<std::uint8_t> chainB = chain_t4_ct32();

    // Expected bank pixel sets for the accept-if-valid predicate.
    std::vector<std::vector<std::uint8_t>> bankPixels;
    for (std::uint32_t b = 0; b < 16; ++b) {
        bankPixels.push_back(expected_t4_bank_rgba(b));
    }

    for (std::size_t n = 0; n < chainB.size(); ++n) {
        const std::span<const std::uint8_t> prefix =
            std::span<const std::uint8_t>(chainB).first(n);
        const std::string what = "B prefix " + std::to_string(n);

        const ImageRecord r = run_convert_image(prefix, provenance, "prefix", what);
        const ImageRecord empty = ImageRecord{};
        if (r != empty) {
            // Only a complete, correct decode with the full 16x16 shape is
            // acceptable; anything else would be a partial/garbage record.
            bool matchesSomeBank = r.width == 16 && r.height == 16 && r.mipCount == 1;
            for (const auto& px : bankPixels) {
                if (r.pixels == px) {
                    matchesSomeBank = true;
                }
            }
            check(matchesSomeBank, what + ": a non-empty record must be a full correct decode");
        }

        const std::uint32_t banks = run_bank_count(prefix, what);
        check(banks == 0 || banks == 1 || banks == 16,
              what + ": bank count stays within the accepted set");
    }

    // Sample the 16.6 KB 8-bit chain: stride keeps the run cheap while still
    // cutting every structural region (tags, register bytes, IMAGE data, END).
    const std::vector<std::uint8_t> chainA = chain_t8_ct32();
    const ImageRecord fullA = run_convert_image(chainA, provenance, "stride", "A full");
    for (std::size_t n = 0; n < chainA.size(); n += 97) {
        const std::span<const std::uint8_t> prefix =
            std::span<const std::uint8_t>(chainA).first(n);
        const std::string what = "A prefix " + std::to_string(n);
        const ImageRecord r = run_convert_image(prefix, provenance, "stride", what);
        check(r == ImageRecord{} || r == fullA,
              what + ": sentinel or byte-identical full decode only");
    }
}

} // namespace

// ---------------------------------------------------------------------------

int main() {
    test_valid_8bit_128x128_ct32();
    test_four_bit_sixteen_banks();
    test_texstate_register_palette_route();
    test_malformed_truncated_oversized_chains();
    test_indexed_without_palette();
    test_truncated_prefixes_stay_in_bounds();

    if (g_failures > 0) {
        std::cerr << "test_image_converter: " << g_failures << " of " << g_checks
                  << " checks FAILED\n";
        return EXIT_FAILURE;
    }
    std::cout << "test_image_converter: " << g_checks << " checks passed\n";
    return EXIT_SUCCESS;
}
