// SPDX-License-Identifier: GPL-3.0-only
#include "finaleffect/core/Version.hpp"

#include <string>

namespace fe::core {
namespace {

std::string makeVersionString() {
    return std::to_string(kVersionMajor) + "." + std::to_string(kVersionMinor) + "." +
           std::to_string(kVersionPatch);
}

std::string makeFullVersionString() {
    return makeVersionString() + "+stage1";
}

std::string makeBuildInfo() {
    std::string info;
    info += std::string(kProductName);
    info += " ";
    info += makeFullVersionString();

#if defined(__clang__)
    info += " | compiler: clang " __clang_version__;
#elif defined(__GNUC__)
    info += " | compiler: gcc " __VERSION__;
#elif defined(_MSC_VER)
    info += " | compiler: msvc " + std::to_string(_MSC_VER);
#else
    info += " | compiler: unknown";
#endif

    info += " | C++ standard: ";
#if defined(_MSVC_LANG)
    info += std::to_string(_MSVC_LANG);
#else
    info += std::to_string(__cplusplus);
#endif

#if defined(NDEBUG)
    info += " | build: release";
#else
    info += " | build: debug";
#endif

#if defined(_WIN32)
    info += " | platform: windows";
#elif defined(__APPLE__)
    info += " | platform: macos";
#elif defined(__linux__)
    info += " | platform: linux";
#elif defined(__EMSCRIPTEN__)
    info += " | platform: emscripten";
#else
    info += " | platform: unknown";
#endif

    return info;
}

}  // namespace

std::string_view versionString() noexcept {
    static const std::string value = makeVersionString();
    return value;
}

std::string_view fullVersionString() noexcept {
    static const std::string value = makeFullVersionString();
    return value;
}

std::string_view buildInfo() noexcept {
    static const std::string value = makeBuildInfo();
    return value;
}

}  // namespace fe::core
