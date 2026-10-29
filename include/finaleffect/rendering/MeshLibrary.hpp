// Final Effect 3D - built-in primitive meshes.
// SPDX-License-Identifier: GPL-3.0-only
//
// Stage 1 ships a small, deterministic set of primitives so that a scene can be
// rendered end to end without an asset pipeline. Generated meshes are original
// code, produced at runtime, and depend on nothing but the standard library.
#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "finaleffect/rendering/Renderer.hpp"

namespace fe::rendering {

class MeshLibrary {
public:
    /// Registers the built-in primitives: "cube", "plane", "sphere".
    MeshLibrary();

    /// Returns nullptr when the name is not registered.
    const MeshData* find(std::string_view name) const noexcept;
    bool contains(std::string_view name) const noexcept { return find(name) != nullptr; }

    std::size_t size() const noexcept { return meshes_.size(); }
    std::vector<std::string> names() const;

    static MeshData makeCube(math::Scalar size = 1.0F);
    static MeshData makePlane(math::Scalar size = 2.0F, int subdivisions = 4);
    static MeshData makeSphere(math::Scalar radius = 0.5F, int segments = 24, int rings = 12);

private:
    void add(MeshData mesh);

    std::vector<MeshData> meshes_;
};

}  // namespace fe::rendering
