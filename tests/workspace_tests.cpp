// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/app/workspace_manager.hpp"
#include "finaleffect/platform/file_system.hpp"
#include "finaleffect/platform/paths.hpp"
#include "test_harness.hpp"

#include <filesystem>

namespace {

using finaleffect::app::RoutedInputTarget;
using finaleffect::app::WorkspaceManager;
using finaleffect::app::WorkspacePreset;
using finaleffect::platform::KeyAction;
using finaleffect::platform::KeyCode;
using finaleffect::platform::KeyEvent;
using finaleffect::project::DockRegion;
using finaleffect::project::WorkspacePanelId;

FE_TEST(Workspace, PanelVisibilityPresetsAndDockGeometry) {
    WorkspaceManager wm;

    // Default Studio layout enables Viewport3D, Outliner, Properties, MaterialEditor, AssetBrowser, Console; Timeline hidden.
    FE_EXPECT_TRUE(wm.isPanelVisible(WorkspacePanelId::Viewport3D));
    FE_EXPECT_TRUE(wm.isPanelVisible(WorkspacePanelId::SceneOutliner));
    FE_EXPECT_TRUE(wm.isPanelVisible(WorkspacePanelId::Properties));
    FE_EXPECT_TRUE(wm.isPanelVisible(WorkspacePanelId::MaterialEditor));
    FE_EXPECT_TRUE(wm.isPanelVisible(WorkspacePanelId::AssetBrowser));
    FE_EXPECT_TRUE(!wm.isPanelVisible(WorkspacePanelId::Timeline));
    FE_EXPECT_TRUE(wm.isPanelVisible(WorkspacePanelId::Console));

    const auto rects = wm.computeDockLayout(1600, 900);
    FE_EXPECT_TRUE(rects.left_dock.width > 0.0f);
    FE_EXPECT_TRUE(rects.right_dock.width > 0.0f);
    FE_EXPECT_TRUE(rects.bottom_dock.height > 0.0f);
    FE_EXPECT_TRUE(rects.center_viewport.width > 600.0f);
    FE_EXPECT_TRUE(rects.center_viewport.height > 400.0f);
    // Center viewport and sidebars never overlap horizontally.
    FE_EXPECT_NEAR(rects.left_dock.width + rects.center_viewport.width + rects.right_dock.width, 1600.0f, 1.0e-3f);

    // Shading preset focuses MaterialEditor; hiding MaterialEditor automatically returns focus to Viewport3D.
    wm.applyPreset(WorkspacePreset::Shading);
    FE_EXPECT_EQ(wm.focusedPanel(), WorkspacePanelId::MaterialEditor);
    wm.setPanelVisible(WorkspacePanelId::MaterialEditor, false);
    FE_EXPECT_EQ(wm.focusedPanel(), WorkspacePanelId::Viewport3D);

    // Modeling preset collapses bottom dock so center_viewport spans full window height!
    wm.applyPreset(WorkspacePreset::Modeling);
    const auto modeling_rects = wm.computeDockLayout(1600, 900);
    FE_EXPECT_NEAR(modeling_rects.bottom_dock.height, 0.0f, 1.0e-4f);
    FE_EXPECT_NEAR(modeling_rects.center_viewport.height, 900.0f, 1.0e-4f);
}

FE_TEST(Workspace, NonDuplicatingInputRoutingAndDragCapture) {
    WorkspaceManager wm; // 1600 x 900
    // Center viewport is roughly x in [304, 1232), y in [0, 702).
    const auto center_hit = wm.routePointerEvent(800.0f, 400.0f, 1600, 900, false);
    FE_EXPECT_EQ(center_hit.target, RoutedInputTarget::Viewport3D);
    FE_EXPECT_EQ(wm.focusedPanel(), WorkspacePanelId::Viewport3D);

    // Clicking inside Left Dock (Scene Outliner) routes exclusively to PanelUI, NOT 3D Viewport!
    const auto left_hit = wm.routePointerEvent(50.0f, 200.0f, 1600, 900, false);
    FE_EXPECT_EQ(left_hit.target, RoutedInputTarget::PanelUI);
    FE_EXPECT_EQ(left_hit.target_panel.value(), WorkspacePanelId::SceneOutliner);

    // While a 3D viewport gizmo/orbit drag is active, pointer movement over Left Dock stays captured by Viewport3D!
    const auto captured_drag = wm.routePointerEvent(20.0f, 200.0f, 1600, 900, true);
    FE_EXPECT_EQ(captured_drag.target, RoutedInputTarget::Viewport3D);

    // Key routing: active text input blocks W/E/R/Tab/Delete from triggering viewport tools.
    KeyEvent w_press{.key = KeyCode::W, .action = KeyAction::Press};
    FE_EXPECT_EQ(wm.routeKeyEvent(w_press, true).target, RoutedInputTarget::TextInput);
    FE_EXPECT_EQ(wm.routeKeyEvent(w_press, false).target, RoutedInputTarget::Viewport3D);

    // Global Ctrl+Z undo shortcut routes to GlobalEditorShortcut once.
    KeyEvent undo_press{.key = KeyCode::Z, .action = KeyAction::Press, .mods = {.ctrl = true}};
    FE_EXPECT_EQ(wm.routeKeyEvent(undo_press, false).target, RoutedInputTarget::GlobalEditorShortcut);
}

FE_TEST(Workspace, StandaloneLayoutJsonPersistenceRoundTrip) {
    WorkspaceManager original;
    original.applyPreset(WorkspacePreset::AnimationPrep);
    original.setPanelDock(WorkspacePanelId::Console, DockRegion::Right, 2);
    original.setDockRatios(0.20f, 0.28f, 0.24f);

    finaleffect::platform::LocalFileSystem fs;
    const auto temp_dir = finaleffect::platform::queryAppPaths("finaleffect3d_wstest").value().temp_directory;
    std::error_code ec;
    std::filesystem::remove_all(temp_dir, ec);
    const auto layout_path = temp_dir / "workspace_layout.json";

    FE_REQUIRE_TRUE(original.saveLayoutToFile(layout_path, fs).hasValue());

    WorkspaceManager restored;
    FE_REQUIRE_TRUE(restored.loadLayoutFromFile(layout_path, fs).hasValue());
    FE_EXPECT_EQ(restored.state().layout_name, std::string("Animation"));
    FE_EXPECT_TRUE(restored.isPanelVisible(WorkspacePanelId::Timeline));
    FE_EXPECT_NEAR(restored.state().right_dock_width_ratio, 0.28f, 1.0e-5f);

    const auto right_panels = restored.visiblePanelsInRegion(DockRegion::Right);
    FE_EXPECT_EQ(right_panels.size(), std::size_t{3}); // Properties, MaterialEditor, Console

    std::filesystem::remove_all(temp_dir, ec);
}

} // namespace

int main(int argc, char** argv) {
    return finaleffect::testing::runAllTests(argc, argv);
}
