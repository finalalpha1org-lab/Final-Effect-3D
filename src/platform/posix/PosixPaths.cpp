// SPDX-License-Identifier: GPL-3.0-only
// POSIX path resolution: Linux, macOS and future POSIX-based hosts (Android, iOS).
#include "finaleffect/platform/Paths.hpp"

#include <cstdlib>
#include <system_error>
#include <utility>

#include <filesystem>

namespace fs = std::filesystem;

namespace fe::platform {
namespace {

std::string environmentOrEmpty(const char* name) {
    const char* value = std::getenv(name);
    return value == nullptr ? std::string() : std::string(value);
}

fs::path homeDirectory() {
    fs::path home = environmentOrEmpty("HOME");
    if (home.empty()) {
        std::error_code errorCode;
        home = fs::temp_directory_path(errorCode);
        if (errorCode) {
            home = fs::path(".");
        }
    }
    return home;
}

}  // namespace

std::string joinPath(std::string_view base, std::string_view leaf) {
    if (base.empty()) {
        return std::string(leaf);
    }
    if (leaf.empty()) {
        return std::string(base);
    }
    std::string result(base);
    if (result.back() != '/') {
        result.push_back('/');
    }
    result.append(leaf);
    return result;
}

core::Result<AppPaths> resolveAppPaths(std::string_view organizationName, std::string_view applicationId) {
    AppPaths paths;
    std::error_code errorCode;
    const fs::path current = fs::current_path(errorCode);
    paths.workingDirectory = errorCode ? std::string(".") : current.string();

#if defined(__APPLE__)
    const fs::path dataRoot = homeDirectory() / "Library" / "Application Support";
#else
    const fs::path dataRoot = homeDirectory() / ".local" / "share";
#endif

    fs::path userData = dataRoot / std::string(organizationName) / std::string(applicationId);
    paths.userDataDirectory = userData.string();
    paths.logDirectory = (userData / "logs").string();

    const fs::path tempRoot = fs::temp_directory_path(errorCode);
    const fs::path temp = errorCode ? fs::path("/tmp") : tempRoot;
    paths.tempDirectory = (temp / std::string(applicationId)).string();

    return paths;
}

}  // namespace fe::platform
