// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/platform/clock.hpp"

#include <chrono>

namespace finaleffect::platform {

namespace {
long long steadyNowNs() noexcept {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
}
} // namespace

SteadyClock::SteadyClock() noexcept : origin_nanoseconds_(steadyNowNs()) {}

double SteadyClock::nowSeconds() const noexcept {
    const long long delta_ns = steadyNowNs() - origin_nanoseconds_;
    return static_cast<double>(delta_ns) * 1.0e-9;
}

} // namespace finaleffect::platform
