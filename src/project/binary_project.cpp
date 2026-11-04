// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/project/binary_project.hpp"
#include "finaleffect/project/binary_checksum.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>

namespace finaleffect::project {

namespace {

constexpr std::uint8_t kTagNull = 0x00;
constexpr std::uint8_t kTagFalse = 0x01;
constexpr std::uint8_t kTagTrue = 0x02;
constexpr std::uint8_t kTagNumber = 0x03;
constexpr std::uint8_t kTagString = 0x04;
constexpr std::uint8_t kTagArray = 0x05;
constexpr std::uint8_t kTagObject = 0x06;

constexpr std::uint32_t kChunkTypeDocument = 1;
constexpr std::uint32_t kChunkTypeGeometry = 2;
constexpr std::uint32_t kFlagHasGeometry = 1u;
constexpr std::size_t kChunkAlignment = 8;
constexpr std::uint32_t kMaxChildCount = (1u << 24u);

void appendU32LE(std::vector<std::uint8_t>& out, std::uint32_t value) {
    for (unsigned int shift = 0; shift < 32u; shift += 8u) {
        out.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFFu));
    }
}

void appendU64LE(std::vector<std::uint8_t>& out, std::uint64_t value) {
    for (unsigned int shift = 0; shift < 64u; shift += 8u) {
        out.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFFu));
    }
}

void appendF64LE(std::vector<std::uint8_t>& out, double value) {
    appendU64LE(out, std::bit_cast<std::uint64_t>(value));
}

void writeU16LEAt(std::vector<std::uint8_t>& out, std::size_t offset, std::uint16_t value) {
    out[offset] = static_cast<std::uint8_t>(value & 0xFFu);
    out[offset + 1] = static_cast<std::uint8_t>((value >> 8u) & 0xFFu);
}

void writeU32LEAt(std::vector<std::uint8_t>& out, std::size_t offset, std::uint32_t value) {
    for (unsigned int shift = 0; shift < 32u; shift += 8u) {
        out[offset + (shift / 8u)] = static_cast<std::uint8_t>((value >> shift) & 0xFFu);
    }
}

void writeU64LEAt(std::vector<std::uint8_t>& out, std::size_t offset, std::uint64_t value) {
    for (unsigned int shift = 0; shift < 64u; shift += 8u) {
        out[offset + (shift / 8u)] = static_cast<std::uint8_t>((value >> shift) & 0xFFu);
    }
}

std::uint16_t readU16LE(const std::uint8_t* p) noexcept {
    return static_cast<std::uint16_t>(static_cast<std::uint16_t>(p[0]) |
                                      static_cast<std::uint16_t>(static_cast<std::uint16_t>(p[1]) << 8u));
}

std::uint32_t readU32LE(const std::uint8_t* p) noexcept {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8u) |
           (static_cast<std::uint32_t>(p[2]) << 16u) | (static_cast<std::uint32_t>(p[3]) << 24u);
}

std::uint64_t readU64LE(const std::uint8_t* p) noexcept {
    std::uint64_t value = 0;
    for (unsigned int i = 0; i < 8u; ++i) {
        value |= static_cast<std::uint64_t>(p[i]) << (8u * i);
    }
    return value;
}

double readF64LE(const std::uint8_t* p) noexcept {
    return std::bit_cast<double>(readU64LE(p));
}

std::uint64_t fnv1a64(const std::uint8_t* data, std::size_t length) noexcept {
    std::uint64_t hash = 0xCBF29CE484222325ull;
    for (std::size_t i = 0; i < length; ++i) {
        hash ^= static_cast<std::uint64_t>(data[i]);
        hash *= 0x100000001B3ull;
    }
    return hash;
}

std::size_t paddedSize(std::size_t length) noexcept {
    const std::size_t remainder = length % kChunkAlignment;
    return remainder == 0 ? length : length + (kChunkAlignment - remainder);
}

// --- node stream writer ---------------------------------------------------

class NodeStreamWriter {
public:
    [[nodiscard]] core::Status writeRoot(const JsonValue& root) {
        return writeValue(root, 0);
    }

    [[nodiscard]] const std::vector<std::string>& symbols() const noexcept { return symbols_; }
    [[nodiscard]] const std::vector<std::uint8_t>& nodes() const noexcept { return nodes_; }

private:
    [[nodiscard]] core::Status writeValue(const JsonValue& value, std::size_t depth) {
        if (depth > kBinaryProjectMaxDepth) {
            return core::Status::fail(core::ErrorCode::ValidationError,
                                      "Project document nesting exceeds the binary container depth limit",
                                      "binary_project");
        }
        if (value.isNull()) {
            nodes_.push_back(kTagNull);
            return core::Status::ok();
        }
        if (value.isBool()) {
            nodes_.push_back(value.asBool() ? kTagTrue : kTagFalse);
            return core::Status::ok();
        }
        if (value.isNumber()) {
            nodes_.push_back(kTagNumber);
            appendF64LE(nodes_, value.asNumber());
            return core::Status::ok();
        }
        if (value.isString()) {
            const std::string& text = value.asString();
            nodes_.push_back(kTagString);
            appendU32LE(nodes_, static_cast<std::uint32_t>(text.size()));
            nodes_.insert(nodes_.end(), text.begin(), text.end());
            return core::Status::ok();
        }
        if (value.isArray()) {
            const JsonArray& items = value.asArray();
            nodes_.push_back(kTagArray);
            appendU32LE(nodes_, static_cast<std::uint32_t>(items.size()));
            for (const JsonValue& item : items) {
                auto status = writeValue(item, depth + 1);
                if (!status) {
                    return status;
                }
            }
            return core::Status::ok();
        }

        const JsonObject& members = value.asObject();
        nodes_.push_back(kTagObject);
        appendU32LE(nodes_, static_cast<std::uint32_t>(members.size()));
        for (const auto& member : members) {
            auto index = internSymbol(member.first);
            if (!index) {
                return index.error();
            }
            appendU32LE(nodes_, index.value());
            auto status = writeValue(member.second, depth + 1);
            if (!status) {
                return status;
            }
        }
        return core::Status::ok();
    }

    [[nodiscard]] core::Result<std::uint32_t> internSymbol(const std::string& key) {
        const auto found = symbol_index_.find(key);
        if (found != symbol_index_.end()) {
            return core::Result<std::uint32_t>::ok(found->second);
        }
        if (symbols_.size() >= kBinaryProjectMaxSymbols) {
            return core::Result<std::uint32_t>::fail(core::ErrorCode::ValidationError,
                                                     "Project document uses more distinct keys than the binary container allows",
                                                     "binary_project");
        }
        const auto index = static_cast<std::uint32_t>(symbols_.size());
        symbols_.push_back(key);
        symbol_index_.emplace(key, index);
        return core::Result<std::uint32_t>::ok(index);
    }

    std::vector<std::string> symbols_{};
    std::unordered_map<std::string, std::uint32_t> symbol_index_{};
    std::vector<std::uint8_t> nodes_{};
};

// --- node stream reader ---------------------------------------------------

class NodeStreamReader {
public:
    NodeStreamReader(const std::uint8_t* data, std::size_t length) noexcept
        : data_(data), length_(length) {}

    [[nodiscard]] core::Result<JsonValue> readRoot() {
        auto symbols = readSymbols();
        if (!symbols) {
            return symbols.error();
        }
        symbols_ = std::move(symbols).value();
        return readValue(0);
    }

    [[nodiscard]] bool atEnd() const noexcept { return offset_ == length_; }

private:
    [[nodiscard]] bool remaining(std::size_t count) const noexcept {
        return count <= length_ - offset_;
    }

    [[nodiscard]] core::Result<std::uint32_t> readU32() {
        if (!remaining(4)) {
            return core::Result<std::uint32_t>::fail(core::ErrorCode::ParseError,
                                                     "Truncated binary project node stream",
                                                     "binary_project");
        }
        const auto value = readU32LE(data_ + offset_);
        offset_ += 4;
        return core::Result<std::uint32_t>::ok(value);
    }

    [[nodiscard]] core::Result<std::vector<std::string>> readSymbols() {
        auto count_res = readU32();
        if (!count_res) {
            return count_res.error();
        }
        const auto count = static_cast<std::size_t>(count_res.value());
        if (count > kBinaryProjectMaxSymbols) {
            return core::Result<std::vector<std::string>>::fail(
                core::ErrorCode::ValidationError, "Binary project symbol table exceeds the supported size", "binary_project");
        }
        std::vector<std::string> symbols;
        symbols.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            auto length_res = readU32();
            if (!length_res) {
                return length_res.error();
            }
            const auto byte_length = static_cast<std::size_t>(length_res.value());
            if (!remaining(byte_length)) {
                return core::Result<std::vector<std::string>>::fail(core::ErrorCode::ParseError,
                                                                    "Truncated binary project symbol name", "binary_project");
            }
            symbols.emplace_back(reinterpret_cast<const char*>(data_ + offset_), byte_length);
            offset_ += byte_length;
        }
        return core::Result<std::vector<std::string>>::ok(std::move(symbols));
    }

    [[nodiscard]] core::Result<std::string> readString() {
        auto length_res = readU32();
        if (!length_res) {
            return length_res.error();
        }
        const auto byte_length = static_cast<std::size_t>(length_res.value());
        if (!remaining(byte_length)) {
            return core::Result<std::string>::fail(core::ErrorCode::ParseError,
                                                   "Truncated binary project string value", "binary_project");
        }
        std::string text(reinterpret_cast<const char*>(data_ + offset_), byte_length);
        offset_ += byte_length;
        return core::Result<std::string>::ok(std::move(text));
    }

    [[nodiscard]] core::Result<JsonValue> readValue(std::size_t depth) {
        if (depth > kBinaryProjectMaxDepth) {
            return core::Result<JsonValue>::fail(core::ErrorCode::ValidationError,
                                                 "Binary project node stream is nested too deeply", "binary_project");
        }
        if (!remaining(1)) {
            return core::Result<JsonValue>::fail(core::ErrorCode::ParseError,
                                                 "Truncated binary project node stream", "binary_project");
        }
        const auto tag = data_[offset_];
        offset_ += 1;
        switch (tag) {
            case kTagNull:
                return core::Result<JsonValue>::ok(JsonValue{nullptr});
            case kTagFalse:
                return core::Result<JsonValue>::ok(JsonValue{false});
            case kTagTrue:
                return core::Result<JsonValue>::ok(JsonValue{true});
            case kTagNumber: {
                if (!remaining(8)) {
                    return core::Result<JsonValue>::fail(core::ErrorCode::ParseError,
                                                         "Truncated binary project number", "binary_project");
                }
                const double number = readF64LE(data_ + offset_);
                offset_ += 8;
                return core::Result<JsonValue>::ok(JsonValue{number});
            }
            case kTagString: {
                auto text_res = readString();
                if (!text_res) {
                    return text_res.error();
                }
                return core::Result<JsonValue>::ok(JsonValue{std::move(text_res).value()});
            }
            case kTagArray: {
                auto count_res = readU32();
                if (!count_res) {
                    return count_res.error();
                }
                if (count_res.value() > kMaxChildCount) {
                    return core::Result<JsonValue>::fail(core::ErrorCode::ValidationError,
                                                         "Binary project array is implausibly large", "binary_project");
                }
                JsonArray items;
                items.reserve(static_cast<std::size_t>(count_res.value()));
                for (std::uint32_t i = 0; i < count_res.value(); ++i) {
                    auto item_res = readValue(depth + 1);
                    if (!item_res) {
                        return item_res.error();
                    }
                    items.push_back(std::move(item_res).value());
                }
                return core::Result<JsonValue>::ok(JsonValue{std::move(items)});
            }
            case kTagObject: {
                auto count_res = readU32();
                if (!count_res) {
                    return count_res.error();
                }
                if (count_res.value() > kMaxChildCount) {
                    return core::Result<JsonValue>::fail(core::ErrorCode::ValidationError,
                                                         "Binary project object is implausibly large", "binary_project");
                }
                JsonObject members;
                members.reserve(static_cast<std::size_t>(count_res.value()));
                for (std::uint32_t i = 0; i < count_res.value(); ++i) {
                    auto index_res = readU32();
                    if (!index_res) {
                        return index_res.error();
                    }
                    const auto symbol_index = static_cast<std::size_t>(index_res.value());
                    if (symbol_index >= symbols_.size()) {
                        return core::Result<JsonValue>::fail(core::ErrorCode::ValidationError,
                                                             "Binary project object key references an unknown symbol", "binary_project");
                    }
                    auto member_res = readValue(depth + 1);
                    if (!member_res) {
                        return member_res.error();
                    }
                    members.emplace_back(symbols_[symbol_index], std::move(member_res).value());
                }
                return core::Result<JsonValue>::ok(JsonValue{std::move(members)});
            }
            default:
                return core::Result<JsonValue>::fail(core::ErrorCode::ParseError,
                                                     "Binary project node stream carries an unknown value tag", "binary_project");
        }
    }

    const std::uint8_t* data_{nullptr};
    std::size_t length_{0};
    std::size_t offset_{0};
    std::vector<std::string> symbols_{};
};

// --- chunk helpers --------------------------------------------------------

struct ChunkView {
    std::uint32_t type{0};
    std::uint64_t content_hash{0};
    std::size_t offset{0};
    std::size_t length{0};
};

[[nodiscard]] core::Result<std::vector<std::uint8_t>> encodeNodeStream(const JsonValue& document) {
    NodeStreamWriter writer;
    auto status = writer.writeRoot(document);
    if (!status) {
        return status.error();
    }

    std::vector<std::uint8_t> payload;
    payload.reserve(writer.nodes().size() + 64);
    appendU32LE(payload, static_cast<std::uint32_t>(writer.symbols().size()));
    for (const std::string& symbol : writer.symbols()) {
        appendU32LE(payload, static_cast<std::uint32_t>(symbol.size()));
        payload.insert(payload.end(), symbol.begin(), symbol.end());
    }
    payload.insert(payload.end(), writer.nodes().begin(), writer.nodes().end());
    return core::Result<std::vector<std::uint8_t>>::ok(std::move(payload));
}

[[nodiscard]] core::Status validateGeometryArchive(const std::vector<std::uint8_t>& bytes) {
    if (bytes.size() < kBinaryProjectHeaderSize) {
        return core::Status::fail(core::ErrorCode::ValidationError,
                                  "Embedded geometry archive is smaller than a container header", "binary_project");
    }
    const std::array<std::uint8_t, 8> expected{'F', 'E', '3', 'D', 'B', 'I', 'N', 0x00};
    for (std::size_t i = 0; i < expected.size(); ++i) {
        if (bytes[i] != expected[i]) {
            return core::Status::fail(core::ErrorCode::ValidationError,
                                      "Embedded geometry archive is not a Final Effect 3D .fe3dbin image", "binary_project");
        }
    }
    return core::Status::ok();
}

} // namespace

bool BinaryProjectCodec::isBinaryProjectImage(std::span<const std::uint8_t> bytes) noexcept {
    if (bytes.size() < kBinaryProjectMagic.size()) {
        return false;
    }
    for (std::size_t i = 0; i < kBinaryProjectMagic.size(); ++i) {
        if (bytes[i] != kBinaryProjectMagic[i]) {
            return false;
        }
    }
    return true;
}

core::Result<std::vector<std::uint8_t>> BinaryProjectCodec::encode(const BinaryProjectImage& image) {
    auto document_res = encodeNodeStream(image.document);
    if (!document_res) {
        return document_res.error();
    }
    if (!image.geometry_archive.empty()) {
        auto geometry_status = validateGeometryArchive(image.geometry_archive);
        if (!geometry_status) {
            return geometry_status.error();
        }
    }

    struct PendingChunk {
        std::uint32_t type{0};
        const std::vector<std::uint8_t>* payload{nullptr};
    };

    const std::vector<std::uint8_t> document_payload = std::move(document_res).value();
    std::vector<PendingChunk> pending;
    pending.push_back(PendingChunk{kChunkTypeDocument, &document_payload});
    if (!image.geometry_archive.empty()) {
        pending.push_back(PendingChunk{kChunkTypeGeometry, &image.geometry_archive});
    }

    std::vector<std::uint8_t> out;
    out.resize(kBinaryProjectHeaderSize, 0);

    struct DirectoryEntry {
        std::uint64_t content_hash{0};
        std::uint32_t type{0};
        std::uint64_t offset{0};
        std::uint64_t length{0};
        std::uint32_t crc32{0};
    };
    std::vector<DirectoryEntry> entries;
    entries.reserve(pending.size());

    for (const PendingChunk& chunk : pending) {
        const std::vector<std::uint8_t>& payload = *chunk.payload;
        DirectoryEntry entry;
        entry.type = chunk.type;
        entry.offset = static_cast<std::uint64_t>(out.size());
        entry.length = static_cast<std::uint64_t>(payload.size());
        entry.content_hash = fnv1a64(payload.data(), payload.size());
        entry.crc32 = crc32Ieee(std::span<const std::uint8_t>(payload.data(), payload.size()));
        out.insert(out.end(), payload.begin(), payload.end());
        const std::size_t pad = paddedSize(payload.size()) - payload.size();
        out.insert(out.end(), pad, 0);
        entries.push_back(entry);
    }

    const std::uint64_t directory_offset = static_cast<std::uint64_t>(out.size());
    std::vector<std::uint8_t> directory;
    directory.reserve(entries.size() * kBinaryProjectDirectoryEntrySize);
    for (const DirectoryEntry& entry : entries) {
        appendU64LE(directory, entry.content_hash);
        appendU32LE(directory, entry.type);
        appendU32LE(directory, 0u);
        appendU64LE(directory, entry.offset);
        appendU64LE(directory, entry.length);
        appendU32LE(directory, entry.crc32);
        appendU32LE(directory, 0u);
    }
    const std::uint32_t directory_crc = crc32Ieee(std::span<const std::uint8_t>(directory.data(), directory.size()));
    out.insert(out.end(), directory.begin(), directory.end());

    const std::uint32_t flags = image.geometry_archive.empty() ? 0u : kFlagHasGeometry;
    for (std::size_t i = 0; i < kBinaryProjectMagic.size(); ++i) {
        out[i] = kBinaryProjectMagic[i];
    }
    writeU16LEAt(out, 8, kBinaryProjectMajor);
    writeU16LEAt(out, 10, kBinaryProjectMinor);
    writeU32LEAt(out, 12, static_cast<std::uint32_t>(entries.size()));
    writeU64LEAt(out, 16, directory_offset);
    writeU32LEAt(out, 24, directory_crc);
    writeU32LEAt(out, 28, flags);

    return core::Result<std::vector<std::uint8_t>>::ok(std::move(out));
}

core::Result<BinaryProjectImage> BinaryProjectCodec::decode(std::span<const std::uint8_t> bytes) {
    if (!isBinaryProjectImage(bytes)) {
        return core::Result<BinaryProjectImage>::fail(
            core::ErrorCode::ValidationError,
            "Not a Final Effect 3D binary project container (expected magic FE3DPRJ)",
            "binary_project");
    }
    if (bytes.size() < kBinaryProjectHeaderSize) {
        return core::Result<BinaryProjectImage>::fail(core::ErrorCode::ParseError,
                                                      "Binary project container is truncated before its header", "binary_project");
    }
    const std::uint16_t major = readU16LE(bytes.data() + 8);
    if (major > kBinaryProjectMajor) {
        return core::Result<BinaryProjectImage>::fail(
            core::ErrorCode::UnsupportedVersion,
            "Binary project container major version is newer than this build supports",
            "binary_project");
    }
    const std::uint32_t chunk_count = readU32LE(bytes.data() + 12);
    const std::uint64_t directory_offset = readU64LE(bytes.data() + 16);
    const std::uint32_t directory_crc = readU32LE(bytes.data() + 24);
    const std::uint32_t flags = readU32LE(bytes.data() + 28);

    if (chunk_count == 0 || chunk_count > kBinaryProjectMaxChunks) {
        return core::Result<BinaryProjectImage>::fail(core::ErrorCode::ValidationError,
                                                      "Binary project container declares an invalid chunk count", "binary_project");
    }
    if (directory_offset < kBinaryProjectHeaderSize || directory_offset > bytes.size()) {
        return core::Result<BinaryProjectImage>::fail(core::ErrorCode::ValidationError,
                                                      "Binary project container directory offset is out of bounds", "binary_project");
    }
    const std::size_t directory_bytes = static_cast<std::size_t>(chunk_count) * kBinaryProjectDirectoryEntrySize;
    if (directory_bytes > bytes.size() - static_cast<std::size_t>(directory_offset)) {
        return core::Result<BinaryProjectImage>::fail(core::ErrorCode::ParseError,
                                                      "Binary project container directory is truncated", "binary_project");
    }
    const auto* directory = bytes.data() + static_cast<std::size_t>(directory_offset);
    if (crc32Ieee(std::span<const std::uint8_t>(directory, directory_bytes)) != directory_crc) {
        return core::Result<BinaryProjectImage>::fail(core::ErrorCode::ValidationError,
                                                      "Binary project directory failed its CRC-32 check", "binary_project");
    }

    std::vector<ChunkView> chunks;
    chunks.reserve(chunk_count);
    for (std::uint32_t i = 0; i < chunk_count; ++i) {
        const std::uint8_t* entry = directory + (static_cast<std::size_t>(i) * kBinaryProjectDirectoryEntrySize);
        ChunkView view;
        view.content_hash = readU64LE(entry);
        view.type = readU32LE(entry + 8);
        const std::uint64_t offset = readU64LE(entry + 16);
        const std::uint64_t length = readU64LE(entry + 24);
        const std::uint32_t crc = readU32LE(entry + 32);
        if (offset < kBinaryProjectHeaderSize || length > bytes.size() ||
            offset > static_cast<std::uint64_t>(directory_offset) ||
            length > static_cast<std::uint64_t>(directory_offset) - offset) {
            return core::Result<BinaryProjectImage>::fail(core::ErrorCode::ValidationError,
                                                          "Binary project chunk lies outside the payload region", "binary_project");
        }
        view.offset = static_cast<std::size_t>(offset);
        view.length = static_cast<std::size_t>(length);
        const std::span<const std::uint8_t> payload(bytes.data() + view.offset, view.length);
        if (crc32Ieee(payload) != crc) {
            return core::Result<BinaryProjectImage>::fail(core::ErrorCode::ValidationError,
                                                          "Binary project chunk failed its CRC-32 check", "binary_project");
        }
        if (fnv1a64(payload.data(), payload.size()) != view.content_hash) {
            return core::Result<BinaryProjectImage>::fail(core::ErrorCode::ValidationError,
                                                          "Binary project chunk content hash mismatch", "binary_project");
        }
        chunks.push_back(view);
    }

    (void)flags;
    BinaryProjectImage image;
    bool have_document = false;
    for (const ChunkView& chunk : chunks) {
        if (chunk.type == kChunkTypeDocument) {
            if (have_document) {
                return core::Result<BinaryProjectImage>::fail(core::ErrorCode::ValidationError,
                                                              "Binary project container carries more than one document chunk", "binary_project");
            }
            NodeStreamReader reader(bytes.data() + chunk.offset, chunk.length);
            auto root_res = reader.readRoot();
            if (!root_res) {
                return root_res.error();
            }
            if (!reader.atEnd()) {
                return core::Result<BinaryProjectImage>::fail(core::ErrorCode::ValidationError,
                                                              "Binary project document chunk has trailing bytes", "binary_project");
            }
            image.document = std::move(root_res).value();
            have_document = true;
        } else if (chunk.type == kChunkTypeGeometry) {
            image.geometry_archive.assign(bytes.begin() + static_cast<std::ptrdiff_t>(chunk.offset),
                                          bytes.begin() + static_cast<std::ptrdiff_t>(chunk.offset + chunk.length));
        } else {
            return core::Result<BinaryProjectImage>::fail(core::ErrorCode::ValidationError,
                                                          "Binary project container carries an unknown chunk type", "binary_project");
        }
    }
    if (!have_document) {
        return core::Result<BinaryProjectImage>::fail(core::ErrorCode::ValidationError,
                                                      "Binary project container is missing its document chunk", "binary_project");
    }
    return core::Result<BinaryProjectImage>::ok(std::move(image));
}

} // namespace finaleffect::project
