// Guest-side voxel rasteriser.
//
// The host triangle rasteriser is far too slow on the device, so the scene is
// drawn here: one ray per screen column (DDA through the voxel grid) and a
// textured vertical span per hit. Occlusion is implicit because only the first
// hit of each column is drawn.
#pragma once

#include <cstdint>

#include "voxel_mesher.hpp"
#include "voxel_world.hpp"

namespace voxel {

/* A write target that follows whatever the window reports at runtime. */
struct Framebuffer {
    std::uint16_t* pixels = nullptr;
    std::int32_t width = 0;
    std::int32_t height = 0;
    std::int32_t stride_pixels = 0;
};

struct RenderSettings {
    /* Vertical field of view in radians. */
    float fov = 1.15f;
    float max_distance = 34.0f;
    /* Internal resolution divisor: 1 renders every pixel, 2 renders a quarter
     * of the pixels and repeats them, which the device needs for speed. */
    int pixel_step = 1;
};

struct RenderStats {
    std::uint32_t columns = 0;
    std::uint32_t hits = 0;
    std::uint32_t pixels = 0;
};

RenderStats render_scene(const Framebuffer& framebuffer, const World& world,
                         const Camera& camera,
                         const RenderSettings& settings) noexcept;

/* Built-in HUD (crosshair, hotbar, action buttons) drawn into the same target. */
struct HudLayout {
    std::int32_t width = 0;
    std::int32_t height = 0;
    std::int32_t hotbar_x = 0;
    std::int32_t hotbar_y = 0;
    std::int32_t slot = 0;
    std::int32_t slot_gap = 0;
    std::int32_t button = 0;
    std::int32_t button_gap = 0;
    std::int32_t button_x = 0;
    std::int32_t button_y = 0;
};

HudLayout hud_layout(std::int32_t width, std::int32_t height) noexcept;

void draw_hud(const Framebuffer& framebuffer, const HudLayout& layout,
              int selected_slot, bool mining, bool flying) noexcept;

}  // namespace voxel
