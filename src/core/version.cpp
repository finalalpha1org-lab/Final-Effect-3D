// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/core/version.hpp"

namespace finaleffect::core {

std::string Version::toString() const {
    return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
}

std::string_view engineName() noexcept {
    return "Final Effect 3D";
}

std::string_view engineVersionString() noexcept {
    return "0.4.0";
}

} // namespace finaleffect::core
