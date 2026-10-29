// SPDX-License-Identifier: GPL-3.0-only
// Windows path resolution (%APPDATA% / %TEMP% based).
#include "finaleffect/platform/Paths.hpp"

#include <cstdlib>
#include <system_error>
#include <utility>

#include <filesystem>

namespace fs = std::filesystem;

namespace fe::platform {
namespace {

std::string environmentOrEmpty(const char* name) {
    char* value = nullptr;
    std::size_t size = 0;
    if (_dupenv_s(&value, &size, name) != 0 || value == nullptr) {
        return std::string();
    }
    std::string result(value);
    std::free(value);
    return result;
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
    if (result.back() != '\\' && result.back() != '/') {
        result.push_back('\\');
    }
    result.append(leaf);
    return result;
}

core::Result<AppPaths> resolveAppPaths(std::string_view organizationName, std::string_view applicationId) {
    AppPaths paths;
    std::error_code errorCode;
    const fs::path current = fs::current_path(errorCode);
    paths.workingDirectory = errorCode ? std::string(".") : current.string();

    fs::path roaming = environmentOrEmpty("APPDATA");
    if (roaming.empty()) {
        roaming = fs::path(paths.workingDirectory);
    }
    const fs::path userData = roaming / std::string(organizationName) / std::string(applicationId);
    paths.userDataDirectory = userData.string();
    paths.logDirectory = (userData / "logs").string();

    std::string temp = environmentOrEmpty("TEMP");
    if (temp.empty()) {
        temp = environmentOrEmpty("TMP");
    }
    if (temp.empty()) {
        temp = paths.workingDirectory;
    }
    paths.tempDirectory = (fs::path(temp) / std::string(applicationId)).string();

    return paths;
}

}  // namespace fe::platform
