// Final Effect 3D - logging with severity levels.
// SPDX-License-Identifier: GPL-3.0-only
//
// Design rules:
//   * There is no global logger and no singleton. Owners create and pass a Logger.
//   * Sinks are injectable, which keeps the core testable and platform free.
//   * Logging is never used to report recoverable failures: those use Result<T>.
#pragma once

#include <chrono>
#include <cstddef>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace fe::core {

enum class LogLevel : int { Trace = 0, Debug = 1, Info = 2, Warning = 3, Error = 4, Critical = 5, Off = 6 };

std::string_view toString(LogLevel level) noexcept;

/// Parses "trace", "debug", "info", "warning", "error", "critical", "off"
/// (case insensitive). Returns false and leaves the output untouched on failure.
bool tryParseLogLevel(std::string_view text, LogLevel& out) noexcept;

struct LogRecord {
    LogLevel level = LogLevel::Info;
    std::string category;
    std::string message;
    /// Seconds since the logger was constructed.
    double timeSeconds = 0.0;
};

class LogSink {
public:
    LogSink() = default;
    virtual ~LogSink();
    LogSink(const LogSink&) = delete;
    LogSink& operator=(const LogSink&) = delete;

    virtual void write(const LogRecord& record) = 0;
    virtual void flush();
};

/// Writes human readable lines to stdout (Trace..Warning) and stderr (Error, Critical).
class OstreamLogSink final : public LogSink {
public:
    explicit OstreamLogSink(bool colour = true);
    void write(const LogRecord& record) override;
    void flush() override;

private:
    bool colour_;
    std::mutex mutex_;
};

/// Captures records in memory. Used by tests and by quiet tool runs.
class MemoryLogSink final : public LogSink {
public:
    void write(const LogRecord& record) override;
    void clear();
    std::size_t size() const;
    const LogRecord& at(std::size_t index) const;
    bool contains(LogLevel level, std::string_view substring) const;
    const std::vector<LogRecord>& records() const;

private:
    mutable std::mutex mutex_;
    std::vector<LogRecord> records_;
};

class Logger {
public:
    explicit Logger(LogLevel minimumLevel = LogLevel::Info, std::string name = "engine");

    void addSink(std::shared_ptr<LogSink> sink);
    void setMinimumLevel(LogLevel level) noexcept;
    LogLevel minimumLevel() const noexcept;
    bool shouldLog(LogLevel level) const noexcept;
    const std::string& name() const noexcept;
    std::size_t sinkCount() const;

    void log(LogLevel level, std::string_view category, std::string_view message);
    void flush();
    double elapsedSeconds() const noexcept;

private:
    mutable std::mutex mutex_;
    std::string name_;
    LogLevel minimumLevel_;
    std::vector<std::shared_ptr<LogSink>> sinks_;
    std::chrono::steady_clock::time_point start_;
};

/// Streaming facade: LogStream(logger, LogLevel::Info, "scene") << "added " << count;
/// The record is emitted (and only then) when the temporary is destroyed.
class LogStream {
public:
    LogStream(Logger& logger, LogLevel level, std::string_view category);
    ~LogStream();
    LogStream(const LogStream&) = delete;
    LogStream& operator=(const LogStream&) = delete;
    LogStream(LogStream&&) = delete;
    LogStream& operator=(LogStream&&) = delete;

    template <typename T>
    LogStream& operator<<(const T& value) {
        buffer_ << value;
        return *this;
    }

private:
    Logger& logger_;
    LogLevel level_;
    std::string category_;
    std::ostringstream buffer_;
};

}  // namespace fe::core
