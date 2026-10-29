// Final Effect 3D - sample scene used by the CLI, the desktop host and tests.
// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "finaleffect/core/Error.hpp"
#include "finaleffect/scene/Scene.hpp"

namespace fe::engine {

/// Replaces the content of the scene with a small deterministic sample: a ground
/// plane, a cube, a sphere and a child object demonstrating the hierarchy.
core::Status buildDemoScene(scene::Scene& scene);

}  // namespace fe::engine
