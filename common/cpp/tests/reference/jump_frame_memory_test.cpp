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
std::array<std::uint16_t,480*480> expected_pixels{},actual_pixels{};
void draw(pxa::game::Renderer& renderer,pxa::ui::DisplayMetrics display,
          const j3_game_t& game,bool reuse) {
    j3_render_t render{};
    j3_render_configure(&render,display.width,display.height,renderer.capabilities());
    j3_render_adapt(&render,display);
    const auto bytes=reuse?memory.draw():std::span{separate_draw};
    assert(j3_render_frame(&render,&game,renderer,bytes.data(),bytes.size(),1));
    auto& pixels=reuse?actual_pixels:expected_pixels;
    pixels.fill(0x5a5a);
    pxa_raster_target_t target{};
    target.pixels=pixels.data();target.width=display.width;target.height=display.height;
    target.stride_pixels=display.width;target.scratch_mode=PXA_RASTER_SCRATCH_NONE;
    pxa_raster_draw_list_view_t view{};
    assert(pxa_raster_validate_draw_list(current->draw.data(),current->draw_bytes,
        &target,&current->resources,&view)==PXA_STATUS_OK);
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
extern "C" int32_t pxa_submit(const uint8_t*,uint32_t){return 0;}
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
    pxa::Transport transport;transport.phase(pxa::Phase::event);
    pxa::game::Renderer renderer(transport,77,PXA_RASTER_CAP_KNOWN_MASK);
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
            current=&shared;resources(renderer,scheme,true,scheme==0);
            assert(dedicated.palette==shared.palette && dedicated.textures==shared.textures);
            for(unsigned phase=0;phase<3;++phase) {
                if(phase==1){j3_game_press(&game);for(int i=0;i<36;++i)j3_game_tick(&game,.02f);}
                if(phase==2){j3_game_release(&game);j3_game_tick(&game,.02f);}
                current=&dedicated;draw(renderer,display,game,false);
                current=&shared;draw(renderer,display,game,true);
                assert(dedicated.draw_bytes==shared.draw_bytes);
                assert(std::memcmp(dedicated.draw.data(),shared.draw.data(),shared.draw_bytes)==0);
                assert(expected_pixels==actual_pixels);++comparisons;
            }
            // A DPI/font reload overwrites the palette scratch; the next sky
            // change must reconstruct it, including every light-level row.
            current=&shared;
            auto upload=memory.upload();j3_font_set_tier((tier+1)%3);
            assert(j3_font_upload(renderer,upload.data(),upload.size()));
        }
    }
    std::printf("shared/dedicated scratch resource, command and Host pixel parity passed (%u frames)\n",comparisons);
}
