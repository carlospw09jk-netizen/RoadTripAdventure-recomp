#pragma once

// FLD/COURSE field-tile header parsing and deferred mesh conversion.

#include "container.h"

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>
#include <vector>

namespace rt::native::assets {

enum class FieldTileError : std::uint8_t {
    Ok = 0,
    Truncated,
    BadHeader,
    ChainDecode,
    BadBatch,
    MissingProvenance,
};

// C++20 expected-like result used for independently testable binary parsing.
template <class Value, class Error>
class expected {
public:
    expected(Value value) : storage_(std::move(value)) {}
    expected(Error error) : storage_(error) {}

    bool has_value() const noexcept { return std::holds_alternative<Value>(storage_); }
    explicit operator bool() const noexcept { return has_value(); }
    Value& operator*() { return std::get<Value>(storage_); }
    const Value& operator*() const { return std::get<Value>(storage_); }
    Value* operator->() { return &std::get<Value>(storage_); }
    const Value* operator->() const { return &std::get<Value>(storage_); }
    Error error() const { return std::get<Error>(storage_); }

private:
    std::variant<Value, Error> storage_;
};

struct FieldTileHeader {
    std::uint32_t offA = 0;
    std::uint32_t offB = 0;
    std::uint32_t offC = 0;
    std::uint32_t fileSize = 0;
};

// Decoded tile catalog mirroring the runtime loader semantics (see
// docs/native-reference/field-topology-status.md): a 65-entry sub-block
// directory relative to offA, a 256-entry attribute table, and a PMP model
// block (magic 0x600 old / 0x608 new) at offB with a relative section
// pointer table.
struct FieldTileCatalog {
    FieldTileHeader header{};
    std::vector<std::uint32_t> subBlocks;    // absolute file offsets of sub-block DMA tag words
    std::uint32_t attributeCount = 0;        // non-zero entries in the 256-entry attribute table
    std::uint32_t pmpMagic = 0;              // 0x600 or 0x608
    std::vector<std::uint32_t> pmpSections;  // absolute file offsets, strictly increasing
    std::uint32_t pmpExtensionOffset = 0;    // absolute; 0 for old-style (0x600) blocks
};

expected<FieldTileCatalog, FieldTileError>
catalogFieldTile(std::span<const std::uint8_t> tileFile);

expected<FieldTileHeader, FieldTileError>
parseFieldTileHeader(std::span<const std::uint8_t> tileFile);

struct FieldTileResult {
    FieldTileError err = FieldTileError::Ok;
    HostAssetContainer value;
    std::uint64_t errorOffset = 0;
};

FieldTileResult convertFieldTile(std::span<const std::uint8_t> tileFile,
                                 const std::string& sourceFile,
                                 const std::array<std::uint8_t, 32>& sha256);

} // namespace rt::native::assets
