// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/app/app_preferences.hpp"
#include "finaleffect/app/editor_controller.hpp"
#include "finaleffect/app/project_hub_service.hpp"
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
#include <chrono>
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
};

std::uint64_t nowEpochMillis() noexcept {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
}

std::uint64_t queryFileByteSize(
    const std::filesystem::path& path,
    const finaleffect::platform::IFileSystem& fs) {
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
        << "  --help                      Show this help message\n"
        << "  --headless                  Run without a desktop window using the CPU reference renderer\n"
        << "  --frames <N>                Run a bounded number of frames then exit cleanly\n"
        << "  --width <px>                Initial framebuffer width (default: 1280)\n"
        << "  --height <px>               Initial framebuffer height (default: 720)\n"
        << "  --config-dir <dir>          Override application preferences/hub directory\n"
        << "  --list-templates            List real C++ ProjectTemplate presets and exit\n"
        << "  --list-recent               Print indexed recent projects JSON and exit\n"
        << "  --list-recovery             Print recovery storage state and snapshots JSON and exit\n"
        << "  --new-project <file.fe3d>   Initialize and save a new project from --template\n"
        << "  --template <slug>           Template slug: starter | minimal_cube | empty_stage | shader_ball\n"
        << "  --project-name <title>      Title for --new-project\n"
        << "  --overwrite                 Confirm overwriting an existing --new-project path\n"
        << "  --project <file.fe3d>       Load an existing Final Effect 3D project file\n"
        << "  --capture-recovery          Save a recovery checkpoint for the loaded/active project\n"
        << "  --recover-latest            Recover the most recent autosave snapshot into the session\n"
        << "  --print-preferences         Print persisted application preferences JSON and exit\n"
        << "  --set-pref-scale <pct>      Persist UI scale preference (85 | 100 | 115 | 125)\n"
        << "  --set-pref-nav <profile>    Persist navigation profile (fe3d_hybrid | blender | maya)\n"
        << "  --system-info               Print real platform/graphics/capability JSON and exit\n"
        << "  --export-project <path>     Save the active scene to an inline .fe3d project file before exit\n"
        << "  --export-bundle <path>      Save .fe3d metadata + deduplicated .fe3dbin binary mesh archive\n"
        << "  --render-ppm <path.ppm>     In --headless mode, save the final rendered frame as PPM\n";
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
    if (opts.list_templates) {
        for (const auto& t : finaleffect::app::TemplateCatalog::all()) {
            std::cout << t.slug << " | " << t.title << " [" << t.category << "] - " << t.summary << '\n';
        }
        return EXIT_SUCCESS;
    }

    finaleffect::core::EngineConfig engine_cfg;
    engine_cfg.application_name = "Final Effect 3D Desktop";
    finaleffect::core::Engine engine(engine_cfg);

    const std::filesystem::path hub_config_dir =
        opts.config_dir.empty() ? engine.appPaths().user_config_directory : opts.config_dir;
    const std::filesystem::path hub_recovery_dir =
        opts.config_dir.empty() ? (engine.appPaths().user_cache_directory / "recovery") : (opts.config_dir / "recovery");
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
        .title = "Final Effect 3D — Desktop Studio",
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
    finaleffect::app::EditorController editor(session);

    auto init_st = engine.initialize(&session);
    if (!init_st) {
        std::cerr << "Engine initialization failed: " << init_st.error().format() << '\n';
        window->shutdown();
        return EXIT_FAILURE;
    }

    if (opts.show_system_info) {
        const auto sys_info = finaleffect::app::querySystemInfo(session, recovery_service.storageState());
        auto json_res = finaleffect::app::serializeSystemInfoJson(sys_info, true);
        if (!json_res) {
            std::cerr << "Failed to serialize system info: " << json_res.error().format() << '\n';
            engine.shutdown();
            window->shutdown();
            return EXIT_FAILURE;
        }
        std::cout << json_res.value() << '\n';
        (void)recovery_service.markCleanShutdown(engine.fileSystem());
        engine.shutdown();
        window->shutdown();
        return EXIT_SUCCESS;
    }

    if (!opts.new_project_path.empty()) {
        const auto parsed_tmpl = finaleffect::app::TemplateCatalog::parseSlug(opts.template_slug);
        if (!parsed_tmpl.has_value()) {
            std::cerr << "Error: Unknown project template '" << opts.template_slug << "'\n";
            engine.shutdown();
            window->shutdown();
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
            engine.shutdown();
            window->shutdown();
            return EXIT_FAILURE;
        }
        recent_index.recordProject(
            session.document(),
            opts.new_project_path,
            queryFileByteSize(opts.new_project_path, engine.fileSystem()),
            nowEpochMillis(),
            prefs.recent_projects_limit);
        (void)recent_index.saveToFile(recents_path, engine.fileSystem());
    }

    if (!opts.input_project.empty()) {
        auto load_st = editor.loadProjectFile(opts.input_project, engine.fileSystem());
        if (!load_st) {
            std::cerr << "Failed to load project: " << load_st.error().format() << '\n';
            engine.shutdown();
            window->shutdown();
            return EXIT_FAILURE;
        }
        recent_index.recordProject(
            session.document(),
            opts.input_project,
            queryFileByteSize(opts.input_project, engine.fileSystem()),
            nowEpochMillis(),
            prefs.recent_projects_limit);
        (void)recent_index.saveToFile(recents_path, engine.fileSystem());
    }

    if (opts.recover_latest) {
        auto recovered = recovery_service.recoverSnapshot("latest", engine.fileSystem());
        if (!recovered) {
            std::cerr << "Failed to recover latest snapshot: " << recovered.error().format() << '\n';
            engine.shutdown();
            window->shutdown();
            return EXIT_FAILURE;
        }
        auto serialized = finaleffect::project::ProjectSerializer::serializeToString(recovered.value(), false);
        if (!serialized) {
            std::cerr << "Failed to restore recovered snapshot: " << serialized.error().format() << '\n';
            engine.shutdown();
            window->shutdown();
            return EXIT_FAILURE;
        }
        auto imp_st = editor.importInlineProjectJson(serialized.value());
        if (!imp_st) {
            std::cerr << "Failed to activate recovered snapshot: " << imp_st.error().format() << '\n';
            engine.shutdown();
            window->shutdown();
            return EXIT_FAILURE;
        }
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
            engine.shutdown();
            window->shutdown();
            return EXIT_FAILURE;
        }
    }

    int exit_code = EXIT_SUCCESS;
    std::uint64_t last_autosave_ms = nowEpochMillis();
    std::uint64_t last_autosave_token = session.commandStack().currentStateToken();

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

        if (prefs.autosave_enabled &&
            recovery_service.storageState() == finaleffect::app::RecoveryStorageState::Available &&
            editor.isDocumentDirty()) {
            const std::uint64_t now_ms = nowEpochMillis();
            const std::uint64_t interval_ms =
                static_cast<std::uint64_t>(prefs.autosave_interval_seconds) * 1000ULL;
            const std::uint64_t cur_tok = session.commandStack().currentStateToken();
            if (cur_tok != last_autosave_token && (now_ms - last_autosave_ms) >= interval_ms) {
                session.syncDocumentState(editor.selectedMaterial());
                if (recovery_service.captureSnapshot(
                        session.document(),
                        editor.activeProjectPath(),
                        "Periodic Autosave",
                        now_ms,
                        engine.fileSystem())) {
                    last_autosave_ms = now_ms;
                    last_autosave_token = cur_tok;
                }
            }
        }
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
    engine.shutdown();
    window->shutdown();
    return exit_code;
}
