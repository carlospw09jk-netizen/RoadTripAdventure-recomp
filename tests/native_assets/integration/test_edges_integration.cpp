// tests/native_assets/integration/test_edges_integration.cpp
//
// End-to-end EDGE-CASE integration test for the native asset pipeline, run
// against the user's own disc file CAR0/Q00.BIN at RUN TIME. Where the two
// sibling integration tests drive the happy path (convert -> serialize ->
// deserialize -> load), this suite drives the pipeline's error and variant
// paths over the same real bytes: every input is either (a) a copy of the
// real disc file / real texture chain with one field or tag patched, or
// (b) a chain re-assembled at run time from the real file's own tag and
// pixel bytes with the register replay varied. No game data ships with this
// test; the disc root comes from the environment (RT_DISC, default
// "~/Library/Application Support/RoadTripRecomp/disc"). A missing file is a
// loud setup failure (nonzero exit), not a silent skip.
//
// Coverage areas, all over real disc bytes:
//   1. VIF/DMA walk variants (decodeVifChain): the real main chain re-wired
//      with REF/REFS/NEXT/REFE tags so the walked VIF stream stays
//      byte-identical to the CNT/RET original (equality asserted against the
//      unpatched decode), plus crafted 16-byte tag chains for the truncated
//      payload / out-of-range target / tag-cap overflow paths and a
//      CALL->RET-return chain built from the real tag bytes.
//   2. VIF command skips: chains whose stream carries STMASK/STROW/STCOL/
//      MPG/DIRECT/DIRECTHL codes (skipped by the decoder) and truncated
//      variants of each (the skip would run past the stream end).
//   3. CAR header rejection paths (convertCarModels): single-word patches
//      of the real header (region bound, first start, empty region, too-short
//      main chain), a broken main-chain tag, a NaN vertex word (found by a
//      deterministic scan of the real chain payload), and broken part-chain
//      starts.
//   4. Texture chain variants (convertImage/Bank/Count): patches of the real
//      texture chain's upload descriptors (direct CT32/CT24/CT16/CT16S
//      targets, palette geometry, palette dbw/format, A+D packet size,
//      unknown register, IMAGE tag flg/nloop), plus re-assembled chains that
//      replay the real pixels under texture-state register choices (two
//      CLUT banks, a CLUT base that matches no upload, a 24-bit palette).
//   5. Container serialize() rejections: real records with one field mutated
//      so validateImage/validateMesh must throw.
//   6. Container deserialize() error paths: wires derived from the real
//      serialized containers (cut payloads, patched lengths/counts) and
//      minimal hand-shaped wires for the reader-level failures.
//   7. loadImage() mip-overflow rejections on mutated real records.
//
// Failure accounting is plain counters and if-statements (no assert()), so
// behaviour is NDEBUG-independent; any failed check exits nonzero.

#include "../../../src/native/assets/container.h"
#include "../../../src/native/assets/image_converter.h"
#include "../../../src/native/assets/image_loader.h"
#include "../../../src/native/assets/model_converter.h"
#include "../../../src/native/assets/vif_chain.h"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <span>
#include <string>
#include <vector>

using rt::native::assets::CarModelError;
using rt::native::assets::CarModelResult;
using rt::native::assets::ContainerError;
using rt::native::assets::ContainerResult;
using rt::native::assets::DecodedBatch;
using rt::native::assets::HostAssetContainer;
using rt::native::assets::ImageRecord;
using rt::native::assets::MaterialRef;
using rt::native::assets::MeshRecord;
using rt::native::assets::PixelFormat;
using rt::native::assets::Provenance;
using rt::native::assets::Vertex;
using rt::native::assets::VifChainError;
using rt::native::assets::VifChainResult;
using rt::native::assets::convertCarModels;
using rt::native::assets::convertImage;
using rt::native::assets::convertImageBank;
using rt::native::assets::convertImageBankCount;
using rt::native::assets::decodeVifChain;
using rt::native::assets::deserialize;
using rt::native::assets::hasValidProvenance;
using rt::native::assets::kContainerVersion;
using rt::native::assets::loadImage;
using rt::native::assets::serialize;

namespace {

// ---------------------------------------------------------------------------
// Failure accounting (NDEBUG-independent; no assert() anywhere in this file).
// ---------------------------------------------------------------------------
int g_checks = 0;
int g_failures = 0;

void check(bool cond, const std::string& what) {
    ++g_checks;
    if (!cond) {
        ++g_failures;
        std::cerr << "FAIL: " << what << '\n';
    }
}

// Prints the summary line and returns the process exit code.
int finish() {
    if (g_failures > 0) {
        std::cerr << "test_edges_integration: " << g_failures << " of " << g_checks
                  << " checks FAILED\n";
        return EXIT_FAILURE;
    }
    std::cout << "test_edges_integration: " << g_checks << " checks passed\n";
    return EXIT_SUCCESS;
}

using Bytes = std::vector<std::uint8_t>;
using Digest = std::array<std::uint8_t, 32>;

// ---------------------------------------------------------------------------
// Byte plumbing
// ---------------------------------------------------------------------------
std::uint32_t readU32(const Bytes& b, std::size_t o) {
    return static_cast<std::uint32_t>(b[o]) |
           (static_cast<std::uint32_t>(b[o + 1]) << 8) |
           (static_cast<std::uint32_t>(b[o + 2]) << 16) |
           (static_cast<std::uint32_t>(b[o + 3]) << 24);
}

std::uint64_t readU64(const Bytes& b, std::size_t o) {
    std::uint64_t v = 0;
    for (int i = 7; i >= 0; --i) {
        v = (v << 8) | static_cast<std::uint64_t>(b[o + static_cast<std::size_t>(i)]);
    }
    return v;
}

void writeU32(Bytes& b, std::size_t o, std::uint32_t v) {
    b[o] = static_cast<std::uint8_t>(v);
    b[o + 1] = static_cast<std::uint8_t>(v >> 8);
    b[o + 2] = static_cast<std::uint8_t>(v >> 16);
    b[o + 3] = static_cast<std::uint8_t>(v >> 24);
}

void writeU64(Bytes& b, std::size_t o, std::uint64_t v) {
    for (int i = 0; i < 8; ++i) {
        b[o + static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(v >> (8 * i));
    }
}

// Replaces one bitfield of the little-endian u64 at byte offset o.
void patchU64Field(Bytes& b, std::size_t o, unsigned shift, unsigned bits,
                   std::uint64_t value) {
    const std::uint64_t mask = (bits >= 64) ? ~std::uint64_t{0}
                                            : ((std::uint64_t{1} << bits) - 1);
    const std::uint64_t v = readU64(b, o);
    writeU64(b, o, (v & ~(mask << shift)) | ((value & mask) << shift));
}

// Disc root from the environment: RT_DISC when set, otherwise the standard
// extracted-disc location under the user's home directory.
std::string discRoot() {
    if (const char* env = std::getenv("RT_DISC"); env != nullptr && env[0] != '\0') {
        return env;
    }
    const char* home = std::getenv("HOME");
    if (home == nullptr || home[0] == '\0') {
        return {};
    }
    return (std::filesystem::path(home) / "Library/Application Support/RoadTripRecomp/disc")
        .string();
}

bool readFileBytes(const std::string& path, Bytes& out) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    out.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return !file.bad();
}

// Placeholder provenance digest: FNV-1a over the bytes, expanded to 32 bytes
// by a xorshift64 stream seeded non-zero. NOT SHA-256: hasValidProvenance()
// only rejects the all-zero digest (same placeholder the pipeline tools use;
// no game bytes are embedded here).
Digest placeholderDigest(std::span<const std::uint8_t> bytes) {
    std::uint64_t hash = 0xcbf29ce484222325ull; // FNV-1a 64-bit offset basis
    for (std::uint8_t byte : bytes) {
        hash ^= byte;
        hash *= 0x100000001b3ull; // FNV-1a 64-bit prime
    }
    Digest digest{};
    std::uint64_t state = hash | 1ull; // never zero, so the digest is never all-zero
    for (std::size_t i = 0; i < digest.size(); ++i) {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17; // xorshift64
        digest[i] = static_cast<std::uint8_t>((state >> 24) & 0xFFu);
    }
    return digest;
}

// True when two decodeVifChain results agree on the error and every batch.
bool sameVifResult(const VifChainResult& a, const VifChainResult& b) {
    if (a.error != b.error || a.batches.size() != b.batches.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.batches.size(); ++i) {
        const DecodedBatch& x = a.batches[i];
        const DecodedBatch& y = b.batches[i];
        if (x.vertices != y.vertices || x.indices != y.indices || x.nloop != y.nloop ||
            x.prim != y.prim || x.materialImage != y.materialImage) {
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// DMA tag / VIF chain crafting (tag bytes come from the real file at runtime)
// ---------------------------------------------------------------------------
constexpr std::uint8_t kDmaTagCnt = 0x10; // image-chain walk: byte 3 identity
constexpr std::uint8_t kDmaTagEnd = 0x70;

// A 16-byte VIF/DMA walk tag: lo = qwc | id<<28 | addr<<32, hi = 8 zero bytes
// (two NOP VIF words, so crafted chains never inject stray commands).
Bytes makeWalkTag(std::uint32_t id, std::uint32_t qwc, std::uint64_t addr) {
    Bytes tag(16, 0);
    writeU64(tag, 0, static_cast<std::uint64_t>(qwc) | (static_cast<std::uint64_t>(id) << 28) |
                        (addr << 32));
    return tag;
}

Bytes concat(std::initializer_list<const Bytes*> parts) {
    Bytes out;
    for (const Bytes* p : parts) {
        out.insert(out.end(), p->begin(), p->end());
    }
    return out;
}

// A 32-byte chain [CNT(qwc=0) whose hi is two NOP words][RET whose hi is
// (NOP, cmd)]. The walked stream is [NOP][NOP][NOP][cmd], so decodeStream
// reads cmd as the very last word: every skip-past-end branch fires.
Bytes makeTruncCommandChain(std::uint32_t cmd) {
    Bytes chain = makeWalkTag(1, 0, 0); // CNT, no payload
    Bytes ret = makeWalkTag(6, 0, 0);   // RET, hi = NOP NOP
    writeU32(ret, 12, cmd);              // second hi word is the stream's last word
    return concat({&chain, &ret});
}

// A CNT..RET chain whose stream is: NOP, NOP (tag hi), then the given words
// (payload, padded to a qwc multiple with NOPs), then NOP, NOP (RET hi).
// All words must be complete VIF command words with their skip data present.
Bytes makeCommandChain(std::span<const std::uint32_t> words) {
    const std::size_t payloadWords = words.size() > 2 ? words.size() - 2 : 0;
    const std::size_t payloadQwc = (payloadWords * 4 + 15) / 16;
    Bytes chain(16, 0);
    writeU64(chain, 0, static_cast<std::uint64_t>(payloadQwc) | (1ull << 28)); // CNT
    if (words.size() >= 1) writeU32(chain, 8, words[0]);
    if (words.size() >= 2) writeU32(chain, 12, words[1]);
    chain.resize(16 + payloadQwc * 16, 0);
    for (std::size_t i = 2; i < words.size(); ++i) {
        writeU32(chain, 16 + (i - 2) * 4, words[i]);
    }
    Bytes ret(16, 0); // RET, hi = two NOP words
    writeU64(ret, 0, 6ull << 28);
    chain.insert(chain.end(), ret.begin(), ret.end());
    return chain;
}

// ---------------------------------------------------------------------------
// GIF upload-packet crafting (image chains; pixels come from the real file)
// ---------------------------------------------------------------------------
struct AdPair {
    std::uint64_t value;
    std::uint64_t addr;
};

std::uint64_t blitbufValue(std::uint32_t dbp, std::uint32_t dbw, std::uint32_t psm) {
    return (static_cast<std::uint64_t>(psm) << 56) | (static_cast<std::uint64_t>(dbw) << 48) |
           (static_cast<std::uint64_t>(dbp) << 32);
}

std::uint64_t trxposValue(std::uint32_t dx, std::uint32_t dy) {
    return (static_cast<std::uint64_t>(dx) << 32) | (static_cast<std::uint64_t>(dy) << 48);
}

std::uint64_t trxregValue(std::uint32_t w, std::uint32_t h) {
    return static_cast<std::uint64_t>(w) | (static_cast<std::uint64_t>(h) << 32);
}

std::uint64_t texStateValue(std::uint32_t tbp, std::uint32_t tbw, std::uint32_t psm,
                            std::uint32_t cbp, std::uint32_t cpsm, std::uint32_t csm,
                            std::uint32_t csa) {
    return static_cast<std::uint64_t>(tbp) | (static_cast<std::uint64_t>(tbw) << 14) |
           (static_cast<std::uint64_t>(psm) << 20) | (static_cast<std::uint64_t>(cbp) << 37) |
           (static_cast<std::uint64_t>(cpsm) << 51) | (static_cast<std::uint64_t>(csm) << 55) |
           (static_cast<std::uint64_t>(csa) << 56);
}

// One GIF A+D packet: 16-byte header (tag qword + regs qword 0xE), then the
// (value, address) pairs, exactly the wire layout parseUploadPayload reads.
Bytes adPacket(std::span<const AdPair> pairs) {
    Bytes packet(16, 0);
    writeU64(packet, 0, static_cast<std::uint64_t>(pairs.size()) | (1ull << 60));
    writeU64(packet, 8, 0xE);
    for (std::size_t i = 0; i < pairs.size(); ++i) {
        const std::size_t at = 16 + 16 * i;
        packet.resize(at + 16, 0);
        writeU64(packet, at, pairs[i].value);
        writeU64(packet, at + 8, pairs[i].addr);
    }
    return packet;
}

// One IMAGE packet: 16-byte header (flg 2, nloop = chunk/16), then the chunk.
Bytes imagePacket(std::span<const std::uint8_t> chunk) {
    Bytes packet(16, 0);
    writeU64(packet, 0, (2ull << 58) | static_cast<std::uint64_t>(chunk.size() / 16));
    packet.insert(packet.end(), chunk.begin(), chunk.end());
    return packet;
}

Bytes cntTag(std::uint32_t qwc) {
    Bytes tag(16, 0);
    tag[3] = kDmaTagCnt;
    tag[0] = static_cast<std::uint8_t>(qwc);
    tag[1] = static_cast<std::uint8_t>(qwc >> 8);
    return tag;
}

Bytes endTag() {
    Bytes tag(16, 0);
    tag[3] = kDmaTagEnd;
    return tag;
}

Bytes buildUploadChain(std::span<const AdPair> targetPairs,
                       std::span<const std::uint8_t> targetPixels,
                       std::span<const AdPair> palettePairs,
                       std::span<const std::uint8_t> palettePixels) {
    Bytes payload1 = adPacket(targetPairs);
    const Bytes img1 = imagePacket(targetPixels);
    payload1.insert(payload1.end(), img1.begin(), img1.end());
    Bytes payload2 = adPacket(palettePairs);
    const Bytes img2 = imagePacket(palettePixels);
    payload2.insert(payload2.end(), img2.begin(), img2.end());

    Bytes chain = cntTag(static_cast<std::uint32_t>(payload1.size() / 16));
    chain.insert(chain.end(), payload1.begin(), payload1.end());
    const Bytes tag2 = cntTag(static_cast<std::uint32_t>(payload2.size() / 16));
    chain.insert(chain.end(), tag2.begin(), tag2.end());
    chain.insert(chain.end(), payload2.begin(), payload2.end());
    const Bytes end = endTag();
    chain.insert(chain.end(), end.begin(), end.end());
    return chain;
}

// ---------------------------------------------------------------------------
// Container wire crafting
// ---------------------------------------------------------------------------
void appendU32(Bytes& b, std::uint32_t v) {
    b.push_back(static_cast<std::uint8_t>(v));
    b.push_back(static_cast<std::uint8_t>(v >> 8));
    b.push_back(static_cast<std::uint8_t>(v >> 16));
    b.push_back(static_cast<std::uint8_t>(v >> 24));
}

void appendU64(Bytes& b, std::uint64_t v) {
    for (int i = 0; i < 8; ++i) {
        b.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
    }
}

// A minimal outer container wire holding exactly these payloads.
Bytes makeWire(const std::vector<Bytes>& imagePayloads,
               const std::vector<Bytes>& meshPayloads) {
    Bytes wire;
    wire.push_back('R');
    wire.push_back('T');
    wire.push_back('A');
    wire.push_back('C');
    appendU32(wire, kContainerVersion);
    appendU32(wire, static_cast<std::uint32_t>(imagePayloads.size()));
    appendU32(wire, static_cast<std::uint32_t>(meshPayloads.size()));
    for (const Bytes& p : imagePayloads) {
        appendU64(wire, p.size());
        wire.insert(wire.end(), p.begin(), p.end());
    }
    for (const Bytes& p : meshPayloads) {
        appendU64(wire, p.size());
        wire.insert(wire.end(), p.begin(), p.end());
    }
    return wire;
}

// The 16-byte header alone, with chosen counts (used for count-driven paths).
Bytes makeHeaderWire(std::uint32_t imageCount, std::uint32_t meshCount) {
    Bytes wire;
    wire.push_back('R');
    wire.push_back('T');
    wire.push_back('A');
    wire.push_back('C');
    appendU32(wire, kContainerVersion);
    appendU32(wire, imageCount);
    appendU32(wire, meshCount);
    return wire;
}

// Parsed layout of one serialized image payload (encodeImagePayload order).
struct ImagePayloadLayout {
    std::size_t vlenOff = 0;  // u64 variant-string length
    std::size_t variantOff = 0;
    std::size_t plenOff = 0;  // u64 pixel-blob length
    std::size_t pixelsOff = 0;
    std::size_t provOff = 0;
    std::size_t slenOff = 0; // provenance: u64 source-file length
    std::size_t srcOff = 0;
    std::size_t offOff = 0;  // provenance: u64 offset
    std::size_t lenOff = 0;  // provenance: u64 length
    std::size_t digestOff = 0;
    std::size_t total = 0;
};

ImagePayloadLayout parseImagePayload(const Bytes& p) {
    ImagePayloadLayout l;
    l.vlenOff = 13; // 3*u32 + format u8
    l.variantOff = l.vlenOff + 8;
    const std::size_t vlen = static_cast<std::size_t>(readU64(p, l.vlenOff));
    l.plenOff = l.variantOff + vlen;
    l.pixelsOff = l.plenOff + 8;
    const std::size_t plen = static_cast<std::size_t>(readU64(p, l.plenOff));
    l.provOff = l.pixelsOff + plen;
    l.slenOff = l.provOff;
    l.srcOff = l.slenOff + 8;
    const std::size_t slen = static_cast<std::size_t>(readU64(p, l.slenOff));
    l.offOff = l.srcOff + slen;
    l.lenOff = l.offOff + 8;
    l.digestOff = l.lenOff + 8;
    l.total = l.digestOff + 32;
    return l;
}

// Parsed layout of one serialized mesh payload (encodeMeshPayload order).
struct MeshPayloadLayout {
    std::size_t vcountOff = 0;
    std::size_t icountOff = 0;
    std::size_t mcountOff = 0;
    std::size_t matsOff = 0;
    std::size_t provOff = 0;
    std::size_t total = 0;
};

MeshPayloadLayout parseMeshPayload(const Bytes& p) {
    MeshPayloadLayout l;
    l.vcountOff = 0;
    const std::size_t vcount = static_cast<std::size_t>(readU64(p, l.vcountOff));
    std::size_t o = 8 + 36 * vcount;
    l.icountOff = o;
    const std::size_t icount = static_cast<std::size_t>(readU64(p, l.icountOff));
    o = l.icountOff + 8 + 4 * icount;
    l.mcountOff = o;
    const std::size_t mcount = static_cast<std::size_t>(readU64(p, l.mcountOff));
    l.matsOff = l.mcountOff + 8;
    o = l.mcountOff + 8 + 20 * mcount;
    l.provOff = o;
    // provenance: u64 source-file length + bytes + u64 offset + u64 length + digest
    l.total = o + 8 + readU64(p, o) + 8 + 8 + 32;
    return l;
}

// Extracts payload span [start, start+len) of record index rec (0-based,
// images first then meshes) from a serialized container wire.
bool extractPayload(const Bytes& wire, bool image, std::size_t rec, Bytes& out) {
    if (wire.size() < 16) return false;
    const std::uint32_t imageCount = readU32(wire, 8);
    std::size_t o = 16;
    const std::size_t skip = image ? 0 : imageCount;
    for (std::size_t i = 0; i < skip + rec + 1 && o + 8 <= wire.size(); ++i) {
        const std::size_t len = static_cast<std::size_t>(readU64(wire, o));
        o += 8;
        if (o + len > wire.size()) return false;
        if (i == skip + rec) {
            out.assign(wire.begin() + static_cast<std::ptrdiff_t>(o),
                       wire.begin() + static_cast<std::ptrdiff_t>(o + len));
            return true;
        }
        o += len;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Section 1/2: VIF/DMA walk and command-skip edges over the real main chain
// ---------------------------------------------------------------------------
void vifWalkEdges(const Bytes& file, const std::uint32_t (&word)[8]) {
    const std::size_t mainStart = static_cast<std::size_t>(word[0]) + 0x10;
    const std::size_t regionEnd = static_cast<std::size_t>(word[1]);
    const Bytes mainChain(file.begin() + static_cast<std::ptrdiff_t>(mainStart),
                          file.begin() + static_cast<std::ptrdiff_t>(regionEnd));

    // Baseline: the real CNT..RET chain decodes with real batches.
    const VifChainResult baseline = decodeVifChain(mainChain);
    check(baseline.error == VifChainError::Code::Ok, "baseline main chain decodes Ok");
    check(!baseline.batches.empty(), "baseline main chain yields real batches");

    // Tag layout of the real chain: CNT(qwc) .. RET, both 16-byte tags.
    const std::uint32_t qwc = static_cast<std::uint32_t>(readU64(mainChain, 0) & 0xFFFF);
    const std::size_t payloadLen = static_cast<std::size_t>(qwc) * 16;
    const std::size_t retOff = 16 + payloadLen;
    check(retOff + 16 <= mainChain.size() && (mainChain[retOff + 3] >> 4) == 6,
          "real main chain ends with a RET tag");

    // The three byte groups the CNT walk contributes to the stream.
    const Bytes cntHi(mainChain.begin() + 8, mainChain.begin() + 16);
    const Bytes payload(mainChain.begin() + 16,
                       mainChain.begin() + static_cast<std::ptrdiff_t>(retOff));
    const Bytes retHi(mainChain.begin() + static_cast<std::ptrdiff_t>(retOff) + 8,
                      mainChain.begin() + static_cast<std::ptrdiff_t>(retOff) + 16);

    // REF: payload read from the target address, walk continues at o+16.
    // [REF tag][RET tag][payload] reproduces the CNT stream byte-for-byte.
    {
        Bytes tag = mainChain;
        writeU64(tag, 0, static_cast<std::uint64_t>(qwc) | (3ull << 28) | (32ull << 32));
        const Bytes tagBytes(tag.begin(), tag.begin() + 16);
        const Bytes retTag(mainChain.begin() + static_cast<std::ptrdiff_t>(retOff),
                           mainChain.begin() + static_cast<std::ptrdiff_t>(retOff) + 16);
        const Bytes chain = concat({&tagBytes, &retTag, &payload});
        const VifChainResult r = decodeVifChain(chain);
        check(r.error == VifChainError::Code::Ok, "REF-rewired chain walks Ok");
        check(sameVifResult(r, baseline), "REF-rewired chain decodes identically");
    }
    // REFS: same tag shape, id 4.
    {
        Bytes tag = mainChain;
        writeU64(tag, 0, static_cast<std::uint64_t>(qwc) | (4ull << 28) | (32ull << 32));
        const Bytes tagBytes(tag.begin(), tag.begin() + 16);
        const Bytes retTag(mainChain.begin() + static_cast<std::ptrdiff_t>(retOff),
                           mainChain.begin() + static_cast<std::ptrdiff_t>(retOff) + 16);
        const Bytes chain = concat({&tagBytes, &retTag, &payload});
        const VifChainResult r = decodeVifChain(chain);
        check(r.error == VifChainError::Code::Ok, "REFS-rewired chain walks Ok");
        check(sameVifResult(r, baseline), "REFS-rewired chain decodes identically");
    }
    // NEXT: payload inline, jump to the RET's address.
    {
        Bytes chain = mainChain;
        writeU64(chain, 0,
                 static_cast<std::uint64_t>(qwc) | (2ull << 28) |
                     (static_cast<std::uint64_t>(retOff) << 32));
        const VifChainResult r = decodeVifChain(chain);
        check(r.error == VifChainError::Code::Ok, "NEXT-rewired chain walks Ok");
        check(sameVifResult(r, baseline), "NEXT-rewired chain decodes identically");
    }
    // REFE: payload from the target, walk terminates. The appended region is
    // payload + the RET's own hi bytes, so the stream equals the original
    // plus two trailing NOP words.
    {
        Bytes tag = mainChain;
        writeU64(tag, 0,
                 static_cast<std::uint64_t>(qwc + 1) | (0ull << 28) | (16ull << 32));
        const Bytes tagBytes(tag.begin(), tag.begin() + 16);
        const Bytes filler(8, 0);
        const Bytes chain = concat({&tagBytes, &payload, &retHi, &filler});
        const VifChainResult r = decodeVifChain(chain);
        check(r.error == VifChainError::Code::Ok, "REFE-terminated chain walks Ok");
        check(sameVifResult(r, baseline), "REFE-terminated chain decodes identically");
    }

    // Crafted 16-byte tags: payload beyond the span, per tag family.
    {
        const Bytes cnt = makeWalkTag(1, 0xFFFF, 0);
        check(decodeVifChain(cnt).error == VifChainError::Code::Truncated,
              "CNT tag with a payload past the span rejects as Truncated");
    }
    {
        const Bytes ref = makeWalkTag(3, 1, 16); // target == span size
        check(decodeVifChain(ref).error == VifChainError::Code::Truncated,
              "REF tag with an out-of-range target rejects as Truncated");
    }
    {
        const Bytes refe = makeWalkTag(0, 0xFFFF, 0);
        check(decodeVifChain(refe).error == VifChainError::Code::Truncated,
              "REFE tag with a payload past the span rejects as Truncated");
    }
    {
        const Bytes next = makeWalkTag(2, 0xFFFF, 0);
        check(decodeVifChain(next).error == VifChainError::Code::Truncated,
              "NEXT tag with a payload past the span rejects as Truncated");
    }
    {
        const Bytes call = makeWalkTag(5, 0xFFFF, 0);
        check(decodeVifChain(call).error == VifChainError::Code::Truncated,
              "CALL tag with a payload past the span rejects as Truncated");
    }
    {
        const Bytes ret = makeWalkTag(6, 0xFFFF, 0);
        check(decodeVifChain(ret).error == VifChainError::Code::Truncated,
              "RET tag with a payload past the span rejects as Truncated");
    }
    {
        const Bytes end = makeWalkTag(7, 0xFFFF, 0);
        check(decodeVifChain(end).error == VifChainError::Code::Truncated,
              "END tag with a payload past the span rejects as Truncated");
    }

    // CALL -> RET with a non-empty return stack: RET pops back to itself,
    // then terminates with an empty stack. Stream is NOP words only.
    {
        const Bytes callTag = makeWalkTag(5, 0, 16); // CALL: jump to the RET at 16
        const Bytes retTag = makeWalkTag(6, 0, 0);
        const Bytes chain = concat({&callTag, &retTag});
        const VifChainResult r = decodeVifChain(chain);
        check(r.error == VifChainError::Code::Ok, "CALL->RET chain walks Ok");
        check(r.batches.empty(), "CALL->RET chain decodes no batches (NOP stream)");
    }

    // Walk cap: 4097 CNT tags with no terminator exceeds the 4096-tag cap.
    {
        Bytes chain;
        chain.reserve(4097 * 16);
        for (int i = 0; i < 4097; ++i) {
            const Bytes tag = makeWalkTag(1, 0, 0);
            chain.insert(chain.end(), tag.begin(), tag.end());
        }
        check(decodeVifChain(chain).error == VifChainError::Code::Overflow,
              "4097 unterminated CNT tags reject as Overflow");
    }

    // VIF command skips: every host->local bookkeeping command the decoder
    // must step over, each with its skip data present in the stream.
    {
        const std::uint32_t nop = 0x00000000;
        const std::array<std::uint32_t, 25> words = {
            0x20000000, nop,             // STMASK + 4 mask bytes
            0x30000000, nop, nop, nop, nop, // STROW + 16 bytes
            0x31000000, nop, nop, nop, nop, // STCOL + 16 bytes
            0x4A010000, nop, nop,         // MPG(num=1) + 8 bytes
            0x50000001, nop, nop, nop, nop, // DIRECT(imm=1) + 16 bytes
            0x51000001, nop, nop, nop, nop, // DIRECTHL(imm=1) + 16 bytes
        };
        const Bytes chain = makeCommandChain(words);
        const VifChainResult r = decodeVifChain(chain);
        check(r.error == VifChainError::Code::Ok, "command-skip chain decodes Ok");
        check(r.batches.empty(), "command-skip chain yields no batches");
    }
    // Truncated variants: the command word is the last stream word, so its
    // skip would run past the end.
    {
        check(decodeVifChain(makeTruncCommandChain(0x20000000)).error ==
                  VifChainError::Code::Truncated,
              "STMASK at the stream end rejects as Truncated");
        check(decodeVifChain(makeTruncCommandChain(0x30000000)).error ==
                  VifChainError::Code::Truncated,
              "STROW at the stream end rejects as Truncated");
        check(decodeVifChain(makeTruncCommandChain(0x31000000)).error ==
                  VifChainError::Code::Truncated,
              "STCOL at the stream end rejects as Truncated");
        check(decodeVifChain(makeTruncCommandChain(0x4A010000)).error ==
                  VifChainError::Code::Truncated,
              "MPG at the stream end rejects as Truncated");
        check(decodeVifChain(makeTruncCommandChain(0x50000001)).error ==
                  VifChainError::Code::Truncated,
              "DIRECT at the stream end rejects as Truncated");
    }
}

// ---------------------------------------------------------------------------
// Section 3: CAR header / chain rejection paths (convertCarModels)
// ---------------------------------------------------------------------------
void modelConverterEdges(const Bytes& realFile, const std::string& path, const Digest& digest) {
    const std::size_t mainStart = static_cast<std::size_t>(
        readU32(realFile, 0) + 0x10);

    auto convertPatched = [&](const Bytes& bytes) {
        return convertCarModels(bytes, path, digest);
    };
    auto patchWord = [&](std::size_t wordIndex, std::uint32_t value) {
        Bytes copy = realFile;
        writeU32(copy, wordIndex * 4, value);
        return convertPatched(copy);
    };

    // Baseline sanity: the real file converts.
    check(convertPatched(realFile).err == CarModelError::Ok,
          "baseline real file converts Ok");

    // Region-length word above the loader's 0x34000 bound.
    {
        const CarModelResult r = patchWord(6, 0x34001);
        check(r.err == CarModelError::BadHeader, "word +0x18 above 0x34000 rejects BadHeader");
        check(r.errorOffset == 0x18, "region-bound error is attributed to word +0x18");
    }
    // First model start inside the eight header words.
    {
        const CarModelResult r = patchWord(0, 0x10);
        check(r.err == CarModelError::BadHeader, "word +0x00 below 0x20 rejects BadHeader");
        check(r.errorOffset == 0x00, "first-start error is attributed to word +0x00");
    }
    // Empty model-0 region (word[1] == word[0]) cannot hold the model header.
    {
        const CarModelResult r = patchWord(1, readU32(realFile, 0));
        check(r.err == CarModelError::Truncated,
              "empty model region rejects Truncated");
        check(r.errorOffset == readU32(realFile, 0),
              "empty-region error is attributed to the model start");
    }
    // Model-0 region that fits the header but no whole main-chain tag.
    {
        const CarModelResult r = patchWord(1, readU32(realFile, 0) + 0x18);
        check(r.err == CarModelError::Truncated,
              "model region too short for the main chain rejects Truncated");
        check(r.errorOffset == mainStart,
              "short-main-chain error is attributed to the main chain start");
    }
    // Broken main-chain tag: CNT whose payload runs past the region.
    {
        Bytes copy = realFile;
        writeU64(copy, mainStart, 0xFFFF | (1ull << 28));
        const CarModelResult r = convertPatched(copy);
        check(r.err == CarModelError::ChainDecode,
              "main chain with a truncated CNT payload rejects ChainDecode");
        check(r.errorOffset == mainStart,
              "main-chain decode error is attributed to the main chain start");
    }
    // Non-finite vertex: scan the real main-chain payload for UNPACK V3-32
    // code words (cmd 0x68) and patch the first following word to a quiet
    // NaN; the first patch that turns the conversion into BadBatch proves
    // the float path over real geometry. Deterministic over the real bytes.
    {
        const std::size_t payloadStart = mainStart + 0x10;
        const std::size_t payloadEnd =
            payloadStart + static_cast<std::size_t>(readU64(realFile, mainStart) & 0xFFFF) * 16;
        bool found = false;
        std::uint64_t foundOffset = 0;
        for (std::size_t o = payloadStart; o + 8 <= payloadEnd && !found; o += 4) {
            if (((readU32(realFile, o) >> 24) & 0x7Fu) != 0x68) {
                continue; // not an UNPACK V3-32 code word
            }
            Bytes copy = realFile;
            writeU32(copy, o + 4, 0x7FC00000u); // quiet NaN as a vertex float
            const CarModelResult r = convertPatched(copy);
            if (r.err == CarModelError::BadBatch) {
                found = true;
                foundOffset = o + 4;
            }
        }
        check(found, "a NaN vertex word in the real chain rejects BadBatch");
        if (found) {
            Bytes copy = realFile;
            writeU32(copy, static_cast<std::size_t>(foundOffset), 0x7FC00000u);
            const CarModelResult r = convertPatched(copy);
            check(r.errorOffset == mainStart,
                  "BadBatch is attributed to the main chain start");
        }
    }
    // Part-chain start inside the 16-byte model header (count gate open).
    {
        Bytes copy = realFile;
        const std::uint32_t model0 = readU32(realFile, 0);
        const std::uint32_t cntA = readU32(realFile, static_cast<std::size_t>(model0) + 8);
        check(cntA != 0, "the real model-0 partA count gate is open");
        writeU32(copy, static_cast<std::size_t>(model0), 0); // partA offset 0
        const CarModelResult r = convertPatched(copy);
        check(r.err == CarModelError::BadHeader,
              "part chain start inside the model header rejects BadHeader");
        check(r.errorOffset == model0,
              "part-start error is attributed to the part-offset word");
    }
    // Broken part-chain start: overwrite the real partA chain's first tag
    // (bytes the main chain never reads) with a CNT whose payload escapes.
    {
        const std::uint32_t model0 = readU32(realFile, 0);
        const std::uint32_t partAOff = readU32(realFile, static_cast<std::size_t>(model0));
        const std::size_t partAStart =
            static_cast<std::size_t>(model0) + (partAOff & ~0xFu);
        const std::size_t regionEnd0 = static_cast<std::size_t>(readU32(realFile, 4));
        check(partAOff != 0 && partAStart + 16 <= regionEnd0,
              "the real partA chain start is inside the model region");
        Bytes copy = realFile;
        const Bytes broken = makeWalkTag(1, 0xFFFF, 0);
        std::copy(broken.begin(), broken.end(),
                  copy.begin() + static_cast<std::ptrdiff_t>(partAStart));
        const CarModelResult r = convertPatched(copy);
        check(r.err == CarModelError::ChainDecode,
              "part chain with a truncated CNT payload rejects ChainDecode");
        check(r.errorOffset == partAStart,
              "part-chain decode error is attributed to the part chain start");
    }
    // Texture-chain start (header word +0x18) whose first tag is not CNT:
    // the loader's own walk fatals on any other identity byte.
    {
        Bytes copy = realFile;
        const std::size_t chainStart = static_cast<std::size_t>(readU32(realFile, 0x18));
        check(chainStart + 16 <= copy.size(),
              "the real texture chain start holds a whole tag");
        copy[chainStart + 3] = 0x00; // neither the CNT nor the END identity byte
        const CarModelResult r = convertPatched(copy);
        check(r.err == CarModelError::BadHeader,
              "texture chain start without a CNT tag rejects BadHeader");
        check(r.errorOffset == chainStart,
              "texture-chain error is attributed to the chain start");
    }
}

// ---------------------------------------------------------------------------
// Section 4: texture chain variants (convertImage / convertImageBank / Count)
// ---------------------------------------------------------------------------
struct ChainLayout {
    std::size_t payload1 = 0; // chain-relative
    std::size_t payload1Len = 0;
    std::size_t payload2 = 0;
    std::size_t blit1 = 0; // BLITBUF value inside payload 1
    std::size_t trxreg1 = 0;
    std::size_t trxdirAddr1 = 0;
    std::size_t imageTag1 = 0;
    std::size_t chunk1 = 0;
    std::size_t blit2 = 0;
    std::size_t trxreg2 = 0;
    std::size_t chunk2 = 0;
};

bool parseChainLayout(const Bytes& chain, ChainLayout& l) {
    if (chain.size() < 0x20 || chain[3] != kDmaTagCnt) return false;
    const std::size_t qwc1 = static_cast<std::size_t>(chain[0]) |
                             (static_cast<std::size_t>(chain[1]) << 8);
    l.payload1 = 0x10;
    l.payload1Len = qwc1 * 16;
    const std::size_t cnt2 = l.payload1 + l.payload1Len;
    if (cnt2 + 0x20 > chain.size() || chain[cnt2 + 3] != kDmaTagCnt) return false;
    l.payload2 = cnt2 + 0x10;
    const std::size_t nloop1 = static_cast<std::size_t>(readU64(chain, l.payload1) & 0x7FFF);
    if (nloop1 != 4) return false;
    l.blit1 = l.payload1 + 0x10;
    l.trxreg1 = l.payload1 + 0x30;
    l.trxdirAddr1 = l.payload1 + 0x48;
    l.imageTag1 = l.payload1 + 16 * (nloop1 + 1);
    l.chunk1 = l.imageTag1 + 0x10;
    const std::size_t nloop2 = static_cast<std::size_t>(readU64(chain, l.payload2) & 0x7FFF);
    if (nloop2 != 4) return false;
    l.blit2 = l.payload2 + 0x10;
    l.trxreg2 = l.payload2 + 0x30;
    const std::size_t imageTag2 = l.payload2 + 16 * (nloop2 + 1);
    l.chunk2 = imageTag2 + 0x10;
    return true;
}

void imageConverterEdges(const Bytes& realChain, const Provenance& prov,
                         const std::string& variantId) {
    ChainLayout lay;
    if (!parseChainLayout(realChain, lay)) {
        check(false, "real texture chain layout parses (two CNT A+D+IMAGE payloads)");
        return;
    }
    check(true, "real texture chain layout parses (two CNT A+D+IMAGE payloads)");

    const std::string tag = "texture chain patch: ";
    auto patched = [&](auto&& mutator) {
        Bytes chain = realChain;
        mutator(chain);
        return chain;
    };
    auto record = [&](const Bytes& chain) { return convertImage(chain, prov, variantId); };
    auto count = [&](const Bytes& chain) { return convertImageBankCount(chain); };
    auto emptyRecord = [](const ImageRecord& r) {
        return r.width == 0 && r.height == 0 && r.mipCount == 0 && r.pixels.empty();
    };

    // Baseline: the real chain decodes to the known 128x128 T8 texture.
    const ImageRecord base = record(realChain);
    check(base.width == 128 && base.height == 128 && base.pixels.size() == 65536,
          "baseline real texture chain decodes 128x128");
    check(count(realChain) == 1, "baseline real texture chain exposes one bank");

    // Direct-colour targets: repatch the target's storage mode and shrink the
    // transfer to 64x64 so the real IMAGE chunk still carries all pixels.
    {
        const struct {
            const char* name;
            std::uint32_t psm;
        } modes[] = {{"CT32", 0x00}, {"CT24", 0x01}, {"CT16", 0x02}, {"CT16S", 0x0A}};
        for (const auto& m : modes) {
            const Bytes chain = patched([&](Bytes& c) {
                patchU64Field(c, lay.blit1, 56, 6, m.psm);
                patchU64Field(c, lay.trxreg1, 0, 12, 64);
                patchU64Field(c, lay.trxreg1, 32, 12, 64);
            });
            const ImageRecord r = record(chain);
            const std::string what = std::string(m.name) + " target decodes 64x64 RGBA8";
            check(r.width == 64 && r.height == 64 && r.mipCount == 1 &&
                      r.format == PixelFormat::R8G8B8A8_UNORM &&
                      r.pixels.size() == 64 * 64 * 4 &&
                      r.provenance == prov && r.sourceVariantId == variantId,
                  tag + what);
            check(count(chain) == 1, tag + std::string(m.name) + " target exposes one bank");
        }
    }
    // Palette geometry that makes the CLUT read-back walk past the uploaded
    // palette: 16x2 (rows beyond the upload) and 8x8 (columns beyond it).
    {
        const Bytes chain = patched([&](Bytes& c) {
            patchU64Field(c, lay.trxreg2, 32, 12, 2); // 16x2
        });
        const ImageRecord r = record(chain);
        check(r.width == 128 && r.height == 128 && r.pixels.size() == 65536,
              tag + "16x2 palette still decodes a full texture");
    }
    {
        const Bytes chain = patched([&](Bytes& c) {
            patchU64Field(c, lay.trxreg2, 0, 12, 8);
            patchU64Field(c, lay.trxreg2, 32, 12, 8); // 8x8
        });
        const ImageRecord r = record(chain);
        check(r.width == 128 && r.height == 128 && r.pixels.size() == 65536,
              tag + "8x8 palette still decodes a full texture");
    }
    // Palette uploaded with dbw != 1: the linear CLUT model refuses.
    {
        const Bytes chain = patched([&](Bytes& c) {
            patchU64Field(c, lay.blit2, 48, 6, 2);
        });
        check(emptyRecord(record(chain)), tag + "palette with dbw 2 rejects empty");
        check(count(chain) == 1, tag + "dbw-2 chain still exposes its bank");
    }
    // 16-bit palette: the CLUT loads as single half-words.
    {
        const Bytes chain = patched([&](Bytes& c) {
            patchU64Field(c, lay.blit2, 56, 6, 0x02); // CT16
        });
        const ImageRecord r = record(chain);
        check(r.width == 128 && r.height == 128 && r.pixels.size() == 65536,
              tag + "CT16 palette still decodes a full texture");
    }
    // A+D packet whose register data runs past the CNT payload.
    {
        const Bytes chain = patched([&](Bytes& c) {
            patchU64Field(c, lay.payload1, 0, 15, 1030); // nloop
        });
        check(emptyRecord(record(chain)), tag + "oversized A+D packet rejects empty");
        check(count(chain) == 0, tag + "oversized A+D packet yields no banks");
    }
    // A+D packet whose data runs past a shrunken CNT payload (qwc cut to 4:
    // a 64-byte payload cannot hold the 80-byte 4-pair packet).
    {
        const Bytes chain = patched([&](Bytes& c) {
            c[0] = 4; // first CNT tag qwc -> 64-byte payload
        });
        check(emptyRecord(record(chain)), tag + "A+D packet past the CNT rejects empty");
        check(count(chain) == 0, tag + "A+D packet past the CNT yields no banks");
    }
    // TRXDIR write redirected to an unknown register: the packet stops
    // describing an upload and the following IMAGE tag is then malformed.
    {
        const Bytes chain = patched([&](Bytes& c) {
            c[lay.trxdirAddr1] = 0x08;
        });
        check(emptyRecord(record(chain)), tag + "unknown register rejects empty");
        check(count(chain) == 0, tag + "unknown register yields no banks");
    }
    // IMAGE packet with the wrong flag and with an oversized chunk.
    {
        const Bytes chain = patched([&](Bytes& c) {
            patchU64Field(c, lay.imageTag1, 58, 2, 0);
        });
        check(emptyRecord(record(chain)), tag + "IMAGE tag with flg 0 rejects empty");
        check(count(chain) == 0, tag + "IMAGE tag with flg 0 yields no banks");
    }
    {
        const Bytes chain = patched([&](Bytes& c) {
            patchU64Field(c, lay.imageTag1, 0, 15, 1030); // nloop -> chunk past payload
        });
        check(emptyRecord(record(chain)), tag + "IMAGE chunk past the CNT rejects empty");
        check(count(chain) == 0, tag + "IMAGE chunk past the CNT yields no banks");
    }

    // Re-assembled chains replaying the real pixels under texture-state
    // register choices (no TEX writes exist in the real Q00 chain).
    const Bytes texPixels(realChain.begin() + static_cast<std::ptrdiff_t>(lay.chunk1),
                          realChain.begin() + static_cast<std::ptrdiff_t>(lay.chunk1) + 16384);
    const Bytes palPixels(realChain.begin() + static_cast<std::ptrdiff_t>(lay.chunk2),
                          realChain.begin() + static_cast<std::ptrdiff_t>(lay.chunk2) + 1024);
    const Bytes pal24(palPixels.begin(), palPixels.begin() + 768);

    const std::array<AdPair, 4> targetUpload = {
        AdPair{blitbufValue(0x100, 2, 0x13), 0x50}, // T8 128x128
        AdPair{trxposValue(0, 0), 0x51},
        AdPair{trxregValue(128, 128), 0x52},
        AdPair{0, 0x53}, // TRXDIR 0
    };
    const std::array<AdPair, 4> paletteUpload = {
        AdPair{blitbufValue(0x200, 1, 0x00), 0x50}, // CT32 16x16
        AdPair{trxposValue(0, 0), 0x51},
        AdPair{trxregValue(16, 16), 0x52},
        AdPair{0, 0x53},
    };
    {
        // Two texture-state selections (csa 0 and 1): sort/unique over two
        // indexed states, two decodable banks, third bank rejected.
        const std::array<AdPair, 6> target6 = {
            targetUpload[0], targetUpload[1], targetUpload[2], targetUpload[3],
            AdPair{texStateValue(0x100, 2, 0x13, 0x200, 0x00, 0, 0), 0x06},
            AdPair{texStateValue(0x100, 2, 0x13, 0x200, 0x00, 0, 1), 0x07},
        };
        const Bytes chain = buildUploadChain(target6, texPixels, paletteUpload, palPixels);
        check(count(chain) == 2, "two texture-state selections expose two banks");
        const ImageRecord b0 = convertImageBank(chain, prov, variantId, 0);
        const ImageRecord b1 = convertImageBank(chain, prov, variantId, 1);
        const ImageRecord b2 = convertImageBank(chain, prov, variantId, 2);
        check(b0.width == 128 && b0.height == 128 && b0.pixels.size() == 65536,
              "texture-state bank 0 decodes the real texture");
        check(b1.width == 128 && b1.height == 128 && b1.pixels.size() == 65536,
              "texture-state bank 1 decodes the real texture");
        check(emptyRecord(b2), "bank beyond the selection count rejects empty");
        check(convertImage(chain, prov, variantId) == b0,
              "convertImage equals convertImageBank(0) for the replayed chain");
    }
    {
        // A texture state whose CLUT base matches no upload: rejected empty.
        const std::array<AdPair, 5> target5 = {
            targetUpload[0], targetUpload[1], targetUpload[2], targetUpload[3],
            AdPair{texStateValue(0x100, 2, 0x13, 0x201, 0x00, 0, 0), 0x06},
        };
        const Bytes chain = buildUploadChain(target5, texPixels, paletteUpload, palPixels);
        check(count(chain) == 1, "mismatched CLUT base still exposes its bank");
        check(emptyRecord(record(chain)),
              "texture state with an unmatched CLUT base rejects empty");
    }
    {
        // A 24-bit palette upload chosen by a texture state: the CLUT loads
        // as split half-words and recombines through rgb24.
        const std::array<AdPair, 5> target5 = {
            targetUpload[0], targetUpload[1], targetUpload[2], targetUpload[3],
            AdPair{texStateValue(0x100, 2, 0x13, 0x210, 0x01, 0, 0), 0x06},
        };
        const std::array<AdPair, 4> pal24Upload = {
            AdPair{blitbufValue(0x210, 1, 0x01), 0x50}, // CT24 16x16
            AdPair{trxposValue(0, 0), 0x51},
            AdPair{trxregValue(16, 16), 0x52},
            AdPair{0, 0x53},
        };
        const Bytes chain = buildUploadChain(target5, texPixels, pal24Upload, pal24);
        const ImageRecord r = record(chain);
        check(r.width == 128 && r.height == 128 && r.pixels.size() == 65536,
              "CT24 palette chosen by a texture state decodes the real texture");
    }
}

// ---------------------------------------------------------------------------
// Section 5: serialize() rejections over mutated real records
// ---------------------------------------------------------------------------
void serializeEdges(const ImageRecord& realImage, const MeshRecord& realMesh) {
    const std::string tag = "serialize rejection: ";

    auto serializeThrows = [&](const HostAssetContainer& c, const std::string& what) {
        try {
            (void)serialize(c);
            check(false, tag + what);
        } catch (const std::invalid_argument&) {
            check(true, tag + what);
        }
    };
    auto imageContainer = [&](const ImageRecord& img) {
        HostAssetContainer c;
        c.version = kContainerVersion;
        c.images.push_back(img);
        return c;
    };
    auto meshContainer = [&](const MeshRecord& mesh) {
        HostAssetContainer c;
        c.version = kContainerVersion;
        c.meshes.push_back(mesh);
        return c;
    };

    // Image record rejections (validateImage).
    {
        ImageRecord img = realImage;
        img.provenance.offset = std::numeric_limits<std::uint64_t>::max();
        img.provenance.length = 1;
        serializeThrows(imageContainer(img), "image provenance range overflow");
    }
    {
        ImageRecord img = realImage;
        img.format = static_cast<PixelFormat>(1);
        serializeThrows(imageContainer(img), "image format not RGBA8");
    }
    {
        ImageRecord img = realImage;
        img.width = 0;
        serializeThrows(imageContainer(img), "image width zero");
    }
    {
        ImageRecord img = realImage;
        img.mipCount = 9; // 128x128 holds 8 complete levels
        serializeThrows(imageContainer(img), "image mipCount past the complete chain");
    }
    {
        ImageRecord img = realImage;
        img.width = 0xFFFFFFFF;
        img.height = 0xFFFFFFFF;
        img.mipCount = 1;
        serializeThrows(imageContainer(img), "image mip byte count overflow (0xFFFFFFFF^2)");
    }
    {
        ImageRecord img = realImage;
        img.width = 0x7FFFFFFF;
        img.height = 0x7FFFFFFF;
        img.mipCount = 2;
        serializeThrows(imageContainer(img), "image mip sum overflow (0x7FFFFFFF^2, 2 mips)");
    }
    {
        ImageRecord img = realImage;
        img.pixels.resize(img.pixels.size() - 1);
        serializeThrows(imageContainer(img), "image pixel blob size mismatch");
    }
    // Mesh record rejections (validateMesh).
    {
        MeshRecord mesh = realMesh;
        mesh.provenance.offset = std::numeric_limits<std::uint64_t>::max();
        mesh.provenance.length = 1;
        serializeThrows(meshContainer(mesh), "mesh provenance range overflow");
    }
    {
        MeshRecord mesh = realMesh;
        const std::uint32_t nan = 0x7FC00000u;
        std::memcpy(&mesh.vertices[0].px, &nan, sizeof(nan));
        serializeThrows(meshContainer(mesh), "mesh vertex non-finite float");
    }
    {
        MeshRecord mesh = realMesh;
        mesh.indices[0] = static_cast<std::uint32_t>(mesh.vertices.size());
        serializeThrows(meshContainer(mesh), "mesh index out of vertex range");
    }
    {
        MeshRecord mesh = realMesh;
        mesh.materials[0].firstIndex = std::numeric_limits<std::uint64_t>::max();
        mesh.materials[0].indexCount = 1;
        serializeThrows(meshContainer(mesh), "mesh material index range overflow");
    }
    {
        MeshRecord mesh = realMesh;
        mesh.materials[0].firstIndex = mesh.indices.size();
        mesh.materials[0].indexCount = 1;
        serializeThrows(meshContainer(mesh), "mesh material range exceeds the index vector");
    }
    {
        MeshRecord mesh = realMesh;
        mesh.materials[0].imageIndex = 0; // container holds no images
        serializeThrows(meshContainer(mesh), "mesh material image index out of range");
    }
    // Version rejection.
    {
        HostAssetContainer c;
        c.version = kContainerVersion + 1;
        c.meshes.push_back(realMesh);
        serializeThrows(c, "container version not kContainerVersion");
    }
}

// ---------------------------------------------------------------------------
// Section 6: deserialize() error paths over wires derived from the real ones
// ---------------------------------------------------------------------------
void deserializeEdges(const Bytes& carWire, const Bytes& textureWire) {
    const std::string tag = "deserialize rejection: ";

    Bytes imagePayload;
    check(extractPayload(textureWire, true, 0, imagePayload),
          "the real texture wire yields its image payload");
    Bytes meshPayload;
    check(extractPayload(carWire, false, 0, meshPayload),
          "the real car wire yields its first mesh payload");
    if (imagePayload.empty() || meshPayload.empty()) {
        return;
    }

    const ImagePayloadLayout il = parseImagePayload(imagePayload);
    const MeshPayloadLayout ml = parseMeshPayload(meshPayload);
    check(il.total == imagePayload.size(), "image payload layout closes exactly");
    check(ml.total == meshPayload.size(), "mesh payload layout closes exactly");

    auto imageWire = [&](const Bytes& payload) {
        return makeWire(std::vector<Bytes>{payload}, {});
    };
    auto meshWire = [&](const Bytes& payload) {
        return makeWire({}, std::vector<Bytes>{payload});
    };
    auto errOf = [&](const Bytes& wire) { return deserialize(wire).err; };
    auto cutImage = [&](std::size_t len) {
        return Bytes(imagePayload.begin(), imagePayload.begin() + static_cast<std::ptrdiff_t>(len));
    };

    // Reader-level truncations inside the image payload.
    check(errOf(imageWire(cutImage(12))) == ContainerError::Truncated,
          tag + "image payload cut before the format byte is Truncated");
    check(errOf(imageWire(cutImage(3))) == ContainerError::Truncated,
          tag + "image payload cut inside the width word is Truncated");
    // Variant-string length that wraps: OffsetOverflow before Truncated.
    {
        Bytes p = imagePayload;
        writeU64(p, il.vlenOff, std::numeric_limits<std::uint64_t>::max());
        check(errOf(imageWire(p)) == ContainerError::OffsetOverflow,
              tag + "wrapping variant length is OffsetOverflow");
    }
    // Provenance cuts: string length, offset, length, digest.
    check(errOf(imageWire(cutImage(il.provOff + 4))) == ContainerError::Truncated,
          tag + "provenance string length cut is Truncated");
    check(errOf(imageWire(cutImage(il.offOff + 4))) == ContainerError::Truncated,
          tag + "provenance offset cut is Truncated");
    check(errOf(imageWire(cutImage(il.lenOff + 4))) == ContainerError::Truncated,
          tag + "provenance length cut is Truncated");
    check(errOf(imageWire(cutImage(il.digestOff + 20))) == ContainerError::Truncated,
          tag + "provenance digest cut is Truncated");
    // Width/height pair whose mip sum overflows u64.
    {
        Bytes p = imagePayload;
        writeU32(p, 0, 0xFFFFFFFF);
        writeU32(p, 4, 0xFFFFFFFF);
        check(errOf(imageWire(p)) == ContainerError::CountOverflow,
              tag + "0xFFFFFFFF x 0xFFFFFFFF image is CountOverflow");
    }
    // Trailing byte inside the image payload.
    {
        Bytes p = imagePayload;
        p.push_back(0);
        check(errOf(imageWire(p)) == ContainerError::TrailingData,
              tag + "image payload with a trailing byte is TrailingData");
    }
    // Mesh count-driven failures over minimal payloads.
    {
        Bytes p(8, 0);
        writeU64(p, 0, 1); // one vertex, no bytes
        check(errOf(meshWire(p)) == ContainerError::Truncated,
              tag + "mesh vertices without bytes is Truncated");
    }
    {
        Bytes p(16, 0);
        writeU64(p, 8, std::numeric_limits<std::uint64_t>::max()); // index count
        check(errOf(meshWire(p)) == ContainerError::CountOverflow,
              tag + "wrapping index count is CountOverflow");
    }
    {
        Bytes p(16, 0);
        writeU64(p, 8, 100); // indices without bytes
        check(errOf(meshWire(p)) == ContainerError::Truncated,
              tag + "mesh indices without bytes is Truncated");
    }
    {
        Bytes p(24, 0);
        writeU64(p, 16, std::numeric_limits<std::uint64_t>::max()); // material count
        check(errOf(meshWire(p)) == ContainerError::CountOverflow,
              tag + "wrapping material count is CountOverflow");
    }
    {
        Bytes p(24, 0);
        writeU64(p, 16, 100); // materials without bytes
        check(errOf(meshWire(p)) == ContainerError::Truncated,
              tag + "mesh materials without bytes is Truncated");
    }
    // Material range that wraps: IndexOutOfBounds on the real mesh payload.
    {
        Bytes p = meshPayload;
        writeU64(p, ml.matsOff, std::numeric_limits<std::uint64_t>::max());
        check(errOf(meshWire(p)) == ContainerError::IndexOutOfBounds,
              tag + "wrapping material range on the real mesh is IndexOutOfBounds");
    }
    // Trailing byte inside the real mesh payload.
    {
        Bytes p = meshPayload;
        p.push_back(0);
        check(errOf(meshWire(p)) == ContainerError::TrailingData,
              tag + "real mesh payload with a trailing byte is TrailingData");
    }
    // Outer counts without payload bytes.
    {
        const Bytes wire = makeHeaderWire(0x10000, 0);
        check(errOf(wire) == ContainerError::Truncated,
              tag + "image count without image bytes is Truncated");
    }
    {
        const Bytes wire = makeHeaderWire(0, 0x10000);
        check(errOf(wire) == ContainerError::Truncated,
              tag + "mesh count without mesh bytes is Truncated");
    }
    // Mesh blob that cannot be read: length past the end, then wrapping.
    {
        Bytes wire = makeHeaderWire(0, 1);
        appendU64(wire, 5);
        check(errOf(wire) == ContainerError::Truncated,
              tag + "mesh blob length past the wire is Truncated");
    }
    {
        Bytes wire = makeHeaderWire(0, 1);
        appendU64(wire, std::numeric_limits<std::uint64_t>::max());
        check(errOf(wire) == ContainerError::OffsetOverflow,
              tag + "wrapping mesh blob length is OffsetOverflow");
    }
}

// ---------------------------------------------------------------------------
// Section 7: loadImage() mip-overflow rejections over mutated real records
// ---------------------------------------------------------------------------
void loadImageEdges(const ImageRecord& realImage) {
    const std::string tag = "loadImage rejection: ";
    auto throwsOn = [&](const ImageRecord& img, const std::string& what) {
        try {
            (void)loadImage(img);
            check(false, tag + what);
        } catch (const std::invalid_argument&) {
            check(true, tag + what);
        }
    };
    {
        ImageRecord img = realImage;
        img.width = 0xFFFFFFFF;
        img.height = 0xFFFFFFFF;
        img.mipCount = 1;
        throwsOn(img, "0xFFFFFFFF x 0xFFFFFFFF mip size overflows host addressable size");
    }
    {
        ImageRecord img = realImage;
        img.width = 0x7FFFFFFF;
        img.height = 0x7FFFFFFF;
        img.mipCount = 2;
        throwsOn(img, "0x7FFFFFFF x 0x7FFFFFFF mip sum overflows host addressable size");
    }
}

} // namespace

int main() {
    const std::string root = discRoot();
    if (root.empty()) {
        std::cerr << "test_edges_integration: cannot resolve the disc root "
                     "(set RT_DISC; HOME is unset)\n";
        return EXIT_FAILURE;
    }
    const std::filesystem::path carPath = std::filesystem::path(root) / "CAR0" / "Q00.BIN";
    Bytes file;
    if (!readFileBytes(carPath.string(), file)) {
        std::cerr << "test_edges_integration: cannot read '" << carPath.string()
                  << "'; set RT_DISC to the extracted disc root "
                     "(no game data ships with this test)\n";
        return EXIT_FAILURE;
    }
    std::cout << "test_edges_integration: " << carPath.string() << " (" << file.size()
              << " bytes)\n";

    check(file.size() >= 0x20, "car file holds the eight header words");
    if (file.size() < 0x20) {
        return finish();
    }
    std::uint32_t word[8] = {};
    for (std::size_t i = 0; i < 8; ++i) {
        word[i] = readU32(file, i * 4);
    }
    const Digest digest = placeholderDigest(file);
    const std::string path = carPath.string();

    // --- 1./2. VIF/DMA walk and command-skip edges -------------------------
    vifWalkEdges(file, word);

    // --- 3. CAR header / chain rejection paths -----------------------------
    modelConverterEdges(file, path, digest);

    // --- 4. texture chain variants -----------------------------------------
    const std::size_t chainStart = static_cast<std::size_t>(word[6]);
    const Bytes realChain(file.begin() + static_cast<std::ptrdiff_t>(chainStart), file.end());
    Provenance prov;
    prov.sourceFile = path;
    prov.offset = chainStart;
    prov.length = realChain.size();
    prov.sha256 = placeholderDigest(realChain);
    const std::string variantId = "CAR0/Q00.BIN#0";
    imageConverterEdges(realChain, prov, variantId);

    // Real pipeline outputs the later sections mutate.
    const ImageRecord realImage = convertImage(realChain, prov, variantId);
    const CarModelResult car = convertCarModels(file, path, digest);
    check(car.err == CarModelError::Ok && !car.value.meshes.empty(),
          "baseline conversion yields meshes for the record mutations");
    if (car.err != CarModelError::Ok || car.value.meshes.empty()) {
        return finish();
    }

    // --- 5./6. container serialize/deserialize edges -----------------------
    serializeEdges(realImage, car.value.meshes[0]);
    Bytes carWire;
    try {
        carWire = serialize(car.value);
    } catch (const std::exception& e) {
        check(false, std::string("serializing the real car container threw: ") + e.what());
        return finish();
    }
    HostAssetContainer textureContainer;
    textureContainer.version = kContainerVersion;
    textureContainer.images.push_back(realImage);
    Bytes textureWire;
    try {
        textureWire = serialize(textureContainer);
    } catch (const std::exception& e) {
        check(false, std::string("serializing the real texture container threw: ") + e.what());
        return finish();
    }
    deserializeEdges(carWire, textureWire);

    // --- 7. loadImage rejections --------------------------------------------
    loadImageEdges(realImage);

    return finish();
}
