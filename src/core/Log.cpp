// SPDX-License-Identifier: GPL-3.0-only
#include "finaleffect/core/Log.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <iostream>

namespace fe::core {
namespace {

std::string lowerCopy(std::string_view text) {
    std::string out(text);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

const char* colourFor(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::Trace: return "\x1b[90m";
        case LogLevel::Debug: return "\x1b[36m";
        case LogLevel::Info: return "\x1b[32m";
        case LogLevel::Warning: return "\x1b[33m";
        case LogLevel::Error: return "\x1b[31m";
        case LogLevel::Critical: return "\x1b[1;31m";
        case LogLevel::Off: break;
    }
    return "";
}

}  // namespace

std::string_view toString(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info: return "INFO";
        case LogLevel::Warning: return "WARN";
        case LogLevel::Error: return "ERROR";
        case LogLevel::Critical: return "CRITICAL";
        case LogLevel::Off: return "OFF";
    }
    return "?";
}

bool tryParseLogLevel(std::string_view text, LogLevel& out) noexcept {
    const std::string key = lowerCopy(text);
    if (key == "trace") { out = LogLevel::Trace; return true; }
    if (key == "debug") { out = LogLevel::Debug; return true; }
    if (key == "info") { out = LogLevel::Info; return true; }
    if (key == "warn" || key == "warning") { out = LogLevel::Warning; return true; }
    if (key == "error") { out = LogLevel::Error; return true; }
    if (key == "critical" || key == "fatal") { out = LogLevel::Critical; return true; }
    if (key == "off" || key == "none" || key == "quiet") { out = LogLevel::Off; return true; }
    return false;
}

LogSink::~LogSink() = default;
void LogSink::flush() {}

OstreamLogSink::OstreamLogSink(bool colour) : colour_(colour) {}

void OstreamLogSink::write(const LogRecord& record) {
    char timeBuffer[32];
    const auto totalMs = static_cast<long long>(record.timeSeconds * 1000.0);
    std::snprintf(timeBuffer,
                  sizeof(timeBuffer),
                  "%02lld:%02lld:%02lld.%03lld",
                  totalMs / 3600000,
                  (totalMs / 60000) % 60,
                  (totalMs / 1000) % 60,
                  totalMs % 1000);

    const bool toErr = record.level >= LogLevel::Error;
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostream& stream = toErr ? std::cerr : std::cout;
    stream << '[' << timeBuffer << "] ";
    if (colour_) {
        stream << colourFor(record.level);
    }
    stream << '[' << toString(record.level) << ']';
    if (colour_) {
        stream << "\x1b[0m";
    }
    if (!record.category.empty()) {
        stream << " [" << record.category << "]";
    }
    stream << ' ' << record.message << '\n';
}

void OstreamLogSink::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout.flush();
    std::cerr.flush();
}

void MemoryLogSink::write(const LogRecord& record) {
    std::lock_guard<std::mutex> lock(mutex_);
    records_.push_back(record);
}

void MemoryLogSink::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    records_.clear();
}

std::size_t MemoryLogSink::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return records_.size();
}

const LogRecord& MemoryLogSink::at(std::size_t index) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return records_.at(index);
}

bool MemoryLogSink::contains(LogLevel level, std::string_view substring) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const LogRecord& record : records_) {
        if (record.level == level && record.message.find(substring) != std::string::npos) {
            return true;
        }
    }
    return false;
}

const std::vector<LogRecord>& MemoryLogSink::records() const {
    return records_;
}

Logger::Logger(LogLevel minimumLevel, std::string name)
    : name_(std::move(name)), minimumLevel_(minimumLevel), start_(std::chrono::steady_clock::now()) {}

void Logger::addSink(std::shared_ptr<LogSink> sink) {
    if (!sink) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    sinks_.push_back(std::move(sink));
}

void Logger::setMinimumLevel(LogLevel level) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    minimumLevel_ = level;
}

LogLevel Logger::minimumLevel() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return minimumLevel_;
}

bool Logger::shouldLog(LogLevel level) const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return level >= minimumLevel_ && minimumLevel_ != LogLevel::Off;
}

const std::string& Logger::name() const noexcept {
    return name_;
}

std::size_t Logger::sinkCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sinks_.size();
}

void Logger::log(LogLevel level, std::string_view category, std::string_view message) {
    LogRecord record;
    record.level = level;
    record.category.assign(category);
    record.message.assign(message);
    record.timeSeconds = elapsedSeconds();

    std::vector<std::shared_ptr<LogSink>> sinks;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (level < minimumLevel_ || minimumLevel_ == LogLevel::Off) {
            return;
        }
        sinks = sinks_;
    }
    for (const std::shared_ptr<LogSink>& sink : sinks) {
        sink->write(record);
    }
}

void Logger::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const std::shared_ptr<LogSink>& sink : sinks_) {
        sink->flush();
    }
}

double Logger::elapsedSeconds() const noexcept {
    const auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(now - start_).count();
}

LogStream::LogStream(Logger& logger, LogLevel level, std::string_view category)
    : logger_(logger), level_(level), category_(category) {}

LogStream::~LogStream() {
    if (logger_.shouldLog(level_)) {
        logger_.log(level_, category_, buffer_.str());
    }
}

}  // namespace fe::core
