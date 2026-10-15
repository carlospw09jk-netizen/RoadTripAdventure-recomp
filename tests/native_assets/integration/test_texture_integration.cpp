// tests/native_assets/integration/test_texture_integration.cpp
//
// End-to-end integration test for the texture side of the native asset
// pipeline, exercised against the user's own disc file CAR0/Q00.BIN at run
// time: read -> locate the texture upload chain (header word +0x18, the
// CNT..END DMA region after the model region) -> convertImage -> serialize ->
// deserialize -> loadImage. No game data ships with this test; the disc root
// comes from the environment (RT_DISC, default "~/Library/Application
// Support/RoadTripRecomp/disc"). A missing file is a loud setup failure
// (nonzero exit), not a silent skip.
//
// Full-pipeline contract asserted on the real chain:
//   1. The header word at +0x18 points inside the file at a CNT tag, and the
//      CNT..END walk terminates with an END tag inside the file (the loader's
//      own chain checks, spec section 8.2).
//   2. convertImageBankCount exposes exactly one palette selection for this
//      chain; convertImage decodes it to a 128x128 R8G8B8A8_UNORM record
//      with 65536 tightly packed pixel bytes, mipCount 1, the caller's
//      variant id and provenance preserved. convertImageBank(0) equals
//      convertImage; a bank beyond the count rejects with an empty record.
//   3. The decoded pixels are real imagery: not one flat colour, and at least
//      one opaque texel (alpha >= 128 after the reference alpha scale).
//   4. A container holding the record serializes and deserializes back to an
//      equal container (byte-stable on re-serialize).
//   5. loadImage turns the record (and its round-tripped copy) into a
//      consistent NativeImage, and rejects the empty record.
//
// Failure accounting is plain counters and if-statements (no assert()), so
// behaviour is NDEBUG-independent; any failed check exits nonzero.

#include "../../../src/native/assets/container.h"
#include "../../../src/native/assets/image_converter.h"
#include "../../../src/native/assets/image_loader.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <span>
#include <string>
#include <vector>

using rt::native::assets::ContainerError;
using rt::native::assets::ContainerResult;
using rt::native::assets::HostAssetContainer;
using rt::native::assets::ImageRecord;
using rt::native::assets::NativeImage;
using rt::native::assets::PixelFormat;
using rt::native::assets::Provenance;
using rt::native::assets::convertImage;
using rt::native::assets::convertImageBank;
using rt::native::assets::convertImageBankCount;
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
        std::cerr << "test_texture_integration: " << g_failures << " of " << g_checks
                  << " checks FAILED\n";
        return EXIT_FAILURE;
    }
    std::cout << "test_texture_integration: " << g_checks << " checks passed\n";
    return EXIT_SUCCESS;
}

using Bytes = std::vector<std::uint8_t>;
using Digest = std::array<std::uint8_t, 32>;

// DMA tag identity byte in byte 3 of the tag qword (loader check, spec 8.2).
constexpr std::uint8_t kDmaTagCnt = 0x10;
constexpr std::uint8_t kDmaTagEnd = 0x70;

std::uint16_t readU16LE(const Bytes& b, std::size_t o) {
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(b[o]) | (static_cast<std::uint16_t>(b[o + 1]) << 8));
}

std::uint32_t readU32LE(const Bytes& b, std::size_t o) {
    return static_cast<std::uint32_t>(b[o]) |
           (static_cast<std::uint32_t>(b[o + 1]) << 8) |
           (static_cast<std::uint32_t>(b[o + 2]) << 16) |
           (static_cast<std::uint32_t>(b[o + 3]) << 24);
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

// Walks the CNT..END DMA region starting at chainStart (the loader's own tag
// rules: byte 3 of the tag qword is 0x10 for CNT and 0x70 for END; qwc is the
// u16 at tag + 0). On success regionEnd is one past the END tag's payload.
bool walkChainRegion(const Bytes& bytes, std::size_t chainStart, std::size_t& regionEnd,
                     int& cntTags) {
    std::size_t o = chainStart;
    cntTags = 0;
    while (o + 16 <= bytes.size()) {
        const std::uint8_t id = bytes[o + 3];
        const std::size_t payload = static_cast<std::size_t>(readU16LE(bytes, o)) * 16;
        if (id == kDmaTagEnd) {
            regionEnd = o + 16 + payload;
            return regionEnd <= bytes.size();
        }
        if (id != kDmaTagCnt) {
            return false;
        }
        ++cntTags;
        o += 16 + payload;
    }
    return false;
}

} // namespace

int main() {
    const std::string root = discRoot();
    if (root.empty()) {
        std::cerr << "test_texture_integration: cannot resolve the disc root "
                     "(set RT_DISC; HOME is unset)\n";
        return EXIT_FAILURE;
    }
    const std::filesystem::path carPath = std::filesystem::path(root) / "CAR0" / "Q00.BIN";
    Bytes bytes;
    if (!readFileBytes(carPath.string(), bytes)) {
        std::cerr << "test_texture_integration: cannot read '" << carPath.string()
                  << "'; set RT_DISC to the extracted disc root "
                     "(no game data ships with this test)\n";
        return EXIT_FAILURE;
    }
    std::cout << "test_texture_integration: " << carPath.string() << " (" << bytes.size()
              << " bytes)\n";

    // --- 1. locate the texture upload chain ---------------------------------
    check(bytes.size() >= 0x20, "car file holds the eight header words");
    if (bytes.size() < 0x20) {
        return finish();
    }
    const std::size_t chainStart = static_cast<std::size_t>(readU32LE(bytes, 0x18));
    check(chainStart >= 0x20 && chainStart + 16 <= bytes.size(),
          "header word +0x18 points at a whole DMA tag inside the file");
    if (chainStart + 16 > bytes.size()) {
        return finish();
    }
    check(bytes[chainStart + 3] == kDmaTagCnt, "texture chain starts with a CNT tag");

    std::size_t regionEnd = 0;
    int cntTags = 0;
    const bool walked = walkChainRegion(bytes, chainStart, regionEnd, cntTags);
    check(walked, "CNT..END walk terminates with an END tag inside the file");
    check(cntTags >= 1, "texture chain holds at least one CNT tag");
    if (!walked) {
        return finish();
    }
    const std::span<const std::uint8_t> region(bytes.data() + chainStart, regionEnd - chainStart);
    std::cout << "test_texture_integration: chain 0x" << std::hex << chainStart << "..0x"
              << regionEnd << std::dec << " (" << region.size() << " bytes, " << cntTags
              << " CNT tags)\n";

    // --- 2. decode the chain's first texture --------------------------------
    const std::uint32_t bankCount = convertImageBankCount(region);
    check(bankCount == 1, "Q00 texture chain exposes exactly one palette selection");

    Provenance provenance;
    provenance.sourceFile = carPath.string();
    provenance.offset = chainStart;
    provenance.length = region.size();
    provenance.sha256 = placeholderDigest(region);
    const std::string variantId = "CAR0/Q00.BIN#0";

    const ImageRecord image = convertImage(region, provenance, variantId);
    check(image.width == 128 && image.height == 128, "decoded texture is 128x128");
    check(image.mipCount == 1, "decoded texture carries a single mip level");
    check(image.format == PixelFormat::R8G8B8A8_UNORM, "decoded format is R8G8B8A8_UNORM");
    check(image.pixels.size() == 65536u, "65536 tightly packed pixel bytes");
    check(image.pixels.size() ==
              static_cast<std::size_t>(image.width) * image.height * 4u,
          "pixel blob equals width * height * 4");
    check(image.sourceVariantId == variantId, "variant id preserved");
    check(image.provenance == provenance, "provenance preserved");
    check(hasValidProvenance(image.provenance), "provenance is valid");

    const ImageRecord bank0 = convertImageBank(region, provenance, variantId, 0);
    check(bank0 == image, "convertImageBank(0) equals convertImage");
    const ImageRecord bank1 = convertImageBank(region, provenance, variantId, 1);
    check(bank1.width == 0 && bank1.height == 0 && bank1.mipCount == 0 && bank1.pixels.empty(),
          "a bank beyond the count rejects with an empty record");

    // --- 3. pixel sanity: real imagery, not a flat or fully transparent fill --
    std::set<std::uint32_t> distinct;
    bool anyOpaque = false;
    for (std::size_t i = 0; i + 3 < image.pixels.size(); i += 4) {
        const std::uint32_t rgba =
            static_cast<std::uint32_t>(image.pixels[i]) |
            (static_cast<std::uint32_t>(image.pixels[i + 1]) << 8) |
            (static_cast<std::uint32_t>(image.pixels[i + 2]) << 16) |
            (static_cast<std::uint32_t>(image.pixels[i + 3]) << 24);
        distinct.insert(rgba);
        if (image.pixels[i + 3] >= 128) {
            anyOpaque = true;
        }
    }
    check(distinct.size() >= 2, "decoded texture is not a single flat colour");
    check(anyOpaque, "decoded texture has opaque texels (alpha >= 128)");

    // --- 4. container round trip ---------------------------------------------
    HostAssetContainer container;
    container.version = kContainerVersion;
    container.images.push_back(image);
    Bytes wire;
    bool serialized = true;
    try {
        wire = serialize(container);
    } catch (const std::exception& e) {
        serialized = false;
        check(false, std::string("serialize threw: ") + e.what());
    }
    if (!serialized) {
        return finish();
    }
    check(wire.size() >= 16 && wire[0] == 'R' && wire[1] == 'T' && wire[2] == 'A' &&
              wire[3] == 'C',
          "serialized container starts with the RTAC magic");
    const ContainerResult back = deserialize(wire);
    check(back.err == ContainerError::Ok, "deserialize accepts its own wire format");
    check(back.value == container, "deserialize round-trips to an equal container");
    check(back.value.images.size() == 1 && back.value.meshes.empty(),
          "round-tripped container holds exactly the one image");
    try {
        check(serialize(back.value) == wire,
              "re-serializing the round-tripped container is byte-stable");
    } catch (const std::exception& e) {
        check(false, std::string("re-serialize threw: ") + e.what());
    }

    // --- 5. loadImage consistency --------------------------------------------
    try {
        const NativeImage native = loadImage(image);
        check(native.width == image.width, "loadImage keeps the width");
        check(native.height == image.height, "loadImage keeps the height");
        check(native.mipCount == image.mipCount, "loadImage keeps the mip count");
        check(native.pixels == image.pixels, "loadImage copies the pixel blob verbatim");
        check(native.variantId == image.sourceVariantId, "loadImage keeps the variant id");

        if (back.err == ContainerError::Ok && back.value.images.size() == 1) {
            const NativeImage native2 = loadImage(back.value.images[0]);
            check(native2.width == native.width && native2.height == native.height &&
                      native2.mipCount == native.mipCount && native2.pixels == native.pixels &&
                      native2.variantId == native.variantId,
                  "loadImage agrees across the serialize/deserialize round trip");
        }
    } catch (const std::exception& e) {
        check(false, std::string("loadImage threw: ") + e.what());
    }

    // Failure contract on real pipeline objects: the rejected bank-1 record
    // never loads.
    bool threw = false;
    try {
        (void)loadImage(bank1);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "loadImage rejects the empty record of an out-of-range bank");

    return finish();
}
