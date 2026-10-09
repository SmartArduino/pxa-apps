#include "../main.cpp"
#include <cassert>
#include <cstdio>
#include <pxa/raster.h>
static std::array<std::uint16_t,296*240> pixels{},depth{};
static std::array<std::uint16_t,16*256> palette{};
static std::array<std::uint8_t,220*220> texture{};
static bool pending_frame = false;
static unsigned draw_submissions = 0;
static std::array<std::uint8_t,49152> last_draw{};
static unsigned last_draw_size=0;
extern "C" std::int32_t pxa_submit(const std::uint8_t*,std::uint32_t){return 0;}
extern "C" std::int32_t pxa_io(std::uint64_t handle,std::uint32_t opcode,std::uint8_t* data,std::uint32_t size){
    assert(handle==77);
    if (opcode==0x102) {
        assert(size==104);std::fill_n(data,size,0);
        pxa::wire::put64(reinterpret_cast<std::byte*>(data),1);
        pxa::wire::put64(reinterpret_cast<std::byte*>(data)+88,pending_frame?0:1);
        return size;
    }
    assert(opcode==0x101);++draw_submissions;
    assert(size<=last_draw.size());std::copy_n(data,size,last_draw.data());last_draw_size=size;
    pxa_raster_target_t target{};target.pixels=pixels.data();target.depth_pixels=depth.data();
    target.width=target.stride_pixels=target.depth_stride_pixels=296;target.height=240;target.scratch_mode=PXA_RASTER_SCRATCH_DEPTH16;
    pxa_raster_resources_t resources{};resources.capabilities=PXA_RASTER_CAP_KNOWN_MASK;
    resources.palette=palette.data();resources.palette_light_levels=16;
    for(auto& slot:resources.textures)slot={texture.data(),220,220};
    pxa_raster_draw_list_view_t view{};
    assert(pxa_raster_validate_draw_list(data,size,&target,&resources,&view)==PXA_STATUS_OK);
    pxa_raster_execute_draw_list(data,&view,&target,&resources,nullptr);
    return size;
}
int main(){
    using enum voxel::Screen;
    static VoxelCraft app;app.apply_display({});assert(app.screen==title);assert(!app.settings.flight&&!app.settings.invert_y&&app.settings.auto_jump);
    app.new_game();assert(app.screen==voxel::Screen::game&&app.session_ready);
    auto pose=app.camera;auto old=app.world.at(10,2,10);app.show(pause);
    pxa::Context ctx;app.velocity_y=7;app.move_forward=1;
    app.on_update(ctx,16000);assert(app.camera.x==pose.x&&app.camera.y==pose.y&&app.velocity_y==7);
    app.menu_action(voxel::UiAction::title);assert(app.screen==confirm_title);
    app.menu_action(voxel::UiAction::back);assert(app.screen==pause);
    app.menu_action(voxel::UiAction::settings);assert(app.screen==settings_page);
    app.menu_action(voxel::UiAction::fly);assert(app.flying);app.menu_action(voxel::UiAction::fly);assert(!app.flying);
    app.menu_action(voxel::UiAction::auto_jump);assert(!app.settings.auto_jump);app.menu_action(voxel::UiAction::auto_jump);assert(app.settings.auto_jump);
    app.show(voxel::Screen::game);app.on_ground=false;
    pxa::ui::CanvasPointer p;p.phase=0;p.pointer_id=1;p.x=app.controls.actions[2].cx();p.y=app.controls.actions[2].cy();
    app.velocity_y=0;app.on_pointer(p);assert(app.velocity_y==0); // no mid-air jumps
    app.on_ground=true;app.on_pointer(p);assert(app.velocity_y>7);
    p.x=app.controls.menu.cx();p.y=app.controls.menu.cy();app.on_pointer(p);assert(app.screen==pause);
    assert(!app.stick_active&&!app.look_active&&!app.mining);
    app.show(voxel::Screen::game);p.x=app.controls.bag.cx();p.y=app.controls.bag.cy();app.on_pointer(p);assert(app.screen==inventory);
    app.menu_action(voxel::UiAction::quick_slot,2);app.menu_action(voxel::UiAction::item,voxel::kTable);assert(app.selected_block()==voxel::kTable);
    pxa::ui::DisplayMetrics dense;dense.width=dense.height=412;dense.density_q16=131072;
    app.apply_display(dense);app.show(game);
    p.x=app.controls.menu.cx()/2;p.y=app.controls.menu.cy()/2;app.on_pointer(p);assert(app.screen==pause);
    app.apply_display({});
    voxel::SaveState state;state.camera=pose;state.generation=3;state.quick=app.quickbar;state.blocks_hash=voxel::checksum(app.world.saved_blocks());
    auto header=voxel::encode_save(state);auto decoded=voxel::decode_save(header);assert(decoded&&decoded->generation==3&&decoded->quick[2]==voxel::kTable);
    auto outside=state;outside.camera={-2.f,256.f,70.f,0.f,0.f};outside.flying=true;
    auto recovered=voxel::decode_save(voxel::encode_save(outside));assert(recovered&&recovered->camera.y==256.f&&recovered->camera.z==70.f);
    outside.camera.x=2097152.f;assert(!voxel::decode_save(voxel::encode_save(outside)));
    for(std::size_t i=0;i<header.size();++i){auto broken=header;broken[i]^=std::byte{1};assert(!voxel::decode_save(broken));}
    auto staged=app.world.load_staging();std::fill(staged.begin(),staged.end(),std::byte{voxel::kGrass});
    app.world.finish_load(false);assert(app.world.at(10,2,10)==old);
    staged=app.world.load_staging();std::fill(staged.begin(),staged.end(),std::byte{voxel::kAir});staged[10]=std::byte{voxel::kTable};
    assert(voxel::valid_saved_blocks(staged));app.world.finish_load(true);assert(app.world.at(10,0,0)==voxel::kTable);
    const auto& rebuilt=app.world.chunk_mesh(1,0,0);assert(rebuilt.count==6); // caches reset after staging
    pxa::Transport transport;transport.phase(pxa::Phase::event);
    pxa::game::RenderInfo info;info.capabilities=PXA_RASTER_CAP_KNOWN_MASK;info.render_width=296;info.render_height=240;info.max_textures=48;info.max_draw_bytes=49152;
    pxa::game::Renderer renderer(transport,77,info);
    // A HUD is an ordered overlay. Its optimized scanlines must preserve
    // cutouts and palette rows while leaving world depth values untouched.
    // The two fixed-point interpolators can resolve exact texel boundaries
    // differently; the synthetic gradient bounds this to adjacent samples.
    for(unsigned y=0;y<220;++y)for(unsigned x=0;x<220;++x)
        texture[y*220+x]=((x/3+y/5)%3==0)?0:std::uint8_t(1+(x+y)%15);
    for(unsigned i=0;i<palette.size();++i)palette[i]=std::uint16_t(i*127);
    std::fill(pixels.begin(),pixels.end(),0x1234);
    std::fill(depth.begin(),depth.end(),0x4321);
    auto hud_frame=renderer.frame(app.commands);app.draw_hud(hud_frame,296,240);
    assert(hud_frame.submit());
    assert(std::all_of(depth.begin(),depth.end(),[](auto v){return v==0x4321;}));
    static auto scanline_pixels=pixels;
    for(unsigned at=PXA_RASTER_DRAW_HEADER_BYTES;at<last_draw_size;at+=pxa::wire::get16(reinterpret_cast<const std::byte*>(last_draw.data()+at+2)))
        if(last_draw[at]==PXA_RASTER_RECORD_TEXTURED_QUAD)
            last_draw[at+1]=(last_draw[at+1]&~PXA_RASTER_QUAD_PAINTER)|PXA_RASTER_QUAD_LIT_PALETTE;
    std::fill(pixels.begin(),pixels.end(),0x1234);
    pxa_raster_target_t hud_target{};hud_target.pixels=pixels.data();hud_target.depth_pixels=depth.data();
    hud_target.width=hud_target.stride_pixels=hud_target.depth_stride_pixels=296;hud_target.height=240;hud_target.scratch_mode=PXA_RASTER_SCRATCH_DEPTH16;
    pxa_raster_resources_t hud_resources{};hud_resources.capabilities=PXA_RASTER_CAP_KNOWN_MASK;hud_resources.palette=palette.data();hud_resources.palette_light_levels=16;
    for(auto& slot:hud_resources.textures)slot={texture.data(),220,220};
    pxa_raster_draw_list_view_t hud_view{};
    assert(pxa_raster_validate_draw_list(last_draw.data(),last_draw_size,&hud_target,&hud_resources,&hud_view)==PXA_STATUS_OK);
    pxa_raster_execute_draw_list(last_draw.data(),&hud_view,&hud_target,&hud_resources,nullptr);
    unsigned changed=0;
    for(unsigned i=0;i<pixels.size();++i)if(pixels[i]!=scanline_pixels[i]){
        assert(pixels[i]!=0x1234&&scanline_pixels[i]!=0x1234); // Same coverage/cutouts.
        const int difference=std::abs(int(pixels[i])-int(scanline_pixels[i]));
        assert(difference==127||difference==14*127); // Adjacent texels, including wrap.
        ++changed;
    }
    assert(changed<=64); // <0.1% of the viewport; catches broad UV/palette shifts.
    std::printf("HUD: depth untouched; %u adjacent-texel tie differences from legacy\n",changed);
    assert(voxel::select_font_size(1.f)==10&&voxel::select_font_size(1.4f)==14&&voxel::select_font_size(2.f)==18);
    for(auto page:{title,pause,settings_page,load_slots,save_slots,inventory,workbench,confirm_title,confirm_overwrite}){
        app.show(page);app.draw_menu(renderer);assert(!app.ui_dirty);
        assert(pixels[120*296+148]!=0); // cleared visible frame
        assert(app.menu_hit_count<=app.menu_hits.size());
    }
    app.renderer.emplace(transport,77,info);
    auto projection=pxa::game3d::Projector::create(296,240,1.15f,.25f,64.f);
    assert(projection);app.projector=*projection;app.settings.auto_distance=false;
    app.show(game);app.hud_window_us=0;
    const auto before=draw_submissions;
    pending_frame=true;app.on_frame(ctx,{100000,16000,16000,1});
    assert(draw_submissions==before&&app.frames==0);
    pending_frame=false;app.on_frame(ctx,{116000,16000,16000,1});
    assert(draw_submissions==before+1&&app.frames==1);
    pending_frame=true;app.on_frame(ctx,{132000,16000,16000,1});
    assert(draw_submissions==before+1&&app.frames==1);
    pending_frame=false;app.on_frame(ctx,{164000,16000,16000,1});
    assert(app.frames==2&&app.hud_window_us==64000); // Includes the skipped interval.
    app.renderer.reset();
    std::puts("Session: title/pause/navigation, grounded jump, inventory, corrupt headers, transactional mesh staging and all menu commands validated/rasterized OK");
}
