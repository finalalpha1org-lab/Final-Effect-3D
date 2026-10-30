// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace finaleffect::core {

enum class ErrorCode : std::uint16_t {
    None = 0,
    InvalidArgument,
    NotFound,
    AlreadyExists,
    HierarchyCycle,
    InvalidState,
    IoError,
    ParseError,
    UnsupportedVersion,
    ValidationError,
    RenderInitFailed,
    ShaderCompileFailed,
    ResourceCreationFailed,
    PlatformUnsupported
};

[[nodiscard]] constexpr std::string_view errorCodeToString(ErrorCode code) noexcept {
    switch (code) {
        case ErrorCode::None:
            return "None";
        case ErrorCode::InvalidArgument:
            return "InvalidArgument";
        case ErrorCode::NotFound:
            return "NotFound";
        case ErrorCode::AlreadyExists:
            return "AlreadyExists";
        case ErrorCode::HierarchyCycle:
            return "HierarchyCycle";
        case ErrorCode::InvalidState:
            return "InvalidState";
        case ErrorCode::IoError:
            return "IoError";
        case ErrorCode::ParseError:
            return "ParseError";
        case ErrorCode::UnsupportedVersion:
            return "UnsupportedVersion";
        case ErrorCode::ValidationError:
            return "ValidationError";
        case ErrorCode::RenderInitFailed:
            return "RenderInitFailed";
        case ErrorCode::ShaderCompileFailed:
            return "ShaderCompileFailed";
        case ErrorCode::ResourceCreationFailed:
            return "ResourceCreationFailed";
        case ErrorCode::PlatformUnsupported:
            return "PlatformUnsupported";
    }
    return "Unknown";
}

struct Error {
    ErrorCode code{ErrorCode::InvalidState};
    std::string message;
    std::string context;

    Error() = default;

    Error(ErrorCode error_code, std::string error_message, std::string error_context = {})
        : code(error_code),
          message(std::move(error_message)),
          context(std::move(error_context)) {}

    [[nodiscard]] std::string format() const {
        std::string result = "[";
        result += errorCodeToString(code);
        result += "] ";
        result += message;
        if (!context.empty()) {
            result += " (";
            result += context;
            result += ")";
        }
        return result;
    }
};

} // namespace finaleffect::core
