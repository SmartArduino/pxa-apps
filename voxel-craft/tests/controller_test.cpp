#include "pxa_control_capture.h"
#include "../main.cpp"
#include <pxa/raster.h>
#include <cassert>

static std::array<std::uint16_t,320*240> pixels{};
static std::array<std::uint16_t,4096> palette{};
static std::array<std::uint8_t,256*256> texture{};
extern "C" std::int32_t pxa_test_control(const std::uint8_t*,std::uint32_t){return 0;}
extern "C" std::int32_t pxa_io(std::uint64_t handle,std::uint32_t opcode,std::uint8_t* data,std::uint32_t size){
    assert(handle==77&&opcode==0x101);
    pxa_raster_target_t target{};target.pixels=pixels.data();target.width=target.stride_pixels=320;target.height=240;
    pxa_raster_resources_t resources{};resources.capabilities=PXA_RASTER_CAP_KNOWN_MASK;
    resources.palette=palette.data();resources.palette_light_levels=16;
    for(auto& slot:resources.textures)slot={texture.data(),256,256};
    pxa_raster_draw_list_view_t view{};
    assert(pxa_raster_validate_draw_list(data,size,&target,&resources,&view)==PXA_STATUS_OK);
    pxa_raster_execute_draw_list(data,&view,&target,&resources,nullptr);return size;
}
template<class T>concept HasView=requires(T& app){app.view();};
static_assert(!HasView<VoxelCraft>);
static_assert(pxa::detail::AppInput<VoxelCraft>::mask==((1u<<9)|(1u<<6)));

static void check_look_ramp(){
    voxel::ControllerLookAxis fine,coarse;
    float a=0,b=0;
    for(int i=0;i<60;++i)a+=fine.step(1,1.f/60,2.f);
    for(int i=0;i<20;++i)b+=coarse.step(1,.05f,2.f);
    assert(std::fabs(a-b)<.00001f);
    const float full=fine.step(1,.016f,2.f);
    assert(std::fabs(full-.032f)<.00001f);
    const float reverse=fine.step(-1,.016f,2.f);
    assert(reverse<0&&-reverse<full*.5f);
    assert(fine.step(0,.016f,2.f)==0);
    const float restart=fine.step(1,.016f,2.f);
    assert(std::fabs(restart+reverse)<.00001f);
    fine.reset();assert(std::fabs(fine.step(1,.016f,2.f)-restart)<.00001f);
}

static void check_controller_placement(){
    using namespace pxa::input;
    static VoxelCraft app;
    pxa::Context context;context.transport().phase(pxa::Phase::event);
    pxa::game::RenderInfo info;info.capabilities=PXA_RASTER_CAP_KNOWN_MASK;
    info.render_width=320;info.render_height=240;
    app.renderer.emplace(context.transport(),77,info);
    std::uint64_t now=1000000;
    auto pad=[&](std::uint32_t buttons,bool connected=true){
        now+=50000;app.on_controller(context,{.buttons=buttons,.timestamp_us=now,.connected=connected});
    };
    auto step=[&](unsigned ticks){for(unsigned i=0;i<ticks;++i){now+=16000;app.on_update(context,16000);}};
    auto scene=[&]{
        pad(0);app.cancel_controls();
        auto data=app.world.load_staging();std::fill(data.begin(),data.end(),std::byte{});app.world.finish_load(true);
        app.screen=voxel::Screen::game;app.camera={20.5f,1.05f,20.5f};app.velocity_y=0;
        app.flying=true;app.inventory={};app.hotbar=0;app.world.set(20,2,25,voxel::kStone);
    };
    scene();pad(controller_b);
    assert(app.world.at(20,2,24)==voxel::kAir&&app.action_hint_seconds>0);
    assert(std::string_view(app.action_hint)=="空手 先挖掘方块");pad(0);
    // Real pad X mining populates the quickbar; an empty selected slot follows
    // the collected material. Pad B then consumes it at the selected face.
    app.hotbar=3;app.world.set(20,2,22,voxel::kBush);app.world.set(20,2,23,voxel::kDirt);
    pad(controller_x);step(35);pad(0);
    assert(app.world.at(20,2,22)==voxel::kAir&&app.inventory.count(voxel::kBush)==1);
    assert(app.hotbar==0&&app.selected_block()==voxel::kBush);
    pad(controller_b);
    assert(app.world.at(20,2,22)==voxel::kBush&&app.inventory.count(voxel::kBush)==0);
    assert(app.action_hint_seconds==0);pad(0);

    scene();app.inventory.slots[0]={voxel::kPlank,4};pad(controller_b);
    assert(app.world.at(20,2,24)==voxel::kPlank&&app.inventory.count(voxel::kPlank)==3);
    pad(controller_b);step(20); // full-state refresh is not another press
    assert(app.inventory.count(voxel::kPlank)==3);
    step(4);assert(app.world.at(20,2,23)==voxel::kPlank&&app.inventory.count(voxel::kPlank)==2);
    step(12);assert(app.world.at(20,2,22)==voxel::kPlank&&app.inventory.count(voxel::kPlank)==1);
    pad(0);step(40);assert(app.inventory.count(voxel::kPlank)==1&&!app.place_repeat_seconds);
    // Select+B remains item selection. Releasing Select while holding B must
    // not place from the newly selected slot, nor start a delayed repeat.
    app.inventory.slots[1]={voxel::kDirt,4};pad(controller_select|controller_b);
    assert(app.hotbar==1);pad(controller_b);step(40);
    assert(app.inventory.count(voxel::kDirt)==4&&!app.place_repeat_seconds);pad(0);
    pad(controller_b);assert(app.world.at(20,2,21)==voxel::kDirt&&app.inventory.count(voxel::kDirt)==3);
    pad(0);pad(controller_b); // next cell would intersect the player
    assert(app.inventory.count(voxel::kDirt)==3&&app.world.at(20,2,20)==voxel::kAir);
    assert(std::string_view(app.action_hint)=="此处无法放置");pad(0);

    scene();app.inventory.slots[0]={voxel::kWoodPickaxe,1,60};pad(controller_b);
    assert(std::string_view(app.action_hint)=="请选择方块"&&app.inventory.slots[0].wear==60);pad(0);
    app.world.set(20,2,22,voxel::kBush);assert(app.mine(app.target()));
    assert(app.hotbar==0&&app.selected_block()==voxel::kWoodPickaxe); // equipped tools stay selected
    scene();app.inventory.slots[0]={voxel::kDirt,4};app.camera.pitch=-1.3f;pad(controller_b);
    assert(std::string_view(app.action_hint)=="准星未指向方块"&&app.inventory.count(voxel::kDirt)==4);pad(0);
    app.camera.pitch=0;step(120);assert(app.action_hint_seconds==0);
    // Lifecycle transitions cancel repeats and quarantine a held key until up.
    pad(controller_b);const auto count=app.inventory.count(voxel::kDirt);
    pad(0,false);step(40);assert(!app.place_repeat_seconds&&app.inventory.count(voxel::kDirt)==count);
    pad(controller_b);step(40);assert(app.inventory.count(voxel::kDirt)==count);pad(0);
    pad(controller_b);const auto before_pause=app.inventory.count(voxel::kDirt);
    pad(controller_b|controller_start);assert(app.screen==voxel::Screen::pause&&!app.place_repeat_seconds);
    pad(0);pad(controller_start);pad(0);step(40);
    assert(app.screen==voxel::Screen::game&&app.inventory.count(voxel::kDirt)==before_pause);
    pad(controller_b);const auto before_background=app.inventory.count(voxel::kDirt);
    app.on_background(context);pad(controller_b);step(40);
    assert(!app.place_repeat_seconds&&app.inventory.count(voxel::kDirt)==before_background);pad(0);
    app.renderer.reset();
}

int main(){
    check_look_ramp();
    check_controller_placement();
    using namespace pxa::input;
    using enum voxel::Screen;
    static VoxelCraft app;
    pxa::Context context;context.transport().phase(pxa::Phase::event);
    pxa::ui::DisplayMetrics display;display.width=320;display.height=240;app.apply_display(display);
    pxa::game::RenderInfo info;info.capabilities=PXA_RASTER_CAP_KNOWN_MASK;
    info.render_width=320;info.render_height=240;
    app.renderer.emplace(context.transport(),77,info);
    std::uint64_t now=1000000;
    auto pad=[&](std::uint32_t buttons,bool connected=true,std::uint8_t id=0){
        now+=50000;app.on_controller(context,{.buttons=buttons,.timestamp_us=now,.controller=id,.connected=connected});
    };
    auto tap=[&](std::uint32_t button){pad(0);pad(button);pad(0);};
    auto draw=[&]{app.draw_menu(*app.renderer);};
    auto border=[&]{
        const voxel::Rect r=app.menu_hits[app.menu_focus].rect;
        assert(r.x>=0&&r.y>=0&&r.x+r.w<=320&&r.y+r.h<=240);
        for(int x=r.x;x<r.x+r.w;++x){assert(pixels[r.y*320+x]==0xfd20);assert(pixels[(r.y+r.h-1)*320+x]==0xfd20);}
        for(int y=r.y;y<r.y+r.h;++y){assert(pixels[y*320+r.x]==0xfd20);assert(pixels[y*320+r.x+r.w-1]==0xfd20);}
    };
    auto focus=[&](voxel::UiAction action,int argument=0){
        int wanted=-1;
        for(unsigned i=0;i<app.menu_hit_count;++i)
            if(app.menu_hits[i].action==action&&app.menu_hits[i].argument==argument)wanted=int(i);
        assert(wanted>=0);
        for(unsigned i=0;app.menu_focus!=wanted&&i<=app.menu_hit_count;++i)tap(controller_r);
        assert(app.menu_focus==wanted);draw();border();
    };
    draw();tap(controller_down);draw();border();
    assert(app.menu_hits[app.menu_focus].action==voxel::UiAction::load_slots);
    tap(controller_a);draw();assert(app.screen==load_slots&&app.menu_hit_count==1);
    assert(app.menu_hits[0].action==voxel::UiAction::back);tap(controller_b);draw();
    focus(voxel::UiAction::settings);tap(controller_a);draw();assert(app.screen==settings_page);
    focus(voxel::UiAction::invert);tap(controller_a);assert(app.settings.invert_y);
    app.settings.invert_y=false;tap(controller_b);draw();assert(app.screen==title);
    focus(voxel::UiAction::new_game);tap(controller_a);assert(app.screen==game&&app.session_ready);
    // Full-state repeats cannot repeat jump/menu actions or carry a held key through a screen change.
    app.on_ground=true;pad(controller_a);const float velocity=app.velocity_y;
    app.on_ground=true;pad(controller_a);assert(app.velocity_y==velocity&&app.on_ground);pad(0);
    auto empty=app.world.load_staging();std::fill(empty.begin(),empty.end(),std::byte{});app.world.finish_load(true);
    for(int x=0;x<voxel::kWorldX;++x)for(int z=0;z<voxel::kWorldZ;++z)app.world.set(x,0,z,voxel::kStone);
    app.camera={20.5f,1.05f,20.5f};app.velocity_y=0;app.settings.auto_jump=false;
    pad(controller_up|controller_right);
    for(int i=0;i<60;++i)app.on_update(context,16000);
    float dx=app.camera.x-20.5f,dz=app.camera.z-20.5f;
    assert(std::fabs(std::sqrt(dx*dx+dz*dz)-4.4f*.96f)<.002f);pad(0);
    const auto position=app.camera;pad(controller_r);
    app.on_update(context,16000);assert(app.camera.yaw>position.yaw);pad(0);
    pad(controller_select|controller_up);app.on_update(context,16000);
    assert(app.camera.pitch<0);pad(0);
    app.on_update(context,16000);
    const auto looking=app.camera;
    pad(controller_select|controller_down);app.on_update(context,16000);
    assert(app.camera.pitch>looking.pitch&&app.camera.x==looking.x&&app.camera.z==looking.z);
    pad(0);const float released_pitch=app.camera.pitch;app.on_update(context,16000);
    assert(app.camera.pitch==released_pitch);
    app.settings.invert_y=true;
    pad(controller_select|controller_down);app.on_update(context,16000);
    assert(app.camera.pitch<released_pitch);pad(0);app.settings.invert_y=false;
    const int previous=app.hotbar;tap(controller_select|controller_b);assert(app.hotbar==(previous+1)%app.kQuickCount);
    tap(controller_select|controller_a);assert(app.hotbar==previous);
    app.flying=true;app.velocity_y=0;
    pad(controller_select|controller_a);const float height=app.camera.y;
    pad(controller_a);app.on_update(context,16000);assert(app.camera.y==height);pad(0);
    pad(controller_a);app.on_update(context,16000);assert(app.camera.y>height);
    const float raised=app.camera.y;pad(controller_a|controller_down);app.on_update(context,16000);
    assert(app.camera.y<raised);pad(0);app.flying=false;

    app.camera={20.5f,1.05f,20.5f};app.velocity_y=0;
    app.world.set(20,2,22,voxel::kTable);
    tap(controller_x);assert(app.screen==workbench&&app.world.at(20,2,22)==voxel::kTable);
    tap(controller_b);assert(app.screen==game);
    pad(controller_x);for(int i=0;i<150;++i)app.on_update(context,16000);
    assert(app.world.at(20,2,22)==voxel::kAir&&app.inventory.count(voxel::kTable)==1);
    pad(0);assert(app.screen==game&&!app.mining);
    app.world.set(20,2,22,voxel::kTable);pad(controller_x);pad(0,false);
    assert(app.screen==game&&!app.mining);pad(0);
    // A controller cannot steal/release a real finger's mining action.
    pxa::input::Pointer touch{.pointer_id=7,.phase=pointer_phase_down};
    touch.x=app.controls.actions[0].cx();touch.y=app.controls.actions[0].cy();app.on_pointer(touch);
    pad(controller_x);pad(0,false);assert(app.mining&&app.mine_pointer==7);
    touch.phase=pointer_phase_cancel;app.on_pointer(touch);pad(0);
    assert(!app.mining&&app.screen==game);

    tap(controller_y);draw();assert(app.screen==inventory);
    app.inventory.slots[9]={voxel::kWood,2};
    focus(voxel::UiAction::item,9);tap(controller_a);assert(app.inventory_source==9);
    focus(voxel::UiAction::craft_cell,0);tap(controller_a);
    assert(app.crafting.result(app.inventory,2)==voxel::ItemStack(voxel::kPlank,4));
    focus(voxel::UiAction::craft);tap(controller_a);
    assert(app.inventory.count(voxel::kPlank)==4&&app.inventory.count(voxel::kWood)==1);
    focus(voxel::UiAction::craft_cell,0);tap(controller_x);assert(app.crafting.sources[0]==-1);
    focus(voxel::UiAction::item,8);draw();border(); // Bottom edge remains inside the inventory cell.
    tap(controller_b);assert(app.screen==game);
    tap(controller_start);draw();assert(app.screen==pause);
    focus(voxel::UiAction::save_slots);tap(controller_a);app.slot_generations[0]=1;draw();
    focus(voxel::UiAction::slot,0);tap(controller_a);draw();assert(app.screen==confirm_overwrite);
    tap(controller_b);draw();assert(app.screen==save_slots);
    tap(controller_b);draw();assert(app.screen==pause);
    focus(voxel::UiAction::title);tap(controller_a);draw();assert(app.screen==confirm_title);
    tap(controller_b);draw();assert(app.screen==pause);
    tap(controller_start);assert(app.screen==game);
    pad(controller_up|controller_x);app.on_background(context);
    assert(!app.controller.buttons()&&!app.mining&&!app.move_forward);
    pad(controller_up|controller_x);assert(!app.mining&&!(app.controller.buttons()&~app.controller_blocked));
    pad(0);pad(controller_a,true,1);assert(!app.controller.buttons());
    app.renderer.reset();
    std::puts("Voxel controller: movement/look/flight, mining/use/place/repeat, menu/save/settings, inventory/craft, four-edge focus, lifecycle/ownership OK");
}
