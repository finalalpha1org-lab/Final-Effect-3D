// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/core/result.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace finaleffect::project {

class JsonValue;

using JsonArray = std::vector<JsonValue>;
// Preserves key insertion order for deterministic, human-readable project files.
using JsonObject = std::vector<std::pair<std::string, JsonValue>>;

class JsonValue {
public:
    using Storage = std::variant<std::nullptr_t, bool, double, std::string, JsonArray, JsonObject>;

    JsonValue() noexcept : storage_(nullptr) {}
    JsonValue(std::nullptr_t) noexcept : storage_(nullptr) {}
    JsonValue(bool b) noexcept : storage_(b) {}
    JsonValue(double num) noexcept : storage_(num) {}
    JsonValue(int num) noexcept : storage_(static_cast<double>(num)) {}
    JsonValue(std::uint64_t num) noexcept : storage_(static_cast<double>(num)) {}
    JsonValue(std::string str) noexcept : storage_(std::move(str)) {}
    JsonValue(const char* str) : storage_(std::string(str)) {}
    JsonValue(JsonArray arr) noexcept : storage_(std::move(arr)) {}
    JsonValue(JsonObject obj) noexcept : storage_(std::move(obj)) {}

    [[nodiscard]] bool isNull() const noexcept { return std::holds_alternative<std::nullptr_t>(storage_); }
    [[nodiscard]] bool isBool() const noexcept { return std::holds_alternative<bool>(storage_); }
    [[nodiscard]] bool isNumber() const noexcept { return std::holds_alternative<double>(storage_); }
    [[nodiscard]] bool isString() const noexcept { return std::holds_alternative<std::string>(storage_); }
    [[nodiscard]] bool isArray() const noexcept { return std::holds_alternative<JsonArray>(storage_); }
    [[nodiscard]] bool isObject() const noexcept { return std::holds_alternative<JsonObject>(storage_); }

    [[nodiscard]] bool asBool() const { return std::get<bool>(storage_); }
    [[nodiscard]] double asNumber() const { return std::get<double>(storage_); }
    [[nodiscard]] const std::string& asString() const { return std::get<std::string>(storage_); }
    [[nodiscard]] const JsonArray& asArray() const { return std::get<JsonArray>(storage_); }
    [[nodiscard]] JsonArray& asArray() { return std::get<JsonArray>(storage_); }
    [[nodiscard]] const JsonObject& asObject() const { return std::get<JsonObject>(storage_); }
    [[nodiscard]] JsonObject& asObject() { return std::get<JsonObject>(storage_); }

    [[nodiscard]] const JsonValue* findKey(std::string_view key) const noexcept;
    void setKey(std::string key, JsonValue value);

private:
    Storage storage_;
};

[[nodiscard]] core::Result<JsonValue> parseJson(std::string_view input, std::size_t max_depth = 64);
[[nodiscard]] core::Result<std::string> writeJson(const JsonValue& root, bool pretty = true);

} // namespace finaleffect::project
