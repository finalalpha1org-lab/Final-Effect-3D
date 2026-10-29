// Final Effect 3D - image output helpers.
// SPDX-License-Identifier: GPL-3.0-only
//
// Stage 1 writes Netpbm PPM (binary, RGB8) because it needs no third-party
// encoder. Test artefacts and CI smoke screenshots use this format.
#pragma once

#include <iosfwd>
#include <string>

#include "finaleffect/core/Error.hpp"
#include "finaleffect/rendering/Renderer.hpp"

namespace fe::rendering {

/// Writes "image" as binary PPM. Returns IoFailure when the file cannot be created.
core::Status writePpm(const ImageBuffer& image, const std::string& path);

/// Same, to an already open stream.
core::Status writePpm(std::ostream& stream, const ImageBuffer& image);

/// True when every pixel is (0, 0, 0, 255) - used to detect a black frame.
bool isCompletelyBlack(const ImageBuffer& image) noexcept;

/// Counts pixels that differ from the frame's most common colour. A cheap
/// "did we actually draw something" check for tests and CI.
std::size_t countNonBackgroundPixels(const ImageBuffer& image) noexcept;

}  // namespace fe::rendering
