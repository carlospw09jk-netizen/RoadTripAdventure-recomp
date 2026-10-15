// Synthetic contract tests plus an optional local-disc integration check.

#include "../../src/native/assets/field_converter.h"

#include <array>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using rt::native::assets::FieldTileError;
using rt::native::assets::FieldTileResult;
using rt::native::assets::convertFieldTile;
using rt::native::assets::parseFieldTileHeader;

namespace {

int g_checks = 0;
int g_failures = 0;

void check(bool condition, const std::string& description) {
    ++g_checks;
    if (!condition) {
        ++g_failures;
        std::cerr << "FAIL: " << description << '\n';
    }
}

using Bytes = std::vector<std::uint8_t>;
using Digest = std::array<std::uint8_t, 32>;

Digest testDigest() {
    Digest digest{};
    digest.fill(0xA5);
    return digest;
}

void setU32(Bytes& bytes, std::size_t offset, std::uint32_t value) {
    for (std::size_t i = 0; i < 4; ++i) {
        bytes[offset + i] = static_cast<std::uint8_t>(value >> (8 * i));
    }
}

FieldTileResult convert(const Bytes& bytes) {
    return convertFieldTile(bytes, "synthetic.fld", testDigest());
}

void testTruncatedHeader() {
    const Bytes bytes(0x1F, 0);
    const FieldTileResult result = convert(bytes);
    check(result.err == FieldTileError::Truncated, "truncated header: returns Truncated");
}

void testOffsetPastFileIsBadHeader() {
    Bytes bytes(0x20, 0);
    setU32(bytes, 0x00, 0x20);
    setU32(bytes, 0x04, 0x21);  // offA exceeds file size
    setU32(bytes, 0x08, 0x21);
    setU32(bytes, 0x0C, 0x21);
    setU32(bytes, 0x10, 0x20);
    const FieldTileResult result = convert(bytes);
    check(result.err == FieldTileError::BadHeader, "out-of-file offA: returns BadHeader");
}

std::filesystem::path discRoot() {
    if (const char* root = std::getenv("RT_DISC")) {
        return root;
    }
    if (const char* home = std::getenv("HOME")) {
        return std::filesystem::path(home) / "Library/Application Support/RoadTripRecomp/disc";
    }
    return {};
}

void testRealFieldTileWhenAvailable() {
    const std::filesystem::path path = discRoot() / "FLD/000.BIN";
    if (path.empty() || !std::filesystem::exists(path)) {
        std::cout << "test_field_converter: SKIP real FLD/000.BIN (not found)\n";
        return;
    }

    std::ifstream input(path, std::ios::binary);
    const Bytes bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    check(!bytes.empty(), "real FLD/000.BIN: file read");
    const auto header = parseFieldTileHeader(bytes);
    check(header.has_value(), "real FLD/000.BIN: header parses");
    if (!header) {
        return;
    }
    check(header->offA == 22528, "real FLD/000.BIN: offA");
    check(header->offB == 1778832, "real FLD/000.BIN: offB");
    check(header->offC == 2505600, "real FLD/000.BIN: offC");
    check(header->fileSize == 2506352, "real FLD/000.BIN: fileSize");
}

} // namespace

int main() {
    testTruncatedHeader();
    testOffsetPastFileIsBadHeader();
    testRealFieldTileWhenAvailable();

    std::cout << "test_field_converter: " << (g_checks - g_failures) << "/" << g_checks
              << " checks passed\n";
    return g_failures == 0 ? 0 : 1;
}
