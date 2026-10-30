// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

namespace finaleffect::platform {

class IClock {
public:
    virtual ~IClock() = default;

    // Monotonic elapsed time in seconds from an unspecified origin.
    [[nodiscard]] virtual double nowSeconds() const noexcept = 0;
};

class SteadyClock final : public IClock {
public:
    SteadyClock() noexcept;
    [[nodiscard]] double nowSeconds() const noexcept override;

private:
    long long origin_nanoseconds_{0};
};

// Deterministic clock for unit tests and offline frame stepping.
class ManualClock final : public IClock {
public:
    explicit ManualClock(double initial_seconds = 0.0) noexcept : current_seconds_(initial_seconds) {}

    [[nodiscard]] double nowSeconds() const noexcept override {
        return current_seconds_;
    }

    void advance(double delta_seconds) noexcept {
        current_seconds_ += delta_seconds;
    }

    void set(double seconds) noexcept {
        current_seconds_ = seconds;
    }

private:
    double current_seconds_{0.0};
};

} // namespace finaleffect::platform
