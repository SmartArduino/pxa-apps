// Column-based voxel rasteriser and the built-in HUD.
#include "voxel_render.hpp"

#include <cmath>
#include <cstring>

#include "voxel_textures.hpp"

namespace voxel {
namespace {

constexpr std::uint16_t rgb565(std::uint8_t r, std::uint8_t g,
                               std::uint8_t b) noexcept {
    return static_cast<std::uint16_t>(((r >> 3) << 11) | ((g >> 2) << 5) |
                                      (b >> 3));
}

std::uint16_t shade(std::uint16_t color, int light_256) noexcept {
    const int r = ((color >> 11) & 31) * light_256 >> 8;
    const int g = ((color >> 5) & 63) * light_256 >> 8;
    const int b = (color & 31) * light_256 >> 8;
    return static_cast<std::uint16_t>((r << 11) | (g << 5) | b);
}

std::uint16_t shade_rgb565(std::uint16_t color, std::uint16_t tint,
                           int light_256) noexcept {
    int r = (((color >> 11) & 31) + ((tint >> 11) & 31)) >> 1;
    int g = (((color >> 5) & 63) + ((tint >> 5) & 63)) >> 1;
    int b = ((color & 31) + (tint & 31)) >> 1;
    r = r * light_256 >> 8;
    g = g * light_256 >> 8;
    b = b * light_256 >> 8;
    return static_cast<std::uint16_t>((r << 11) | (g << 5) | b);
}

/* Per-face shading, matching the reference app's look. */
constexpr int kFaceLight[6] = {255, 210, 175, 130, 190, 165};

}  // namespace

RenderStats render_scene(const Framebuffer& framebuffer, const World& world,
                         const Camera& camera,
                         const RenderSettings& settings) noexcept {
    RenderStats stats;
    if (framebuffer.pixels == nullptr || framebuffer.width <= 0 ||
        framebuffer.height <= 0)
        return stats;

    const float cy = std::cos(camera.yaw);
    const float sy = std::sin(camera.yaw);
    const float cp = std::cos(camera.pitch);
    const float sp = std::sin(camera.pitch);
    /* Camera axes in world space (y is up). */
    const float fx = sy * cp, fy = -sp, fz = cy * cp;   /* forward */
    const float rx = cy, rz = -sy;                      /* right   */

    const int step = settings.pixel_step < 1 ? 1 : settings.pixel_step;
    const float focal = static_cast<float>(framebuffer.height) /
                        (2.0f * std::tan(settings.fov * 0.5f));
    const float horizon = static_cast<float>(framebuffer.height) * 0.5f +
                          sp * focal;
    const float tan_half = std::tan(settings.fov * 0.5f);

    /* Sky and floor gradients, drawn with the same pixel step. */
    for (int y = 0; y < framebuffer.height; y += step) {
        std::uint16_t color;
        if (static_cast<float>(y) < horizon) {
            const float t = horizon > 1.0f
                                ? static_cast<float>(y) / horizon
                                : 0.0f;
            color = rgb565(static_cast<std::uint8_t>(90 + 60 * t),
                           static_cast<std::uint8_t>(150 + 50 * t),
                           static_cast<std::uint8_t>(230 + 20 * t));
        } else {
            const float t = (static_cast<float>(y) - horizon) /
                            (static_cast<float>(framebuffer.height) - horizon +
                             1.0f);
            color = rgb565(static_cast<std::uint8_t>(120 + 40 * t),
                           static_cast<std::uint8_t>(150 + 30 * t),
                           static_cast<std::uint8_t>(110 + 30 * t));
        }
        auto* row = framebuffer.pixels + static_cast<std::size_t>(y) *
                                            framebuffer.stride_pixels;
        for (int x = 0; x < framebuffer.width; x += step) {
            for (int i = 0; i < step && x + i < framebuffer.width; ++i)
                row[x + i] = color;
        }
    }
    stats.pixels = static_cast<std::uint32_t>(framebuffer.width) *
                   static_cast<std::uint32_t>(framebuffer.height);

    std::array<std::uint8_t, 256 * 4> column{}; /* index, shade, u, valid */
    std::array<float, 256> distance{};

    for (int x = 0; x < framebuffer.width; x += step) {
        const float camera_x = (static_cast<float>(x) -
                                static_cast<float>(framebuffer.width) * 0.5f) /
                               focal;
        float dx = fx + rx * camera_x;
        float dz = fz + rz * camera_x;
        const float length = std::sqrt(dx * dx + fy * fy + dz * dz);
        if (length <= 0.0001f) continue;
        dx /= length;
        const float dy = fy / length;
        dz /= length;

        const RayHit hit = raycast(world, camera.x, camera.y + 1.55f, camera.z,
                                   dx, dy, dz, settings.max_distance);
        ++stats.columns;
        if (!hit.hit) continue;
        ++stats.hits;

        const int face = hit.ny > 0    ? 0
                         : hit.nx != 0 ? (hit.nx > 0 ? 1 : 4)
                         : hit.nz != 0 ? (hit.nz > 0 ? 5 : 2)
                                       : 3;
        std::uint8_t block = world.at(hit.x, hit.y, hit.z);
        if (block == kAir) block = kStone;
        const auto& info = block_info(block);
        const int slot = info.texture[face == 0   ? kFaceTop
                                      : face == 3 ? kFaceBottom
                                                  : kFaceSide];
        if (slot < 0 || slot >= kTextureCount) continue;
        const std::uint8_t* tile = kTextureTiles[slot];
        const int light = kFaceLight[face];

        /* Distance to the hit point along the ray, in blocks. */
        const float hit_distance =
            (hit.distance > 0.0001f ? hit.distance : length);
        if (hit_distance <= 0.05f) continue;

        /* Vertical extent of one block at this distance. */
        const float block_pixels = focal / hit_distance;
        const float center =
            horizon + (camera.y + 1.55f - (static_cast<float>(hit.y) + 0.5f)) *
                          block_pixels;
        int y0 = static_cast<int>(center - block_pixels * 0.5f);
        int y1 = static_cast<int>(center + block_pixels * 0.5f);
        if (y1 <= y0) y1 = y0 + 1;
        distance[static_cast<std::size_t>(x)] = hit_distance;

        /* Horizontal texture coordinate from where the ray crossed the face. */
        float u = 0.0f;
        const float px = camera.x + dx * hit_distance;
        const float pz = camera.z + dz * hit_distance;
        const float py = camera.y + 1.55f + dy * hit_distance;
        if (hit.nx != 0)
            u = (hit.nx > 0 ? pz : 1.0f - pz) - std::floor(pz);
        else if (hit.nz != 0)
            u = (hit.nz > 0 ? 1.0f - px : px) - std::floor(px);
        else
            u = px - std::floor(px);
        (void)py;
        int tex_u = static_cast<int>(u * 16.0f);
        if (tex_u < 0) tex_u = 0;
        if (tex_u > 15) tex_u = 15;

        for (int y = y0; y < y1; ++y) {
            if (y < 0 || y >= framebuffer.height) continue;
            const float v = static_cast<float>(y - y0) /
                            static_cast<float>(y1 - y0);
            int tex_v = static_cast<int>(v * 16.0f);
            if (tex_v < 0) tex_v = 0;
            if (tex_v > 15) tex_v = 15;
            const std::uint8_t index = tile[tex_v * 16 + tex_u];
            if (index == 0 && info.cutout) continue; /* leaves stay cut out */
            const std::uint16_t color = kPalette[index];
            const std::uint16_t shaded = shade(color, light);
            auto* row = framebuffer.pixels +
                        static_cast<std::size_t>(y) * framebuffer.stride_pixels;
            for (int i = 0; i < step && x + i < framebuffer.width; ++i)
                row[x + i] = shaded;
            ++stats.pixels;
        }
    }
    (void)shade_rgb565;
    return stats;
}

HudLayout hud_layout(std::int32_t width, std::int32_t height) noexcept {
    HudLayout layout;
    layout.width = width;
    layout.height = height;
    layout.slot = height >= 300 ? 26 : 22;
    layout.slot_gap = 2;
    const int slots = kHotbarCount;
    const int total = slots * layout.slot + (slots - 1) * layout.slot_gap;
    layout.hotbar_x = (width - total) / 2;
    layout.hotbar_y = height - layout.slot - 6;
    layout.button = layout.slot + 4;
    layout.button_gap = 4;
    layout.button_x = width - layout.button - 6;
    layout.button_y = 26;
    return layout;
}

void draw_hud(const Framebuffer& framebuffer, const HudLayout& layout,
              int selected_slot, bool mining, bool flying) noexcept {
    if (framebuffer.pixels == nullptr) return;
    auto fill = [&](int x0, int y0, int x1, int y1, std::uint16_t color) {
        if (x0 < 0) x0 = 0;
        if (y0 < 0) y0 = 0;
        if (x1 > framebuffer.width) x1 = framebuffer.width;
        if (y1 > framebuffer.height) y1 = framebuffer.height;
        for (int y = y0; y < y1; ++y) {
            auto* row = framebuffer.pixels +
                        static_cast<std::size_t>(y) * framebuffer.stride_pixels;
            for (int x = x0; x < x1; ++x) row[x] = color;
        }
    };
    constexpr std::uint16_t panel = 0x2104;
    constexpr std::uint16_t icon = 0xef7d;
    constexpr std::uint16_t select = 0xffe0;

    /* Hotbar with block icons taken from the baked textures. */
    for (int index = 0; index < kHotbarCount; ++index) {
        const int x0 = layout.hotbar_x + index * (layout.slot + layout.slot_gap);
        const int y0 = layout.hotbar_y;
        fill(x0, y0, x0 + layout.slot, y0 + layout.slot, panel);
        const std::uint8_t block = kHotbarBlocks[index];
        const int slot = block_info(block).texture[kFaceSide];
        if (slot >= 0 && slot < kTextureCount) {
            const std::uint8_t* tile = kTextureTiles[slot];
            const int inner = layout.slot - 4;
            for (int ty = 0; ty < inner; ++ty) {
                for (int tx = 0; tx < inner; ++tx) {
                    const int sx = tx * 16 / inner;
                    const int sy = ty * 16 / inner;
                    const std::uint8_t value = tile[sy * 16 + sx];
                    if (value == 0) continue;
                    fill(x0 + 2 + tx, y0 + 2 + ty, x0 + 3 + tx, y0 + 3 + ty,
                         shade(kPalette[value], 255));
                }
            }
        }
        if (index == selected_slot) {
            fill(x0, y0, x0 + layout.slot, y0 + 2, select);
            fill(x0, y0 + layout.slot - 2, x0 + layout.slot, y0 + layout.slot,
                 select);
            fill(x0, y0, x0 + 2, y0 + layout.slot, select);
            fill(x0 + layout.slot - 2, y0, x0 + layout.slot, y0 + layout.slot,
                 select);
        }
    }

    /* Action buttons on the right: mine, place, jump, fly. */
    const int bx = layout.button_x;
    int by = layout.button_y;
    for (int index = 0; index < 4; ++index, by += layout.button + layout.button_gap) {
        const std::uint16_t tint =
            (index == 2 && mining) ? 0xf800 : (index == 3 && flying ? 0x07e0
                                                                   : panel);
        fill(bx, by, bx + layout.button, by + layout.button, tint);
        /* Mirrored, upright triangles and bars: kept minimal on purpose. */
        if (index == 0) {
            for (int i = 0; i < layout.button / 2; ++i)
                fill(bx + layout.button / 2 - i, by + layout.button / 2 - i,
                     bx + layout.button / 2 + i + 1,
                     by + layout.button / 2 - i + 1, icon);
        } else if (index == 1) {
            fill(bx + 4, by + 4, bx + layout.button - 4,
                 by + layout.button - 4, icon);
        } else if (index == 2) {
            for (int i = 0; i < layout.button / 2; ++i)
                fill(bx + layout.button / 2 - i, by + 4 + i,
                     bx + layout.button / 2 + i + 1, by + 5 + i, icon);
        } else {
            for (int i = 0; i < layout.button / 2; ++i)
                fill(bx + 4 + i, by + 4 + i, bx + 6 + i,
                     by + 6 + i, icon);
        }
    }

    /* Crosshair at the centre. */
    const int cx = framebuffer.width / 2;
    const int cy = static_cast<int>(framebuffer.height * 0.5f);
    fill(cx - 5, cy - 1, cx + 6, cy + 1, icon);
    fill(cx - 1, cy - 5, cx + 1, cy + 6, icon);
}

}  // namespace voxel
