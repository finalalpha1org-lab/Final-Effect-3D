// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/core/engine.hpp"
#include "finaleffect/core/version.hpp"

#include <algorithm>
#include <utility>

namespace finaleffect::core {

Engine::Engine(
    EngineConfig config,
    std::unique_ptr<platform::IFileSystem> file_system,
    std::unique_ptr<platform::IClock> clock)
    : config_(std::move(config)),
      logger_(config_.minimum_log_level),
      file_system_(file_system ? std::move(file_system) : std::make_unique<platform::LocalFileSystem>()),
      clock_(clock ? std::move(clock) : std::make_unique<platform::SteadyClock>()) {
    if (config_.attach_console_log_sink) {
        logger_.addSink(std::make_shared<ConsoleLogSink>());
    }
}

Engine::~Engine() {
    if (state_ == EngineState::Initialized || state_ == EngineState::Running) {
        shutdown();
    }
}

Status Engine::initialize(IApplication* application) {
    if (state_ != EngineState::Uninitialized) {
        return Status::fail(
            ErrorCode::InvalidState,
            "Engine::initialize() requires Uninitialized state");
    }
    if (config_.application_name.empty()) {
        return Status::fail(
            ErrorCode::InvalidArgument,
            "EngineConfig::application_name must not be empty");
    }
    if (config_.max_frame_delta_seconds <= 0.0) {
        return Status::fail(
            ErrorCode::InvalidArgument,
            "EngineConfig::max_frame_delta_seconds must be positive");
    }

    auto paths_res = platform::queryAppPaths("finaleffect3d");
    if (!paths_res) {
        return paths_res.error();
    }
    app_paths_ = std::move(paths_res).value();

    start_time_seconds_ = clock_->nowSeconds();
    last_frame_time_seconds_ = start_time_seconds_;
    frame_index_ = 0;
    application_ = application;
    state_ = EngineState::Initialized;

    logger_.info(
        "Core.Engine",
        "Initialized " + config_.application_name + " (engine v" + std::string(engineVersionString()) +
            ", host=" + std::string(platform::hostPlatformName()) + ")",
        0.0);

    if (application_ != nullptr) {
        auto app_status = application_->onInitialize(*this);
        if (!app_status) {
            logger_.error("Core.Engine", "Application initialization failed: " + app_status.error().format(), 0.0);
            application_ = nullptr;
            state_ = EngineState::Terminated;
            return app_status;
        }
    }

    return Status::ok();
}

Status Engine::stepFrame() {
    if (state_ != EngineState::Initialized && state_ != EngineState::Running) {
        return Status::fail(
            ErrorCode::InvalidState,
            "Engine::stepFrame() called before initialize() or after shutdown()");
    }

    state_ = EngineState::Running;
    const double now = clock_->nowSeconds();
    double raw_delta = now - last_frame_time_seconds_;
    if (raw_delta < 0.0) {
        raw_delta = 0.0;
    }
    const double clamped_delta = std::min(raw_delta, config_.max_frame_delta_seconds);
    last_frame_time_seconds_ = now;

    FrameTimings timings{
        .frame_index = frame_index_,
        .delta_seconds = clamped_delta,
        .elapsed_seconds = std::max(0.0, now - start_time_seconds_),
    };

    if (application_ != nullptr) {
        auto status = application_->onUpdate(*this, timings);
        if (!status) {
            logger_.error(
                "Core.Engine",
                "Application update failed on frame " + std::to_string(frame_index_) + ": " +
                    status.error().format(),
                timings.elapsed_seconds);
            return status;
        }
    }

    ++frame_index_;
    return Status::ok();
}

Status Engine::runFrames(std::uint64_t frame_count) {
    if (state_ != EngineState::Initialized && state_ != EngineState::Running) {
        return Status::fail(
            ErrorCode::InvalidState,
            "Engine::runFrames() requires an initialized engine");
    }

    for (std::uint64_t i = 0; i < frame_count; ++i) {
        auto status = stepFrame();
        if (!status) {
            return status;
        }
    }
    return Status::ok();
}

void Engine::shutdown() noexcept {
    if (state_ == EngineState::Terminated || state_ == EngineState::Uninitialized) {
        state_ = EngineState::Terminated;
        return;
    }

    state_ = EngineState::ShuttingDown;
    const double elapsed = std::max(0.0, clock_->nowSeconds() - start_time_seconds_);

    if (application_ != nullptr) {
        application_->onShutdown(*this);
        application_ = nullptr;
    }

    logger_.info(
        "Core.Engine",
        "Engine shut down cleanly after " + std::to_string(frame_index_) + " frame(s).",
        elapsed);
    state_ = EngineState::Terminated;
}

} // namespace finaleffect::core
