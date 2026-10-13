#pragma once
#include <pxa/game3d_material.hpp>
namespace voxel {
using DrawPath = pxa::game3d::MaterialPath;
inline DrawPath choose_draw_path(std::span<const pxa::game::Vertex> vertices,
        unsigned tile_span, bool cutout) noexcept {
    return pxa::game3d::choose_material_path(vertices, tile_span*16u, cutout);
}
} // namespace voxel
