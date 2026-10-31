// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/core/result.hpp"
#include "finaleffect/scene/object_id.hpp"
#include "finaleffect/scene/scene.hpp"

namespace finaleffect::app {

struct StarterSceneHandles {
    scene::ObjectId rig_pivot{scene::ObjectId::invalid()};
    scene::ObjectId hero_cube{scene::ObjectId::invalid()};
    scene::ObjectId accent_pyramid{scene::ObjectId::invalid()};
    scene::ObjectId camera{scene::ObjectId::invalid()};
    scene::ObjectId sun_light{scene::ObjectId::invalid()};
};

// Populates a Scene with Final Effect 3D's default Stage 1 3D showcase scene.
[[nodiscard]] core::Result<StarterSceneHandles> populateStarterScene(scene::Scene& scene);

} // namespace finaleffect::app
