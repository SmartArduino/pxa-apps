// Voxel Craft C++ - a first-person voxel sandbox on the PXA C++26 Guest SDK.
//
// Terrain uses clipped, perspective-correct quads and triangles sharing one
// depth buffer. A full-window Canvas routes touch input to the movement stick,
// camera and HUD; simulation catch-up remains bounded independently of FPS.
#include <pxa/app.hpp>
#include <pxa/game3d.hpp>
#include <pxa/game_utils.hpp>
#include <pxa/game_pacing.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#if VOXEL_PROFILE
#include <chrono>
#endif
#include <cstdio>
#include <optional>
#include <span>
#include <string>

#include "voxel_mesher.hpp"
#include "../common/voxel_benchmark.h"
#include "voxel_render.hpp"
#include "voxel_world.hpp"
#include "voxel_ui.hpp"
#include "voxel_save.hpp"
#include "voxel_font.hpp"

#ifndef VOXEL_BENCH_DISTANCE
#define VOXEL_BENCH_DISTANCE 0
#endif
#ifndef VOXEL_CHEAP_PATHS
#define VOXEL_CHEAP_PATHS 0
#endif
#ifndef VOXEL_BENCH_CHEAP
#define VOXEL_BENCH_CHEAP 0
#endif
#ifndef VOXEL_BENCH_HUD
#define VOXEL_BENCH_HUD 0
#endif
#ifndef VOXEL_VALIDATE
#define VOXEL_VALIDATE 0
#endif
#ifndef VOXEL_DRAW_BUDGET
#define VOXEL_DRAW_BUDGET 40000
#endif
static_assert(VOXEL_DRAW_BUDGET >= 40 && VOXEL_DRAW_BUDGET <= 44000);

namespace {

constexpr int kHudTexture = 45;    /* HUD atlas slot */
constexpr int kBlockTextures = 45; /* assets/b00.pxr .. assets/b44.pxr */
constexpr float kWalkSpeed = 4.4f;
constexpr float kFlySpeed = 7.5f;
constexpr float kGravity = 24.0f;
constexpr float kJumpSpeed = 7.6f;
constexpr float kPlayerHeight = 1.7f;
constexpr float kPlayerRadius = 0.30f;
constexpr float kEyeHeight = 1.55f;
constexpr float kReach = 5.5f;

struct VoxelCraft {
    // 10 Hz rendering needs six 16 ms simulation steps. The former four-step
    // budget discarded movement and gravity time on every slow frame.
    static constexpr pxa::game::LoopOptions loop_options{.maximum_updates = 8};
    pxa::game::DrawBuffer<49152> commands;
    std::optional<pxa::game::Renderer> renderer;
    std::optional<pxa::game3d::Projector> projector;

    voxel::World world;
    voxel::Camera camera;
    float velocity_y = 0.0f;
    bool flying = false;
    bool on_ground = false;
    int hotbar = 0;
#if VOXEL_VALIDATE
    std::uint32_t edit_count = 0, last_edit = 0;
    void record_edit(int x, int y, int z, unsigned block, unsigned kind) noexcept {
        last_edit = unsigned(x) | (unsigned(y) << 6) | (unsigned(z) << 11) |
                    (block << 17) | (kind << 22);
        ++edit_count;
    }
#endif

    static constexpr int kMaxParticles = 64;
    pxa::game::RecyclingPool<voxel::Particle, kMaxParticles> particles;
    std::uint32_t rng = 0x2545f491u;

    std::uint32_t next_random() noexcept {
        rng ^= rng << 13;
        rng ^= rng >> 17;
        rng ^= rng << 5;
        return rng;
    }

    float random_unit() noexcept {
        return static_cast<float>(next_random() & 0xffffu) / 65535.0f;
    }

    /* Flat debris colour per block, kept close to the texture's average. */
    static std::uint16_t block_color(std::uint8_t block) noexcept {
        switch (block) {
            case voxel::kGrass: return 0x4d44;   /* green */
            case voxel::kDirt: return 0x6b2a;    /* brown */
            case voxel::kStone: return 0x8410;   /* grey */
            case voxel::kSand: return 0xdeca;    /* pale yellow */
            case voxel::kWood: return 0x8a20;    /* dark wood */
            case voxel::kLeaves: return 0x3c85;  /* leaf green */
            case voxel::kWater: return 0x2c9d;   /* blue */
            case voxel::kPlank: return 0xb4a1;
            case voxel::kBrick: return 0x9a40;
            case voxel::kGlass: return 0xc71a;
            default: return 0x8410;
        }
    }

    void spawn_debris(int x, int y, int z, std::uint8_t block) noexcept {
        const std::uint16_t color = block_color(block);
        for (int index = 0; index < 10; ++index) {
            auto& particle = particles.acquire();
            particle.x = static_cast<float>(x) + 0.15f + random_unit() * 0.7f;
            particle.y = static_cast<float>(y) + 0.15f + random_unit() * 0.7f;
            particle.z = static_cast<float>(z) + 0.15f + random_unit() * 0.7f;
            particle.vx = (random_unit() - 0.5f) * 2.6f;
            particle.vy = 1.2f + random_unit() * 2.4f;
            particle.vz = (random_unit() - 0.5f) * 2.6f;
            particle.life = 0.45f + random_unit() * 0.35f;
            particle.color = color;
        }
    }

    bool initializing = false;
    std::uint32_t frames = 0;
    std::uint32_t last_faces = 0;
    std::uint32_t last_batches = 0;
    bool last_exhausted = false;
    bool submit_error_logged = false;
    std::uint32_t hud_window_us = 0;
    std::uint32_t fps_frames = 0;

    voxel::HudLayout hud;
#include "voxel_session.inc"
#include "voxel_menu.inc"

#if VOXEL_PROFILE
    pxa::game::StageStatistics<6> stage_stats;
#endif

    /* ---- movement ------------------------------------------------------ */

    bool collides(float px, float py, float pz) const noexcept {
        const int x0 = static_cast<int>(std::floor(px - kPlayerRadius));
        const int x1 = static_cast<int>(std::floor(px + kPlayerRadius));
        const int y0 = static_cast<int>(std::floor(py));
        const int y1 = static_cast<int>(std::floor(py + kPlayerHeight));
        const int z0 = static_cast<int>(std::floor(pz - kPlayerRadius));
        const int z1 = static_cast<int>(std::floor(pz + kPlayerRadius));
        for (int x = x0; x <= x1; ++x)
            for (int y = y0; y <= y1; ++y)
                for (int z = z0; z <= z1; ++z)
                    if (world.solid(x, y, z)) return true;
        return false;
    }

    void move_axis(int axis, float amount) noexcept {
        if (amount == 0.0f) return;
        const float x = camera.x + (axis == 0 ? amount : 0.0f);
        const float y = camera.y + (axis == 1 ? amount : 0.0f);
        const float z = camera.z + (axis == 2 ? amount : 0.0f);
        if (!collides(x, y, z)) {
            camera.x = x;
            camera.y = y;
            camera.z = z;
        } else if (axis == 1) {
            velocity_y = 0.0f;
        }
    }

    void respawn() noexcept {
        int spot_x = voxel::kWorldX / 2;
        int spot_z = voxel::kWorldZ / 2;
        for (int radius = 0; radius < 12; ++radius) {
            bool found = false;
            for (int offset = -radius; offset <= radius && !found; offset += 2) {
                const int x = voxel::kWorldX / 2 + offset;
                const int z = voxel::kWorldZ / 2 + (radius - std::abs(offset));
                if (!world.in_bounds(x, 2, z)) continue;
                const int ground = world.highest_block(x, z);
                if (ground + 3 >= voxel::kWorldY) continue;
                if (world.at(x, ground + 1, z) != voxel::kAir ||
                    world.at(x, ground + 2, z) != voxel::kAir)
                    continue;
                spot_x = x;
                spot_z = z;
                found = true;
            }
            if (found) break;
        }
        const int ground = world.highest_block(spot_x, spot_z);
        camera.x = static_cast<float>(spot_x) + 0.5f;
        camera.z = static_cast<float>(spot_z) + 0.5f;
        camera.y = static_cast<float>(ground) + 1.05f;
        camera.yaw = 0.0f;
        camera.pitch = 0.0f;
        velocity_y = 0.0f;
        flying = false;
    }

    void look(float yaw_delta, float pitch_delta) noexcept {
        camera.yaw += yaw_delta;
        camera.pitch = std::clamp(camera.pitch + pitch_delta, -1.35f, 1.35f);
        if (camera.yaw > 3.14159265f) camera.yaw -= 6.28318531f;
        if (camera.yaw < -3.14159265f) camera.yaw += 6.28318531f;
    }

    voxel::RayHit target() const noexcept {
        float dx = 0.0f;
        float dy = 0.0f;
        float dz = 0.0f;
        camera.forward(dx, dy, dz);
        return voxel::raycast(world, camera.x, camera.y + kEyeHeight, camera.z,
                              dx, dy, dz, kReach);
    }

    void mine() noexcept {
        const voxel::RayHit hit = target();
        if (!hit.hit) return;
        if (world.at(hit.x, hit.y, hit.z) == voxel::kBedrock) return;
        const std::uint8_t removed = world.at(hit.x, hit.y, hit.z);
        world.set(hit.x, hit.y, hit.z, voxel::kAir);
        dirty_save=true;
#if VOXEL_VALIDATE
        record_edit(hit.x, hit.y, hit.z, removed, 1);
#endif
        spawn_debris(hit.x, hit.y, hit.z, removed);
    }

    void place() noexcept {
        const voxel::RayHit hit = target();
        if (!hit.hit) return;
        const int x = hit.x + hit.nx;
        const int y = hit.y + hit.ny;
        const int z = hit.z + hit.nz;
        if (!world.in_bounds(x, y, z)) return;
        const std::uint8_t existing = world.at(x, y, z);
        if (existing != voxel::kAir && !voxel::block_info(existing).cutout) return;
        const float fx = static_cast<float>(x);
        const float fy = static_cast<float>(y);
        const float fz = static_cast<float>(z);
        const bool overlaps = fx + 1.0f > camera.x - kPlayerRadius &&
                              fx < camera.x + kPlayerRadius &&
                              fz + 1.0f > camera.z - kPlayerRadius &&
                              fz < camera.z + kPlayerRadius &&
                              fy + 1.0f > camera.y &&
                              fy < camera.y + kPlayerHeight;
        if (overlaps) return;
        world.set(x, y, z, selected_block());
        dirty_save=true;
#if VOXEL_VALIDATE
        record_edit(x, y, z, selected_block(), 2);
#endif
        spawn_debris(x, y, z, selected_block());
    }

    void cycle_hotbar(int delta) noexcept {
        hotbar = (hotbar + delta + kQuickCount) % kQuickCount;
    }

    std::uint8_t selected_block() const noexcept {
        return quickbar[hotbar % kQuickCount];
    }

    /* ---- UI ------------------------------------------------------------ */

    /* ---- input: raw pointer events through the SDK canvas -------------- */

    /* Left half drives a movement stick, the right half looks around, the
     * right-hand buttons mine/place/jump/fly and the hotbar selects a block:
     * the same scheme as the reference app. */
    bool stick_active = false;
    int stick_pointer = -1;
    float stick_origin_x = 0.0f;
    float stick_origin_y = 0.0f;
    float move_right = 0.0f;
    float move_forward = 0.0f;
    bool look_active = false;
    int look_pointer = -1;
    int mine_pointer = -1, up_pointer = -1, down_pointer = -1;
    float look_last_x = 0.0f;
    float look_last_y = 0.0f;
    std::uint64_t last_draw_tick_us = 0;
    float last_tap_x = 0.0f;
    float last_tap_y = 0.0f;
    bool mining = false;
    float mine_timer = 0.0f;
    bool up_held = false;
    bool down_held = false;

    auto view() {
        using namespace pxa::ui;
        /* One full-window canvas supplies the pointer events; every pixel of
         * the HUD is drawn by the renderer below. */
        return Canvas().input_only().on_pointer(
            [this](const CanvasPointer& pointer) { on_pointer(pointer); });
    }

    static constexpr int kActionButtons = 5;

    void button_center(int index, float& cx, float& cy) const noexcept {
        cx=static_cast<float>(controls.actions[index].cx());
        cy=static_cast<float>(controls.actions[index].cy());
    }

    void on_pointer(const pxa::ui::CanvasPointer& input) noexcept {
        auto pointer=input;
        pointer.x=pxa::ui::canvas_to_surface_coordinate(input.x,display);
        pointer.y=pxa::ui::canvas_to_surface_coordinate(input.y,display);
        using namespace pxa::ui;using enum voxel::Screen;
        if(screen!=game){
            if(busy||initializing)return;
            if(pointer.phase==pointer_phase_down&&ui_pointer<0){
                for(unsigned i=0;i<menu_hit_count;++i)if(menu_hits[i].rect.contains(pointer.x,pointer.y)){ui_pointer=pointer.pointer_id;ui_pressed=int(i);ui_dirty=true;break;}
            }else if(pointer.pointer_id==ui_pointer&&(pointer.phase==pointer_phase_up||pointer.phase==pointer_phase_cancel)){
                int pressed=ui_pressed;ui_pointer=ui_pressed=-1;ui_dirty=true;
                if(pointer.phase==pointer_phase_up&&pressed>=0&&unsigned(pressed)<menu_hit_count&&menu_hits[pressed].rect.contains(pointer.x,pointer.y)){auto hit=menu_hits[pressed];menu_action(hit.action,hit.argument);}
            }return;
        }
        if(busy)return;
        if(pointer.phase==pointer_phase_down){
            if(controls.menu.contains(pointer.x,pointer.y)){show(pause);return;}
            if(controls.bag.contains(pointer.x,pointer.y)){show(inventory);return;}
            for(int i=0;i<(flying?5:3);++i)if(controls.actions[i].contains(pointer.x,pointer.y)){
                if(i==0){
                    if(mine_pointer<0){mine_pointer=pointer.pointer_id;mining=true;mine_timer=0;mine_long_pressed=false;}
                }else if(i==1)place();
                else if(i==2){if(!flying&&on_ground){velocity_y=kJumpSpeed;on_ground=false;}}
                else if(i==3&&up_pointer<0){up_pointer=pointer.pointer_id;up_held=true;}
                else if(i==4&&down_pointer<0){down_pointer=pointer.pointer_id;down_held=true;}
                return;
            }
            if(controls.hotbar.contains(pointer.x,pointer.y)){
                int rel=pointer.x-controls.hotbar.x;int slot=rel/(controls.slot+controls.gap);
                if(slot<kQuickCount&&rel%(controls.slot+controls.gap)<controls.slot){hotbar=slot;dirty_save=true;}return;
            }
            if(pointer.x<hud.width/2&&!stick_active){
                stick_pointer=pointer.pointer_id;stick_active=true;stick_origin_x=float(pointer.x);stick_origin_y=float(pointer.y);move_right=move_forward=0;
            }else if(pointer.x>=hud.width/2&&!look_active){
                look_pointer=pointer.pointer_id;look_active=true;look_last_x=float(pointer.x);look_last_y=float(pointer.y);
            }return;
        }
        if(pointer.phase==pointer_phase_move){
            if(stick_active&&pointer.pointer_id==stick_pointer){
                float x=(float(pointer.x)-stick_origin_x)/controls.stick_radius,y=(stick_origin_y-float(pointer.y))/controls.stick_radius;
                float length=std::sqrt(x*x+y*y);
                if(length<.10f)move_right=move_forward=0;
                else{float gain=(std::min(length,1.f)-.10f)/(.90f*length);move_right=x*gain;move_forward=y*gain;}
            }else if(look_active&&pointer.pointer_id==look_pointer){
                float dx=float(pointer.x)-look_last_x,dy=float(pointer.y)-look_last_y;look_last_x=float(pointer.x);look_last_y=float(pointer.y);
                const float sensitivity[]={.65f,1.f,1.5f};
                float gain=sensitivity[settings.sensitivity]/std::max(.5f,controls.scale);
                look(dx*.006f*gain,dy*.005f*gain*(settings.invert_y?-1.f:1.f));dirty_save=true;
            }return;
        }
        if(pointer.phase==pointer_phase_up||pointer.phase==pointer_phase_cancel){
            if(pointer.pointer_id==mine_pointer){
                const bool use=pointer.phase==pointer_phase_up&&!mine_long_pressed&&controls.actions[0].contains(pointer.x,pointer.y);
                mining=false;mine_pointer=-1;mine_timer=0;
                if(use){auto hit=target();if(hit.hit&&world.at(hit.x,hit.y,hit.z)==voxel::kTable)show(workbench);}
            }
            if(pointer.pointer_id==up_pointer){up_held=false;up_pointer=-1;}
            if(pointer.pointer_id==down_pointer){down_held=false;down_pointer=-1;}
            if(pointer.pointer_id==stick_pointer){stick_active=false;stick_pointer=-1;move_right=move_forward=0;}
            if(pointer.pointer_id==look_pointer){look_active=false;look_pointer=-1;}
        }
    }

    /* C-style built-in HUD drawn from the atlas asset: bottom hotbar with
     * block icons and stack counts, round action buttons on the right. */
    template<class Qx, class Qy>
    void blit_hud(pxa::game::Frame& frame, float x, float y, float w, float h,
                  float u0, float v0, float u1, float v1,
                  const Qx& qx,
                  const Qy& qy,
                  pxa::game::Color565 flat) const noexcept {
        const std::array<pxa::game::Vertex, 4> quad{{
            {.x_q4 = qx(x), .y_q4 = qy(y), .u_q4 = static_cast<std::int16_t>(u0),
             .v_q4 = static_cast<std::int16_t>(v0), .light = 0,
             .depth_q8 = 1},
            {.x_q4 = qx(x + w), .y_q4 = qy(y),
             .u_q4 = static_cast<std::int16_t>(u1),
             .v_q4 = static_cast<std::int16_t>(v0), .light = 0, .depth_q8 = 1},
            {.x_q4 = qx(x + w), .y_q4 = qy(y + h),
             .u_q4 = static_cast<std::int16_t>(u1),
             .v_q4 = static_cast<std::int16_t>(v1), .light = 0, .depth_q8 = 1},
            {.x_q4 = qx(x), .y_q4 = qy(y + h),
             .u_q4 = static_cast<std::int16_t>(u0),
             .v_q4 = static_cast<std::int16_t>(v1), .light = 0, .depth_q8 = 1}}};
        pxa::game::PolygonOptions options;
        options.affine_uv = true;
        options.lit_palette = true;
        options.transparent_index0 = true;
        frame.textured_quad(pxa::game::AtlasBinding{
                                static_cast<std::uint8_t>(kHudTexture)},
                            quad, options);
    }

    void draw_hud(pxa::game::Frame& frame, std::uint16_t width,
                  std::uint16_t height) const noexcept {
        const float scale_x =
            static_cast<float>(width) / static_cast<float>(hud.width) * 16.0f;
        const float scale_y =
            static_cast<float>(height) / static_cast<float>(hud.height) * 16.0f;
        auto qx = [scale_x](float logical) {
            return static_cast<std::int16_t>(std::lround(logical * scale_x));
        };
        auto qy = [scale_y](float logical) {
            return static_cast<std::int16_t>(std::lround(logical * scale_y));
        };
        auto rect = [&](float x0, float y0, float x1, float y1,
                        pxa::game::Color565 color) {
            frame.quad({qx(x0), qy(y0), qx(x1), qy(y0), qx(x1), qy(y1),
                        qx(x0), qy(y1)},
                       color);
        };
        constexpr pxa::game::Color565 kPanel{0x2104};
        constexpr pxa::game::Color565 kIcon{0xef7d};
        constexpr pxa::game::Color565 kSelect{0xffe0};
        constexpr pxa::game::Color565 kActive{0xf800};
        /* Atlas layout: 16x16 cells. (0,0) round button, (1,0) pickaxe,
         * (2,0) block, (3,0) jump, (0,1) fly, digits from (4,1) at 4 px pitch. */
        const float uv = 16.0f; /* one atlas texel in q4 units */
        const float cell = 16.0f * uv;

        for (int index = 0; index < kQuickCount; ++index) {
            const float slot = static_cast<float>(hud.slot);
            const float x0 = static_cast<float>(hud.hotbar_x) +
                             static_cast<float>(index) * (slot + hud.slot_gap);
            const float y0 = static_cast<float>(hud.hotbar_y);
            rect(x0, y0, x0 + slot, y0 + slot, kPanel);
            const std::uint8_t block = quickbar[index];
            const std::uint8_t texture = voxel::block_info(block).texture[1];
            const std::array<pxa::game::Vertex, 4> icon{{
                {.x_q4 = qx(x0 + 2.0f), .y_q4 = qy(y0 + 2.0f), .u_q4 = 0,
                 .v_q4 = 0, .light = 0, .depth_q8 = 1},
                {.x_q4 = qx(x0 + slot - 2.0f), .y_q4 = qy(y0 + 2.0f),
                 .u_q4 = 256, .v_q4 = 0, .light = 0, .depth_q8 = 1},
                {.x_q4 = qx(x0 + slot - 2.0f), .y_q4 = qy(y0 + slot - 2.0f),
                 .u_q4 = 256, .v_q4 = 256, .light = 0, .depth_q8 = 1},
                {.x_q4 = qx(x0 + 2.0f), .y_q4 = qy(y0 + slot - 2.0f),
                 .u_q4 = 0, .v_q4 = 256, .light = 0, .depth_q8 = 1}}};
            pxa::game::PolygonOptions icon_options;
            icon_options.affine_uv = true;
            icon_options.lit_palette = true;
            icon_options.transparent_index0 = true;
            frame.textured_quad(pxa::game::AtlasBinding{texture}, icon,
                                icon_options);
            if (index == hotbar) {
                rect(x0, y0, x0 + slot, y0 + 1.6f, kSelect);
                rect(x0, y0 + slot - 1.6f, x0 + slot, y0 + slot, kSelect);
                rect(x0, y0, x0 + 1.6f, y0 + slot, kSelect);
                rect(x0 + slot - 1.6f, y0, x0 + slot, y0 + slot, kSelect);
            }
            /* Stack count: two digits from the atlas (creative stack of 64). */
            const float digit_w = slot * 0.28f;
            const float digit_h = digit_w * 6.0f / 4.0f;
            blit_hud(frame, x0 + slot - digit_w * 2.4f,
                     y0 + slot - digit_h - 1.0f, digit_w, digit_h,
                     16.0f * uv + 6 * 4.0f * uv, 20.0f * uv,
                     16.0f * uv + 7 * 4.0f * uv, 25.0f * uv, qx, qy, kIcon);
            blit_hud(frame, x0 + slot - digit_w * 1.1f,
                     y0 + slot - digit_h - 1.0f, digit_w, digit_h,
                     16.0f * uv + 4 * 4.0f * uv, 20.0f * uv,
                     16.0f * uv + 5 * 4.0f * uv, 25.0f * uv, qx, qy, kIcon);
        }

        for (int index = 0; index < (flying?5:3); ++index) {
            float cx = 0.0f;
            float cy = 0.0f;
            button_center(index, cx, cy);
            const float radius = static_cast<float>(hud.button) * 0.5f;
            const bool active = (index == 0 && mining) || (index == 3 && up_held) || (index == 4 && down_held);
            if (active)
                rect(cx - radius - 1.5f, cy - radius - 1.5f, cx + radius + 1.5f,
                     cy + radius + 1.5f, kActive);
            blit_hud(frame, cx - radius, cy - radius, radius * 2.0f,
                     radius * 2.0f, 0.0f, 0.0f, cell, cell, qx, qy, kPanel);
            if(index>=3){
                // Distinct up/down arrows without another texture or buffer.
                const float direction=index==3?-1.f:1.f;
                const float tip_y=cy+direction*radius*.55f;
                const float base_y=cy-direction*radius*.10f;
                frame.quad({qx(cx-radius*.45f),qy(base_y),qx(cx),qy(tip_y),
                            qx(cx+radius*.45f),qy(base_y),qx(cx+radius*.45f),qy(base_y)},kIcon);
                rect(cx-radius*.13f,std::min(cy-direction*radius*.48f,base_y),
                     cx+radius*.13f,std::max(cy-direction*radius*.48f,base_y),kIcon);
                continue;
            }
            /* 0 pickaxe, 1 block, 2 jump */
            float glyph_u = 0.0f;
            float glyph_v = 0.0f;
            switch (index) {
                case 0: glyph_u = cell; break;
                case 1: glyph_u = 2.0f * cell; break;
                case 2: glyph_u = 3.0f * cell; break;
                default: glyph_u = 0.0f; glyph_v = cell; break;
            }
            blit_hud(frame, cx - radius, cy - radius, radius * 2.0f,
                     radius * 2.0f, glyph_u, glyph_v, glyph_u + cell,
                     glyph_v + cell, qx, qy, kIcon);
        }

        // Menu and backpack are separate from look gestures, with identical
        // visual/hit rectangles and no double-tap flight shortcut.
        auto menu=controls.menu;rect(menu.x,menu.y,menu.x+menu.w,menu.y+menu.h,kPanel);
        for(int i=0;i<3;++i)rect(menu.x+menu.w*.25f,menu.y+menu.h*(.28f+.20f*i),menu.x+menu.w*.75f,menu.y+menu.h*(.34f+.20f*i),kIcon);
        auto bag=controls.bag;rect(bag.x,bag.y,bag.x+bag.w,bag.y+bag.h,kPanel);
        for(int i=0;i<9;++i){float x=bag.x+bag.w*(.20f+.23f*(i%3)),y=bag.y+bag.h*(.20f+.23f*(i/3));rect(x,y,x+bag.w*.14f,y+bag.h*.14f,kIcon);}
        const std::int16_t cross_x = static_cast<std::int16_t>(width * 8);
        const std::int16_t cross_y = static_cast<std::int16_t>(height * 8);
        constexpr std::int16_t arm = 12 * 16;
        constexpr std::int16_t thin = 1 * 16;
        rect(static_cast<float>(cross_x - arm) / 16.0f,
             static_cast<float>(cross_y - thin) / 16.0f,
             static_cast<float>(cross_x + arm) / 16.0f,
             static_cast<float>(cross_y + thin) / 16.0f, kIcon);
        rect(static_cast<float>(cross_x - thin) / 16.0f,
             static_cast<float>(cross_y - arm) / 16.0f,
             static_cast<float>(cross_x + thin) / 16.0f,
             static_cast<float>(cross_y + arm) / 16.0f, kIcon);
    }

    static void line_quad(pxa::game::Frame& frame, std::int16_t x0,
                          std::int16_t y0, std::int16_t x1, std::int16_t y1,
                          float thickness, float sx, float sy,
                          pxa::game::Color565 color) noexcept {
        const float dx = static_cast<float>(x1 - x0);
        const float dy = static_cast<float>(y1 - y0);
        const float length = std::sqrt(dx * dx + dy * dy);
        if (length < 0.5f) return;
        const float half = thickness * 0.5f;
        const float nx = -dy / length * half;
        const float ny = dx / length * half;
        frame.quad({static_cast<std::int16_t>(x0 + nx),
                    static_cast<std::int16_t>(y0 + ny),
                    static_cast<std::int16_t>(x1 + nx),
                    static_cast<std::int16_t>(y1 + ny),
                    static_cast<std::int16_t>(x1 - nx),
                    static_cast<std::int16_t>(y1 - ny),
                    static_cast<std::int16_t>(x0 - nx),
                    static_cast<std::int16_t>(y0 - ny)},
                   color);
        (void)sx;
        (void)sy;
    }

    /* ---- lifecycle ----------------------------------------------------- */

    pxa::Task<void> initialize(pxa::Context& context) {
        auto window = co_await context.window().snapshot();
        if (!window) {
            initializing = false;
            co_return std::unexpected(window.error());
        }
        /* Full screen: whatever the compositor reports, no downscale. */
        std::uint16_t width = static_cast<std::uint16_t>(window->pixel_width);
        std::uint16_t height = static_cast<std::uint16_t>(window->pixel_height);
        if (width < 64) width = 64;
        if (height < 64) height = 64;
        pxa::game::RenderOptions options;
        on_window_changed(*window);
        options.width = width;
        options.height = height;
        options.scratch = pxa::game::Scratch::depth16;
        // All visible content, including the HUD, is drawn into this surface.
        // The input-only UI tree still lets the Host fall back for overlays.
        options.direct_scanout = true;
        /* Render scaling needs the automatic target (width/height zero plus
         * RenderOptions::scale). That path does not come up on this host yet,
         * so render at native size and let the quality ladder manage cost. */
        options.scale = 0;
        /* The default 4 KiB draw list is far too small for a voxel mesh; the
         * host accepts up to 48 KiB and PXA_RASTER_MAX_DRAW_BYTES is 49152. */
        options.max_draw_bytes = 49152;
        std::optional<pxa::game::Renderer> created_renderer;
        pxa::Error create_error = pxa::Error::internal;
        {
            auto created = co_await context.game().create(options);
            if (created) {
                created_renderer.emplace(std::move(*created));
            } else {
                create_error = created.error();
                if (options.scale != 0) {
                    /* Unsupported scale: retry with the host default. */
                    options.scale = 0;
                    auto fallback = co_await context.game().create(options);
                    if (fallback)
                        created_renderer.emplace(std::move(*fallback));
                    else
                        create_error = fallback.error();
                }
            }
        }
        if (!created_renderer) {
            initializing = false;
            co_return std::unexpected(create_error);
        }
        auto created = std::move(created_renderer);
        const auto render_info = created->info();
        auto projection = pxa::game3d::Projector::create(
            render_info.render_width, render_info.render_height,
            VOXEL_BENCH_SCENE ? VOXEL_BENCH_FOV : 1.15f, 0.25f,
            VOXEL_BENCH_SCENE ? VOXEL_BENCH_FAR : 64.0f);
        if (!projection) {
            initializing = false;
            co_return std::unexpected(projection.error());
        }

        /* "assets/bNN.pxr": 'b' sits at 7, the two digits at 8 and 9. */
        char path[] = "assets/b00.pxr";
        for (int index = 0; index < kBlockTextures; ++index) {
            path[8] = static_cast<char>('0' + index / 10);
            path[9] = static_cast<char>('0' + index % 10);
            auto loaded =
                co_await context.assets().load(pxa::AssetKind::texture, path);
            if (!loaded) {
                char message[64];
                const int written = std::snprintf(
                    message, sizeof(message), "texture %d failed err=%d", index,
                    static_cast<int>(loaded.error()));
                if (written > 0 && written < static_cast<int>(sizeof(message)))
                    (void)context.log().write(
                        pxa::LogLevel::error,
                        std::string_view(message,
                                         static_cast<std::size_t>(written)));
                initializing = false;
                co_return std::unexpected(loaded.error());
            }
            auto bound = created->bind_asset(*loaded, {static_cast<std::uint8_t>(index)});
            if (!bound) { initializing = false; co_return std::unexpected(bound.error()); }
            // The renderer owns its reference; release each temporary load
            // handle before requesting the next texture.

        }
        {
            auto hud_asset =
                co_await context.assets().load(pxa::AssetKind::texture,
                                               "assets/hud.pxr");
            if (!hud_asset) {
                (void)context.log().write(pxa::LogLevel::error,
                                          "hud atlas load failed");
                initializing = false;
                co_return std::unexpected(hud_asset.error());
            }
            auto bound = created->bind_asset(*hud_asset, {kHudTexture});
            if (!bound) { initializing = false; co_return std::unexpected(bound.error()); }
        }
        auto loaded_palette = co_await context.assets().load(
            pxa::AssetKind::palette, "assets/palette-lit.pxr");
        if (!loaded_palette) {
            initializing = false;
            co_return std::unexpected(loaded_palette.error());
        }
        auto bound = created->bind_asset(*loaded_palette);
        if (!bound) {
            initializing = false;
            co_return std::unexpected(bound.error());
        }
        if constexpr(!VOXEL_BENCH_SCENE&&!VOXEL_BENCH_DISTANCE){
            font_size=voxel::select_font_size(controls.text_scale);
            char font_path[40];std::snprintf(font_path,sizeof(font_path),"assets/menu-font-%d.pxr",font_size);
            auto font=co_await context.assets().load(pxa::AssetKind::texture,font_path);
            if(!font){initializing=false;co_return std::unexpected(font.error());}
            auto bound_font=created->bind_asset(*font,{46});
            if(!bound_font){initializing=false;co_return std::unexpected(bound_font.error());}
        }
        renderer.emplace(std::move(*created));
        projector.emplace(*projection);
        {
            const auto caps = renderer->capabilities();
            constexpr std::uint32_t kPainterNeeds =
                (1u << 1) | (1u << 5) | (1u << 7) | (1u << 8) | (1u << 15);
            char message[128];
            const int written = std::snprintf(
                message, sizeof(message),
                "gfx caps=0x%08x painter_needs=0x%08x missing=0x%08x",
                static_cast<unsigned>(caps),
                static_cast<unsigned>(kPainterNeeds),
                static_cast<unsigned>(kPainterNeeds & ~caps));
            if (written > 0 && written < static_cast<int>(sizeof(message)))
                (void)context.log().write(
                    pxa::LogLevel::info,
                    std::string_view(message,
                                     static_cast<std::size_t>(written)));
        }
        {
            const auto& info = renderer->info();
            char message[128];
            const int written = std::snprintf(
                message, sizeof(message),
                "gfx render=%ux%u scale=%u max_draw=%u tex=%u",
                static_cast<unsigned>(info.render_width),
                static_cast<unsigned>(info.render_height),
                static_cast<unsigned>(info.render_scale),
                static_cast<unsigned>(info.max_draw_bytes),
                static_cast<unsigned>(info.max_textures));
            if (written > 0 && written < static_cast<int>(sizeof(message)))
                (void)context.log().write(
                    pxa::LogLevel::info,
                    std::string_view(message,
                                     static_cast<std::size_t>(written)));
        }

        if constexpr (VOXEL_BENCH_SCENE||VOXEL_BENCH_DISTANCE){world.generate(0x5ae1u);respawn();session_ready=true;}
        else {if(!catalog_ready)(void)co_await read_catalog();auto now=co_await context.clock().now();if(now)rng^=std::uint32_t(*now);}

#if VOXEL_BENCH_SCENE
        camera.x=VOXEL_BENCH_X; camera.y=VOXEL_BENCH_EYE_Y-kEyeHeight;
        camera.z=VOXEL_BENCH_Z; camera.yaw=VOXEL_BENCH_YAW; camera.pitch=VOXEL_BENCH_PITCH;
        budget_max_distance=VOXEL_BENCH_FAR;
#endif
        initializing = false;
        (void)context.log().write(pxa::LogLevel::info, "Voxel Craft C++ ready");
        co_return pxa::Result<void>{};
    }

    /* Adaptive view distance, mirroring the reference app: shrink when the
     * frame rate dips, grow back when there is headroom. */
    std::uint32_t fps_window_frames = 0;
    std::uint32_t fps_window_us = 0;
    float budget_max_distance = 32.0f;
    bool low_quality = false; /* tier 2: unshaded palette colors */
    bool quality_painter = true;
    bool host_slow = false;
    std::uint64_t last_dropped_frames = 0;
    bool fullscreen_requested = false;

    /* The Guest submit rate says nothing about what the player sees: the host
     * raster is the real cost, and telemetry reports it directly. */
    void sample_host_cost(pxa::game::Renderer& renderer) noexcept {
        auto telemetry = renderer.telemetry();
        if (!telemetry) return;
        const std::uint64_t dropped = telemetry->dropped_frames;
        const std::uint64_t dropped_delta =
            dropped >= last_dropped_frames ? dropped - last_dropped_frames : 0;
        last_dropped_frames = dropped;
        host_raster_us = telemetry->last_host_raster_us;
        /* 16 fps leaves about 60 ms per frame, and the host also needs to
         * present the frame after rasterising it. */
        host_slow = host_raster_us > 45000u || dropped_delta > 2;
    }

    std::uint32_t host_raster_us = 0;
    bool host_sample_due = false;

    void update_view_distance(std::uint32_t delta_us) noexcept {
        fps_window_us += delta_us;
        ++fps_window_frames;
        if (fps_window_us < 2000000u) return; /* two second window */
        const std::uint32_t fps10 =
            fps_window_frames * 10000000u / fps_window_us;
        float distance = budget_max_distance;
        if (host_slow && distance > 12.0f)
            distance = distance > 14.0f ? distance - 2.0f : 12.0f;
        else if (!host_slow && fps10 > 190u && distance < 32.0f)
            distance += 1.0f;
        if (distance < 12.0f) distance = 12.0f;
        if (distance > 32.0f) distance = 32.0f;
        budget_max_distance = distance;
        /* Second tier: once the distance is already at the floor and the frame
         * rate is still poor, use unshaded palette colors. Restore shading only
         * with clear headroom so the tier does not oscillate. */
        if (!low_quality && host_slow && distance <= 12.5f) {
            low_quality = true; /* tier 2: select the unshaded palette row */
        } else if (low_quality && !host_slow && fps10 > 200u) {
            low_quality = false;
        }
        fps_window_us = 0;
        fps_window_frames = 0;
        host_sample_due = true;
    }

    void on_foreground(pxa::Context& context) {
        app_context=&context;ui_dirty=true;
        if (!fullscreen_requested) {
            fullscreen_requested = true;
            const auto requested = context.window().fullscreen();
            if (!requested)
                (void)context.log().write(pxa::LogLevel::warning,
                                          "fullscreen request failed");
        }
        if (renderer || initializing) return;
        initializing = true;
        auto started = context.tasks().start(initialize(context));
        if (!started) {
            initializing = false;
            (void)context.log().write(pxa::LogLevel::error, "init task failed");
        }
    }

    void on_background(pxa::Context&) {
        cancel_controls();ui_dirty=true;
        stick_active = look_active = mining = up_held = down_held = false;
        stick_pointer = look_pointer = mine_pointer = up_pointer = down_pointer = -1;
        move_right = move_forward = 0.0f;
        last_draw_tick_us = 0;
        hud_window_us = fps_frames = fps_window_us = fps_window_frames = 0;
    }

    void on_update(pxa::Context&, std::uint32_t delta_us) {
        // Measurement builds hold the spawn camera and view distance fixed.
        if constexpr (VOXEL_BENCH_DISTANCE > 0 || VOXEL_BENCH_SCENE) {
#if VOXEL_PROFILE
            if (frames >= 120) stage_stats.record(5,0);
#endif
            return;
        }
        if (!renderer||screen!=voxel::Screen::game||busy) return;
#if VOXEL_PROFILE
        const auto update_start=std::chrono::steady_clock::now();
#endif
        const float dt =
            std::min(static_cast<float>(delta_us) / 1000000.0f, 0.05f);
        const float old_x=camera.x,old_y=camera.y,old_z=camera.z;
        const float speed = flying ? kFlySpeed : kWalkSpeed;
        const float rx = std::cos(camera.yaw);
        const float rz = -std::sin(camera.yaw);
        // Ground movement follows yaw independently of look pitch. Preserve
        // analog stick magnitude; normalize only an overlong diagonal.
        float wish_x = -rz * move_forward + rx * move_right;
        float wish_z = rx * move_forward + rz * move_right;
        const float length_squared = wish_x * wish_x + wish_z * wish_z;
        if (length_squared > 1.0f) {
            const float inverse_length = 1.0f / std::sqrt(length_squared);
            wish_x *= inverse_length;
            wish_z *= inverse_length;
        }
        move_axis(0, wish_x * speed * dt);
        move_axis(2, wish_z * speed * dt);
        // Only probe extra collision cells after walking is actually blocked.
        // One-block clearance and current headroom prevent jumps into walls
        // or low ceilings. Flying and airborne movement never auto-jump.
        if(settings.auto_jump&&!flying&&on_ground&&length_squared>.04f&&
           std::fabs(camera.x-old_x)+std::fabs(camera.z-old_z)+.0001f<
               (std::fabs(wish_x)+std::fabs(wish_z))*speed*dt){
            const float length=std::sqrt(wish_x*wish_x+wish_z*wish_z);
            const float px=camera.x+wish_x/length*.5f,pz=camera.z+wish_z/length*.5f;
            if(collides(px,camera.y,pz)&&!collides(px,camera.y+1.05f,pz)&&
               !collides(camera.x,camera.y+1.05f,camera.z)){
                velocity_y=kJumpSpeed;on_ground=false;
            }
        }
        for (auto& particle : particles.items()) {
            if (particle.life <= 0.0f) continue;
            particle.life -= dt;
            particle.vy -= 9.0f * dt;
            particle.x += particle.vx * dt;
            particle.y += particle.vy * dt;
            particle.z += particle.vz * dt;
        }
        if (flying) {
            velocity_y = 0.0f;
            if (up_held) move_axis(1, kFlySpeed * dt);
            if (down_held) move_axis(1, -kFlySpeed * dt);
        } else {
            velocity_y -= kGravity * dt;
            if (velocity_y < -30.0f) velocity_y = -30.0f;
            move_axis(1, velocity_y * dt);
            on_ground = collides(camera.x, camera.y - 0.06f, camera.z);
            if (on_ground && velocity_y < 0.0f) velocity_y = 0.0f;
        }
        if(camera.x!=old_x||camera.y!=old_y||camera.z!=old_z)dirty_save=true;
        if (mining) {
            mine_timer += dt;
            if (mine_timer >= (mine_long_pressed?0.32f:0.38f)) {
                mine_long_pressed=true;
                mine_timer = 0.0f;
                mine();
            }
        }
#if VOXEL_PROFILE
        if (frames >= 120) stage_stats.record(5,static_cast<std::uint32_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now()-update_start).count()));
#endif
    }


    void on_frame(pxa::Context& context, pxa::game::FrameTick tick) {
        if (!renderer || !projector) return;
        if(screen!=voxel::Screen::game){if(ui_dirty)draw_menu(*renderer);return;}
        // Avoid competing with the rasterizer for external memory. Scanout
        // still overlaps the next build; simulation/input keep their tick.
        if (auto pipeline = renderer->telemetry();
            pipeline && !pxa::game::can_build_frame(*pipeline, 1)) return;
        // Skipped render ticks still count toward displayed pacing and the
        // adaptive view budget.
        if (last_draw_tick_us && tick.timestamp_us >= last_draw_tick_us)
            tick.frame_delta_us = static_cast<std::uint32_t>(std::min(
                tick.timestamp_us - last_draw_tick_us, std::uint64_t(UINT32_MAX)));
        last_draw_tick_us = tick.timestamp_us;
        const auto width = renderer->info().render_width;
        const auto height = renderer->info().render_height;
        if (width < 32 || height < 32) return;
        voxel::DrawBudget budget;
        budget.max_distance = VOXEL_BENCH_DISTANCE > 0
                                  ? VOXEL_BENCH_DISTANCE : budget_max_distance;
        budget.lit_palette = !low_quality;
        budget.max_faces = 1600;
        budget.max_bytes = VOXEL_DRAW_BUDGET; /* leave room for the HUD in the list */
        budget.width = static_cast<std::int32_t>(width);
        budget.height = static_cast<std::int32_t>(height);
        budget.focal = projector->focal_length();
        (void)projector->clip_range(0.25f, budget.max_distance);
        budget.painter = quality_painter;
        budget.cheap_paths = VOXEL_CHEAP_PATHS != 0 &&
                             (!VOXEL_BENCH_SCENE || VOXEL_BENCH_CHEAP != 0);
#if VOXEL_PROFILE
        const auto build_start = std::chrono::steady_clock::now();
#endif
        auto eye_camera = camera;
        eye_camera.y += kEyeHeight;
        auto frame = renderer->frame(commands);
        frame.clear({0x867d});
#if VOXEL_PROFILE
        budget.staging=commands.bytes().subspan(frame.bytes_used());
#endif
        const auto stats = voxel::draw_world(frame, *projector, world, eye_camera, budget);
#if VOXEL_PROFILE
        const auto geometry_end=std::chrono::steady_clock::now();
#endif
        voxel::draw_particles(frame, *projector, eye_camera, particles.items());
        if constexpr (!VOXEL_BENCH_SCENE || VOXEL_BENCH_HUD) draw_hud(frame, width, height);
        const auto used = frame.bytes_used();
#if VOXEL_PROFILE
        const auto build_us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - build_start).count();
#endif
#if VOXEL_PROFILE
        const auto submit_start=std::chrono::steady_clock::now();
#endif
        const auto submitted = frame.submit();
#if VOXEL_PROFILE
        const auto submit_end=std::chrono::steady_clock::now();
        auto micros=[](auto duration) { return static_cast<std::uint32_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(duration).count()); };
        // Initial mesh building belongs to launch measurements. Steady phase
        // statistics start after 120 completed frames, as the device capture.
        if (frames >= 120) {
            stage_stats.record(0,micros(submit_end-build_start));
            stage_stats.record(3,micros(submit_start-geometry_end));
            stage_stats.record(4,micros(submit_end-submit_start));
            stage_stats.record(1,stats.geometry_us);
            stage_stats.record(2,stats.encode_us);
        }
#endif
        if (!submitted) {
            if (!submit_error_logged) {
                submit_error_logged = true;
                char message[96];
                const int written = std::snprintf(
                    message, sizeof(message), "submit failed err=%d bytes=%u",
                    static_cast<int>(submitted.error()),
                    static_cast<unsigned>(used));
                if (written > 0 && written < static_cast<int>(sizeof(message)))
                    (void)context.log().write(
                        pxa::LogLevel::error,
                        std::string_view(message,
                                         static_cast<std::size_t>(written)));
            }
            return;
        }
        ++frames;
        if constexpr (VOXEL_BENCH_DISTANCE == 0 && !VOXEL_BENCH_SCENE)
            if(settings.auto_distance)update_view_distance(tick.frame_delta_us);
        if (host_sample_due) {
            host_sample_due = false;
            sample_host_cost(*renderer);
        }
        last_faces = stats.faces_drawn;
        last_batches = stats.batches;
        last_exhausted = stats.exhausted;
        hud_window_us += tick.frame_delta_us;
        ++fps_frames;
        if (hud_window_us >= 1000000u) {
            char message[160];
            const int written = std::snprintf(
                message, sizeof(message), "VOXEL-CPP fps10=%u faces=%u batches=%u view=%u q=%u raster_us=%u affine=%u solid=%u"
#if VOXEL_PROFILE
                " guest_us=%u"
#endif
                ,
                static_cast<unsigned>(std::uint64_t(fps_frames) * 10000000u / hud_window_us),
                static_cast<unsigned>(last_faces),
                static_cast<unsigned>(last_batches),
                static_cast<unsigned>(budget.max_distance),
                static_cast<unsigned>(low_quality ? 1 : 0),
                static_cast<unsigned>(host_raster_us),
                static_cast<unsigned>(stats.affine_faces),
                static_cast<unsigned>(stats.solid_faces)
#if VOXEL_PROFILE
                ,static_cast<unsigned>(build_us)
#endif
                );
            if (written > 0 && written < static_cast<int>(sizeof(message)))
                (void)context.log().write(
                    pxa::LogLevel::info,
                    std::string_view(message,
                                     static_cast<std::size_t>(written)));
#if VOXEL_VALIDATE
            const auto hit = target();
            const int state_written = std::snprintf(message, sizeof(message),
                "VOXEL-STATE xq8=%d yq8=%d zq8=%d yawq10=%d pitchq10=%d fly=%u ground=%u hit=%u bx=%d by=%d bz=%d block=%u",
                int(camera.x * 256), int(camera.y * 256), int(camera.z * 256),
                int(camera.yaw * 1024), int(camera.pitch * 1024),
                unsigned(flying), unsigned(on_ground), unsigned(hit.hit),
                hit.x, hit.y, hit.z, unsigned(hit.hit ? world.at(hit.x, hit.y, hit.z) : 0));
            if (state_written > 0 && state_written < static_cast<int>(sizeof(message)))
                (void)context.log().write(pxa::LogLevel::info,
                    std::string_view(message, static_cast<std::size_t>(state_written)));
            const int edit_written = std::snprintf(message, sizeof(message),
                "VOXEL-EDIT count=%u packed=%u budget=%u exhausted=%u bytes=%u",
                unsigned(edit_count), unsigned(last_edit), unsigned(budget.max_bytes),
                unsigned(stats.exhausted), unsigned(frame.bytes_used()));
            if (edit_written > 0 && edit_written < static_cast<int>(sizeof(message)))
                (void)context.log().write(pxa::LogLevel::info,
                    std::string_view(message, static_cast<std::size_t>(edit_written)));
#endif
#if VOXEL_PROFILE
            if (auto pipeline=renderer->telemetry()) {
                const int pipeline_written=std::snprintf(message,sizeof(message),
                    "VOXEL-PIPE sub=%llu render=%llu visible=%llu drop=%llu queue_us=%llu present_us=%llu host_us=%u",
                    static_cast<unsigned long long>(pipeline->submitted_frames),
                    static_cast<unsigned long long>(pipeline->rendered_frames),
                    static_cast<unsigned long long>(pipeline->visible_frames),
                    static_cast<unsigned long long>(pipeline->dropped_frames),
                    static_cast<unsigned long long>(pipeline->queue_wait_us),
                    static_cast<unsigned long long>(pipeline->present_us),pipeline->last_host_raster_us);
                if (pipeline_written>0 && pipeline_written<int(sizeof(message)))
                    (void)context.log().write(pxa::LogLevel::info,
                        std::string_view(message,static_cast<std::size_t>(pipeline_written)));
            }
            const int stage_written=std::snprintf(message,sizeof(message),
                "VOXEL-STAGES total_us=%u geometry_us=%u encode_us=%u hud_us=%u submit_us=%u update_us=%u samples=%u",
                stage_stats.stages[0].mean_us(),stage_stats.stages[1].mean_us(),
                stage_stats.stages[2].mean_us(),stage_stats.stages[3].mean_us(),
                stage_stats.stages[4].mean_us(),stage_stats.stages[5].mean_us(),stage_stats.stages[1].samples);
            if (stage_written>0 && stage_written<int(sizeof(message)))
                (void)context.log().write(pxa::LogLevel::info,
                    std::string_view(message,static_cast<std::size_t>(stage_written)));
#endif
            fps_frames = 0;
            hud_window_us = 0;
        }
    }
};

}  // namespace

PXA_GAME(VoxelCraft)
