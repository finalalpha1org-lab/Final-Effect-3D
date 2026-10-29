// Final Effect 3D - application paths.
// SPDX-License-Identifier: GPL-3.0-only
//
// resolveAppPaths() is the single place where platform differences leak into the
// engine, and it is implemented once per platform under src/platform/<platform>/.
#pragma once

#include <string>
#include <string_view>

#include "finaleffect/core/Error.hpp"

namespace fe::platform {

struct AppPaths {
    /// Directory the process was started in.
    std::string workingDirectory;
    /// Per-user writable location for preferences, caches and recent files.
    std::string userDataDirectory;
    /// Scratch space, cleaned up by the tooling rather than by the engine.
    std::string tempDirectory;
    /// Directory used for log files; a subdirectory of userDataDirectory.
    std::string logDirectory;
};

/// Implemented per platform:
///   src/platform/posix/PosixPaths.cpp      (Linux, macOS, and future POSIX hosts)
///   src/platform/windows/WindowsPaths.cpp  (Windows)
core::Result<AppPaths> resolveAppPaths(std::string_view organizationName, std::string_view applicationId);

/// Joins two path fragments with the platform separator.
std::string joinPath(std::string_view base, std::string_view leaf);

}  // namespace fe::platform
