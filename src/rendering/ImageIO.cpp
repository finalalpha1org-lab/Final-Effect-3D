// SPDX-License-Identifier: GPL-3.0-only
#include "finaleffect/rendering/ImageIO.hpp"

#include <fstream>
#include <ostream>
#include <unordered_map>

namespace fe::rendering {

core::Status writePpm(std::ostream& stream, const ImageBuffer& image) {
    if (image.isEmpty()) {
        return core::makeError(core::ErrorCode::InvalidArgument, "image buffer is empty", "image io");
    }
    stream << "P6\n" << image.width << ' ' << image.height << "\n255\n";
    for (std::size_t i = 0; i < image.pixelCount(); ++i) {
        const std::size_t offset = i * 4U;
        stream.put(static_cast<char>(image.rgba[offset + 0U]));
        stream.put(static_cast<char>(image.rgba[offset + 1U]));
        stream.put(static_cast<char>(image.rgba[offset + 2U]));
    }
    if (!stream) {
        return core::makeError(core::ErrorCode::IoFailure, "failed while writing PPM data", "image io");
    }
    return core::successStatus();
}

core::Status writePpm(const ImageBuffer& image, const std::string& path) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        return core::makeError(core::ErrorCode::IoFailure, "cannot open '" + path + "' for writing", "image io");
    }
    const core::Status status = writePpm(file, image);
    file.close();
    return status;
}

bool isCompletelyBlack(const ImageBuffer& image) noexcept {
    if (image.isEmpty()) {
        return true;
    }
    for (std::size_t i = 0; i < image.pixelCount(); ++i) {
        const std::size_t offset = i * 4U;
        if (image.rgba[offset] != 0U || image.rgba[offset + 1U] != 0U || image.rgba[offset + 2U] != 0U) {
            return false;
        }
    }
    return true;
}

std::size_t countNonBackgroundPixels(const ImageBuffer& image) noexcept {
    if (image.isEmpty()) {
        return 0;
    }
    std::unordered_map<std::uint32_t, std::size_t> histogram;
    for (std::size_t i = 0; i < image.pixelCount(); ++i) {
        const std::size_t offset = i * 4U;
        const std::uint32_t packed = (static_cast<std::uint32_t>(image.rgba[offset]) << 16U) |
                                     (static_cast<std::uint32_t>(image.rgba[offset + 1U]) << 8U) |
                                     static_cast<std::uint32_t>(image.rgba[offset + 2U]);
        ++histogram[packed];
    }
    std::size_t mostCommon = 0;
    for (const auto& entry : histogram) {
        mostCommon = entry.second > mostCommon ? entry.second : mostCommon;
    }
    return image.pixelCount() - mostCommon;
}

}  // namespace fe::rendering
