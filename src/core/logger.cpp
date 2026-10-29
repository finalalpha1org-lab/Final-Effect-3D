// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/core/logger.hpp"

#include <cstdio>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <utility>

namespace finaleffect::core {

void ConsoleLogSink::write(const LogEntry& entry) {
    std::ostringstream oss;
    oss << "[" << std::fixed << std::setprecision(3) << entry.timestamp_seconds << "s] "
        << "[" << logLevelToString(entry.level) << "] "
        << "[" << entry.category << "] "
        << entry.message << '\n';

    if (entry.level >= LogLevel::Error) {
        std::cerr << oss.str();
    } else {
        std::cout << oss.str();
    }
}

void MemoryLogSink::write(const LogEntry& entry) {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.push_back(entry);
}

std::vector<LogEntry> MemoryLogSink::entries() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_;
}

void MemoryLogSink::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.clear();
}

Logger::Logger(LogLevel minimum_level) : minimum_level_(minimum_level) {}

void Logger::setMinimumLevel(LogLevel level) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    minimum_level_ = level;
}

LogLevel Logger::minimumLevel() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return minimum_level_;
}

void Logger::addSink(std::shared_ptr<ILogSink> sink) {
    if (!sink) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    sinks_.push_back(std::move(sink));
}

void Logger::clearSinks() {
    std::lock_guard<std::mutex> lock(mutex_);
    sinks_.clear();
}

void Logger::log(LogLevel level, std::string_view category, std::string_view message, double timestamp_seconds) {
    std::vector<std::shared_ptr<ILogSink>> sinks_snapshot;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (level < minimum_level_) {
            return;
        }
        sinks_snapshot = sinks_;
    }

    if (sinks_snapshot.empty()) {
        return;
    }

    LogEntry entry{
        .level = level,
        .timestamp_seconds = timestamp_seconds,
        .category = std::string(category),
        .message = std::string(message),
    };

    for (const auto& sink : sinks_snapshot) {
        sink->write(entry);
    }
}

} // namespace finaleffect::core
