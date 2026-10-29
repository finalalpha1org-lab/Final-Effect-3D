// SPDX-License-Identifier: GPL-3.0-only
#include "finaleffect/platform/Clock.hpp"

#include <chrono>

namespace fe::platform {
namespace {

double steadyNowSeconds() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration<double>(now).count();
}

}  // namespace

Clock::~Clock() = default;

SteadyClock::SteadyClock() : startSeconds_(steadyNowSeconds()) {}

double SteadyClock::nowSeconds() const {
    return steadyNowSeconds();
}

double SteadyClock::elapsedSeconds() const {
    return steadyNowSeconds() - startSeconds_;
}

void SteadyClock::reset() {
    startSeconds_ = steadyNowSeconds();
}

ManualClock::ManualClock(double startSeconds) : now_(startSeconds), start_(startSeconds) {}

double ManualClock::nowSeconds() const {
    return now_;
}

double ManualClock::elapsedSeconds() const {
    return now_ - start_;
}

void ManualClock::reset() {
    start_ = now_;
}

void ManualClock::advance(double deltaSeconds) {
    now_ += deltaSeconds;
}

}  // namespace fe::platform
