// Exercise all game phases through the real Host validator and rasterizer.
#include "render.hpp"
#include "font_data.hpp"
#include "strings.hpp"
#include <pxa/raster.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <vector>
static double raster_ns=0;
static unsigned list_bytes=0;
static std::vector<uint8_t> encoded;
extern "C" void pd_c_language(int);
extern "C" size_t pd_c_game_size();
extern "C" size_t pd_c_layout_size();
extern "C" int pd_c_render(const void*,const void*,unsigned char*,unsigned long long,unsigned);
using namespace dungeon;
static pxa_raster_resources_t resources{};static pxa_raster_target_t target{};
static uint16_t pixels[800*800]{};static uint8_t draw[PD_MAX_DRAW_BYTES]{};
static unsigned frames=0;
extern "C" int32_t pxa_submit(const uint8_t* bytes,uint32_t size){
    if(size>32){for(uint32_t i=24;i<size;++i)if(bytes[i]>=32&&bytes[i]<127)std::fputc(bytes[i],stderr);std::fputc('\n',stderr);}return 0;}
extern "C" int32_t pxa_io(uint64_t,uint32_t opcode,uint8_t* bytes,uint32_t size){
    assert(opcode==0x101);pxa_raster_draw_list_view_t view{};
    auto status=pxa_raster_validate_draw_list(bytes,size,&target,&resources,&view);
    if(status!=PXA_STATUS_OK){std::fprintf(stderr,"Host rejected %d bytes=%u commands=%u\n",status,size,pxa::wire::get32(reinterpret_cast<std::byte*>(bytes)+16));
        for(uint32_t at=32;at<size;){auto length=pxa::wire::get16(reinterpret_cast<std::byte*>(bytes)+at+2);
            uint8_t one[PD_MAX_DRAW_BYTES];std::memcpy(one,bytes,32);std::memcpy(one+32,bytes+at,length);
            pxa::wire::put32(reinterpret_cast<std::byte*>(one)+8,32+length);pxa::wire::put32(reinterpret_cast<std::byte*>(one)+16,1);
            auto issue=pxa_raster_validate_draw_list(one,32+length,&target,&resources,&view);
            if(issue){std::fprintf(stderr,"bad cmd=%u flags=%u offset=%u slot=%u status=%d len=%u\n",bytes[at],bytes[at+1],at,bytes[at+4],issue,length);
                for(uint32_t j=at;j<at+length;++j)std::fprintf(stderr,"%02x%s",bytes[j],(j-at)%16==15?"\n":" ");std::fputc('\n',stderr);}
            at+=length;
        }
        return status;}
    encoded.assign(bytes+32,bytes+size);auto started=std::chrono::steady_clock::now();pxa_raster_execute_draw_list(bytes,&view,&target,&resources,nullptr);raster_ns+=std::chrono::duration<double,std::nano>(std::chrono::steady_clock::now()-started).count();list_bytes=size;++frames;return size;
}
int main(){
    resources.capabilities=PXA_RASTER_CAP_KNOWN_MASK;resources.palette=pd_palette;resources.palette_light_levels=1;
    resources.textures[0]={pd_tile_atlas,256,256};resources.textures[1]={pd_sprite_atlas,256,256};
    resources.textures[2]={pd_font_ascii,128,126};resources.textures[3]={pd_font_cjk,256,256};
    resources.textures[5]={pd_ui_atlas,256,192};resources.textures[6]={pd_title_atlas,256,128};
    resources.textures[7]={pd_wall_atlas,256,256};resources.textures[9]={pd_fire_atlas,PD_FIRE_ATLAS_WIDTH,PD_FIRE_ATLAS_HEIGHT};
    resources.textures[10]={pd_font_cjk_extra,256,48};
    uint8_t fog[256],search[1024];std::memset(fog,255,sizeof(fog));std::memset(search,254,sizeof(search));
    resources.textures[4]={fog,16,16};resources.textures[8]={search,64,16};
    pxa::Transport transport;transport.phase(pxa::Phase::event);arcade::logging_transport=&transport;
    pxa::game::Renderer renderer(transport,77,resources.capabilities);
    target.pixels=pixels;target.stride_pixels=800;target.scratch_mode=PXA_RASTER_SCRATCH_NONE;
    assert(pd_c_game_size()==sizeof(pd_game_t)&&pd_c_layout_size()==sizeof(pd_layout_t));
    for(int language:{0,1})for(int phase:{PD_PHASE_TITLE,PD_PHASE_PLAY,PD_PHASE_BAG}){
        pd_c_language(language);pd_strings_set_language(language);pd_game_t game{};pd_layout_t layout{};
        pd_game_reset(&game,0x51ed270b);pd_game_start_run(&game,0);game.phase=phase;
        pd_layout_build(&layout,296,240,8,10,8,10);target.width=296;target.height=240;
        std::vector<uint16_t> first;std::vector<uint8_t> command_first;
        for(int cpp:{0,1}){
            double elapsed=0;raster_ns=0;unsigned bytes=0;
            for(int rep=0;rep<200;++rep){
                auto start=std::chrono::steady_clock::now();
                assert(cpp?pd_render_present(renderer,resources.capabilities,&game,&layout,draw,sizeof(draw),frames+1,0):pd_c_render(&game,&layout,draw,frames+1,0));
                elapsed+=std::chrono::duration<double,std::nano>(std::chrono::steady_clock::now()-start).count();bytes+=list_bytes;
            }
            std::vector<uint16_t> result(pixels,pixels+800*240);
            if(!cpp){first=result;command_first=encoded;}else {assert(result==first);assert(encoded==command_first);}
            std::printf("{\"language\":%d,\"phase\":%d,\"cpp\":%d,\"total_us\":%.3f,\"raster_us\":%.3f,\"guest_validation_us\":%.3f,\"draw_bytes\":%u}\n",language,phase,cpp,elapsed/200000,raster_ns/200000,(elapsed-raster_ns)/200000,bytes/200);
        }
    }
}
