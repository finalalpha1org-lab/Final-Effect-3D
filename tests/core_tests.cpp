// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "test_harness.hpp"

#include "finaleffect/core/application.hpp"
#include "finaleffect/core/engine.hpp"
#include "finaleffect/core/logger.hpp"
#include "finaleffect/core/result.hpp"
#include "finaleffect/core/version.hpp"
#include "finaleffect/platform/clock.hpp"

#include <memory>

namespace {

using namespace finaleffect::core;
using namespace finaleffect::platform;

class ProbeApplication final : public IApplication {
public:
    bool fail_init{false};
    bool fail_on_second_frame{false};
    int init_calls{0};
    int update_calls{0};
    int shutdown_calls{0};
    double accumulated_delta{0.0};

    Status onInitialize(Engine&) override {
        ++init_calls;
        if (fail_init) {
            return Status::fail(ErrorCode::InvalidState, "Simulated app init failure");
        }
        return Status::ok();
    }

    Status onUpdate(Engine&, const FrameTimings& timings) override {
        ++update_calls;
        accumulated_delta += timings.delta_seconds;
        if (fail_on_second_frame && timings.frame_index == 1) {
            return Status::fail(ErrorCode::InvalidState, "Simulated frame update failure");
        }
        return Status::ok();
    }

    void onShutdown(Engine&) noexcept override {
        ++shutdown_calls;
    }
};

FE_TEST(Core, VersionAndResult) {
    FE_EXPECT_EQ(kEngineVersion.toString(), std::string("0.1.0"));
    FE_EXPECT_TRUE((Version{0, 2, 0} > Version{0, 1, 9}));

    auto ok_res = Result<int>::ok(42);
    FE_REQUIRE_TRUE(ok_res.hasValue());
    FE_EXPECT_EQ(ok_res.value(), 42);

    auto err_res = Result<int>::fail(ErrorCode::ValidationError, "bad value", "field_x");
    FE_REQUIRE_TRUE(err_res.isError());
    FE_EXPECT_EQ(err_res.error().code, ErrorCode::ValidationError);
    FE_EXPECT_EQ(err_res.valueOr(7), 7);
    FE_EXPECT_TRUE(err_res.error().format().find("ValidationError") != std::string::npos);
}

FE_TEST(Core, LoggerFilteringAndIsolation) {
    Logger logger(LogLevel::Warning);
    auto sink = std::make_shared<MemoryLogSink>();
    logger.addSink(sink);

    logger.debug("Test", "filtered out", 0.1);
    logger.info("Test", "also filtered", 0.2);
    logger.warning("Test", "kept warning", 0.3);
    logger.error("Test", "kept error", 0.4);

    const auto entries = sink->entries();
    FE_REQUIRE_TRUE(entries.size() == 2);
    FE_EXPECT_EQ(entries[0].level, LogLevel::Warning);
    FE_EXPECT_EQ(entries[0].message, std::string("kept warning"));
    FE_EXPECT_EQ(entries[1].level, LogLevel::Error);
}

FE_TEST(Core, EngineLifecycleAndDeterministicTiming) {
    auto clock = std::make_unique<ManualClock>(10.0);
    auto* clock_ptr = clock.get();

    EngineConfig cfg;
    cfg.application_name = "TestHost";
    cfg.attach_console_log_sink = false;
    cfg.max_frame_delta_seconds = 0.1;

    Engine engine(cfg, nullptr, std::move(clock));
    auto log_sink = std::make_shared<MemoryLogSink>();
    engine.logger().addSink(log_sink);

    // Calling stepFrame before initialize must fail cleanly.
    FE_EXPECT_TRUE(engine.stepFrame().isError());
    FE_EXPECT_EQ(engine.state(), EngineState::Uninitialized);

    ProbeApplication app;
    FE_REQUIRE_TRUE(engine.initialize(&app).hasValue());
    FE_EXPECT_EQ(engine.state(), EngineState::Initialized);
    FE_EXPECT_EQ(app.init_calls, 1);

    // Double initialize must be rejected.
    FE_EXPECT_EQ(engine.initialize(&app).error().code, ErrorCode::InvalidState);

    clock_ptr->advance(0.016);
    FE_REQUIRE_TRUE(engine.stepFrame().hasValue());

    // Spike beyond max_frame_delta_seconds (0.1) must be clamped.
    clock_ptr->advance(2.5);
    FE_REQUIRE_TRUE(engine.stepFrame().hasValue());

    FE_EXPECT_EQ(engine.frameCount(), static_cast<std::uint64_t>(2));
    FE_EXPECT_EQ(app.update_calls, 2);
    FE_EXPECT_NEAR(app.accumulated_delta, 0.116, 1e-6);

    engine.shutdown();
    FE_EXPECT_EQ(engine.state(), EngineState::Terminated);
    FE_EXPECT_EQ(app.shutdown_calls, 1);

    // Stepping after shutdown must fail cleanly.
    FE_EXPECT_EQ(engine.stepFrame().error().code, ErrorCode::InvalidState);
}

FE_TEST(Core, EnginePropagatesAppInitFailure) {
    EngineConfig cfg;
    cfg.attach_console_log_sink = false;
    Engine engine(cfg);

    ProbeApplication failing_app;
    failing_app.fail_init = true;

    const auto status = engine.initialize(&failing_app);
    FE_REQUIRE_TRUE(status.isError());
    FE_EXPECT_EQ(status.error().code, ErrorCode::InvalidState);
    FE_EXPECT_EQ(engine.state(), EngineState::Terminated);
    FE_EXPECT_EQ(failing_app.shutdown_calls, 0);
}

} // namespace

int main(int argc, char** argv) {
    return finaleffect::testing::runAllTests(argc, argv);
}
