// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/project/json.hpp"

#include <cctype>
#include <cerrno>
#include <clocale>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace finaleffect::project {

const JsonValue* JsonValue::findKey(std::string_view key) const noexcept {
    if (!isObject()) {
        return nullptr;
    }
    const auto& obj = asObject();
    for (const auto& [k, v] : obj) {
        if (k == key) {
            return &v;
        }
    }
    return nullptr;
}

void JsonValue::setKey(std::string key, JsonValue value) {
    if (!isObject()) {
        storage_ = JsonObject{};
    }
    auto& obj = asObject();
    for (auto& [k, v] : obj) {
        if (k == key) {
            v = std::move(value);
            return;
        }
    }
    obj.emplace_back(std::move(key), std::move(value));
}

namespace {

void appendUtf8(std::string& out, std::uint32_t codepoint) {
    if (codepoint <= 0x7Fu) {
        out.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7FFu) {
        out.push_back(static_cast<char>(0xC0u | ((codepoint >> 6u) & 0x1Fu)));
        out.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
    } else if (codepoint <= 0xFFFFu) {
        out.push_back(static_cast<char>(0xE0u | ((codepoint >> 12u) & 0x0Fu)));
        out.push_back(static_cast<char>(0x80u | ((codepoint >> 6u) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
    } else {
        out.push_back(static_cast<char>(0xF0u | ((codepoint >> 18u) & 0x07u)));
        out.push_back(static_cast<char>(0x80u | ((codepoint >> 12u) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | ((codepoint >> 6u) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
    }
}

class JsonParser {
public:
    JsonParser(std::string_view input, std::size_t max_depth)
        : input_(input), max_depth_(max_depth) {}

    core::Result<JsonValue> parseDocument() {
        skipWhitespace();
        if (atEnd()) {
            return makeError("Empty JSON document");
        }
        auto val = parseValue(0);
        if (!val) {
            return val;
        }
        skipWhitespace();
        if (!atEnd()) {
            return makeError("Unexpected trailing characters after root JSON value");
        }
        return val;
    }

private:
    [[nodiscard]] bool atEnd() const noexcept { return pos_ >= input_.size(); }
    [[nodiscard]] char peek() const noexcept { return atEnd() ? '\0' : input_[pos_]; }

    char advance() noexcept {
        const char ch = input_[pos_++];
        if (ch == '\n') {
            ++line_;
            column_ = 1;
        } else {
            ++column_;
        }
        return ch;
    }

    void skipWhitespace() noexcept {
        while (!atEnd()) {
            const char c = peek();
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                advance();
            } else {
                break;
            }
        }
    }

    [[nodiscard]] core::Error makeError(std::string message) const {
        return core::Error(
            core::ErrorCode::ParseError,
            std::move(message),
            "line " + std::to_string(line_) + ", col " + std::to_string(column_));
    }

    core::Result<JsonValue> parseValue(std::size_t depth) {
        if (depth > max_depth_) {
            return makeError("JSON nesting exceeds maximum allowed depth");
        }
        skipWhitespace();
        if (atEnd()) {
            return makeError("Unexpected end of input while expecting JSON value");
        }

        const char c = peek();
        switch (c) {
            case '{':
                return parseObject(depth + 1);
            case '[':
                return parseArray(depth + 1);
            case '"': {
                auto s = parseString();
                if (!s) {
                    return s.error();
                }
                return core::Result<JsonValue>::ok(JsonValue(std::move(s).value()));
            }
            case 't':
                return parseLiteral("true", JsonValue(true));
            case 'f':
                return parseLiteral("false", JsonValue(false));
            case 'n':
                return parseLiteral("null", JsonValue(nullptr));
            default:
                if (c == '-' || (c >= '0' && c <= '9')) {
                    return parseNumber();
                }
                return makeError("Unexpected character in JSON input");
        }
    }

    core::Result<JsonValue> parseLiteral(std::string_view literal, JsonValue result) {
        for (const char expected : literal) {
            if (atEnd() || peek() != expected) {
                return makeError("Invalid JSON literal; expected '" + std::string(literal) + "'");
            }
            advance();
        }
        return core::Result<JsonValue>::ok(std::move(result));
    }

    core::Result<std::uint16_t> parseHexQuad() {
        std::uint16_t value = 0;
        for (int i = 0; i < 4; ++i) {
            if (atEnd()) {
                return makeError("Incomplete \\uXXXX escape sequence");
            }
            const char c = advance();
            value = static_cast<std::uint16_t>(value << 4u);
            if (c >= '0' && c <= '9') {
                value = static_cast<std::uint16_t>(value | static_cast<std::uint16_t>(c - '0'));
            } else if (c >= 'a' && c <= 'f') {
                value = static_cast<std::uint16_t>(value | static_cast<std::uint16_t>(10 + (c - 'a')));
            } else if (c >= 'A' && c <= 'F') {
                value = static_cast<std::uint16_t>(value | static_cast<std::uint16_t>(10 + (c - 'A')));
            } else {
                return makeError("Invalid hex digit in \\uXXXX escape");
            }
        }
        return core::Result<std::uint16_t>::ok(value);
    }

    core::Result<std::string> parseString() {
        advance(); // consume opening '"'
        std::string out;
        while (!atEnd()) {
            const char c = advance();
            if (c == '"') {
                return core::Result<std::string>::ok(std::move(out));
            }
            if (static_cast<unsigned char>(c) < 0x20u) {
                return makeError("Unescaped control character in JSON string");
            }
            if (c != '\\') {
                out.push_back(c);
                continue;
            }
            if (atEnd()) {
                return makeError("Unterminated escape sequence in JSON string");
            }
            const char esc = advance();
            switch (esc) {
                case '"':
                    out.push_back('"');
                    break;
                case '\\':
                    out.push_back('\\');
                    break;
                case '/':
                    out.push_back('/');
                    break;
                case 'b':
                    out.push_back('\b');
                    break;
                case 'f':
                    out.push_back('\f');
                    break;
                case 'n':
                    out.push_back('\n');
                    break;
                case 'r':
                    out.push_back('\r');
                    break;
                case 't':
                    out.push_back('\t');
                    break;
                case 'u': {
                    auto high = parseHexQuad();
                    if (!high) {
                        return high.error();
                    }
                    std::uint32_t codepoint = high.value();
                    if (codepoint >= 0xD800u && codepoint <= 0xDBFFu) {
                        if (pos_ + 2 > input_.size() || input_[pos_] != '\\' || input_[pos_ + 1] != 'u') {
                            return makeError("Invalid UTF-16 high surrogate without following low surrogate");
                        }
                        advance();
                        advance();
                        auto low = parseHexQuad();
                        if (!low) {
                            return low.error();
                        }
                        if (low.value() < 0xDC00u || low.value() > 0xDFFFu) {
                            return makeError("Invalid UTF-16 low surrogate value");
                        }
                        codepoint = 0x10000u + (((codepoint - 0xD800u) << 10u) | (low.value() - 0xDC00u));
                    } else if (codepoint >= 0xDC00u && codepoint <= 0xDFFFu) {
                        return makeError("Lone UTF-16 low surrogate in JSON string");
                    }
                    appendUtf8(out, codepoint);
                    break;
                }
                default:
                    return makeError("Invalid escape character in JSON string");
            }
        }
        return makeError("Unterminated JSON string");
    }

    core::Result<JsonValue> parseNumber() {
        const std::size_t start = pos_;
        if (peek() == '-') {
            advance();
        }
        if (atEnd()) {
            return makeError("Incomplete negative number");
        }
        if (peek() == '0') {
            advance();
            if (!atEnd() && peek() >= '0' && peek() <= '9') {
                return makeError("Leading zeros are not permitted in JSON numbers");
            }
        } else if (peek() >= '1' && peek() <= '9') {
            while (!atEnd() && peek() >= '0' && peek() <= '9') {
                advance();
            }
        } else {
            return makeError("Invalid JSON number syntax");
        }

        if (!atEnd() && peek() == '.') {
            advance();
            if (atEnd() || peek() < '0' || peek() > '9') {
                return makeError("Expected digit after decimal point in JSON number");
            }
            while (!atEnd() && peek() >= '0' && peek() <= '9') {
                advance();
            }
        }

        if (!atEnd() && (peek() == 'e' || peek() == 'E')) {
            advance();
            if (!atEnd() && (peek() == '+' || peek() == '-')) {
                advance();
            }
            if (atEnd() || peek() < '0' || peek() > '9') {
                return makeError("Expected exponent digits in JSON number");
            }
            while (!atEnd() && peek() >= '0' && peek() <= '9') {
                advance();
            }
        }

        std::string token(input_.substr(start, pos_ - start));
        const struct lconv* lc = std::localeconv();
        const char locale_dec = (lc != nullptr && lc->decimal_point != nullptr && lc->decimal_point[0] != '\0')
                                    ? lc->decimal_point[0]
                                    : '.';
        if (locale_dec != '.') {
            for (char& ch : token) {
                if (ch == '.') {
                    ch = locale_dec;
                }
            }
        }

        errno = 0;
        char* end_ptr = nullptr;
        const double value = std::strtod(token.c_str(), &end_ptr);
        if (errno == ERANGE || end_ptr != token.c_str() + token.size() || !std::isfinite(value)) {
            return makeError("JSON number out of representable finite range");
        }
        return core::Result<JsonValue>::ok(JsonValue(value));
    }

    core::Result<JsonValue> parseArray(std::size_t depth) {
        advance(); // '['
        JsonArray arr;
        skipWhitespace();
        if (!atEnd() && peek() == ']') {
            advance();
            return core::Result<JsonValue>::ok(JsonValue(std::move(arr)));
        }

        while (true) {
            auto elem = parseValue(depth);
            if (!elem) {
                return elem;
            }
            arr.push_back(std::move(elem).value());
            skipWhitespace();
            if (atEnd()) {
                return makeError("Unterminated JSON array");
            }
            const char sep = advance();
            if (sep == ']') {
                break;
            }
            if (sep != ',') {
                return makeError("Expected ',' or ']' in JSON array");
            }
        }
        return core::Result<JsonValue>::ok(JsonValue(std::move(arr)));
    }

    core::Result<JsonValue> parseObject(std::size_t depth) {
        advance(); // '{'
        JsonObject obj;
        skipWhitespace();
        if (!atEnd() && peek() == '}') {
            advance();
            return core::Result<JsonValue>::ok(JsonValue(std::move(obj)));
        }

        while (true) {
            skipWhitespace();
            if (atEnd() || peek() != '"') {
                return makeError("Expected string key in JSON object");
            }
            auto key = parseString();
            if (!key) {
                return key.error();
            }
            skipWhitespace();
            if (atEnd() || advance() != ':') {
                return makeError("Expected ':' after JSON object key");
            }
            auto val = parseValue(depth);
            if (!val) {
                return val;
            }
            obj.emplace_back(std::move(key).value(), std::move(val).value());

            skipWhitespace();
            if (atEnd()) {
                return makeError("Unterminated JSON object");
            }
            const char sep = advance();
            if (sep == '}') {
                break;
            }
            if (sep != ',') {
                return makeError("Expected ',' or '}' in JSON object");
            }
        }
        return core::Result<JsonValue>::ok(JsonValue(std::move(obj)));
    }

    std::string_view input_;
    std::size_t max_depth_{64};
    std::size_t pos_{0};
    std::size_t line_{1};
    std::size_t column_{1};
};

void writeEscapedString(std::string& out, std::string_view str) {
    out.push_back('"');
    for (const char raw_ch : str) {
        const auto ch = static_cast<unsigned char>(raw_ch);
        switch (ch) {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\b':
                out += "\\b";
                break;
            case '\f':
                out += "\\f";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                if (ch < 0x20u) {
                    char buf[7];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned int>(ch));
                    out += buf;
                } else {
                    out.push_back(static_cast<char>(ch));
                }
                break;
        }
    }
    out.push_back('"');
}

void writeIndent(std::string& out, std::size_t depth) {
    out.append(depth * 2, ' ');
}

core::Status serializeValue(const JsonValue& val, bool pretty, std::size_t depth, std::string& out) {
    if (val.isNull()) {
        out += "null";
        return core::Status::ok();
    }
    if (val.isBool()) {
        out += val.asBool() ? "true" : "false";
        return core::Status::ok();
    }
    if (val.isNumber()) {
        const double num = val.asNumber();
        if (!std::isfinite(num)) {
            return core::Status::fail(
                core::ErrorCode::ValidationError,
                "Cannot serialize non-finite floating-point value to JSON");
        }
        char buf[64];
        const double normalized_zero = (num == 0.0) ? 0.0 : num;
        std::snprintf(buf, sizeof(buf), "%.9g", normalized_zero);
        for (char* p = buf; *p != '\0'; ++p) {
            if (*p == ',') {
                *p = '.';
            }
        }
        out += buf;
        return core::Status::ok();
    }
    if (val.isString()) {
        writeEscapedString(out, val.asString());
        return core::Status::ok();
    }
    if (val.isArray()) {
        const auto& arr = val.asArray();
        if (arr.empty()) {
            out += "[]";
            return core::Status::ok();
        }
        out.push_back('[');
        for (std::size_t i = 0; i < arr.size(); ++i) {
            if (pretty) {
                out.push_back('\n');
                writeIndent(out, depth + 1);
            }
            auto st = serializeValue(arr[i], pretty, depth + 1, out);
            if (!st) {
                return st;
            }
            if (i + 1 < arr.size()) {
                out.push_back(',');
            }
        }
        if (pretty) {
            out.push_back('\n');
            writeIndent(out, depth);
        }
        out.push_back(']');
        return core::Status::ok();
    }

    const auto& obj = val.asObject();
    if (obj.empty()) {
        out += "{}";
        return core::Status::ok();
    }
    out.push_back('{');
    for (std::size_t i = 0; i < obj.size(); ++i) {
        if (pretty) {
            out.push_back('\n');
            writeIndent(out, depth + 1);
        }
        writeEscapedString(out, obj[i].first);
        out += pretty ? ": " : ":";
        auto st = serializeValue(obj[i].second, pretty, depth + 1, out);
        if (!st) {
            return st;
        }
        if (i + 1 < obj.size()) {
            out.push_back(',');
        }
    }
    if (pretty) {
        out.push_back('\n');
        writeIndent(out, depth);
    }
    out.push_back('}');
    return core::Status::ok();
}

} // namespace

core::Result<JsonValue> parseJson(std::string_view input, std::size_t max_depth) {
    JsonParser parser(input, max_depth);
    return parser.parseDocument();
}

core::Result<std::string> writeJson(const JsonValue& root, bool pretty) {
    std::string out;
    out.reserve(512);
    auto st = serializeValue(root, pretty, 0, out);
    if (!st) {
        return st.error();
    }
    if (pretty) {
        out.push_back('\n');
    }
    return core::Result<std::string>::ok(std::move(out));
}

} // namespace finaleffect::project
