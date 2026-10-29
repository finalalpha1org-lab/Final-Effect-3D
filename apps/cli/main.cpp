// Final Effect 3D - fe3d_tool, the headless command-line front end.
// SPDX-License-Identifier: GPL-3.0-only
//
// Commands:
//   fe3d_tool info                          version, build and path information
//   fe3d_tool demo-save <file.fe3d>          writes the sample scene
//   fe3d_tool inspect <file.fe3d>            loads a project and prints its content
//   fe3d_tool render [file.fe3d] --out a.ppm renders one frame with the CPU renderer
//
// Everything here runs without a GPU, a window or a display server, which is what
// makes the CLI usable in CI and on servers.
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "finaleffect/core/Version.hpp"
#include "finaleffect/engine/DemoScene.hpp"
#include "finaleffect/engine/Engine.hpp"
#include "finaleffect/rendering/ImageIO.hpp"
#include "finaleffect/rendering/SoftwareRenderer.hpp"

namespace {

struct Options {
    std::string command;
    std::string projectPath;
    std::string outputPath;
    std::uint32_t width = 960;
    std::uint32_t height = 540;
    bool verbose = false;
};

void printUsage() {
    std::cout << "Final Effect 3D tool " << fe::core::fullVersionString() << "\n\n"
              << "usage: fe3d_tool <command> [options]\n\n"
              << "commands:\n"
              << "  info                        print version, build and path information\n"
              << "  demo-save <file.fe3d>       write the sample scene to a project file\n"
              << "  inspect <file.fe3d>         load and print a project file\n"
              << "  render [file.fe3d]          render one frame with the CPU renderer\n\n"
              << "options:\n"
              << "  --out <file.ppm>            output image for 'render'\n"
              << "  --width <pixels>            render width (default 960)\n"
              << "  --height <pixels>           render height (default 540)\n"
              << "  --verbose                   also log debug messages\n"
              << "  --help                      print this text\n";
}

bool parseArguments(int argc, char** argv, Options& options) {
    std::vector<std::string> arguments;
    arguments.reserve(static_cast<std::size_t>(argc > 0 ? argc - 1 : 0));
    for (int i = 1; i < argc; ++i) {
        arguments.emplace_back(argv[i]);
    }
    if (arguments.empty()) {
        return false;
    }

    std::size_t index = 0;
    options.command = arguments[index++];
    while (index < arguments.size()) {
        const std::string& argument = arguments[index];
        if (argument == "--help") {
            return false;
        }
        if (argument == "--verbose") {
            options.verbose = true;
            ++index;
            continue;
        }
        if (argument == "--out" || argument == "--width" || argument == "--height") {
            if (index + 1 >= arguments.size()) {
                std::cerr << "missing value for " << argument << "\n";
                return false;
            }
            const std::string value = arguments[index + 1];
            if (argument == "--out") {
                options.outputPath = value;
            } else {
                const int parsed = std::atoi(value.c_str());
                if (parsed <= 0 || parsed > 16384) {
                    std::cerr << "invalid value for " << argument << ": " << value << "\n";
                    return false;
                }
                if (argument == "--width") {
                    options.width = static_cast<std::uint32_t>(parsed);
                } else {
                    options.height = static_cast<std::uint32_t>(parsed);
                }
            }
            index += 2;
            continue;
        }
        if (argument.rfind("--", 0) == 0) {
            std::cerr << "unknown option " << argument << "\n";
            return false;
        }
        if (!options.projectPath.empty()) {
            std::cerr << "unexpected extra argument " << argument << "\n";
            return false;
        }
        options.projectPath = argument;
        ++index;
    }
    return true;
}

fe::core::Status runInfo(fe::engine::Engine& engine) {
    std::cout << "product       : " << fe::core::kProductName << "\n"
              << "version       : " << fe::core::fullVersionString() << "\n"
              << "build         : " << fe::core::buildInfo() << "\n"
              << "working dir   : " << engine.paths().workingDirectory << "\n"
              << "user data dir : " << engine.paths().userDataDirectory << "\n"
              << "log dir       : " << engine.paths().logDirectory << "\n"
              << "temp dir      : " << engine.paths().tempDirectory << "\n"
              << "primitives    :";
    for (const std::string& name : engine.meshLibrary().names()) {
        std::cout << ' ' << name;
    }
    std::cout << '\n';
    return fe::core::successStatus();
}

fe::core::Status runDemoSave(fe::engine::Engine& engine, const Options& options) {
    if (options.projectPath.empty()) {
        return fe::core::makeError(fe::core::ErrorCode::InvalidArgument,
                                   "demo-save needs an output project path",
                                   "cli");
    }
    const fe::core::Status built = fe::engine::buildDemoScene(engine.scene());
    if (!built.ok()) {
        return built;
    }
    const fe::core::Status saved = engine.saveProject(options.projectPath);
    if (!saved.ok()) {
        return saved;
    }
    std::cout << "wrote " << engine.scene().objectCount() << " objects to " << options.projectPath << "\n";
    return fe::core::successStatus();
}

fe::core::Status runInspect(fe::engine::Engine& engine, const Options& options) {
    if (options.projectPath.empty()) {
        return fe::core::makeError(fe::core::ErrorCode::InvalidArgument, "inspect needs a project path", "cli");
    }
    const fe::core::Status loaded = engine.loadProject(options.projectPath);
    if (!loaded.ok()) {
        return loaded;
    }
    std::cout << "objects: " << engine.scene().objectCount() << "\n";
    for (const fe::scene::SceneObject& object : engine.scene().objects()) {
        std::cout << "  [" << fe::scene::toString(object.id) << "] " << object.name << "  mesh="
                  << (object.mesh.empty() ? std::string("<none>") : object.mesh)
                  << "  parent=" << (object.parent.isValid() ? fe::scene::toString(object.parent)
                                                            : std::string("<root>"))
                  << "  children=" << object.children.size()
                  << "  world=( ";
        const fe::math::Vec3 worldPosition = engine.scene().worldMatrix(object.id).translation();
        std::cout << worldPosition.x << ' ' << worldPosition.y << ' ' << worldPosition.z << " )\n";
    }
    return fe::core::successStatus();
}

fe::core::Status runRender(fe::engine::Engine& engine, const Options& options) {
    if (options.projectPath.empty()) {
        const fe::core::Status built = fe::engine::buildDemoScene(engine.scene());
        if (!built.ok()) {
            return built;
        }
    } else {
        const fe::core::Status loaded = engine.loadProject(options.projectPath);
        if (!loaded.ok()) {
            return loaded;
        }
    }

    auto renderer = std::make_unique<fe::rendering::SoftwareRenderer>();
    fe::rendering::RendererInitInfo info;
    info.applicationName = std::string(fe::core::kProductName);
    info.width = options.width;
    info.height = options.height;
    info.verticalSync = false;

    const fe::core::Status initialized = renderer->initialize(info);
    if (!initialized.ok()) {
        return initialized;
    }
    const fe::core::Status attached = engine.setRenderer(std::move(renderer));
    if (!attached.ok()) {
        return attached;
    }

    const fe::core::Result<fe::rendering::ImageBuffer> frame = engine.captureFrame(engine.defaultCamera());
    if (!frame.ok()) {
        return frame.error();
    }

    if (options.outputPath.empty()) {
        std::cout << "rendered " << frame.value().width << "x" << frame.value().height << " ("
                  << fe::rendering::countNonBackgroundPixels(frame.value()) << " non-background pixels)\n";
        return fe::core::successStatus();
    }
    const fe::core::Status written = fe::rendering::writePpm(frame.value(), options.outputPath);
    if (!written.ok()) {
        return written;
    }
    std::cout << "wrote " << options.outputPath << " (" << frame.value().width << 'x' << frame.value().height
              << ", " << fe::rendering::countNonBackgroundPixels(frame.value()) << " non-background pixels)\n";
    return fe::core::successStatus();
}

}  // namespace

int main(int argc, char** argv) {
    Options options;
    if (!parseArguments(argc, argv, options)) {
        printUsage();
        return argc <= 1 ? 0 : 2;
    }

    fe::engine::Engine engine;
    fe::engine::EngineConfig config;
    config.logLevel = options.verbose ? fe::core::LogLevel::Debug : fe::core::LogLevel::Warning;
    config.attachConsoleLogSink = true;

    fe::core::Status status = engine.initialize(config);
    if (!status.ok()) {
        std::cerr << "engine initialization failed: " << status.error().toString() << "\n";
        return 1;
    }

    if (options.command == "info") {
        status = runInfo(engine);
    } else if (options.command == "demo-save") {
        status = runDemoSave(engine, options);
    } else if (options.command == "inspect") {
        status = runInspect(engine, options);
    } else if (options.command == "render") {
        status = runRender(engine, options);
    } else {
        std::cerr << "unknown command '" << options.command << "'\n";
        printUsage();
        (void)engine.shutdown();
        return 2;
    }

    if (!status.ok()) {
        std::cerr << "error: " << status.error().toString() << "\n";
        std::cerr << "suggested action: " << fe::core::suggestedAction(status.error().code()) << "\n";
    }

    const fe::core::Status shutdownStatus = engine.shutdown();
    if (!shutdownStatus.ok() && status.ok()) {
        status = shutdownStatus;
    }
    return status.ok() ? 0 : 1;
}
