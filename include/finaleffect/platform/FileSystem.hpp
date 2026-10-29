// Final Effect 3D - file access service.
// SPDX-License-Identifier: GPL-3.0-only
//
// The engine never calls <filesystem> directly outside this module, so tests can
// substitute an in-memory or failure-injecting implementation, and future
// sandboxed targets (browser, mobile) can provide their own backend.
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "finaleffect/core/Error.hpp"

namespace fe::platform {

class FileSystem {
public:
    FileSystem() = default;
    virtual ~FileSystem();
    FileSystem(const FileSystem&) = delete;
    FileSystem& operator=(const FileSystem&) = delete;

    virtual bool exists(const std::string& path) const = 0;
    virtual bool isDirectory(const std::string& path) const = 0;

    virtual core::Result<std::string> readTextFile(const std::string& path) const = 0;
    virtual core::Status writeTextFile(const std::string& path, std::string_view contents) = 0;

    /// Writes to a sibling temporary file and renames it over the destination, so a
    /// crash cannot leave a half written project behind.
    virtual core::Status writeTextFileAtomic(const std::string& path, std::string_view contents) = 0;

    virtual core::Status createDirectories(const std::string& path) = 0;
    virtual core::Status removeFile(const std::string& path) = 0;
    virtual core::Result<std::vector<std::string>> listDirectory(const std::string& path) const = 0;
};

/// Portable implementation backed by std::filesystem. Used by the desktop host and
/// by the CLI; other hosts can provide their own FileSystem implementation.
class StdFileSystem final : public FileSystem {
public:
    bool exists(const std::string& path) const override;
    bool isDirectory(const std::string& path) const override;
    core::Result<std::string> readTextFile(const std::string& path) const override;
    core::Status writeTextFile(const std::string& path, std::string_view contents) override;
    core::Status writeTextFileAtomic(const std::string& path, std::string_view contents) override;
    core::Status createDirectories(const std::string& path) override;
    core::Status removeFile(const std::string& path) override;
    core::Result<std::vector<std::string>> listDirectory(const std::string& path) const override;
};

}  // namespace fe::platform
