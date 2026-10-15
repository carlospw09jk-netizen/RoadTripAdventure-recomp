#pragma once

// FLD/COURSE field-tile header parsing and deferred mesh conversion.

#include "container.h"

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <variant>

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
