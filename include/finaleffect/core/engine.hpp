// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/core/application.hpp"
#include "finaleffect/core/logger.hpp"
#include "finaleffect/core/result.hpp"
#include "finaleffect/platform/clock.hpp"
#include "finaleffect/platform/file_system.hpp"
#include "finaleffect/platform/paths.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace finaleffect::core {

enum class EngineState : std::uint8_t {
    Uninitialized = 0,
    Initialized,
    Running,
    ShuttingDown,
    Terminated
};

struct EngineConfig {
    std::string application_name{"Final Effect 3D"};
    LogLevel minimum_log_level{LogLevel::Info};
    bool attach_console_log_sink{true};
    double max_frame_delta_seconds{0.25};
};

// Owns core runtime services with explicit lifetimes and zero global state.
class Engine {
public:
    explicit Engine(
        EngineConfig config = {},
        std::unique_ptr<platform::IFileSystem> file_system = nullptr,
        std::unique_ptr<platform::IClock> clock = nullptr);

    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    [[nodiscard]] Status initialize(IApplication* application = nullptr);
    [[nodiscard]] Status stepFrame();
    [[nodiscard]] Status runFrames(std::uint64_t frame_count);
    void shutdown() noexcept;

    [[nodiscard]] EngineState state() const noexcept { return state_; }
    [[nodiscard]] Logger& logger() noexcept { return logger_; }
    [[nodiscard]] const Logger& logger() const noexcept { return logger_; }
    [[nodiscard]] platform::IFileSystem& fileSystem() noexcept { return *file_system_; }
    [[nodiscard]] const platform::IFileSystem& fileSystem() const noexcept { return *file_system_; }
    [[nodiscard]] const platform::IClock& clock() const noexcept { return *clock_; }
    [[nodiscard]] const platform::AppPaths& appPaths() const noexcept { return app_paths_; }
    [[nodiscard]] std::uint64_t frameCount() const noexcept { return frame_index_; }

private:
    EngineConfig config_;
    Logger logger_;
    std::unique_ptr<platform::IFileSystem> file_system_;
    std::unique_ptr<platform::IClock> clock_;
    platform::AppPaths app_paths_;
    IApplication* application_{nullptr};
    EngineState state_{EngineState::Uninitialized};
    double start_time_seconds_{0.0};
    double last_frame_time_seconds_{0.0};
    std::uint64_t frame_index_{0};
};

} // namespace finaleffect::core
