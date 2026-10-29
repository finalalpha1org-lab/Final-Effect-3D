// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include <cstddef>
#include <cstdint>

namespace finaleffect::core {

// Lightweight, deterministic telemetry baseline for low-memory tracking (Stage 1).
struct ResourceStats {
    std::size_t scene_object_count{0};
    std::size_t mesh_asset_count{0};
    std::size_t total_cpu_vertices{0};
    std::size_t total_cpu_indices{0};
    std::size_t cpu_mesh_bytes{0};
    std::size_t gpu_mesh_buffer_count{0};
    std::size_t gpu_mesh_bytes{0};
    std::size_t undo_command_count{0};
    std::size_t redo_command_count{0};
    std::size_t command_history_bytes{0};
};

} // namespace finaleffect::core
