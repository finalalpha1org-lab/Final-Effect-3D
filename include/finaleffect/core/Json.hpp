// Final Effect 3D - dependency-free JSON value, parser and writer.
// SPDX-License-Identifier: GPL-3.0-only
//
// The project format needs a small, auditable, standard-library-only JSON
// implementation. This is an original implementation written for Final Effect 3D:
// it is not copied from any third-party library and it introduces no build
// dependency on the network.
//
// Guarantees:
//   * Objects keep insertion order, so serialized files are deterministic.
//   * Parsing is iterative-depth limited (JsonValue::kMaxDepth) and never recurses
//     without a bound, so hostile input cannot exhaust the stack.
//   * Numbers are parsed and written with std::from_chars / std::to_chars, which
//     are locale independent.
//   * Non-finite numbers are written as null; callers that must not lose data
//     (the project serializer) validate finiteness before writing.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fe::core {

using JsonMember = std::pair<std::string, class JsonValue>;

class JsonValue;
struct JsonParseResult;

class JsonValue {
public:
    enum class Type { Null, Boolean, Number, String, Array, Object };

    static constexpr std::size_t kMaxDepth = 64;

    JsonValue() noexcept = default;
    JsonValue(std::nullptr_t) noexcept {}  // NOLINT(google-explicit-constructor)
    JsonValue(bool value) noexcept : type_(Type::Boolean), boolean_(value) {}
    JsonValue(double value) noexcept : type_(Type::Number), number_(value) {}
    JsonValue(int value) noexcept : type_(Type::Number), number_(static_cast<double>(value)) {}
    JsonValue(std::int64_t value) noexcept : type_(Type::Number), number_(static_cast<double>(value)) {}
    JsonValue(std::string value) : type_(Type::String), string_(std::move(value)) {}
    JsonValue(const char* value)
        : type_(Type::String), string_(value == nullptr ? std::string() : std::string(value)) {}

    static JsonValue array();
    static JsonValue object();

    Type type() const noexcept { return type_; }
    bool isNull() const noexcept { return type_ == Type::Null; }
    bool isBoolean() const noexcept { return type_ == Type::Boolean; }
    bool isNumber() const noexcept { return type_ == Type::Number; }
    bool isString() const noexcept { return type_ == Type::String; }
    bool isArray() const noexcept { return type_ == Type::Array; }
    bool isObject() const noexcept { return type_ == Type::Object; }

    bool asBoolean(bool fallback = false) const noexcept;
    double asNumber(double fallback = 0.0) const noexcept;
    /// Writes the number into out only when this value is a finite number.
    bool numberIfFinite(double& out) const noexcept;
    const std::string& asString() const noexcept;
    const std::vector<JsonValue>& items() const noexcept;
    const std::vector<JsonMember>& members() const noexcept;

    /// Object member lookup. Returns nullptr when absent or when this is not an object.
    const JsonValue* find(std::string_view key) const noexcept;
    /// Convenience accessors; they return the fallback when the key is absent or
    /// when the stored type does not match.
    double numberOr(std::string_view key, double fallback = 0.0) const noexcept;
    bool booleanOr(std::string_view key, bool fallback = false) const noexcept;
    std::string stringOr(std::string_view key, std::string fallback = {}) const;

    /// Object write. A null value becomes an empty object first. Existing keys are
    /// replaced in place (order preserved), new keys are appended.
    JsonValue& set(std::string_view key, JsonValue value);
    /// Array append. A null value becomes an empty array first.
    JsonValue& append(JsonValue value);

    std::size_t size() const noexcept;
    void reset() noexcept;

    /// Serializes with the given indent (0 = compact single line).
    std::string dump(int indent = 2) const;

    /// Parses one complete JSON document. Trailing content (other than whitespace)
    /// is reported as an error, so a truncated or concatenated file is rejected.
    static JsonParseResult parse(std::string_view text);

private:
    Type type_ = Type::Null;
    bool boolean_ = false;
    double number_ = 0.0;
    std::string string_;
    std::vector<JsonValue> items_;
    std::vector<JsonMember> members_;
};

/// Result of JsonValue::parse. Declared after JsonValue because it holds one.
struct JsonParseResult {
    bool ok = false;
    JsonValue value;
    std::string error;
    std::size_t offset = 0;
    int line = 1;
    int column = 1;
};

}  // namespace fe::core
