// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/app/studio_session.hpp"
#include "finaleffect/core/engine.hpp"
#include "finaleffect/core/version.hpp"
#include "finaleffect/platform/window.hpp"
#include "finaleffect/rendering/opengl33_render_device.hpp"
#include "finaleffect/rendering/software_render_device.hpp"

#if defined(FINALEFFECT_HAS_GLFW_WINDOW)
#include "finaleffect/platform/glfw_window.hpp"
#endif

#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace {

struct CliOptions {
    bool show_help{false};
    bool headless{false};
    std::uint32_t width{1280};
    std::uint32_t height{720};
    std::optional<std::uint64_t> max_frames;
    std::filesystem::path input_project;
    std::filesystem::path export_project;
    std::filesystem::path render_ppm;
};

bool parseUint64Arg(std::string_view text, std::uint64_t& out) {
    const auto* begin = text.data();
    const auto* end = text.data() + text.size();
    const auto [ptr, ec] = std::from_chars(begin, end, out);
    return ec == std::errc{} && ptr == end;
}

void printUsage() {
    std::cout
        << "Final Effect 3D Desktop Host (v" << finaleffect::core::engineVersionString() << ")\n"
        << "Usage: finaleffect_desktop [options]\n\n"
        << "Options:\n"
        << "  --help                   Show this help message\n"
        << "  --headless               Run without a desktop window using the CPU reference renderer\n"
        << "  --frames <N>             Run a bounded number of frames then exit cleanly\n"
        << "  --width <px>             Initial framebuffer width (default: 1280)\n"
        << "  --height <px>            Initial framebuffer height (default: 720)\n"
        << "  --project <file.fe3d>    Load an existing Final Effect 3D project file\n"
        << "  --export-project <path>  Save the active scene to a .fe3d project file before exit\n"
        << "  --render-ppm <path.ppm>  In --headless mode, save the final rendered frame as PPM\n";
}

bool parseCli(int argc, char** argv, CliOptions& opts) {
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        auto nextValue = [&](std::string_view flag_name) -> std::optional<std::string_view> {
            if (i + 1 >= argc) {
                std::cerr << "Error: Missing argument for " << flag_name << '\n';
                return std::nullopt;
            }
            return std::string_view(argv[++i]);
        };

        if (arg == "--help" || arg == "-h") {
            opts.show_help = true;
        } else if (arg == "--headless") {
            opts.headless = true;
        } else if (arg == "--frames") {
            auto val = nextValue(arg);
            std::uint64_t n = 0;
            if (!val || !parseUint64Arg(*val, n)) {
                std::cerr << "Error: Invalid integer for --frames\n";
                return false;
            }
            opts.max_frames = n;
        } else if (arg == "--width") {
            auto val = nextValue(arg);
            std::uint64_t w = 0;
            if (!val || !parseUint64Arg(*val, w) || w == 0 || w > 4096) {
                std::cerr << "Error: --width must be in 1..4096\n";
                return false;
            }
            opts.width = static_cast<std::uint32_t>(w);
        } else if (arg == "--height") {
            auto val = nextValue(arg);
            std::uint64_t h = 0;
            if (!val || !parseUint64Arg(*val, h) || h == 0 || h > 4096) {
                std::cerr << "Error: --height must be in 1..4096\n";
                return false;
            }
            opts.height = static_cast<std::uint32_t>(h);
        } else if (arg == "--project") {
            auto val = nextValue(arg);
            if (!val) return false;
            opts.input_project = std::filesystem::path(*val);
        } else if (arg == "--export-project") {
            auto val = nextValue(arg);
            if (!val) return false;
            opts.export_project = std::filesystem::path(*val);
        } else if (arg == "--render-ppm") {
            auto val = nextValue(arg);
            if (!val) return false;
            opts.render_ppm = std::filesystem::path(*val);
        } else {
            std::cerr << "Error: Unknown option '" << arg << "'. Use --help for usage.\n";
            return false;
        }
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    CliOptions opts;
    if (!parseCli(argc, argv, opts)) {
        return EXIT_FAILURE;
    }
    if (opts.show_help) {
        printUsage();
        return EXIT_SUCCESS;
    }

    finaleffect::core::EngineConfig engine_cfg;
    engine_cfg.application_name = "Final Effect 3D Desktop";
    finaleffect::core::Engine engine(engine_cfg);

    finaleffect::platform::WindowDesc win_desc{
        .title = "Final Effect 3D — Stage 1 Prototype",
        .width = opts.width,
        .height = opts.height,
        .resizable = true,
        .vsync = true,
    };

    std::unique_ptr<finaleffect::platform::IWindow> window;
    std::unique_ptr<finaleffect::rendering::IRenderDevice> render_device;
    finaleffect::rendering::SoftwareRenderDevice* software_device_ptr = nullptr;

    if (opts.headless) {
        if (!opts.max_frames.has_value()) {
            opts.max_frames = 3;
        }
        window = std::make_unique<finaleffect::platform::HeadlessWindow>();
        auto init_win = window->initialize(win_desc);
        if (!init_win) {
            std::cerr << "Window init failed: " << init_win.error().format() << '\n';
            return EXIT_FAILURE;
        }
        auto sw = std::make_unique<finaleffect::rendering::SoftwareRenderDevice>();
        software_device_ptr = sw.get();
        render_device = std::move(sw);
    } else {
#if defined(FINALEFFECT_HAS_GLFW_WINDOW)
        if (!opts.render_ppm.empty()) {
            std::cerr << "Error: --render-ppm requires --headless mode.\n";
            return EXIT_FAILURE;
        }
        window = std::make_unique<finaleffect::platform::GlfwWindow>();
        auto init_win = window->initialize(win_desc);
        if (!init_win) {
            std::cerr << "Failed to open desktop OpenGL 3.3 window: " << init_win.error().format() << '\n';
            return EXIT_FAILURE;
        }
        render_device = std::make_unique<finaleffect::rendering::OpenGL33RenderDevice>(
            window->glProcLoader());
#else
        std::cerr
            << "[RenderInitFailed] Interactive desktop windowing is unavailable in this build because "
            << "GLFW (>= 3.3) and system OpenGL development libraries were not present at CMake configure time.\n"
            << "Run with '--headless' (for example: finaleffect_desktop --headless --frames 5) "
            << "or rebuild with system GLFW + OpenGL installed.\n";
        return EXIT_FAILURE;
#endif
    }

    finaleffect::app::StudioSession session(*window, std::move(render_device));

    if (!opts.input_project.empty()) {
        auto load_st = session.loadProjectFromFile(opts.input_project, engine.fileSystem());
        if (!load_st) {
            std::cerr << "Failed to load project: " << load_st.error().format() << '\n';
            window->shutdown();
            return EXIT_FAILURE;
        }
    }

    auto init_st = engine.initialize(&session);
    if (!init_st) {
        std::cerr << "Engine initialization failed: " << init_st.error().format() << '\n';
        window->shutdown();
        return EXIT_FAILURE;
    }

    int exit_code = EXIT_SUCCESS;
    while (!window->shouldClose()) {
        if (opts.max_frames.has_value() && engine.frameCount() >= opts.max_frames.value()) {
            break;
        }
        auto step_st = engine.stepFrame();
        if (!step_st) {
            std::cerr << "Runtime frame error: " << step_st.error().format() << '\n';
            exit_code = EXIT_FAILURE;
            break;
        }
    }

    if (exit_code == EXIT_SUCCESS && !opts.export_project.empty()) {
        auto save_st = session.saveProjectToFile(opts.export_project, engine.fileSystem());
        if (!save_st) {
            std::cerr << "Failed to export project: " << save_st.error().format() << '\n';
            exit_code = EXIT_FAILURE;
        }
    }

    if (exit_code == EXIT_SUCCESS && !opts.render_ppm.empty() && software_device_ptr != nullptr) {
        auto ppm_st = engine.fileSystem().writeTextFile(opts.render_ppm, software_device_ptr->encodePpm());
        if (!ppm_st) {
            std::cerr << "Failed to write PPM image: " << ppm_st.error().format() << '\n';
            exit_code = EXIT_FAILURE;
        }
    }

    engine.shutdown();
    window->shutdown();
    return exit_code;
}
