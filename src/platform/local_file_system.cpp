// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/platform/file_system.hpp"

#include <fstream>
#include <sstream>
#include <system_error>

namespace finaleffect::platform {

bool LocalFileSystem::exists(const std::filesystem::path& path) const {
    std::error_code ec;
    return std::filesystem::exists(path, ec) && !ec;
}

core::Status LocalFileSystem::createDirectories(const std::filesystem::path& path) {
    if (path.empty()) {
        return core::Status::ok();
    }
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
    if (ec) {
        return core::Status::fail(
            core::ErrorCode::IoError,
            "Failed to create directories: " + ec.message(),
            path.string());
    }
    return core::Status::ok();
}

core::Result<std::string> LocalFileSystem::readTextFile(const std::filesystem::path& path) const {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || ec) {
        return core::Result<std::string>::fail(
            core::ErrorCode::NotFound,
            "File does not exist",
            path.string());
    }
    if (std::filesystem::is_directory(path, ec)) {
        return core::Result<std::string>::fail(
            core::ErrorCode::IoError,
            "Path refers to a directory, not a file",
            path.string());
    }

    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        return core::Result<std::string>::fail(
            core::ErrorCode::IoError,
            "Unable to open file for reading",
            path.string());
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    if (file.bad()) {
        return core::Result<std::string>::fail(
            core::ErrorCode::IoError,
            "I/O failure while reading text file",
            path.string());
    }
    return core::Result<std::string>::ok(buffer.str());
}

core::Status LocalFileSystem::writeTextFile(const std::filesystem::path& path, std::string_view content) {
    if (path.empty()) {
        return core::Status::fail(core::ErrorCode::InvalidArgument, "Target file path cannot be empty");
    }

    const auto parent = path.parent_path();
    if (!parent.empty()) {
        auto dir_status = createDirectories(parent);
        if (!dir_status) {
            return dir_status;
        }
    }

    std::ofstream file(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        return core::Status::fail(
            core::ErrorCode::IoError,
            "Unable to open file for writing",
            path.string());
    }

    file.write(content.data(), static_cast<std::streamsize>(content.size()));
    file.flush();
    if (!file.good()) {
        return core::Status::fail(
            core::ErrorCode::IoError,
            "I/O failure while writing text file",
            path.string());
    }
    return core::Status::ok();
}

core::Result<std::vector<std::uint8_t>> LocalFileSystem::readBinaryFile(
    const std::filesystem::path& path) const {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || ec) {
        return core::Result<std::vector<std::uint8_t>>::fail(
            core::ErrorCode::NotFound,
            "File does not exist",
            path.string());
    }

    const auto file_size = std::filesystem::file_size(path, ec);
    if (ec) {
        return core::Result<std::vector<std::uint8_t>>::fail(
            core::ErrorCode::IoError,
            "Unable to query file size: " + ec.message(),
            path.string());
    }

    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        return core::Result<std::vector<std::uint8_t>>::fail(
            core::ErrorCode::IoError,
            "Unable to open binary file for reading",
            path.string());
    }

    std::vector<std::uint8_t> data(static_cast<std::size_t>(file_size));
    if (!data.empty()) {
        file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
        if (!file) {
            return core::Result<std::vector<std::uint8_t>>::fail(
                core::ErrorCode::IoError,
                "Failed to read complete binary file",
                path.string());
        }
    }
    return core::Result<std::vector<std::uint8_t>>::ok(std::move(data));
}

core::Status LocalFileSystem::writeBinaryFile(
    const std::filesystem::path& path,
    const std::vector<std::uint8_t>& bytes) {
    if (path.empty()) {
        return core::Status::fail(core::ErrorCode::InvalidArgument, "Target file path cannot be empty");
    }

    const auto parent = path.parent_path();
    if (!parent.empty()) {
        auto dir_status = createDirectories(parent);
        if (!dir_status) {
            return dir_status;
        }
    }

    std::ofstream file(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        return core::Status::fail(
            core::ErrorCode::IoError,
            "Unable to open binary file for writing",
            path.string());
    }

    if (!bytes.empty()) {
        file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    file.flush();
    if (!file.good()) {
        return core::Status::fail(
            core::ErrorCode::IoError,
            "I/O failure while writing binary file",
            path.string());
    }
    return core::Status::ok();
}

} // namespace finaleffect::platform
