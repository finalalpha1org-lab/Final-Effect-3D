// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/core/error.hpp"

#include <cstdlib>
#include <utility>
#include <variant>

namespace finaleffect::core {

template <typename T>
class Result {
public:
    Result(const T& val) : storage_(val) {}
    Result(T&& val) noexcept(std::is_nothrow_move_constructible_v<T>) : storage_(std::move(val)) {}
    Result(const Error& err) : storage_(err) {}
    Result(Error&& err) noexcept : storage_(std::move(err)) {}

    [[nodiscard]] static Result ok(T val) {
        return Result(std::move(val));
    }

    [[nodiscard]] static Result fail(ErrorCode code, std::string message, std::string context = {}) {
        return Result(Error(code, std::move(message), std::move(context)));
    }

    [[nodiscard]] bool hasValue() const noexcept {
        return std::holds_alternative<T>(storage_);
    }

    [[nodiscard]] bool isError() const noexcept {
        return std::holds_alternative<Error>(storage_);
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return hasValue();
    }

    [[nodiscard]] T& value() & {
        if (!hasValue()) {
            std::abort();
        }
        return std::get<T>(storage_);
    }

    [[nodiscard]] const T& value() const& {
        if (!hasValue()) {
            std::abort();
        }
        return std::get<T>(storage_);
    }

    [[nodiscard]] T&& value() && {
        if (!hasValue()) {
            std::abort();
        }
        return std::get<T>(std::move(storage_));
    }

    [[nodiscard]] T valueOr(T fallback) const& {
        return hasValue() ? std::get<T>(storage_) : std::move(fallback);
    }

    [[nodiscard]] const Error& error() const& {
        if (!isError()) {
            std::abort();
        }
        return std::get<Error>(storage_);
    }

    [[nodiscard]] Error error() && {
        if (!isError()) {
            std::abort();
        }
        return std::get<Error>(std::move(storage_));
    }

private:
    std::variant<T, Error> storage_;
};

template <>
class Result<void> {
public:
    Result() noexcept : storage_(std::monostate{}) {}
    Result(const Error& err) : storage_(err) {}
    Result(Error&& err) noexcept : storage_(std::move(err)) {}

    [[nodiscard]] static Result ok() noexcept {
        return Result();
    }

    [[nodiscard]] static Result fail(ErrorCode code, std::string message, std::string context = {}) {
        return Result(Error(code, std::move(message), std::move(context)));
    }

    [[nodiscard]] bool hasValue() const noexcept {
        return std::holds_alternative<std::monostate>(storage_);
    }

    [[nodiscard]] bool isError() const noexcept {
        return std::holds_alternative<Error>(storage_);
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return hasValue();
    }

    [[nodiscard]] const Error& error() const& {
        if (!isError()) {
            std::abort();
        }
        return std::get<Error>(storage_);
    }

    [[nodiscard]] Error error() && {
        if (!isError()) {
            std::abort();
        }
        return std::get<Error>(std::move(storage_));
    }

private:
    std::variant<std::monostate, Error> storage_;
};

using Status = Result<void>;

} // namespace finaleffect::core
