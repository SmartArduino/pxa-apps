#include "../main.cpp"
#include <cassert>
extern "C" std::int32_t pxa_submit(const std::uint8_t*, std::uint32_t) { return 0; }
static unsigned sounds=0;
extern "C" std::int32_t pxa_io(std::uint64_t handle, std::uint32_t opcode, std::uint8_t* data, std::uint32_t size) {
    if(handle==88&&opcode==2){assert(size>0&&size<=640&&size%2==0);++sounds;return size;}
    return -3;
}
int main() {
    static VoxelCraft game;
    game.screen=voxel::Screen::game;game.apply_display({});
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
    assert(!game.flying); // Drag cannot enable flight.
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

    // Partial stick deflection must allow precise movement; looking up/down
    // must not alter ground speed, and a diagonal must not move faster.
    pxa::Transport transport;
    transport.phase(pxa::Phase::event);
    game.renderer.emplace(transport, 77, 0u);
    for (int x = 0; x < voxel::kWorldX; ++x)
        for (int z = 0; z < voxel::kWorldZ; ++z)
            game.world.set(x, 0, z, voxel::kStone);
    for (float pitch : {0.0f, 1.2f}) {
        for (float yaw : {0.0f, 0.7f}) {
            for (float stick : {0.25f, 1.0f}) {
                game.camera = {20.5f, 1.05f, 20.5f, yaw, pitch};
                game.move_forward = stick;
                game.move_right = 0;
                game.velocity_y = 0;
                for (int i = 0; i < 60; ++i) game.on_update(context, 16000);
                const float dx = game.camera.x - 20.5f, dz = game.camera.z - 20.5f;
                assert(std::fabs(std::sqrt(dx * dx + dz * dz) - 4.4f * .96f * stick) < .002f);
                assert(game.on_ground && !game.collides(game.camera.x, game.camera.y, game.camera.z));
            }
        }
    }
    game.camera = {20.5f, 1.05f, 20.5f};
    game.move_forward = game.move_right = 1;
    for (int i = 0; i < 60; ++i) game.on_update(context, 16000);
    const float dx = game.camera.x - 20.5f, dz = game.camera.z - 20.5f;
    assert(std::fabs(std::sqrt(dx * dx + dz * dz) - 4.4f * .96f) < .002f);
    // A wall remains solid throughout repeated updates; removing its blocks
    // permits movement immediately, independently of render mesh rebuilding.
    for (int y = 1; y <= 3; ++y) game.world.set(20, y, 24, voxel::kStone);
    game.camera = {20.5f, 1.05f, 20.5f};
    game.move_right = 0;
    for (int i = 0; i < 60; ++i) game.on_update(context, 16000);
    assert(game.camera.z < 23.7f && game.camera.z > 23.5f);
    for (int y = 1; y <= 3; ++y) game.world.set(20, y, 24, voxel::kAir);
    for (int i = 0; i < 30; ++i) game.on_update(context, 16000);
    assert(game.camera.z > 25);
    // Auto-jump clears a single block, but neither tall walls nor ceilings.
    for(bool enabled:{false,true}){
        game.settings.auto_jump=enabled;game.camera={20.5f,1.05f,22.5f};game.on_ground=true;
        game.velocity_y=0;game.move_forward=1;game.world.set(20,1,23,voxel::kStone);
        float peak=game.camera.y;
        for(int i=0;i<65;++i){game.on_update(context,16000);peak=std::max(peak,game.camera.y);}
        assert(enabled?(peak>2.f&&game.camera.z>23.8f):(peak<1.1f&&game.camera.z<22.8f));
    }
    game.settings.auto_jump=true;game.world.set(20,2,23,voxel::kStone);
    game.camera={20.5f,1.05f,22.5f};game.on_ground=true;game.velocity_y=0;
    for(int i=0;i<50;++i)game.on_update(context,16000);
    assert(game.camera.y<1.1f&&game.camera.z<22.8f);
    game.world.set(20,2,23,voxel::kAir);game.world.set(20,3,22,voxel::kStone);
    game.camera={20.5f,1.05f,22.5f};game.on_ground=true;game.velocity_y=0;
    for(int i=0;i<50;++i)game.on_update(context,16000);
    assert(game.camera.y<1.1f&&game.camera.z<22.8f);
    game.world.set(20,3,22,voxel::kAir);game.world.set(20,1,23,voxel::kAir);
    game.settings.auto_jump=false;game.move_forward=0;game.camera={20.5f,1.05f,20.5f};game.on_ground=true;game.velocity_y=0;
    game.world.set(20,2,22,voxel::kTable);
    auto press=pointer(9,0,game.controls.actions[0].cx(),game.controls.actions[0].cy());
    auto release=press;release.phase=2;
    game.on_pointer(press);assert(game.screen==voxel::Screen::game&&game.mining);
    for(int i=0;i<5;++i)game.on_update(context,16000);
    game.on_pointer(release);assert(game.screen==voxel::Screen::workbench&&game.world.at(20,2,22)==voxel::kTable);
    game.show(voxel::Screen::game);game.on_pointer(press);
    for(int i=0;i<25;++i)game.on_update(context,16000);
    assert(game.world.at(20,2,22)==voxel::kTable&&game.digging.progress()>0&&game.digging.progress()<1);
    for(int i=0;i<100;++i)game.on_update(context,16000);
    assert(game.screen==voxel::Screen::game&&game.world.at(20,2,22)==voxel::kAir&&game.mine_long_pressed);
    assert(game.inventory.count(voxel::kTable)==1);
    game.on_pointer(release);assert(game.screen==voxel::Screen::game);
    // Canceling a short press must never invoke the workbench.
    game.world.set(20,2,22,voxel::kTable);game.on_pointer(press);release.phase=3;game.on_pointer(release);assert(game.screen==voxel::Screen::game);
    // Collect foliage without deleting the dirt underneath; place consumes
    // exactly one stack item. Capacity failure preserves the selected block.
    game.world.set(20,2,22,voxel::kBush);game.world.set(20,2,23,voxel::kDirt);
    game.inventory={};
    pxa::RequestTable requests;game.audio.emplace(transport,requests,88,pxa::AudioFormat{16000,1,20});
    game.on_pointer(press);for(int i=0;i<20;++i)game.on_update(context,16000);
    assert(game.world.at(20,2,22)==voxel::kBush&&game.digging.progress()>0);
    for(int i=0;i<12;++i)game.on_update(context,16000);
    assert(game.world.at(20,2,22)==voxel::kAir&&game.world.at(20,2,23)==voxel::kDirt);
    game.cancel_controls();assert(game.digging.key==0&&game.inventory.count(voxel::kBush)==1);
    game.hotbar=0;game.place();assert(game.world.at(20,2,22)==voxel::kBush&&game.inventory.count(voxel::kBush)==0);
    const unsigned sounded=sounds;game.place();assert(sounds==sounded); // Empty hand, no phantom placement/sound.
    for(auto& s:game.inventory.slots)s={voxel::kStone,64};
    game.on_pointer(press);for(int i=0;i<40;++i)game.on_update(context,16000);
    assert(game.world.at(20,2,22)==voxel::kBush&&game.inventory.count(voxel::kBush)==0);
    game.on_background(context);assert(game.digging.key==0&&sounds>0);
    game.audio.reset();
    game.renderer.reset();
    std::printf("Gameplay: finite mining/drop/place/full-capacity rollback and %u bounded sound commands OK\n",sounds);
}
