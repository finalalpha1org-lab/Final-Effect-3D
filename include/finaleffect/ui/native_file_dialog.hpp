// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include <cstdint>
#include <deque>
#include <filesystem>
#include <string>
#include <string_view>

namespace finaleffect::ui {

enum class FileDialogStatus : std::uint8_t {
    Selected = 0,
    Cancelled = 1,
    Unavailable = 2,
    Error = 3,
};

struct FileDialogResult {
    FileDialogStatus status{FileDialogStatus::Unavailable};
    std::filesystem::path selected_path{};
    std::string error_message{};
};

// Isolated native operating-system file & directory selection contract.
class INativeFileDialog {
public:
    virtual ~INativeFileDialog() = default;

    [[nodiscard]] virtual bool isNativePortalAvailable() const noexcept = 0;
    [[nodiscard]] virtual std::string_view backendLabel() const noexcept = 0;

    [[nodiscard]] virtual FileDialogResult pickOpenProjectFile(
        std::string_view title,
        const std::filesystem::path& initial_directory) = 0;

    [[nodiscard]] virtual FileDialogResult pickDirectory(
        std::string_view title,
        const std::filesystem::path& initial_directory) = 0;
};

// Production OS dialog implementation (Win32 IFileOpenDialog, macOS Cocoa AppleScript,
// Linux XDG Zenity/KDialog) with honest Unavailable reporting when no GUI portal exists.
class SystemNativeFileDialog final : public INativeFileDialog {
public:
    SystemNativeFileDialog();

    [[nodiscard]] bool isNativePortalAvailable() const noexcept override { return portal_available_; }
    [[nodiscard]] std::string_view backendLabel() const noexcept override { return backend_label_; }

    [[nodiscard]] FileDialogResult pickOpenProjectFile(
        std::string_view title,
        const std::filesystem::path& initial_directory) override;

    [[nodiscard]] FileDialogResult pickDirectory(
        std::string_view title,
        const std::filesystem::path& initial_directory) override;

private:
    bool portal_available_{false};
    std::string backend_label_{"In-App File Navigator"};
#if defined(__linux__)
    int linux_tool_{0}; // 0 = none, 1 = zenity, 2 = kdialog
#endif
};

// Deterministic test double for automated C++ UI & workflow verification.
class MockNativeFileDialog final : public INativeFileDialog {
public:
    explicit MockNativeFileDialog(bool available = true) : available_(available) {}

    [[nodiscard]] bool isNativePortalAvailable() const noexcept override { return available_; }
    [[nodiscard]] std::string_view backendLabel() const noexcept override {
        return available_ ? "Mock Native OS Dialog" : "Unavailable Mock Dialog";
    }

    void setAvailable(bool available) noexcept { available_ = available; }
    void queueOpenResult(FileDialogResult result) { open_queue_.push_back(std::move(result)); }
    void queueDirectoryResult(FileDialogResult result) { dir_queue_.push_back(std::move(result)); }

    [[nodiscard]] std::size_t openCallCount() const noexcept { return open_calls_; }
    [[nodiscard]] std::size_t directoryCallCount() const noexcept { return dir_calls_; }

    [[nodiscard]] FileDialogResult pickOpenProjectFile(
        std::string_view /*title*/,
        const std::filesystem::path& /*initial_directory*/) override {
        ++open_calls_;
        if (!available_) {
            return FileDialogResult{
                .status = FileDialogStatus::Unavailable,
                .selected_path = {},
                .error_message = "Native file dialog is unavailable in this environment.",
            };
        }
        if (open_queue_.empty()) {
            return FileDialogResult{.status = FileDialogStatus::Cancelled};
        }
        FileDialogResult res = std::move(open_queue_.front());
        open_queue_.pop_front();
        return res;
    }

    [[nodiscard]] FileDialogResult pickDirectory(
        std::string_view /*title*/,
        const std::filesystem::path& /*initial_directory*/) override {
        ++dir_calls_;
        if (!available_) {
            return FileDialogResult{
                .status = FileDialogStatus::Unavailable,
                .selected_path = {},
                .error_message = "Native folder dialog is unavailable in this environment.",
            };
        }
        if (dir_queue_.empty()) {
            return FileDialogResult{.status = FileDialogStatus::Cancelled};
        }
        FileDialogResult res = std::move(dir_queue_.front());
        dir_queue_.pop_front();
        return res;
    }

private:
    bool available_{true};
    std::size_t open_calls_{0};
    std::size_t dir_calls_{0};
    std::deque<FileDialogResult> open_queue_{};
    std::deque<FileDialogResult> dir_queue_{};
};

} // namespace finaleffect::ui
