#include "pxa_control_capture.h"
#include "frame_memory.hpp"
#include "jump3d_render.hpp"
#include <pxa/game_upload.hpp>
#include <pxa/raster.h>
#include <array>
#include <cassert>
#include <cstring>
#include <cstdio>

namespace {
using namespace jump;
struct Captured {
    std::array<std::uint16_t,J3_PALETTE_ENTRIES> palette{};
    std::array<std::array<std::uint8_t,J3_FONT_MAX_ATLAS_BYTES>,13> textures{};
    pxa_raster_resources_t resources{};
    std::array<std::uint8_t,FrameMemory::draw_capacity> draw{};
    std::uint32_t draw_bytes=0;
};
// Test-only Host copies. No corresponding storage is added to the application.
Captured dedicated,shared;
Captured* current=nullptr;
FrameMemory memory;
std::array<std::uint8_t,FrameMemory::upload_capacity> separate_upload{};
std::array<std::uint8_t,FrameMemory::draw_capacity> separate_draw{};
std::array<std::uint16_t,J3_PALETTE_ENTRIES> separate_palette{};
std::array<std::uint16_t,800*480> expected_pixels{},actual_pixels{};
void draw(pxa::game::Renderer& renderer,pxa::ui::DisplayMetrics display,
          const j3_game_t& game,bool reuse) {
    j3_render_t render{};
    j3_render_configure(&render,display.width,display.height,renderer.capabilities());
    j3_render_adapt(&render,display);
    render.clock_minutes=8*60+8; // Most illuminated segments in a valid HH:MM.
    const auto bytes=reuse?memory.draw():std::span{separate_draw};
    const bool accepted=j3_render_frame(&render,&game,renderer,bytes.data(),bytes.size());
    if(!accepted)std::fprintf(stderr,"effect overflow: kinds=%u,%u,%u state=%u age=%f bytes=%zu dropped=%u\n",
        game.blocks[0].kind,game.blocks[1].kind,game.blocks[2].kind,
        game.state,game.special_age,render.list.frame->bytes_used(),render.dropped);
    assert(accepted);
    auto& pixels=reuse?actual_pixels:expected_pixels;
    pixels.fill(0x5a5a);
    pxa_raster_target_t target{};
    target.pixels=pixels.data();target.width=display.width;target.height=display.height;
    target.stride_pixels=display.width;target.scratch_mode=PXA_RASTER_SCRATCH_NONE;
    pxa_raster_draw_list_view_t view{};
    const auto status=pxa_raster_validate_draw_list(current->draw.data(),current->draw_bytes,
        &target,&current->resources,&view);
    if(status!=PXA_STATUS_OK)std::fprintf(stderr,"invalid effect frame: kind=%u state=%u age=%f status=%d bytes=%u\n",game.blocks[0].kind,game.state,game.special_age,status,current->draw_bytes);
    assert(status==PXA_STATUS_OK);
    assert(!view.uses_depth);
    pxa_raster_execute_draw_list(current->draw.data(),&view,&target,&current->resources,nullptr);
}
void resources(pxa::game::Renderer& renderer,std::uint8_t scheme,bool reuse,bool first) {
    auto upload=reuse?memory.upload():std::span{separate_upload};
    auto palette=reuse?memory.palette():std::span{separate_palette};
    j3_palette_build(palette.data());
    if(first)assert(j3_render_upload_resources(renderer,upload.data(),upload.size(),palette.data(),scheme));
    else assert(j3_render_upload_sky(renderer,palette.data(),scheme,upload.data(),upload.size()));
    assert(pxa::game::Upload(renderer,std::as_writable_bytes(upload)).palette(palette,J3_LIGHT_LEVELS));
    assert(j3_font_upload(renderer,upload.data(),upload.size()));
}
}
extern "C" int32_t pxa_test_control(const uint8_t*,uint32_t){return 0;}
extern "C" int32_t pxa_io(uint64_t handle,uint32_t operation,uint8_t* data,uint32_t size) {
    assert(handle==77 && current);
    if(operation==0x101) {
        assert(size<=current->draw.size());
        std::memcpy(current->draw.data(),data,size);current->draw_bytes=size;
    } else {
        assert(operation==0x100);
        pxa_raster_upload_view_t upload{};
        assert(pxa_raster_decode_upload(data,size,&upload)==PXA_STATUS_OK);
        if(upload.kind==PXA_RASTER_UPLOAD_LIT_PALETTE_RGB565) {
            assert(upload.payload_bytes==sizeof(current->palette));
            std::memcpy(current->palette.data(),upload.payload,upload.payload_bytes);
            current->resources.palette=current->palette.data();
            current->resources.palette_light_levels=upload.height;
        } else {
            assert(upload.kind==PXA_RASTER_UPLOAD_TEXTURE_INDEX8 && upload.slot<13);
            assert(upload.payload_bytes<=J3_FONT_MAX_ATLAS_BYTES);
            auto& texture=current->textures[upload.slot];
            texture.fill(0);
            std::memcpy(texture.data(),upload.payload,upload.payload_bytes);
            current->resources.textures[upload.slot]={texture.data(),upload.width,upload.height};
        }
    }
    return size;
}
int main() {
    static_assert(sizeof(j3_game_t)==324);
    static_assert(sizeof(j3_render_t)==9520);
    static_assert(sizeof(FrameMemory)==32788);
    pxa::Transport transport;transport.phase(pxa::Phase::event);
    pxa::game::Renderer renderer(transport,77,PXA_RASTER_CAP_KNOWN_MASK);
    pxa::game::Renderer shared_renderer(transport,77,PXA_RASTER_CAP_KNOWN_MASK);
    dedicated.resources.capabilities=shared.resources.capabilities=renderer.capabilities();
    unsigned comparisons=0;
    for(unsigned tier=0;tier<3;++tier) {
        pxa::ui::DisplayMetrics display;
        display.width=display.height=176+152*tier;display.density_q16=32768u<<tier;
        for(unsigned scheme=0;scheme<J3_BG_SCHEMES;++scheme) {
            j3_game_t game{};j3_game_reset(&game,0x51ed2701);
            game.jump_count=15*scheme;
            j3_font_set_tier(tier);
            current=&dedicated;resources(renderer,scheme,false,scheme==0);
            current=&shared;resources(shared_renderer,scheme,true,scheme==0);
            assert(dedicated.palette==shared.palette && dedicated.textures==shared.textures);
            for(unsigned phase=0;phase<3;++phase) {
                if(phase==1){j3_game_press(&game);for(int i=0;i<36;++i)j3_game_tick(&game,.02f);}
                if(phase==2){j3_game_release(&game);j3_game_tick(&game,.02f);}
                current=&dedicated;draw(renderer,display,game,false);
                current=&shared;draw(shared_renderer,display,game,true);
                assert(dedicated.draw_bytes==shared.draw_bytes);
                assert(std::memcmp(dedicated.draw.data(),shared.draw.data(),shared.draw_bytes)==0);
                assert(expected_pixels==actual_pixels);++comparisons;
            }
            // A DPI/font reload overwrites the palette scratch; the next sky
            // change must reconstruct it, including every light-level row.
            current=&shared;
            auto upload=memory.upload();j3_font_set_tier((tier+1)%3);
            assert(j3_font_upload(shared_renderer,upload.data(),upload.size()));
        }
    }
    unsigned maximum_bytes=0;
    pxa::ui::DisplayMetrics display;display.width=display.height=480;
    display.density_q16=2u<<16;
    j3_font_set_tier(2);
    current=&shared;resources(shared_renderer,0,true,true);
    unsigned effect_frames=0;
    unsigned boundary_pixels=0;
    for(unsigned kind=J3_KIND_BOX;kind<J3_KIND_COUNT;++kind){
        for(float age:{0.0F,0.05F,0.15F,0.32F,0.4F,0.6F,1.0F,3.0F}){
            j3_game_t game{};j3_game_reset(&game,kind);
            game.blocks[0].kind=kind;game.blocks[0].bonus_done=1;game.special_age=age;
            for(unsigned i=0;i<J3_WAVE_MAX;++i){auto& wave=game.waves[i];
                wave.active=1;wave.power=i+1;wave.t=.1F;wave.x=game.px;wave.z=game.pz;}
            for(unsigned phase=0;phase<3;++phase){
                if(phase==1){j3_game_press(&game);j3_game_charge(&game,age);}
                if(phase==2){j3_game_release(&game);j3_game_tick(&game,.14F);}
                // Decorations may occupy all three slots. Stay-bonus blocks
                // are spaced at least five jumps apart, so only one can occur
                // in the three-slot scene. Test the reachable worst cases.
                if(kind>=J3_KIND_CLOCK)
                    for(auto& block:game.blocks) block.kind=kind;
                game.block_count=3;
                draw(shared_renderer,display,game,true);++effect_frames;
                maximum_bytes=std::max(maximum_bytes,shared.draw_bytes);
            }
        }
    }
    // Mixed catalog entries, maximum feedback and full uint32 HUD counters.
    // Avoid assuming that the most expensive single kind is also the worst mix.
    unsigned mixed_frames=0;
    display.width=800; // Korvo needs more background commands than 480 x 480.
    for(unsigned a=J3_KIND_BOX;a<J3_KIND_COUNT;++a)
    for(unsigned b=J3_KIND_BOX;b<J3_KIND_COUNT;++b)
    for(unsigned c=J3_KIND_BOX;c<J3_KIND_COUNT;++c){
        j3_game_t game{};j3_game_reset(&game,123);
        game.blocks[0].kind=a;game.blocks[1].kind=b;game.blocks[2].kind=c;
        const unsigned bonus_blocks=!!j3_block_bonus(&game.blocks[0])+
            !!j3_block_bonus(&game.blocks[1])+!!j3_block_bonus(&game.blocks[2]);
        if(bonus_blocks>1)continue;
        game.block_count=3;game.score=game.best=UINT32_MAX;
        game.popup_timer=1;game.popup_points=32;game.popup_kind=3;
        game.bonus_popup_timer=1;game.bonus_points=30;game.bonus_kind=J3_KIND_MUSIC;
        for(auto& block:game.blocks){block.appear=1;block.center_dot=1;}
        for(auto& wave:game.waves){wave.active=1;wave.t=.10F;wave.x=game.px;wave.z=game.pz;}
        j3_game_press(&game);j3_game_charge(&game,1.5F);
        draw(shared_renderer,display,game,true);++mixed_frames;
        maximum_bytes=std::max(maximum_bytes,shared.draw_bytes);
    }
    std::printf("effects budget: %u frames accepted, maximum=%u / %zu bytes, no depth buffer\n",effect_frames,maximum_bytes,FrameMemory::draw_capacity);
    std::printf("mixed catalog budget: %u frames accepted with ten-digit HUD and all feedback\n",mixed_frames);
    pxa::game::Renderer fallback(transport,77,PXA_RASTER_CAP_KNOWN_MASK &
        ~pxa::game::capability_bit(pxa::game::RenderCapability::flat_quad));
    current=&dedicated;resources(fallback,0,false,true);
    display.width=480;
    for(unsigned kind=J3_KIND_BOX;kind<J3_KIND_COUNT;++kind){
        j3_game_t game{};j3_game_reset(&game,123);
        for(auto& block:game.blocks)block.kind=kind;
        game.time=1.25F;
        current=&dedicated;draw(fallback,display,game,false);
        current=&shared;draw(shared_renderer,display,game,true);
        unsigned differences=0;
        for(unsigned i=0;i<expected_pixels.size();++i)if(expected_pixels[i]!=actual_pixels[i]){
            // The two Host scan converters round polygon boundaries differently.
            // Permit one-pixel coverage changes, never a different interior colour.
            bool expected_nearby=false,actual_nearby=false;
            const int x=i%480,y=i/480;
            for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){
                const int nx=x+dx,ny=y+dy;
                if(nx<0||nx>=480||ny<0||ny>=480)continue;
                expected_nearby|=actual_pixels[ny*480+nx]==expected_pixels[i];
                actual_nearby|=expected_pixels[ny*480+nx]==actual_pixels[i];
            }
            if(!expected_nearby||!actual_nearby)
                std::fprintf(stderr,"interior mismatch: kind=%u x=%d y=%d\n",kind,x,y);
            assert(expected_nearby&&actual_nearby);++differences;
        }
        assert(differences<480*480/100);boundary_pixels+=differences;
    }
    std::printf("flat/fallback interior colours and coverage passed for %u kinds (%u boundary pixels differ)\n",J3_KIND_COUNT-1,boundary_pixels);
    std::printf("shared/dedicated scratch resource, command and Host pixel parity passed (%u frames)\n",comparisons);
}
