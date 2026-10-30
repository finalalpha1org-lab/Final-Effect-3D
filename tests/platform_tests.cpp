// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "test_harness.hpp"

#include "finaleffect/platform/clock.hpp"
#include "finaleffect/platform/file_system.hpp"
#include "finaleffect/platform/paths.hpp"
#include "finaleffect/platform/window.hpp"

#include <filesystem>

namespace {

using namespace finaleffect::platform;
using namespace finaleffect::core;

FE_TEST(Platform, AppPathsResolution) {
    const auto paths_res = queryAppPaths("finaleffect3d_test");
    FE_REQUIRE_TRUE(paths_res.hasValue());
    const auto& paths = paths_res.value();

    FE_EXPECT_FALSE(paths.working_directory.empty());
    FE_EXPECT_FALSE(paths.user_config_directory.empty());
    FE_EXPECT_FALSE(paths.user_data_directory.empty());
    FE_EXPECT_FALSE(paths.user_cache_directory.empty());
    FE_EXPECT_FALSE(paths.temp_directory.empty());
    FE_EXPECT_FALSE(hostPlatformName().empty());

    FE_EXPECT_EQ(queryAppPaths("").error().code, ErrorCode::InvalidArgument);
}

FE_TEST(Platform, LocalFileSystemTextAndBinaryRoundTrip) {
    LocalFileSystem fs;
    const auto paths_res = queryAppPaths("finaleffect3d_fstest");
    FE_REQUIRE_TRUE(paths_res.hasValue());

    const auto test_root = paths_res.value().temp_directory / "io_roundtrip";
    std::error_code ec;
    std::filesystem::remove_all(test_root, ec);

    const auto text_path = test_root / "nested" / "sample.txt";
    FE_REQUIRE_TRUE(fs.writeTextFile(text_path, "Final Effect 3D\nLine 2").hasValue());
    FE_EXPECT_TRUE(fs.exists(text_path));

    const auto read_text = fs.readTextFile(text_path);
    FE_REQUIRE_TRUE(read_text.hasValue());
    FE_EXPECT_EQ(read_text.value(), std::string("Final Effect 3D\nLine 2"));

    const std::vector<std::uint8_t> payload{0x46, 0x45, 0x33, 0x44, 0x00, 0xFF};
    const auto bin_path = test_root / "nested" / "sample.bin";
    FE_REQUIRE_TRUE(fs.writeBinaryFile(bin_path, payload).hasValue());

    const auto read_bin = fs.readBinaryFile(bin_path);
    FE_REQUIRE_TRUE(read_bin.hasValue());
    FE_EXPECT_EQ(read_bin.value().size(), payload.size());
    FE_EXPECT_TRUE(read_bin.value() == payload);

    FE_EXPECT_EQ(fs.readTextFile(test_root / "missing.txt").error().code, ErrorCode::NotFound);

    std::filesystem::remove_all(test_root, ec);
}

FE_TEST(Platform, HeadlessWindowLifecycle) {
    HeadlessWindow window;
    FE_EXPECT_EQ(window.initialize(WindowDesc{.width = 0, .height = 720}).error().code, ErrorCode::InvalidArgument);

    FE_REQUIRE_TRUE(window.initialize(WindowDesc{.title = "Test", .width = 800, .height = 600}).hasValue());
    FE_EXPECT_FALSE(window.shouldClose());
    FE_EXPECT_EQ(window.framebufferSize().width, 800u);
    FE_EXPECT_EQ(window.framebufferSize().height, 600u);

    window.swapBuffers();
    window.resize(1920, 1080);
    FE_EXPECT_EQ(window.framebufferSize().width, 1920u);
    FE_EXPECT_EQ(window.swappedFrameCount(), static_cast<std::uint64_t>(1));

    window.requestClose();
    FE_EXPECT_TRUE(window.shouldClose());
    window.shutdown();
}

} // namespace

int main(int argc, char** argv) {
    return finaleffect::testing::runAllTests(argc, argv);
}
