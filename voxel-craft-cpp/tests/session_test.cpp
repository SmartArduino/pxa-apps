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
    assert(app.inventory.count(voxel::kTable)==0);
    app.inventory.slots[9]={voxel::kTable,3};
    app.menu_action(voxel::UiAction::item,9);app.menu_action(voxel::UiAction::item,2);
    assert(app.selected_block()==voxel::kTable&&app.inventory.slots[2].count==3&&!app.inventory.slots[9].count);
    app.inventory.slots[9]={voxel::kWood,2};
    app.menu_action(voxel::UiAction::item,9);app.menu_action(voxel::UiAction::craft_cell,4);
    assert(app.crafting.result(app.inventory,2)==voxel::ItemStack(voxel::kPlank,4));
    app.menu_action(voxel::UiAction::item,2); // switching ingredient must not transfer reserved stacks
    assert(app.inventory.slots[9]==voxel::ItemStack(voxel::kWood,2)&&app.crafting.sources[4]==9);
    app.menu_action(voxel::UiAction::back);
    assert(app.inventory.count(voxel::kWood)==2&&app.crafting.sources[4]==-1); // close never loses materials
    pxa::ui::DisplayMetrics dense;dense.width=dense.height=412;dense.density_q16=131072;
    app.apply_display(dense);app.show(game);
    p.x=app.controls.menu.cx()/2;p.y=app.controls.menu.cy()/2;app.on_pointer(p);assert(app.screen==pause);
    app.apply_display({});
    voxel::SaveState state;state.camera=pose;state.generation=3;state.inventory=app.inventory;state.blocks_hash=voxel::checksum(app.world.saved_blocks());
    auto header=voxel::encode_save(state);auto decoded=voxel::decode_save(header);assert(decoded&&decoded->generation==3&&decoded->quick[2]==voxel::kTable);
    auto saved_items=std::as_bytes(std::span{state.inventory.slots});
    assert(voxel::decode_inventory(*decoded,saved_items)&&decoded->inventory==app.inventory);
    auto corrupt_items=state.inventory;corrupt_items.slots[2].count^=1;
    assert(!voxel::decode_inventory(*decoded,std::as_bytes(std::span{corrupt_items.slots})));
    // v2 two-byte stacks migrate without changing quantities or inventing wear.
    std::array<std::byte,voxel::kInventorySlots*2> v2items{};
    for(int i=0;i<voxel::kInventorySlots;++i){v2items[i*2]=std::byte(state.inventory.slots[i].block);v2items[i*2+1]=std::byte(state.inventory.slots[i].count);}
    auto v2header=header;pxa::wire::put16(v2header.data()+4,2);pxa::wire::put16(v2header.data()+66,v2items.size());
    pxa::wire::put32(v2header.data()+62,voxel::checksum(v2items));pxa::wire::put32(v2header.data()+76,voxel::checksum(std::span{v2header}.first(76)));
    auto v2=voxel::decode_save(v2header);assert(v2&&voxel::decode_inventory(*v2,v2items)&&v2->inventory==state.inventory);
    auto tools=state;tools.inventory.slots[0]={voxel::kStonePickaxe,1,93};
    auto saved_tool=voxel::decode_save(voxel::encode_save(tools));assert(saved_tool&&voxel::decode_inventory(*saved_tool,std::as_bytes(std::span{tools.inventory.slots}))&&saved_tool->inventory.slots[0].wear==93);
    auto legacy=header;pxa::wire::put16(legacy.data()+4,1);
    for(int i=0;i<voxel::kHeaderQuickCount;++i)legacy[52+i]=std::byte(voxel::kDirt);
    pxa::wire::put32(legacy.data()+76,voxel::checksum(std::span{legacy}.first(76)));
    auto migrated=voxel::decode_save(legacy);assert(migrated&&migrated->legacy&&migrated->inventory.count(voxel::kDirt)==8);
    auto outside=state;outside.camera={-2.f,256.f,70.f,0.f,0.f};outside.flying=true;
    auto recovered=voxel::decode_save(voxel::encode_save(outside));assert(recovered&&recovered->camera.y==256.f&&recovered->camera.z==70.f);
    outside.camera.x=2097152.f;assert(!voxel::decode_save(voxel::encode_save(outside)));
    for(std::size_t i=0;i<header.size();++i){auto broken=header;broken[i]^=std::byte{1};assert(!voxel::decode_save(broken));}
    auto staged=app.world.load_staging();std::fill(staged.begin(),staged.end(),std::byte{voxel::kGrass});
    app.world.finish_load(false);assert(app.world.at(10,2,10)==old);
    staged=app.world.load_staging();std::fill(staged.begin(),staged.end(),std::byte{voxel::kAir});staged[10]=std::byte{voxel::kTable};
    assert(voxel::valid_saved_blocks(staged));app.world.finish_load(true);assert(app.world.at(10,0,0)==voxel::kTable);
    const auto& rebuilt=app.world.chunk_mesh(10/voxel::kChunkSize,0,0);assert(rebuilt.count==6); // caches reset after staging
    pxa::Transport transport;transport.phase(pxa::Phase::event);
    pxa::game::RenderInfo info;info.capabilities=PXA_RASTER_CAP_KNOWN_MASK;info.render_width=296;info.render_height=240;info.max_textures=48;info.max_draw_bytes=49152;
    pxa::game::Renderer renderer(transport,77,info);
    // A HUD is an ordered overlay. Its optimized scanlines must preserve
    // cutouts and palette rows while leaving world depth values untouched.
    // The two fixed-point interpolators can resolve exact texel boundaries
    // differently; the synthetic gradient bounds this to adjacent samples.
    for(int i=0;i<voxel::kQuickCount;++i)app.inventory.slots[i]={voxel::kDirt,64};
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
        if(page==inventory||page==workbench)for(unsigned i=0;i<app.menu_hit_count;++i){
            voxel::Rect r=app.menu_hits[i].rect;auto [l,right]=app.controls.horizontal(r.y,r.h);
            assert(r.x>=l&&r.x+r.w<=right&&r.y>=int(app.display.safe.top)&&r.y+r.h<=int(app.display.height-app.display.safe.bottom));
        }
    }
    std::printf("Session storage %zu B; inventory %zu B; crafting %zu B; hit table %zu B\n",sizeof(VoxelCraft),sizeof(app.inventory),sizeof(app.crafting),sizeof(app.menu_hits));
    for(auto& stack:app.inventory.slots)stack={voxel::kStonePickaxe,1,132};
    for(auto page:{inventory,workbench}){app.show(page);app.draw_menu(renderer);assert(last_draw_size<PXA_RASTER_MAX_DRAW_BYTES);}
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
    // Mining cracks share world depth. A nearer wall blocks them completely,
    // and cutout targets keep their holes without an opaque crack decal.
    app.inventory={};
    auto empty_world=app.world.load_staging();std::fill(empty_world.begin(),empty_world.end(),std::byte{});app.world.finish_load(true);
    app.camera={20.5f,1.f,20.5f,0,0};app.world.set(20,2,22,voxel::kStone);
    auto eye=app.camera;eye.y+=kEyeHeight;
    app.mining=true;const auto hit=app.target();assert(hit.hit);
    assert(!app.digging.advance(hit,voxel::kStone,1.6f));
    auto occluded=renderer.frame(app.commands);occluded.clear({0x1234});
    std::array<pxa::game::Vertex,4> foreground{{
        {.x_q4=0,.y_q4=0,.depth_q8=128},{.x_q4=296*16,.y_q4=0,.depth_q8=128},
        {.x_q4=296*16,.y_q4=240*16,.depth_q8=128},{.x_q4=0,.y_q4=240*16,.depth_q8=128}}};
    occluded.solid_depth_quad(foreground,{0x1234});app.draw_mining_cracks(occluded,eye);assert(occluded.submit());
    assert(std::all_of(pixels.begin(),pixels.end(),[](auto pixel){return pixel==0x1234;}));
    auto visible=renderer.frame(app.commands);visible.clear({0x1234});app.draw_mining_cracks(visible,eye);assert(visible.submit());
    assert(std::any_of(pixels.begin(),pixels.end(),[](auto pixel){return pixel==0x2104;}));
    app.world.set(20,2,22,voxel::kAir);app.world.set(20,2,25,voxel::kStone);app.digging.reset();
    const auto far_hit=app.target();assert(far_hit.hit);
    assert(!app.digging.advance(far_hit,voxel::kStone,1.6f));
    auto far_cracks=renderer.frame(app.commands);far_cracks.clear({0x1234});app.draw_mining_cracks(far_cracks,eye);assert(far_cracks.submit());
    assert(std::count(pixels.begin(),pixels.end(),0x2104)>=15);
    // A decal must survive the real textured block's quantized Z, not only
    // draw against cleared depth. This caught invisible far-range cracks.
    auto on_block=renderer.frame(app.commands);on_block.clear({0x1234});
    voxel::DrawBudget block_budget;block_budget.max_distance=32;
    const auto drawn=voxel::draw_world(on_block,*app.projector,app.world,eye,block_budget);
    assert(drawn.faces_drawn);app.draw_mining_cracks(on_block,eye);assert(on_block.submit());
    assert(std::count(pixels.begin(),pixels.end(),0x2104)>=15);
    app.world.set(20,2,22,voxel::kLeaves);app.digging.reset();assert(!app.digging.advance(hit,voxel::kLeaves,.2f));
    auto transparent=renderer.frame(app.commands);app.draw_mining_cracks(transparent,eye);assert(transparent.bytes_used()==PXA_RASTER_DRAW_HEADER_BYTES);
    std::puts("Session: title/pause/navigation, grounded jump, inventory, corrupt headers, transactional mesh staging and all menu commands validated/rasterized OK");
}
