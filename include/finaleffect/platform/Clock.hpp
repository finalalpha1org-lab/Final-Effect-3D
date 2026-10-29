// Final Effect 3D - timing service.
// SPDX-License-Identifier: GPL-3.0-only
//
// Time is injected, never read from a global. Tests use ManualClock to make the
// frame loop deterministic.
#pragma once

#include <cstdint>

namespace fe::platform {

class Clock {
public:
    Clock() = default;
    virtual ~Clock();
    Clock(const Clock&) = delete;
    Clock& operator=(const Clock&) = delete;

    /// Monotonic time source in seconds.
    virtual double nowSeconds() const = 0;
    /// Seconds since the last reset() (or construction).
    virtual double elapsedSeconds() const = 0;
    virtual void reset() = 0;
};

class SteadyClock final : public Clock {
public:
    SteadyClock();
    double nowSeconds() const override;
    double elapsedSeconds() const override;
    void reset() override;

private:
    double startSeconds_ = 0.0;
};

/// Deterministic clock for tests: time advances only when advance() is called.
class ManualClock final : public Clock {
public:
    explicit ManualClock(double startSeconds = 0.0);
    double nowSeconds() const override;
    double elapsedSeconds() const override;
    void reset() override;
    void advance(double deltaSeconds);

private:
    double now_ = 0.0;
    double start_ = 0.0;
};

}  // namespace fe::platform
