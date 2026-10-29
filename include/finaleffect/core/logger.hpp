// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace finaleffect::core {

enum class LogLevel : std::uint8_t {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warning = 3,
    Error = 4,
    Fatal = 5
};

[[nodiscard]] constexpr std::string_view logLevelToString(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::Trace:
            return "TRACE";
        case LogLevel::Debug:
            return "DEBUG";
        case LogLevel::Info:
            return "INFO";
        case LogLevel::Warning:
            return "WARN";
        case LogLevel::Error:
            return "ERROR";
        case LogLevel::Fatal:
            return "FATAL";
    }
    return "UNKNOWN";
}

struct LogEntry {
    LogLevel level{LogLevel::Info};
    double timestamp_seconds{0.0};
    std::string category;
    std::string message;
};

class ILogSink {
public:
    virtual ~ILogSink() = default;
    virtual void write(const LogEntry& entry) = 0;
};

class ConsoleLogSink final : public ILogSink {
public:
    void write(const LogEntry& entry) override;
};

class MemoryLogSink final : public ILogSink {
public:
    void write(const LogEntry& entry) override;
    [[nodiscard]] std::vector<LogEntry> entries() const;
    void clear();

private:
    mutable std::mutex mutex_;
    std::vector<LogEntry> entries_;
};

// Explicitly owned logger instance (no global singleton state).
class Logger {
public:
    explicit Logger(LogLevel minimum_level = LogLevel::Info);

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void setMinimumLevel(LogLevel level) noexcept;
    [[nodiscard]] LogLevel minimumLevel() const noexcept;

    void addSink(std::shared_ptr<ILogSink> sink);
    void clearSinks();

    void log(LogLevel level, std::string_view category, std::string_view message, double timestamp_seconds = 0.0);

    void trace(std::string_view category, std::string_view message, double ts = 0.0) {
        log(LogLevel::Trace, category, message, ts);
    }
    void debug(std::string_view category, std::string_view message, double ts = 0.0) {
        log(LogLevel::Debug, category, message, ts);
    }
    void info(std::string_view category, std::string_view message, double ts = 0.0) {
        log(LogLevel::Info, category, message, ts);
    }
    void warning(std::string_view category, std::string_view message, double ts = 0.0) {
        log(LogLevel::Warning, category, message, ts);
    }
    void error(std::string_view category, std::string_view message, double ts = 0.0) {
        log(LogLevel::Error, category, message, ts);
    }
    void fatal(std::string_view category, std::string_view message, double ts = 0.0) {
        log(LogLevel::Fatal, category, message, ts);
    }

private:
    mutable std::mutex mutex_;
    LogLevel minimum_level_;
    std::vector<std::shared_ptr<ILogSink>> sinks_;
};

} // namespace finaleffect::core
