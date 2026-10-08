#include "../main.cpp"
#include <cassert>
extern "C" std::int32_t pxa_submit(const std::uint8_t*, std::uint32_t) { return 0; }
extern "C" std::int32_t pxa_io(std::uint64_t, std::uint32_t, std::uint8_t*, std::uint32_t) { return -3; }
int main() {
    static VoxelCraft game;
    game.hud = voxel::hud_layout(296, 240);
    auto pointer = [](unsigned id, unsigned phase, int x, int y, std::uint64_t time = 1000000) {
        pxa::ui::CanvasPointer p;
        p.pointer_id = id; p.phase = phase; p.x = x; p.y = y; p.timestamp_us = time;
        return p;
    };
    game.on_pointer(pointer(1, 0, 40, 90));
    game.on_pointer(pointer(2, 0, 200, 90));
    assert(game.stick_active && game.look_active);
    game.on_pointer(pointer(1, 1, 60, 60));
    const auto movement = game.move_forward;
    game.on_pointer(pointer(2, 1, 225, 100));
    assert(game.camera.yaw > 0 && game.move_forward == movement);
    game.on_pointer(pointer(2, 2, 225, 100));
    assert(!game.look_active && game.stick_active && game.move_forward > 0);
    assert(!game.flying && game.last_tap_us == 0); // Drag cannot become a double tap.
    game.on_pointer(pointer(1, 3, 60, 60));
    assert(!game.stick_active && game.move_forward == 0 && game.move_right == 0);
    game.hotbar = 3;
    game.on_pointer(pointer(3, 0, game.hud.hotbar_x - 1, game.hud.hotbar_y + 2));
    assert(game.hotbar == 3); // Signed division must not select slot zero outside the bar.
    game.on_pointer(pointer(3, 3, 0, 0));
    float x, y;
    game.button_center(0, x, y);
    game.on_pointer(pointer(4, 0, x, y));
    assert(game.mining);
    game.on_pointer(pointer(5, 2, 10, 10));
    assert(game.mining); // An unrelated pointer cannot release a held action.
    game.on_pointer(pointer(4, 2, x, y));
    assert(!game.mining);
    pxa::Context context;
    game.on_pointer(pointer(6, 0, 30, 60));
    game.on_background(context);
    assert(!game.stick_active && !game.look_active && !game.up_held && !game.down_held);
}
