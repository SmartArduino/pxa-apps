// Camera basis and the voxel face mesher that feeds the GameRender 3D path.
#pragma once

#include <pxa/game.hpp>
#include <pxa/game3d.hpp>

#include "voxel_world.hpp"

namespace voxel {

struct Camera {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float yaw = 0.0f;   /* radians, 0 looks towards +Z */
    float pitch = 0.0f; /* radians, positive looks up */

    void to_camera(float wx, float wy, float wz,
                   pxa::game3d::Vec3& out) const noexcept;
    void forward(float& dx, float& dy, float& dz) const noexcept;
    void right(float& dx, float& dz) const noexcept;
};

struct DrawBudget {
    /* Viewport description so painter quads can be projected here. */
    std::int32_t width = 296;
    std::int32_t height = 240;
    float focal = 168.0f;
    float max_distance = 30.0f;
    std::uint32_t max_faces = 1500;
    /* Stop feeding the draw list before the host's per-frame budget is used
     * up; the guest command buffer is capped in RenderOptions. */
    std::uint32_t max_bytes = 44000;
    /* Quality tier: dropping per-pixel palette lighting roughly halves the
     * per-pixel work in the host rasteriser. */
    bool lit_palette = true;
    /* Use the scanline painter-depth kernel for projected quads. Clipped
     * polygons use triangles in the same depth buffer. */
    bool painter = true;
};

struct DrawStats {
    std::uint32_t faces_drawn = 0;
    std::uint32_t faces_culled = 0;
    std::uint32_t batches = 0;
    bool exhausted = false;
};

/* Short-lived block debris: camera-facing quads with a flat colour. */
struct Particle {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    float vz = 0.0f;
    float life = 0.0f;
    std::uint16_t color = 0;
};

void draw_particles(pxa::game::Frame& frame,
                    const pxa::game3d::Projector& projector,
                    const Camera& camera,
                    std::span<const Particle> particles) noexcept;

DrawStats draw_world(pxa::game::Frame& frame,
                     const pxa::game3d::Projector& projector,
                     const World& world, const Camera& camera,
                     const DrawBudget& budget) noexcept;

}  // namespace voxel
