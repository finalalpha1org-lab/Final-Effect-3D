// Final Effect 3D - core version information.
// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Final Effect 3D. See LICENSE and NOTICE at the repository root.
#pragma once

#include <string_view>

namespace fe::core {

/// Numeric version of the engine/toolchain ABI described by this source tree.
inline constexpr int kVersionMajor = 0;
inline constexpr int kVersionMinor = 1;
inline constexpr int kVersionPatch = 0;

/// Human readable product name. Never used as an internal identifier.
inline constexpr std::string_view kProductName = "Final Effect 3D";

/// Stable, lowercase identifier used in file formats and directories.
inline constexpr std::string_view kProductId = "finaleffect3d";

/// Semantic version string, for example "0.1.0".
std::string_view versionString() noexcept;

/// Full version string, for example "0.1.0+stage1".
std::string_view fullVersionString() noexcept;

/// Build identification: compiler, standard, build type, platform. Useful in logs.
std::string_view buildInfo() noexcept;

}  // namespace fe::core
