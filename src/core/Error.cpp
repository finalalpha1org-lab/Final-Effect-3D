// SPDX-License-Identifier: GPL-3.0-only
#include "finaleffect/core/Error.hpp"

namespace fe::core {

std::string_view toString(ErrorCode code) noexcept {
    switch (code) {
        case ErrorCode::Ok: return "Ok";
        case ErrorCode::Unknown: return "Unknown";
        case ErrorCode::InvalidArgument: return "InvalidArgument";
        case ErrorCode::OutOfRange: return "OutOfRange";
        case ErrorCode::NotFound: return "NotFound";
        case ErrorCode::AlreadyExists: return "AlreadyExists";
        case ErrorCode::PermissionDenied: return "PermissionDenied";
        case ErrorCode::IoFailure: return "IoFailure";
        case ErrorCode::ParseError: return "ParseError";
        case ErrorCode::ValidationFailed: return "ValidationFailed";
        case ErrorCode::UnsupportedVersion: return "UnsupportedVersion";
        case ErrorCode::CycleDetected: return "CycleDetected";
        case ErrorCode::InitializationFailed: return "InitializationFailed";
        case ErrorCode::NotInitialized: return "NotInitialized";
        case ErrorCode::GraphicsUnavailable: return "GraphicsUnavailable";
        case ErrorCode::UnsupportedOperation: return "UnsupportedOperation";
        case ErrorCode::OutOfMemory: return "OutOfMemory";
    }
    return "Unknown";
}

bool isFailure(ErrorCode code) noexcept {
    return code != ErrorCode::Ok;
}

std::string_view suggestedAction(ErrorCode code) noexcept {
    switch (code) {
        case ErrorCode::Ok: return "no action required";
        case ErrorCode::InvalidArgument: return "fix the argument at the call site";
        case ErrorCode::OutOfRange: return "clamp or validate the incoming value";
        case ErrorCode::NotFound: return "check the identifier or path before use";
        case ErrorCode::AlreadyExists: return "reuse the existing entry instead of creating a duplicate";
        case ErrorCode::PermissionDenied: return "check file permissions or credentials";
        case ErrorCode::IoFailure: return "check the path, free space and device availability";
        case ErrorCode::ParseError: return "inspect the reported line and column";
        case ErrorCode::ValidationFailed: return "repair the file or reject it and keep the current state";
        case ErrorCode::UnsupportedVersion: return "upgrade Final Effect 3D or export from an older version";
        case ErrorCode::CycleDetected: return "break the hierarchy cycle in the data";
        case ErrorCode::InitializationFailed: return "check dependency and environment availability";
        case ErrorCode::NotInitialized: return "initialize the subsystem before using it";
        case ErrorCode::GraphicsUnavailable: return "run with a software renderer or check the GPU driver";
        case ErrorCode::UnsupportedOperation: return "use a backend that implements this operation";
        case ErrorCode::OutOfMemory: return "reduce the working set or increase available memory";
        case ErrorCode::Unknown: break;
    }
    return "inspect the error context";
}

Error::Error(ErrorCode code, std::string message, std::string context)
    : code_(code), message_(std::move(message)), context_(std::move(context)) {}

std::string Error::toString() const {
    std::string out;
    out += std::string(fe::core::toString(code_));
    if (!message_.empty()) {
        out += ": ";
        out += message_;
    }
    if (!context_.empty()) {
        out += " (";
        out += context_;
        out += ")";
    }
    return out;
}

Error makeError(ErrorCode code, std::string message, std::string context) {
    return Error(code, std::move(message), std::move(context));
}

}  // namespace fe::core
