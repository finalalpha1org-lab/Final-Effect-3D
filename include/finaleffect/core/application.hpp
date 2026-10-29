// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/core/result.hpp"

#include <cstdint>

namespace finaleffect::core {

class Engine;

struct FrameTimings {
    std::uint64_t frame_index{0};
    double delta_seconds{0.0};
    double elapsed_seconds{0.0};
};

// Application contract decoupled from the underlying core::Engine lifecycle.
class IApplication {
public:
    virtual ~IApplication() = default;

    [[nodiscard]] virtual Status onInitialize(Engine& engine) = 0;
    [[nodiscard]] virtual Status onUpdate(Engine& engine, const FrameTimings& timings) = 0;
    virtual void onShutdown(Engine& engine) noexcept = 0;
};

} // namespace finaleffect::core
