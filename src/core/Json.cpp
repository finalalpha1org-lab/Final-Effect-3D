// SPDX-License-Identifier: GPL-3.0-only
#include "finaleffect/core/Json.hpp"

#include <charconv>
#include <cmath>
#include <cstdio>
#include <limits>

namespace fe::core {
namespace {

const std::string& emptyString() {
    static const std::string value;
    return value;
}
const std::vector<JsonValue>& emptyItems() {
    static const std::vector<JsonValue> value;
    return value;
}
const std::vector<JsonMember>& emptyMembers() {
    static const std::vector<JsonMember> value;
    return value;
}

void appendIndent(std::string& out, int indent, int depth) {
    if (indent <= 0) {
        return;
    }
    out.push_back('\n');
    out.append(static_cast<std::size_t>(indent * depth), ' ');
}

void appendEscapedString(std::string& out, std::string_view text) {
    out.push_back('"');
    for (const char rawChar : text) {
        const auto c = static_cast<unsigned char>(rawChar);
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buffer[8];
                    std::snprintf(buffer, sizeof(buffer), "\\u%04x", c);
                    out += buffer;
                } else {
                    out.push_back(rawChar);
                }
                break;
        }
    }
    out.push_back('"');
}

void appendNumber(std::string& out, double value) {
    if (!std::isfinite(value)) {
        out += "null";
        return;
    }
    char buffer[40];
    const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
    if (result.ec != std::errc()) {
        out += "null";
        return;
    }
    out.append(buffer, result.ptr);
}

void writeValue(std::string& out, const JsonValue& value, int indent, int depth) {
    switch (value.type()) {
        case JsonValue::Type::Null:
            out += "null";
            break;
        case JsonValue::Type::Boolean:
            out += value.asBoolean() ? "true" : "false";
            break;
        case JsonValue::Type::Number:
            appendNumber(out, value.asNumber());
            break;
        case JsonValue::Type::String:
            appendEscapedString(out, value.asString());
            break;
        case JsonValue::Type::Array: {
            const std::vector<JsonValue>& items = value.items();
            if (items.empty()) {
                out += "[]";
                break;
            }
            out.push_back('[');
            for (std::size_t i = 0; i < items.size(); ++i) {
                if (i != 0) {
                    out.push_back(',');
                }
                appendIndent(out, indent, depth + 1);
                writeValue(out, items[i], indent, depth + 1);
            }
            appendIndent(out, indent, depth);
            out.push_back(']');
            break;
        }
        case JsonValue::Type::Object: {
            const std::vector<JsonMember>& members = value.members();
            if (members.empty()) {
                out += "{}";
                break;
            }
            out.push_back('{');
            for (std::size_t i = 0; i < members.size(); ++i) {
                if (i != 0) {
                    out.push_back(',');
                }
                appendIndent(out, indent, depth + 1);
                appendEscapedString(out, members[i].first);
                out.push_back(':');
                if (indent > 0) {
                    out.push_back(' ');
                }
                writeValue(out, members[i].second, indent, depth + 1);
            }
            appendIndent(out, indent, depth);
            out.push_back('}');
            break;
        }
    }
}

class Parser {
public:
    explicit Parser(std::string_view text) : text_(text) {}

    JsonParseResult run() {
        JsonParseResult result;
        JsonValue root;
        skipWhitespace();
        if (!parseValue(root, 0)) {
            return fail(result);
        }
        skipWhitespace();
        if (!atEnd()) {
            setError("unexpected trailing content after the JSON document");
            return fail(result);
        }
        result.ok = true;
        result.value = std::move(root);
        return result;
    }

private:
    JsonParseResult fail(JsonParseResult& result) const {
        result.ok = false;
        result.error = error_;
        result.offset = errorOffset_;
        computeLineColumn(result.offset, result.line, result.column);
        return result;
    }

    void computeLineColumn(std::size_t offset, int& line, int& column) const {
        line = 1;
        column = 1;
        const std::size_t limit = offset < text_.size() ? offset : text_.size();
        for (std::size_t i = 0; i < limit; ++i) {
            if (text_[i] == '\n') {
                ++line;
                column = 1;
            } else {
                ++column;
            }
        }
    }

    void setError(std::string message) {
        error_ = std::move(message);
        errorOffset_ = position_;
    }

    bool atEnd() const { return position_ >= text_.size(); }
    char peek() const { return atEnd() ? '\0' : text_[position_]; }

    void skipWhitespace() {
        while (!atEnd()) {
            const char c = text_[position_];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                ++position_;
            } else {
                break;
            }
        }
    }

    bool expect(char c) {
        if (peek() != c) {
            std::string message = "expected '";
            message.push_back(c);
            message += "'";
            setError(std::move(message));
            return false;
        }
        ++position_;
        return true;
    }

    bool parseLiteral(std::string_view literal, const JsonValue& value, JsonValue& out) {
        if (text_.compare(position_, literal.size(), literal) != 0) {
            setError("invalid literal");
            return false;
        }
        position_ += literal.size();
        out = value;
        return true;
    }

    bool parseString(std::string& out) {
        if (!expect('"')) {
            return false;
        }
        out.clear();
        while (true) {
            if (atEnd()) {
                setError("unterminated string");
                return false;
            }
            const char c = text_[position_++];
            if (c == '"') {
                return true;
            }
            if (static_cast<unsigned char>(c) < 0x20) {
                --position_;
                setError("unescaped control character in string");
                return false;
            }
            if (c != '\\') {
                out.push_back(c);
                continue;
            }
            if (atEnd()) {
                setError("unterminated escape sequence");
                return false;
            }
            const char escape = text_[position_++];
            switch (escape) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u': {
                    unsigned int codePoint = 0;
                    if (!parseHex4(codePoint)) {
                        return false;
                    }
                    if (codePoint >= 0xD800 && codePoint <= 0xDBFF) {
                        if (!parseLowSurrogate(codePoint)) {
                            return false;
                        }
                    } else if (codePoint >= 0xDC00 && codePoint <= 0xDFFF) {
                        setError("unpaired UTF-16 low surrogate");
                        return false;
                    }
                    appendUtf8(out, codePoint);
                    break;
                }
                default:
                    --position_;
                    setError("invalid escape sequence");
                    return false;
            }
        }
    }

    bool parseHex4(unsigned int& out) {
        if (position_ + 4 > text_.size()) {
            setError("truncated \\u escape");
            return false;
        }
        out = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = text_[position_ + static_cast<std::size_t>(i)];
            out <<= 4U;
            if (c >= '0' && c <= '9') {
                out |= static_cast<unsigned int>(c - '0');
            } else if (c >= 'a' && c <= 'f') {
                out |= static_cast<unsigned int>(c - 'a' + 10);
            } else if (c >= 'A' && c <= 'F') {
                out |= static_cast<unsigned int>(c - 'A' + 10);
            } else {
                setError("invalid hexadecimal digit in \\u escape");
                return false;
            }
        }
        position_ += 4;
        return true;
    }

    bool parseLowSurrogate(unsigned int& high) {
        if (position_ + 2 > text_.size() || text_[position_] != '\\' || text_[position_ + 1] != 'u') {
            setError("high surrogate without a following low surrogate");
            return false;
        }
        position_ += 2;
        unsigned int low = 0;
        if (!parseHex4(low)) {
            return false;
        }
        if (low < 0xDC00 || low > 0xDFFF) {
            setError("high surrogate without a following low surrogate");
            return false;
        }
        high = 0x10000U + ((high - 0xD800U) << 10U) + (low - 0xDC00U);
        return true;
    }

    static void appendUtf8(std::string& out, unsigned int codePoint) {
        if (codePoint <= 0x7FU) {
            out.push_back(static_cast<char>(codePoint));
        } else if (codePoint <= 0x7FFU) {
            out.push_back(static_cast<char>(0xC0U | (codePoint >> 6U)));
            out.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
        } else if (codePoint <= 0xFFFFU) {
            out.push_back(static_cast<char>(0xE0U | (codePoint >> 12U)));
            out.push_back(static_cast<char>(0x80U | ((codePoint >> 6U) & 0x3FU)));
            out.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
        } else {
            out.push_back(static_cast<char>(0xF0U | (codePoint >> 18U)));
            out.push_back(static_cast<char>(0x80U | ((codePoint >> 12U) & 0x3FU)));
            out.push_back(static_cast<char>(0x80U | ((codePoint >> 6U) & 0x3FU)));
            out.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
        }
    }

    bool parseNumber(JsonValue& out) {
        const std::size_t start = position_;
        if (peek() == '-') {
            ++position_;
        }
        if (atEnd() || peek() < '0' || peek() > '9') {
            setError("invalid number");
            return false;
        }
        if (peek() == '0') {
            // RFC 8259: "0" is a number, "01" is not.
            ++position_;
            if (!atEnd() && peek() >= '0' && peek() <= '9') {
                setError("numbers must not have leading zeroes");
                return false;
            }
        } else {
            while (!atEnd() && peek() >= '0' && peek() <= '9') {
                ++position_;
            }
        }
        if (!atEnd() && peek() == '.') {
            ++position_;
            if (atEnd() || peek() < '0' || peek() > '9') {
                setError("invalid fraction in number");
                return false;
            }
            while (!atEnd() && peek() >= '0' && peek() <= '9') {
                ++position_;
            }
        }
        if (!atEnd() && (peek() == 'e' || peek() == 'E')) {
            ++position_;
            if (!atEnd() && (peek() == '+' || peek() == '-')) {
                ++position_;
            }
            if (atEnd() || peek() < '0' || peek() > '9') {
                setError("invalid exponent in number");
                return false;
            }
            while (!atEnd() && peek() >= '0' && peek() <= '9') {
                ++position_;
            }
        }
        double value = 0.0;
        const char* begin = text_.data() + start;
        const char* end = text_.data() + position_;
        const auto result = std::from_chars(begin, end, value);
        if (result.ec != std::errc() || result.ptr != end) {
            setError("number is out of range or malformed");
            return false;
        }
        out = JsonValue(value);
        return true;
    }

    bool parseValue(JsonValue& out, std::size_t depth) {
        if (depth > JsonValue::kMaxDepth) {
            setError("maximum nesting depth exceeded");
            return false;
        }
        if (atEnd()) {
            setError("unexpected end of input");
            return false;
        }
        switch (peek()) {
            case 'n': return parseLiteral("null", JsonValue(), out);
            case 't': return parseLiteral("true", JsonValue(true), out);
            case 'f': return parseLiteral("false", JsonValue(false), out);
            case '"': {
                std::string text;
                if (!parseString(text)) {
                    return false;
                }
                out = JsonValue(std::move(text));
                return true;
            }
            case '[': {
                ++position_;
                JsonValue arrayValue = JsonValue::array();
                skipWhitespace();
                if (peek() == ']') {
                    ++position_;
                    out = std::move(arrayValue);
                    return true;
                }
                while (true) {
                    JsonValue element;
                    skipWhitespace();
                    if (!parseValue(element, depth + 1)) {
                        return false;
                    }
                    arrayValue.append(std::move(element));
                    skipWhitespace();
                    if (peek() == ',') {
                        ++position_;
                        continue;
                    }
                    if (peek() == ']') {
                        ++position_;
                        out = std::move(arrayValue);
                        return true;
                    }
                    setError("expected ',' or ']' in array");
                    return false;
                }
            }
            case '{': {
                ++position_;
                JsonValue objectValue = JsonValue::object();
                skipWhitespace();
                if (peek() == '}') {
                    ++position_;
                    out = std::move(objectValue);
                    return true;
                }
                while (true) {
                    skipWhitespace();
                    std::string key;
                    if (!parseString(key)) {
                        return false;
                    }
                    skipWhitespace();
                    if (!expect(':')) {
                        return false;
                    }
                    skipWhitespace();
                    JsonValue member;
                    if (!parseValue(member, depth + 1)) {
                        return false;
                    }
                    objectValue.set(key, std::move(member));
                    skipWhitespace();
                    if (peek() == ',') {
                        ++position_;
                        continue;
                    }
                    if (peek() == '}') {
                        ++position_;
                        out = std::move(objectValue);
                        return true;
                    }
                    setError("expected ',' or '}' in object");
                    return false;
                }
            }
            default:
                if (peek() == '-' || (peek() >= '0' && peek() <= '9')) {
                    return parseNumber(out);
                }
                setError("unexpected character");
                return false;
        }
    }

    std::string_view text_;
    std::size_t position_ = 0;
    std::string error_;
    std::size_t errorOffset_ = 0;
};

}  // namespace

JsonValue JsonValue::array() {
    JsonValue value;
    value.type_ = Type::Array;
    return value;
}

JsonValue JsonValue::object() {
    JsonValue value;
    value.type_ = Type::Object;
    return value;
}

bool JsonValue::asBoolean(bool fallback) const noexcept {
    return type_ == Type::Boolean ? boolean_ : fallback;
}

double JsonValue::asNumber(double fallback) const noexcept {
    return type_ == Type::Number ? number_ : fallback;
}

bool JsonValue::numberIfFinite(double& out) const noexcept {
    if (type_ != Type::Number || !std::isfinite(number_)) {
        return false;
    }
    out = number_;
    return true;
}

const std::string& JsonValue::asString() const noexcept {
    return type_ == Type::String ? string_ : emptyString();
}

const std::vector<JsonValue>& JsonValue::items() const noexcept {
    return type_ == Type::Array ? items_ : emptyItems();
}

const std::vector<JsonMember>& JsonValue::members() const noexcept {
    return type_ == Type::Object ? members_ : emptyMembers();
}

const JsonValue* JsonValue::find(std::string_view key) const noexcept {
    if (type_ != Type::Object) {
        return nullptr;
    }
    for (const JsonMember& member : members_) {
        if (member.first == key) {
            return &member.second;
        }
    }
    return nullptr;
}

double JsonValue::numberOr(std::string_view key, double fallback) const noexcept {
    const JsonValue* member = find(key);
    return member == nullptr ? fallback : member->asNumber(fallback);
}

bool JsonValue::booleanOr(std::string_view key, bool fallback) const noexcept {
    const JsonValue* member = find(key);
    return member == nullptr ? fallback : member->asBoolean(fallback);
}

std::string JsonValue::stringOr(std::string_view key, std::string fallback) const {
    const JsonValue* member = find(key);
    if (member == nullptr || !member->isString()) {
        return fallback;
    }
    return member->asString();
}

JsonValue& JsonValue::set(std::string_view key, JsonValue value) {
    if (type_ == Type::Null) {
        type_ = Type::Object;
    }
    if (type_ != Type::Object) {
        return *this;
    }
    for (JsonMember& member : members_) {
        if (member.first == key) {
            member.second = std::move(value);
            return *this;
        }
    }
    members_.emplace_back(std::string(key), std::move(value));
    return *this;
}

JsonValue& JsonValue::append(JsonValue value) {
    if (type_ == Type::Null) {
        type_ = Type::Array;
    }
    if (type_ == Type::Array) {
        items_.push_back(std::move(value));
    }
    return *this;
}

std::size_t JsonValue::size() const noexcept {
    switch (type_) {
        case Type::Array: return items_.size();
        case Type::Object: return members_.size();
        case Type::String: return string_.size();
        default: return 0;
    }
}

void JsonValue::reset() noexcept {
    type_ = Type::Null;
    boolean_ = false;
    number_ = 0.0;
    string_.clear();
    items_.clear();
    members_.clear();
}

std::string JsonValue::dump(int indent) const {
    std::string out;
    writeValue(out, *this, indent < 0 ? 0 : indent, 0);
    if (indent > 0) {
        out.push_back('\n');
    }
    return out;
}

JsonParseResult JsonValue::parse(std::string_view text) {
    Parser parser(text);
    return parser.run();
}

}  // namespace fe::core
