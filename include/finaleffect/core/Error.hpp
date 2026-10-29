// Final Effect 3D - structured error reporting.
// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <string>
#include <string_view>
#include <utility>

namespace fe::core {

/// Machine readable failure classification. Never contains free-form text.
enum class ErrorCode : int {
    Ok = 0,
    Unknown,
    InvalidArgument,
    OutOfRange,
    NotFound,
    AlreadyExists,
    PermissionDenied,
    IoFailure,
    ParseError,
    ValidationFailed,
    UnsupportedVersion,
    CycleDetected,
    InitializationFailed,
    NotInitialized,
    GraphicsUnavailable,
    UnsupportedOperation,
    OutOfMemory,
};

std::string_view toString(ErrorCode code) noexcept;

/// True for every code except ErrorCode::Ok.
bool isFailure(ErrorCode code) noexcept;

/// Short, human readable hint about what a caller is expected to do next.
std::string_view suggestedAction(ErrorCode code) noexcept;

/// An ErrorCode plus human readable detail. Default constructed errors are Ok.
class Error {
public:
    Error() = default;
    Error(ErrorCode code, std::string message, std::string context = {});

    ErrorCode code() const noexcept { return code_; }
    bool isOk() const noexcept { return code_ == ErrorCode::Ok; }
    const std::string& message() const noexcept { return message_; }
    const std::string& context() const noexcept { return context_; }

    /// "ValidationFailed: scene object 4 has an unknown parent (project load)"
    std::string toString() const;

    bool operator==(ErrorCode code) const noexcept { return code_ == code; }
    bool operator!=(ErrorCode code) const noexcept { return code_ != code; }

private:
    ErrorCode code_ = ErrorCode::Ok;
    std::string message_;
    std::string context_;
};

Error makeError(ErrorCode code, std::string message, std::string context = {});

namespace detail {

/// Shared storage for Result<T> and Result<void>. Not part of the public API.
class ResultStorage {
public:
    bool ok() const noexcept { return ok_; }
    explicit operator bool() const noexcept { return ok_; }
    const Error& error() const noexcept { return error_; }

protected:
    ResultStorage(bool ok, Error error) noexcept : ok_(ok), error_(std::move(error)) {}
    ResultStorage(const ResultStorage&) = default;
    ResultStorage(ResultStorage&&) noexcept = default;
    ResultStorage& operator=(const ResultStorage&) = default;
    ResultStorage& operator=(ResultStorage&&) noexcept = default;
    ~ResultStorage() = default;

private:
    bool ok_;
    Error error_;
};

}  // namespace detail

/// Value-or-error type. No exceptions are thrown anywhere in the engine core:
/// failures travel through Result<T> / Status values.
template <class T>
class Result final : public detail::ResultStorage {
public:
    Result(T value) : detail::ResultStorage(true, Error{}), value_(std::move(value)) {}
    Result(Error error) : detail::ResultStorage(false, std::move(error)), value_() {}

    static Result success(T value) { return Result(std::move(value)); }
    static Result failure(Error error) { return Result(std::move(error)); }

    const T& value() const noexcept { return value_; }
    T& value() noexcept { return value_; }
    T valueOr(T fallback) const {
        return ok() ? value_ : std::move(fallback);
    }

    const T* operator->() const noexcept { return &value_; }
    T* operator->() noexcept { return &value_; }
    const T& operator*() const noexcept { return value_; }
    T& operator*() noexcept { return value_; }

private:
    T value_;
};

/// Result carrying no value. Use Result<void>::success() / Status{}.
template <>
class Result<void> final : public detail::ResultStorage {
public:
    Result() : detail::ResultStorage(true, Error{}) {}
    Result(Error error) : detail::ResultStorage(false, std::move(error)) {}

    static Result success() { return Result(); }
    static Result failure(Error error) { return Result(std::move(error)); }
};

using Status = Result<void>;

/// Convenience: a Status that is Ok.
inline Status successStatus() {
    return Status::success();
}

}  // namespace fe::core
