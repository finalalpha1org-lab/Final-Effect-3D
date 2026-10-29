// SPDX-License-Identifier: GPL-3.0-only
#include "finaleffect/platform/FileSystem.hpp"

#include <fstream>
#include <sstream>
#include <system_error>
#include <utility>

#include <filesystem>

namespace fs = std::filesystem;

namespace fe::platform {
namespace {

core::Error ioError(const std::string& message, const std::string& path) {
    return core::makeError(core::ErrorCode::IoFailure, message, path);
}

core::Status writeAll(const fs::path& path, std::string_view contents) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        return ioError("cannot open file for writing", path.string());
    }
    file.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    if (!file) {
        return ioError("failed while writing file", path.string());
    }
    file.close();
    return core::successStatus();
}

}  // namespace

FileSystem::~FileSystem() = default;

bool StdFileSystem::exists(const std::string& path) const {
    std::error_code errorCode;
    return fs::exists(fs::path(path), errorCode);
}

bool StdFileSystem::isDirectory(const std::string& path) const {
    std::error_code errorCode;
    return fs::is_directory(fs::path(path), errorCode);
}

core::Result<std::string> StdFileSystem::readTextFile(const std::string& path) const {
    std::ifstream file(fs::path(path), std::ios::binary);
    if (!file.is_open()) {
        return ioError("cannot open file for reading", path);
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    if (file.bad()) {
        return ioError("failed while reading file", path);
    }
    return buffer.str();
}

core::Status StdFileSystem::writeTextFile(const std::string& path, std::string_view contents) {
    return writeAll(fs::path(path), contents);
}

core::Status StdFileSystem::writeTextFileAtomic(const std::string& path, std::string_view contents) {
    const fs::path destination(path);
    fs::path temporary = destination;
    temporary += ".tmp";

    const core::Status writeStatus = writeAll(temporary, contents);
    if (!writeStatus.ok()) {
        return writeStatus;
    }

    std::error_code errorCode;
    fs::rename(temporary, destination, errorCode);
    if (errorCode) {
        // Windows refuses to rename over an existing file; remove and retry once.
        std::error_code removeCode;
        fs::remove(destination, removeCode);
        errorCode.clear();
        fs::rename(temporary, destination, errorCode);
        if (errorCode) {
            std::error_code cleanupCode;
            fs::remove(temporary, cleanupCode);
            return ioError("cannot replace destination file: " + errorCode.message(), path);
        }
    }
    return core::successStatus();
}

core::Status StdFileSystem::createDirectories(const std::string& path) {
    if (path.empty()) {
        return core::makeError(core::ErrorCode::InvalidArgument, "directory path is empty", "file system");
    }
    std::error_code errorCode;
    fs::create_directories(fs::path(path), errorCode);
    if (errorCode && !fs::is_directory(fs::path(path))) {
        return ioError("cannot create directory: " + errorCode.message(), path);
    }
    return core::successStatus();
}

core::Status StdFileSystem::removeFile(const std::string& path) {
    std::error_code errorCode;
    const bool removed = fs::remove(fs::path(path), errorCode);
    if (errorCode) {
        return ioError("cannot remove file: " + errorCode.message(), path);
    }
    if (!removed) {
        return core::makeError(core::ErrorCode::NotFound, "file does not exist", path);
    }
    return core::successStatus();
}

core::Result<std::vector<std::string>> StdFileSystem::listDirectory(const std::string& path) const {
    std::error_code errorCode;
    fs::directory_iterator iterator(fs::path(path), errorCode);
    if (errorCode) {
        return ioError("cannot open directory: " + errorCode.message(), path);
    }
    std::vector<std::string> entries;
    for (const fs::directory_entry& entry : iterator) {
        entries.push_back(entry.path().filename().string());
    }
    return entries;
}

}  // namespace fe::platform
