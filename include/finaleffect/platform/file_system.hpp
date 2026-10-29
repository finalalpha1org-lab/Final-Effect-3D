// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/core/result.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace finaleffect::platform {

class IFileSystem {
public:
    virtual ~IFileSystem() = default;

    [[nodiscard]] virtual bool exists(const std::filesystem::path& path) const = 0;
    [[nodiscard]] virtual core::Status createDirectories(const std::filesystem::path& path) = 0;
    [[nodiscard]] virtual core::Result<std::string> readTextFile(const std::filesystem::path& path) const = 0;
    [[nodiscard]] virtual core::Status writeTextFile(const std::filesystem::path& path, std::string_view content) = 0;
    [[nodiscard]] virtual core::Result<std::vector<std::uint8_t>> readBinaryFile(const std::filesystem::path& path) const = 0;
    [[nodiscard]] virtual core::Status writeBinaryFile(
        const std::filesystem::path& path,
        const std::vector<std::uint8_t>& bytes) = 0;
};

class LocalFileSystem final : public IFileSystem {
public:
    [[nodiscard]] bool exists(const std::filesystem::path& path) const override;
    [[nodiscard]] core::Status createDirectories(const std::filesystem::path& path) override;
    [[nodiscard]] core::Result<std::string> readTextFile(const std::filesystem::path& path) const override;
    [[nodiscard]] core::Status writeTextFile(const std::filesystem::path& path, std::string_view content) override;
    [[nodiscard]] core::Result<std::vector<std::uint8_t>> readBinaryFile(const std::filesystem::path& path) const override;
    [[nodiscard]] core::Status writeBinaryFile(
        const std::filesystem::path& path,
        const std::vector<std::uint8_t>& bytes) override;
};

} // namespace finaleffect::platform
