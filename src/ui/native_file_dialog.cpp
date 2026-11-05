// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/ui/native_file_dialog.hpp"

#include <cstdlib>
#include <sstream>
#include <system_error>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <objbase.h>
#include <shobjidl.h>
#elif defined(__linux__) || defined(__APPLE__)
#include <cstdio>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace finaleffect::ui {

namespace {

#if defined(__linux__) || defined(__APPLE__)
std::string trimNewlines(std::string s) {
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) {
        s.pop_back();
    }
    return s;
}

std::string shellQuoteSingle(std::string_view sv) {
    std::string out = "'";
    for (const char ch : sv) {
        if (ch == '\'') {
            out += "'\\''";
        } else {
            out.push_back(ch);
        }
    }
    out.push_back('\'');
    return out;
}

bool executableExistsOnPath(std::string_view exe_name) {
    const char* path_env = std::getenv("PATH");
    if (path_env == nullptr) {
        return false;
    }
    std::istringstream iss(path_env);
    std::string dir;
    while (std::getline(iss, dir, ':')) {
        if (dir.empty()) continue;
        const std::filesystem::path candidate = std::filesystem::path(dir) / exe_name;
        if (::access(candidate.c_str(), X_OK) == 0) {
            return true;
        }
    }
    return false;
}

FileDialogResult runCommandCapturePath(const std::string& cmd) {
    FILE* pipe = ::popen(cmd.c_str(), "r");
    if (pipe == nullptr) {
        return FileDialogResult{
            .status = FileDialogStatus::Error,
            .selected_path = {},
            .error_message = "Failed to launch native file dialog helper process.",
        };
    }
    std::string output;
    char buffer[512]{};
    while (std::fgets(buffer, static_cast<int>(sizeof(buffer)), pipe) != nullptr) {
        output += buffer;
    }
    const int status = ::pclose(pipe);
    output = trimNewlines(std::move(output));

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif
    const bool exited_normally = WIFEXITED(status);
    const int exit_code = exited_normally ? WEXITSTATUS(status) : -1;
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

    if (exited_normally) {
        if (exit_code == 0 && !output.empty()) {
            return FileDialogResult{
                .status = FileDialogStatus::Selected,
                .selected_path = std::filesystem::path(output),
                .error_message = {},
            };
        }
        if (exit_code == 1 || output.empty()) {
            return FileDialogResult{.status = FileDialogStatus::Cancelled};
        }
    }
    return FileDialogResult{
        .status = FileDialogStatus::Unavailable,
        .selected_path = {},
        .error_message = "Native desktop dialog portal closed unexpectedly.",
    };
}
#endif

#if defined(_WIN32)
std::wstring utf8ToWide(std::string_view sv) {
    if (sv.empty()) {
        return {};
    }
    const int needed = ::MultiByteToWideChar(
        CP_UTF8,
        0,
        sv.data(),
        static_cast<int>(sv.size()),
        nullptr,
        0);
    if (needed <= 0) {
        return {};
    }
    std::wstring wide(static_cast<std::size_t>(needed), L'\0');
    ::MultiByteToWideChar(
        CP_UTF8,
        0,
        sv.data(),
        static_cast<int>(sv.size()),
        wide.data(),
        needed);
    return wide;
}

FileDialogResult runWin32CommonItemDialog(
    std::string_view title,
    const std::filesystem::path& initial_directory,
    bool pick_directory) {
    const HRESULT co_hr = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    const bool must_uninit = SUCCEEDED(co_hr);

    IFileOpenDialog* dialog = nullptr;
    const HRESULT create_hr = ::CoCreateInstance(
        CLSID_FileOpenDialog,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&dialog));
    if (FAILED(create_hr) || dialog == nullptr) {
        if (must_uninit) ::CoUninitialize();
        return FileDialogResult{
            .status = FileDialogStatus::Unavailable,
            .selected_path = {},
            .error_message = "Win32 Common Item Dialog COM service is unavailable.",
        };
    }

    DWORD opts = 0;
    if (SUCCEEDED(dialog->GetOptions(&opts))) {
        opts |= FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST;
        if (pick_directory) {
            opts |= FOS_PICKFOLDERS;
        } else {
            opts |= FOS_FILEMUSTEXIST;
        }
        (void)dialog->SetOptions(opts);
    }

    const std::wstring wtitle = utf8ToWide(title);
    if (!wtitle.empty()) {
        (void)dialog->SetTitle(wtitle.c_str());
    }

    if (!pick_directory) {
        const COMDLG_FILTERSPEC filters[] = {
            {L"Final Effect 3D Project (*.fe3d)", L"*.fe3d"},
            {L"All Files (*.*)", L"*.*"},
        };
        (void)dialog->SetFileTypes(2u, filters);
        (void)dialog->SetFileTypeIndex(1u);
    }

    std::error_code ec;
    if (!initial_directory.empty() && std::filesystem::exists(initial_directory, ec)) {
        IShellItem* folder_item = nullptr;
        const std::wstring wdir = initial_directory.wstring();
        if (SUCCEEDED(::SHCreateItemFromParsingName(wdir.c_str(), nullptr, IID_PPV_ARGS(&folder_item))) &&
            folder_item != nullptr) {
            (void)dialog->SetFolder(folder_item);
            folder_item->Release();
        }
    }

    const HRESULT show_hr = dialog->Show(nullptr);
    if (show_hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
        dialog->Release();
        if (must_uninit) ::CoUninitialize();
        return FileDialogResult{.status = FileDialogStatus::Cancelled};
    }
    if (FAILED(show_hr)) {
        dialog->Release();
        if (must_uninit) ::CoUninitialize();
        return FileDialogResult{
            .status = FileDialogStatus::Error,
            .selected_path = {},
            .error_message = "Win32 file dialog failed to display.",
        };
    }

    IShellItem* item = nullptr;
    FileDialogResult result{.status = FileDialogStatus::Cancelled};
    if (SUCCEEDED(dialog->GetResult(&item)) && item != nullptr) {
        PWSTR raw_path = nullptr;
        if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &raw_path)) && raw_path != nullptr) {
            result.status = FileDialogStatus::Selected;
            result.selected_path = std::filesystem::path(raw_path);
            ::CoTaskMemFree(raw_path);
        }
        item->Release();
    }

    dialog->Release();
    if (must_uninit) {
        ::CoUninitialize();
    }
    return result;
}
#endif

} // namespace

SystemNativeFileDialog::SystemNativeFileDialog() {
#if defined(_WIN32)
    portal_available_ = true;
    backend_label_ = "Win32 Common Item Dialog";
#elif defined(__APPLE__)
    portal_available_ = executableExistsOnPath("osascript");
    backend_label_ = portal_available_ ? "macOS Cocoa File Panel" : "In-App File Navigator";
#elif defined(__linux__)
    const bool has_display =
        (std::getenv("DISPLAY") != nullptr && std::getenv("DISPLAY")[0] != '\0') ||
        (std::getenv("WAYLAND_DISPLAY") != nullptr && std::getenv("WAYLAND_DISPLAY")[0] != '\0');
    if (has_display && executableExistsOnPath("zenity")) {
        portal_available_ = true;
        linux_tool_ = 1;
        backend_label_ = "Linux Desktop Portal (Zenity)";
    } else if (has_display && executableExistsOnPath("kdialog")) {
        portal_available_ = true;
        linux_tool_ = 2;
        backend_label_ = "Linux Desktop Portal (KDialog)";
    } else {
        portal_available_ = false;
        linux_tool_ = 0;
        backend_label_ = "In-App Native File Navigator";
    }
#endif
}

FileDialogResult SystemNativeFileDialog::pickOpenProjectFile(
    std::string_view title,
    const std::filesystem::path& initial_directory) {
#if defined(_WIN32)
    return runWin32CommonItemDialog(title, initial_directory, false);
#elif defined(__APPLE__)
    if (!portal_available_) {
        return FileDialogResult{.status = FileDialogStatus::Unavailable};
    }
    std::string script = "POSIX path of (choose file with prompt " + shellQuoteSingle(title) +
                         " of type {\"fe3d\", \"json\"})";
    const std::string cmd = "osascript -e " + shellQuoteSingle(script) + " 2>/dev/null";
    (void)initial_directory;
    return runCommandCapturePath(cmd);
#elif defined(__linux__)
    if (!portal_available_) {
        return FileDialogResult{.status = FileDialogStatus::Unavailable};
    }
    const std::string start = initial_directory.empty() ? std::string(".") : (initial_directory.string() + "/");
    if (linux_tool_ == 1) {
        const std::string cmd =
            "zenity --file-selection --title=" + shellQuoteSingle(title) +
            " --filename=" + shellQuoteSingle(start) +
            " --file-filter='Final Effect 3D Project (*.fe3d) | *.fe3d *.json' "
            "--file-filter='All Files | *' 2>/dev/null";
        return runCommandCapturePath(cmd);
    }
    const std::string cmd =
        "kdialog --title " + shellQuoteSingle(title) +
        " --getopenfilename " + shellQuoteSingle(start) + " '*.fe3d *.json' 2>/dev/null";
    return runCommandCapturePath(cmd);
#else
    (void)title;
    (void)initial_directory;
    return FileDialogResult{.status = FileDialogStatus::Unavailable};
#endif
}

FileDialogResult SystemNativeFileDialog::pickDirectory(
    std::string_view title,
    const std::filesystem::path& initial_directory) {
#if defined(_WIN32)
    return runWin32CommonItemDialog(title, initial_directory, true);
#elif defined(__APPLE__)
    if (!portal_available_) {
        return FileDialogResult{.status = FileDialogStatus::Unavailable};
    }
    std::string script = "POSIX path of (choose folder with prompt " + shellQuoteSingle(title) + ")";
    const std::string cmd = "osascript -e " + shellQuoteSingle(script) + " 2>/dev/null";
    (void)initial_directory;
    return runCommandCapturePath(cmd);
#elif defined(__linux__)
    if (!portal_available_) {
        return FileDialogResult{.status = FileDialogStatus::Unavailable};
    }
    const std::string start = initial_directory.empty() ? std::string(".") : (initial_directory.string() + "/");
    if (linux_tool_ == 1) {
        const std::string cmd =
            "zenity --file-selection --directory --title=" + shellQuoteSingle(title) +
            " --filename=" + shellQuoteSingle(start) + " 2>/dev/null";
        return runCommandCapturePath(cmd);
    }
    const std::string cmd =
        "kdialog --title " + shellQuoteSingle(title) +
        " --getexistingdirectory " + shellQuoteSingle(start) + " 2>/dev/null";
    return runCommandCapturePath(cmd);
#else
    (void)title;
    (void)initial_directory;
    return FileDialogResult{.status = FileDialogStatus::Unavailable};
#endif
}

} // namespace finaleffect::ui
