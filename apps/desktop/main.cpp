// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/app/app_preferences.hpp"
#include "finaleffect/app/editor_controller.hpp"
#include "finaleffect/app/project_hub_service.hpp"
#include "finaleffect/app/studio_session.hpp"
#include "finaleffect/core/application.hpp"
#include "finaleffect/core/engine.hpp"
#include "finaleffect/core/version.hpp"
#include "finaleffect/platform/paths.hpp"
#include "finaleffect/platform/window.hpp"
#include "finaleffect/rendering/opengl33_render_device.hpp"
#include "finaleffect/rendering/software_render_device.hpp"

#if defined(FINALEFFECT_HAS_GLFW_WINDOW)
#include "finaleffect/platform/glfw_window.hpp"
#endif

#if defined(FINALEFFECT_HAS_NATIVE_UI)
#include "finaleffect/ui/imgui_software_rasterizer.hpp"
#include "finaleffect/ui/native_home_hub.hpp"
#include "finaleffect/ui/png_encoder.hpp"
#include <imgui.h>
#endif

#if defined(FINALEFFECT_HAS_IMGUI_OPENGL_GLFW)
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#endif

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace {

struct CliOptions {
    bool show_help{false};
    bool headless{false};
    bool start_in_studio{false};
    bool start_in_hub{false};
    bool list_templates{false};
    bool list_recent{false};
    bool list_recovery{false};
    bool capture_recovery{false};
    bool show_system_info{false};
    bool print_preferences{false};
    bool recover_latest{false};
    bool allow_overwrite{false};
    std::uint32_t width{1280};
    std::uint32_t height{720};
    std::optional<std::uint64_t> max_frames;
    std::optional<std::uint32_t> set_pref_scale;
    std::optional<std::string> set_pref_nav;
    std::string template_slug{"starter"};
    std::string new_project_name{};
    std::filesystem::path config_dir;
    std::filesystem::path new_project_path;
    std::filesystem::path input_project;
    std::filesystem::path export_project;
    std::filesystem::path export_bundle;
    std::filesystem::path render_ppm;
    std::filesystem::path capture_hub_screenshots_dir;
};

std::uint64_t nowEpochMillis() noexcept {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
}

std::uint64_t queryFileByteSize(
    const std::filesystem::path& path,
    const finaleffect::platform::IFileSystem& fs) {
    if (auto bin = fs.readBinaryFile(path); bin) {
        return static_cast<std::uint64_t>(bin.value().size());
    }
    if (auto txt = fs.readTextFile(path); txt) {
        return static_cast<std::uint64_t>(txt.value().size());
    }
    return 0u;
}

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
        << "  --help                              Show this help message\n"
        << "  --headless                          Run without a desktop window using the CPU reference renderer\n"
        << "  --start-in-hub                      Force launch into the Native Desktop Home Hub\n"
        << "  --start-in-studio                   Boot directly into the 3D Studio starter scene\n"
        << "  --frames <N>                        Run a bounded number of frames then exit cleanly\n"
        << "  --width <px>                        Initial framebuffer width (default: 1280)\n"
        << "  --height <px>                       Initial framebuffer height (default: 720)\n"
        << "  --config-dir <dir>                  Override application preferences/hub directory\n"
        << "  --list-templates                    List real C++ ProjectTemplate presets and exit\n"
        << "  --list-recent                       Print indexed recent projects JSON and exit\n"
        << "  --list-recovery                     Print recovery storage state and snapshots JSON and exit\n"
        << "  --new-project <file.fe3d>           Initialize and save a new project from --template\n"
        << "  --template <slug>                   Template slug: starter | minimal_cube | empty_stage | shader_ball\n"
        << "  --project-name <title>              Title for --new-project\n"
        << "  --overwrite                         Confirm overwriting an existing --new-project path\n"
        << "  --project <file.fe3d>               Load an existing Final Effect 3D project file\n"
        << "  --capture-recovery                  Save a recovery checkpoint for the loaded/active project\n"
        << "  --recover-latest                    Recover the most recent autosave snapshot into the session\n"
        << "  --print-preferences                 Print persisted application preferences JSON and exit\n"
        << "  --set-pref-scale <pct>              Persist UI scale preference (85 | 100 | 115 | 125)\n"
        << "  --set-pref-nav <profile>            Persist navigation profile (fe3d_hybrid | blender | maya)\n"
        << "  --system-info                       Print real platform/graphics/capability JSON and exit\n"
        << "  --export-project <path>             Save the active scene to an inline .fe3d project file before exit\n"
        << "  --export-bundle <path>              Save .fe3d metadata + deduplicated .fe3dbin binary mesh archive\n"
        << "  --render-ppm <path.ppm>             In --headless mode, save the final rendered frame as PPM\n"
        << "  --capture-hub-screenshots <dir>     Render and capture genuine PNG screenshots of all 7 Native Hub views\n";
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
        } else if (arg == "--start-in-studio") {
            opts.start_in_studio = true;
        } else if (arg == "--start-in-hub") {
            opts.start_in_hub = true;
        } else if (arg == "--list-templates") {
            opts.list_templates = true;
        } else if (arg == "--list-recent") {
            opts.list_recent = true;
        } else if (arg == "--list-recovery") {
            opts.list_recovery = true;
        } else if (arg == "--capture-recovery") {
            opts.capture_recovery = true;
        } else if (arg == "--system-info") {
            opts.show_system_info = true;
        } else if (arg == "--print-preferences") {
            opts.print_preferences = true;
        } else if (arg == "--recover-latest") {
            opts.recover_latest = true;
        } else if (arg == "--overwrite") {
            opts.allow_overwrite = true;
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
        } else if (arg == "--config-dir") {
            auto val = nextValue(arg);
            if (!val) return false;
            opts.config_dir = std::filesystem::path(*val);
        } else if (arg == "--new-project") {
            auto val = nextValue(arg);
            if (!val) return false;
            opts.new_project_path = std::filesystem::path(*val);
        } else if (arg == "--template") {
            auto val = nextValue(arg);
            if (!val) return false;
            opts.template_slug = std::string(*val);
        } else if (arg == "--project-name") {
            auto val = nextValue(arg);
            if (!val) return false;
            opts.new_project_name = std::string(*val);
        } else if (arg == "--set-pref-scale") {
            auto val = nextValue(arg);
            std::uint64_t pct = 0;
            if (!val || !parseUint64Arg(*val, pct)) {
                std::cerr << "Error: Invalid integer for --set-pref-scale\n";
                return false;
            }
            opts.set_pref_scale = static_cast<std::uint32_t>(pct);
        } else if (arg == "--set-pref-nav") {
            auto val = nextValue(arg);
            if (!val) return false;
            opts.set_pref_nav = std::string(*val);
        } else if (arg == "--project" || arg == "--open-project") {
            auto val = nextValue(arg);
            if (!val) return false;
            opts.input_project = std::filesystem::path(*val);
        } else if (arg == "--export-project") {
            auto val = nextValue(arg);
            if (!val) return false;
            opts.export_project = std::filesystem::path(*val);
        } else if (arg == "--export-bundle") {
            auto val = nextValue(arg);
            if (!val) return false;
            opts.export_bundle = std::filesystem::path(*val);
        } else if (arg == "--render-ppm") {
            auto val = nextValue(arg);
            if (!val) return false;
            opts.render_ppm = std::filesystem::path(*val);
        } else if (arg == "--capture-hub-screenshots") {
            auto val = nextValue(arg);
            if (!val) return false;
            opts.capture_hub_screenshots_dir = std::filesystem::path(*val);
        } else {
            std::cerr << "Error: Unknown option '" << arg << "'. Use --help for usage.\n";
            return false;
        }
    }
    return true;
}

#if defined(FINALEFFECT_HAS_NATIVE_UI)

// Unified Stage 1.1 Desktop Application Host orchestrating the single window/GL context
// across NativeHomeHub and the existing StudioSession without duplicating engine services.
class DesktopApplicationHost final : public finaleffect::core::IApplication {
public:
    DesktopApplicationHost(
        finaleffect::platform::IWindow& window,
        finaleffect::app::StudioSession& session,
        finaleffect::ui::NativeHomeHub& home_hub,
        finaleffect::rendering::OpenGL33RenderDevice* opengl_device,
        finaleffect::rendering::SoftwareRenderDevice* software_device,
        bool use_opengl_imgui_backend)
        : window_(window),
          session_(session),
          home_hub_(home_hub),
          opengl_device_(opengl_device),
          software_device_(software_device),
          use_opengl_imgui_backend_(use_opengl_imgui_backend) {}

    [[nodiscard]] finaleffect::core::Status onInitialize(finaleffect::core::Engine& engine) override {
        return session_.onInitialize(engine);
    }

    [[nodiscard]] finaleffect::core::Status onUpdate(
        finaleffect::core::Engine& engine,
        const finaleffect::core::FrameTimings& timings) override {
        window_.pollEvents();

        const auto pending_events = window_.consumeInputEvents();
        if (!use_opengl_imgui_backend_) {
            home_hub_.feedHeadlessInputEvents(pending_events);
        }

        // Pre-NewFrame scale & TrueType font atlas check
        const bool font_rebuilt = home_hub_.prepareFrameScale();
#if defined(FINALEFFECT_HAS_IMGUI_OPENGL_GLFW)
        if (use_opengl_imgui_backend_) {
            if (font_rebuilt) {
                ImGui_ImplOpenGL3_DestroyDeviceObjects();
                ImGui_ImplOpenGL3_CreateDeviceObjects();
            }
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
        } else
#endif
        {
            (void)font_rebuilt;
            ImGuiIO& io = ImGui::GetIO();
            const auto fb = window_.framebufferSize();
            io.DisplaySize = ImVec2(
                static_cast<float>(std::max(1u, fb.width)),
                static_cast<float>(std::max(1u, fb.height)));
            io.DeltaTime = std::max(1.0e-4f, static_cast<float>(timings.delta_seconds));
        }

        ImGui::NewFrame();
        home_hub_.renderCurrentFrameContent();
        ImGui::Render();

        const auto fb = window_.framebufferSize();
        auto& render_dev = session_.renderer().device();

        if (home_hub_.mode() == finaleffect::ui::DesktopMode::HomeHub) {
            auto rsz = render_dev.resize(std::max(1u, fb.width), std::max(1u, fb.height));
            if (!rsz) return rsz;
            // Clear to Stage 0 Obsidian Slate (#0F1115)
            const finaleffect::rendering::FrameClearDesc clear{
                .clear_color = finaleffect::math::Vec4{15.0f / 255.0f, 17.0f / 255.0f, 21.0f / 255.0f, 1.0f},
                .clear_depth = 1.0f,
            };
            auto bf = render_dev.beginFrame(clear);
            if (!bf) return bf;
            auto ef = render_dev.endFrame();
            if (!ef) return ef;
        } else {
            // Forward uncaptured input events to the 3D Studio viewport
            const ImGuiIO& io = ImGui::GetIO();
            for (const auto& ev : pending_events) {
                const bool is_mouse = std::holds_alternative<finaleffect::platform::MouseButtonEvent>(ev) ||
                                      std::holds_alternative<finaleffect::platform::CursorMoveEvent>(ev) ||
                                      std::holds_alternative<finaleffect::platform::ScrollEvent>(ev);
                const bool is_key = std::holds_alternative<finaleffect::platform::KeyEvent>(ev);
                if (is_mouse && io.WantCaptureMouse) continue;
                if (is_key && io.WantCaptureKeyboard) continue;
                auto ev_st = session_.handleInputEvent(ev);
                if (!ev_st) return ev_st;
            }
            auto st = session_.onUpdate(engine, timings);
            if (!st) return st;
        }

        // Composite Native UI Draw Lists onto the active framebuffer
#if defined(FINALEFFECT_HAS_IMGUI_OPENGL_GLFW)
        if (use_opengl_imgui_backend_) {
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        } else
#endif
        if (software_device_ != nullptr) {
            compositeSoftwareUiFrame(*software_device_);
        }

        if (capture_next_frame_) {
            capture_status_ = captureActiveFramebuffer(captured_rgba_, captured_w_, captured_h_);
            capture_next_frame_ = false;
        }

        window_.swapBuffers();
        return finaleffect::core::Status::ok();
    }

    void onShutdown(finaleffect::core::Engine& engine) noexcept override {
        session_.onShutdown(engine);
    }

    void requestFramebufferCapture() noexcept {
        capture_next_frame_ = true;
        capture_status_ = finaleffect::core::Status::ok();
    }

    [[nodiscard]] const finaleffect::core::Status& lastCaptureStatus() const noexcept { return capture_status_; }
    [[nodiscard]] const std::vector<std::uint8_t>& capturedRgba() const noexcept { return captured_rgba_; }
    [[nodiscard]] std::uint32_t capturedWidth() const noexcept { return captured_w_; }
    [[nodiscard]] std::uint32_t capturedHeight() const noexcept { return captured_h_; }

private:
    void compositeSoftwareUiFrame(finaleffect::rendering::SoftwareRenderDevice& sw) {
        const std::uint32_t w = sw.width();
        const std::uint32_t h = sw.height();
        if (w == 0 || h == 0) return;

        software_rgba_scratch_.resize(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4u);
        const auto& src = sw.colorBuffer();
        for (std::size_t i = 0; i < src.size(); ++i) {
            software_rgba_scratch_[i * 4u + 0u] = src[i].r;
            software_rgba_scratch_[i * 4u + 1u] = src[i].g;
            software_rgba_scratch_[i * 4u + 2u] = src[i].b;
            software_rgba_scratch_[i * 4u + 3u] = src[i].a;
        }

        unsigned char* atlas_px = nullptr;
        int atlas_w = 0;
        int atlas_h = 0;
        ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&atlas_px, &atlas_w, &atlas_h);
        finaleffect::ui::rasterizeImDrawDataToRgba8(
            ImGui::GetDrawData(),
            finaleffect::ui::FontAtlasRgbaView{atlas_px, atlas_w, atlas_h},
            w,
            h,
            software_rgba_scratch_);
    }

    [[nodiscard]] finaleffect::core::Status captureActiveFramebuffer(
        std::vector<std::uint8_t>& out_rgba,
        std::uint32_t& out_w,
        std::uint32_t& out_h) {
        if (opengl_device_ != nullptr) {
            return opengl_device_->readFramebufferRgba8(out_rgba, out_w, out_h);
        }
        if (software_device_ != nullptr) {
            out_w = software_device_->width();
            out_h = software_device_->height();
            out_rgba = software_rgba_scratch_;
            return finaleffect::core::Status::ok();
        }
        return finaleffect::core::Status::fail(
            finaleffect::core::ErrorCode::InvalidState,
            "No readable framebuffer device is active");
    }

    finaleffect::platform::IWindow& window_;
    finaleffect::app::StudioSession& session_;
    finaleffect::ui::NativeHomeHub& home_hub_;
    finaleffect::rendering::OpenGL33RenderDevice* opengl_device_{nullptr};
    finaleffect::rendering::SoftwareRenderDevice* software_device_{nullptr};
    bool use_opengl_imgui_backend_{false};

    bool capture_next_frame_{false};
    finaleffect::core::Status capture_status_{finaleffect::core::Status::ok()};
    std::vector<std::uint8_t> captured_rgba_{};
    std::uint32_t captured_w_{0};
    std::uint32_t captured_h_{0};
    std::vector<std::uint8_t> software_rgba_scratch_{};
};

bool verifyNonUniformFrame(const std::vector<std::uint8_t>& rgba, std::uint32_t w, std::uint32_t h) {
    if (w == 0 || h == 0 || rgba.size() < static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4u) {
        return false;
    }
    std::unordered_set<std::uint32_t> distinct_colors;
    const std::size_t stride = std::max<std::size_t>(1u, (static_cast<std::size_t>(w) * static_cast<std::size_t>(h)) / 20000u);
    const std::size_t pixel_count = static_cast<std::size_t>(w) * static_cast<std::size_t>(h);
    for (std::size_t p = 0; p < pixel_count && distinct_colors.size() < 24u; p += stride) {
        const std::uint32_t packed =
            (static_cast<std::uint32_t>(rgba[p * 4u + 0u]) << 16u) |
            (static_cast<std::uint32_t>(rgba[p * 4u + 1u]) << 8u) |
            static_cast<std::uint32_t>(rgba[p * 4u + 2u]);
        distinct_colors.insert(packed);
    }
    return distinct_colors.size() >= 12u;
}

bool runScreenshotCaptureSuite(
    finaleffect::core::Engine& engine,
    DesktopApplicationHost& host,
    finaleffect::ui::NativeHomeHub& hub,
    std::uint32_t scale_pct,
    const std::filesystem::path& out_dir) {
    if (!engine.fileSystem().createDirectories(out_dir)) {
        std::cerr << "Failed to create screenshot output directory: " << out_dir << '\n';
        return false;
    }

    auto stepAndWritePng = [&](const std::string& base_name) -> bool {
        if (!engine.runFrames(2)) {
            return false;
        }
        host.requestFramebufferCapture();
        if (!engine.stepFrame()) {
            return false;
        }
        if (!host.lastCaptureStatus()) {
            std::cerr << "Framebuffer capture failed for " << base_name << ": "
                      << host.lastCaptureStatus().error().format() << '\n';
            return false;
        }
        const auto w = host.capturedWidth();
        const auto h = host.capturedHeight();
        if (!verifyNonUniformFrame(host.capturedRgba(), w, h)) {
            std::cerr << "Captured frame for " << base_name
                      << " failed non-uniform UI pixel verification (" << w << "x" << h << ")\n";
            return false;
        }
        auto png_res = finaleffect::ui::encodeRgba8ToPng(w, h, host.capturedRgba());
        if (!png_res) {
            std::cerr << "PNG encoding failed for " << base_name << ": "
                      << png_res.error().format() << '\n';
            return false;
        }
        const std::string file_name =
            base_name + "_" + std::to_string(w) + "x" + std::to_string(h) + "_" +
            std::to_string(scale_pct) + "pct.png";
        const auto out_path = out_dir / file_name;
        auto wr = engine.fileSystem().writeBinaryFile(out_path, png_res.value());
        if (!wr) {
            std::cerr << "Failed to write screenshot PNG " << out_path << ": "
                      << wr.error().format() << '\n';
            return false;
        }
        std::cout << "[ScreenshotVerified] " << out_path.generic_string()
                  << " (" << w << "x" << h << ", " << png_res.value().size() << " bytes)\n";
        return true;
    };

    struct ViewShot {
        finaleffect::ui::HomeHubViewId id;
        const char* stem;
    };
    constexpr ViewShot kShots[7] = {
        {finaleffect::ui::HomeHubViewId::ProjectsStart, "01_projects_start"},
        {finaleffect::ui::HomeHubViewId::Templates, "02_templates"},
        {finaleffect::ui::HomeHubViewId::NewProject, "03_new_project"},
        {finaleffect::ui::HomeHubViewId::OpenProject, "04_open_project"},
        {finaleffect::ui::HomeHubViewId::Recovery, "05_recovery_autosave"},
        {finaleffect::ui::HomeHubViewId::Preferences, "06_preferences"},
        {finaleffect::ui::HomeHubViewId::About, "07_about_system_info"},
    };

    for (const auto& shot : kShots) {
        hub.navigateTo(shot.id);
        if (!stepAndWritePng(shot.stem)) {
            return false;
        }
    }

    // Verify genuine Home Hub -> Create Starter Showcase -> 3D Studio graphical transition
    if (!hub.executeQuickCreateTemplate(finaleffect::app::ProjectTemplate::StarterShowcase, true)) {
        std::cerr << "Failed to create StarterShowcase project during screenshot suite\n";
        return false;
    }
    if (!stepAndWritePng("08_home_to_studio_transition")) {
        return false;
    }

    // Verify genuine 3D Studio -> Projects & Start populated return transition
    hub.returnToHomeHub();
    if (!stepAndWritePng("09_studio_to_home_populated")) {
        return false;
    }

    return true;
}

#endif // FINALEFFECT_HAS_NATIVE_UI

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
    if (opts.list_templates) {
        for (const auto& t : finaleffect::app::TemplateCatalog::all()) {
            std::cout << t.slug << " | " << t.title << " [" << t.category << "] - " << t.summary << '\n';
        }
        return EXIT_SUCCESS;
    }

    finaleffect::core::EngineConfig engine_cfg;
    engine_cfg.application_name = "Final Effect 3D Desktop";
    finaleffect::core::Engine engine(engine_cfg);

    finaleffect::platform::AppPaths os_paths{};
    if (auto queried_paths = finaleffect::platform::queryAppPaths("finaleffect3d"); queried_paths) {
        os_paths = std::move(queried_paths).value();
    } else {
        os_paths.user_config_directory = std::filesystem::temp_directory_path() / "finaleffect3d" / "config";
        os_paths.user_cache_directory = std::filesystem::temp_directory_path() / "finaleffect3d" / "cache";
        os_paths.user_data_directory = std::filesystem::temp_directory_path() / "finaleffect3d" / "data";
    }

    const std::filesystem::path hub_config_dir =
        opts.config_dir.empty() ? os_paths.user_config_directory : opts.config_dir;
    const std::filesystem::path hub_recovery_dir =
        opts.config_dir.empty() ? (os_paths.user_cache_directory / "recovery") : (opts.config_dir / "recovery");
    const std::filesystem::path default_projects_dir =
        opts.config_dir.empty() ? (os_paths.user_data_directory / "projects") : (opts.config_dir / "projects");
    const std::filesystem::path prefs_path = hub_config_dir / "preferences.json";
    const std::filesystem::path recents_path = hub_config_dir / "recent_projects.json";

    finaleffect::app::AppPreferences prefs{};
    if (auto loaded_prefs = finaleffect::app::AppPreferencesStore::loadOrDefault(prefs_path, engine.fileSystem());
        loaded_prefs) {
        prefs = std::move(loaded_prefs).value();
    }

    bool prefs_modified = false;
    if (opts.set_pref_scale.has_value()) {
        prefs.ui_scale_percent = opts.set_pref_scale.value();
        prefs.sanitize();
        prefs_modified = true;
    }
    if (opts.set_pref_nav.has_value()) {
        const auto parsed_nav = finaleffect::app::parseNavigationProfileSlug(opts.set_pref_nav.value());
        if (!parsed_nav.has_value()) {
            std::cerr << "Error: Unknown navigation profile '" << opts.set_pref_nav.value() << "'\n";
            return EXIT_FAILURE;
        }
        prefs.navigation_profile = parsed_nav.value();
        prefs_modified = true;
    }
    if (prefs_modified) {
        auto save_pref_st = finaleffect::app::AppPreferencesStore::saveToFile(prefs, prefs_path, engine.fileSystem());
        if (!save_pref_st) {
            std::cerr << "Failed to persist application preferences: " << save_pref_st.error().format() << '\n';
            return EXIT_FAILURE;
        }
    }
    if (opts.print_preferences) {
        auto pref_json = finaleffect::app::AppPreferencesStore::serializeToString(prefs, true);
        if (!pref_json) {
            std::cerr << "Failed to serialize preferences: " << pref_json.error().format() << '\n';
            return EXIT_FAILURE;
        }
        std::cout << pref_json.value() << '\n';
        return EXIT_SUCCESS;
    }

    finaleffect::app::RecentProjectsIndex recent_index{};
    (void)recent_index.loadFromFile(recents_path, engine.fileSystem());
    if (opts.list_recent) {
        const auto rows = recent_index.query();
        std::cout << "{\n  \"count\": " << rows.size() << ",\n  \"entries\": [\n";
        for (std::size_t i = 0; i < rows.size(); ++i) {
            const auto& r = rows[i];
            std::cout << "    {\"id\": \"" << r.id
                      << "\", \"project_name\": \"" << r.project_name
                      << "\", \"file_path\": \"" << r.file_path
                      << "\", \"object_count\": " << r.object_count
                      << ", \"mesh_count\": " << r.mesh_count
                      << ", \"exists_on_disk\": " << (r.exists_on_disk ? "true" : "false")
                      << ", \"pinned\": " << (r.pinned ? "true" : "false") << "}"
                      << (i + 1 < rows.size() ? "," : "") << '\n';
        }
        std::cout << "  ]\n}\n";
        return EXIT_SUCCESS;
    }

    finaleffect::app::RecoveryService recovery_service{};
    (void)recovery_service.initialize(hub_recovery_dir, engine.fileSystem());
    if (opts.list_recovery) {
        const bool avail = recovery_service.storageState() == finaleffect::app::RecoveryStorageState::Available;
        const auto& snaps = recovery_service.snapshots();
        std::cout << "{\n  \"storage_state\": \"" << (avail ? "available" : "unavailable")
                  << "\",\n  \"had_unclean_session\": "
                  << (recovery_service.hadUncleanPreviousSession() ? "true" : "false")
                  << ",\n  \"snapshot_count\": " << snaps.size() << ",\n  \"snapshots\": [\n";
        for (std::size_t i = 0; i < snaps.size(); ++i) {
            const auto& s = snaps[i];
            std::cout << "    {\"id\": \"" << s.id
                      << "\", \"project_name\": \"" << s.project_name
                      << "\", \"slot_index\": " << s.slot_index
                      << ", \"object_count\": " << s.object_count
                      << ", \"byte_size\": " << s.byte_size << "}"
                      << (i + 1 < snaps.size() ? "," : "") << '\n';
        }
        std::cout << "  ]\n}\n";
        (void)recovery_service.markCleanShutdown(engine.fileSystem());
        return EXIT_SUCCESS;
    }

    finaleffect::platform::WindowDesc win_desc{
        .title = "Final Effect 3D — Projects & Start",
        .width = opts.width,
        .height = opts.height,
        .resizable = true,
        .vsync = opts.capture_hub_screenshots_dir.empty(),
    };

    std::unique_ptr<finaleffect::platform::IWindow> window;
    std::unique_ptr<finaleffect::rendering::IRenderDevice> render_device;
    finaleffect::rendering::SoftwareRenderDevice* software_device_ptr = nullptr;
    finaleffect::rendering::OpenGL33RenderDevice* opengl_device_ptr = nullptr;
#if defined(FINALEFFECT_HAS_GLFW_WINDOW)
    finaleffect::platform::GlfwWindow* glfw_window_ptr = nullptr;
#endif

    if (opts.headless) {
        if (!opts.max_frames.has_value() && opts.capture_hub_screenshots_dir.empty()) {
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
        auto glfw_win = std::make_unique<finaleffect::platform::GlfwWindow>();
        auto init_win = glfw_win->initialize(win_desc);
        if (!init_win) {
            std::cerr << "Failed to open desktop OpenGL 3.3 window: " << init_win.error().format() << '\n';
            return EXIT_FAILURE;
        }
        glfw_window_ptr = glfw_win.get();
        window = std::move(glfw_win);
        auto gl_dev = std::make_unique<finaleffect::rendering::OpenGL33RenderDevice>(
            window->glProcLoader());
        opengl_device_ptr = gl_dev.get();
        render_device = std::move(gl_dev);
#else
        std::cerr
            << "[RenderInitFailed] Interactive desktop windowing is unavailable in this build because "
            << "GLFW (>= 3.3) and system OpenGL development libraries were not present at CMake configure time.\n"
            << "Run with '--headless' (for example: finaleffect_desktop --headless --frames 5) "
            << "or rebuild with system GLFW + OpenGL installed.\n";
        return EXIT_FAILURE;
#endif
    }

    finaleffect::app::StudioSessionConfig session_cfg{};
#if defined(FINALEFFECT_HAS_NATIVE_UI)
    session_cfg.poll_window_events_on_update = false;
    session_cfg.consume_window_input_on_update = false;
    session_cfg.swap_buffers_on_update = false;
#endif

    finaleffect::app::StudioSession session(*window, std::move(render_device), session_cfg);
    finaleffect::app::EditorController editor(session);

    const bool explicit_scene_requested =
        !opts.new_project_path.empty() ||
        !opts.input_project.empty() ||
        opts.recover_latest ||
        opts.start_in_studio ||
        !opts.render_ppm.empty();

#if defined(FINALEFFECT_HAS_NATIVE_UI)
    const finaleffect::ui::DesktopMode initial_mode =
        opts.start_in_hub
            ? finaleffect::ui::DesktopMode::HomeHub
            : (explicit_scene_requested ||
                       prefs.startup_environment == finaleffect::app::StartupEnvironment::DefaultScene
                   ? finaleffect::ui::DesktopMode::Studio
                   : finaleffect::ui::DesktopMode::HomeHub);

    finaleffect::ui::NativeHomeHub home_hub(
        *window,
        engine.fileSystem(),
        session,
        editor,
        prefs,
        recent_index,
        recovery_service,
        finaleffect::ui::NativeHomeHubPaths{
            .preferences_path = prefs_path,
            .recent_projects_path = recents_path,
            .recovery_directory = hub_recovery_dir,
            .default_projects_directory = default_projects_dir,
        });

    auto hub_init_st = home_hub.initialize(initial_mode);
    if (!hub_init_st) {
        std::cerr << "Native Home Hub initialization failed: " << hub_init_st.error().format() << '\n';
        window->shutdown();
        return EXIT_FAILURE;
    }

    bool imgui_gl_backend_initialized = false;
#if defined(FINALEFFECT_HAS_IMGUI_OPENGL_GLFW)
    if (glfw_window_ptr != nullptr && glfw_window_ptr->nativeHandle() != nullptr) {
        if (!ImGui_ImplGlfw_InitForOpenGL(glfw_window_ptr->nativeHandle(), true) ||
            !ImGui_ImplOpenGL3_Init("#version 330 core")) {
            std::cerr << "Failed to initialize OpenGL 3.3 / GLFW native UI backend\n";
            home_hub.shutdown();
            window->shutdown();
            return EXIT_FAILURE;
        }
        imgui_gl_backend_initialized = true;
    }
#endif

    DesktopApplicationHost app_host(
        *window,
        session,
        home_hub,
        opengl_device_ptr,
        software_device_ptr,
        imgui_gl_backend_initialized);

    auto cleanupDesktop = [&]() noexcept {
#if defined(FINALEFFECT_HAS_IMGUI_OPENGL_GLFW)
        if (imgui_gl_backend_initialized) {
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            imgui_gl_backend_initialized = false;
        }
#endif
        home_hub.shutdown();
        engine.shutdown();
        window->shutdown();
    };

    auto init_st = engine.initialize(&app_host);
#else
    (void)opengl_device_ptr;
    (void)explicit_scene_requested;
    auto cleanupDesktop = [&]() noexcept {
        engine.shutdown();
        window->shutdown();
    };
    auto init_st = engine.initialize(&session);
#endif

    if (!init_st) {
        std::cerr << "Engine initialization failed: " << init_st.error().format() << '\n';
        cleanupDesktop();
        return EXIT_FAILURE;
    }

    if (opts.show_system_info) {
        const auto sys_info = finaleffect::app::querySystemInfo(session, recovery_service.storageState());
        auto json_res = finaleffect::app::serializeSystemInfoJson(sys_info, true);
        if (!json_res) {
            std::cerr << "Failed to serialize system info: " << json_res.error().format() << '\n';
            cleanupDesktop();
            return EXIT_FAILURE;
        }
        std::cout << json_res.value() << '\n';
        (void)recovery_service.markCleanShutdown(engine.fileSystem());
        cleanupDesktop();
        return EXIT_SUCCESS;
    }

    if (!opts.new_project_path.empty()) {
        const auto parsed_tmpl = finaleffect::app::TemplateCatalog::parseSlug(opts.template_slug);
        if (!parsed_tmpl.has_value()) {
            std::cerr << "Error: Unknown project template '" << opts.template_slug << "'\n";
            cleanupDesktop();
            return EXIT_FAILURE;
        }
        const std::string resolved_title = opts.new_project_name.empty()
            ? opts.new_project_path.stem().string()
            : opts.new_project_name;
        auto create_st = finaleffect::app::createNewProjectAtPath(
            editor,
            parsed_tmpl.value(),
            resolved_title,
            prefs.default_author,
            opts.new_project_path,
            opts.allow_overwrite,
            engine.fileSystem());
        if (!create_st) {
            std::cerr << "Failed to create project: " << create_st.error().format() << '\n';
            cleanupDesktop();
            return EXIT_FAILURE;
        }
        recent_index.recordProject(
            session.document(),
            opts.new_project_path,
            queryFileByteSize(opts.new_project_path, engine.fileSystem()),
            nowEpochMillis(),
            prefs.recent_projects_limit);
        (void)recent_index.saveToFile(recents_path, engine.fileSystem());
#if defined(FINALEFFECT_HAS_NATIVE_UI)
        home_hub.enterStudioMode();
#endif
    }

    if (!opts.input_project.empty()) {
        auto load_st = editor.loadProjectFile(opts.input_project, engine.fileSystem());
        if (!load_st) {
            std::cerr << "Failed to load project: " << load_st.error().format() << '\n';
            cleanupDesktop();
            return EXIT_FAILURE;
        }
        recent_index.recordProject(
            session.document(),
            opts.input_project,
            queryFileByteSize(opts.input_project, engine.fileSystem()),
            nowEpochMillis(),
            prefs.recent_projects_limit);
        (void)recent_index.saveToFile(recents_path, engine.fileSystem());
#if defined(FINALEFFECT_HAS_NATIVE_UI)
        home_hub.enterStudioMode();
#endif
    }

    if (opts.recover_latest) {
        auto recovered = recovery_service.recoverSnapshot("latest", engine.fileSystem());
        if (!recovered) {
            std::cerr << "Failed to recover latest snapshot: " << recovered.error().format() << '\n';
            cleanupDesktop();
            return EXIT_FAILURE;
        }
        auto serialized = finaleffect::project::ProjectSerializer::serializeToString(recovered.value(), false);
        if (!serialized) {
            std::cerr << "Failed to restore recovered snapshot: " << serialized.error().format() << '\n';
            cleanupDesktop();
            return EXIT_FAILURE;
        }
        auto imp_st = editor.importInlineProjectJson(serialized.value());
        if (!imp_st) {
            std::cerr << "Failed to activate recovered snapshot: " << imp_st.error().format() << '\n';
            cleanupDesktop();
            return EXIT_FAILURE;
        }
#if defined(FINALEFFECT_HAS_NATIVE_UI)
        home_hub.enterStudioMode();
#endif
    }

    if (opts.capture_recovery) {
        session.syncDocumentState(editor.selectedMaterial());
        auto snap_res = recovery_service.captureSnapshot(
            session.document(),
            editor.activeProjectPath(),
            "Manual Desktop Checkpoint",
            nowEpochMillis(),
            engine.fileSystem());
        if (!snap_res) {
            std::cerr << "Failed to capture recovery snapshot: " << snap_res.error().format() << '\n';
            cleanupDesktop();
            return EXIT_FAILURE;
        }
    }

#if defined(FINALEFFECT_HAS_NATIVE_UI)
    if (!opts.capture_hub_screenshots_dir.empty()) {
        const bool ok = runScreenshotCaptureSuite(
            engine,
            app_host,
            home_hub,
            prefs.ui_scale_percent,
            opts.capture_hub_screenshots_dir);
        if (ok) {
            (void)recovery_service.markCleanShutdown(engine.fileSystem());
        }
        cleanupDesktop();
        return ok ? EXIT_SUCCESS : EXIT_FAILURE;
    }
#endif

    int exit_code = EXIT_SUCCESS;
    while (true) {
        if (opts.max_frames.has_value() && engine.frameCount() >= opts.max_frames.value()) {
            break;
        }
        if (window->shouldClose()) {
#if defined(FINALEFFECT_HAS_NATIVE_UI)
            if (!opts.max_frames.has_value() && home_hub.interceptWindowCloseIfDirty()) {
                // Keep running so the Unsaved Changes Exit Confirmation Modal is displayed
            } else {
                break;
            }
#else
            break;
#endif
        }

        auto step_st = engine.stepFrame();
        if (!step_st) {
            std::cerr << "Runtime frame error: " << step_st.error().format() << '\n';
            exit_code = EXIT_FAILURE;
            break;
        }

#if defined(FINALEFFECT_HAS_NATIVE_UI)
        home_hub.tickPeriodicAutosave(nowEpochMillis());
#endif
    }

    if (exit_code == EXIT_SUCCESS && !opts.export_project.empty()) {
        session.syncDocumentState(editor.selectedMaterial());
        auto save_st = session.saveProjectToFile(opts.export_project, engine.fileSystem());
        if (!save_st) {
            std::cerr << "Failed to export project: " << save_st.error().format() << '\n';
            exit_code = EXIT_FAILURE;
        } else {
            editor.setActiveProjectPath(opts.export_project.lexically_normal().generic_string());
            editor.markDocumentSaved();
            recent_index.recordProject(
                session.document(),
                opts.export_project,
                queryFileByteSize(opts.export_project, engine.fileSystem()),
                nowEpochMillis(),
                prefs.recent_projects_limit);
            (void)recent_index.saveToFile(recents_path, engine.fileSystem());
        }
    }

    if (exit_code == EXIT_SUCCESS && !opts.export_bundle.empty()) {
        auto bundle_st = editor.saveBundleProject(opts.export_bundle, engine.fileSystem());
        if (!bundle_st) {
            std::cerr << "Failed to export project bundle: " << bundle_st.error().format() << '\n';
            exit_code = EXIT_FAILURE;
        } else {
            recent_index.recordProject(
                session.document(),
                opts.export_bundle,
                queryFileByteSize(opts.export_bundle, engine.fileSystem()),
                nowEpochMillis(),
                prefs.recent_projects_limit);
            (void)recent_index.saveToFile(recents_path, engine.fileSystem());
        }
    }

    if (exit_code == EXIT_SUCCESS && !opts.render_ppm.empty() && software_device_ptr != nullptr) {
        auto ppm_st = engine.fileSystem().writeTextFile(opts.render_ppm, software_device_ptr->encodePpm());
        if (!ppm_st) {
            std::cerr << "Failed to write PPM image: " << ppm_st.error().format() << '\n';
            exit_code = EXIT_FAILURE;
        }
    }

    if (exit_code == EXIT_SUCCESS) {
        (void)recovery_service.markCleanShutdown(engine.fileSystem());
    }
    cleanupDesktop();
    return exit_code;
}
