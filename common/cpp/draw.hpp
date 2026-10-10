#pragma once
#include <pxa/game.hpp>
#include <pxa/game_upload.hpp>
#include <pxa/log.hpp>
#include <cstring>
#include <cstdio>

// Shared bounded drawing helpers for the migrated games. All encoding,
// validation and Host calls belong to the independent C++ SDK.
namespace arcade {
inline pxa::Transport* logging_transport=nullptr;
inline int log(unsigned level,const char* message) {
    if(!logging_transport)return 0;
    auto result=pxa::LogService(*logging_transport).write(static_cast<pxa::LogLevel>(level),message);
    return result?0:static_cast<int>(result.error());
}
enum : unsigned {quad_solid_color=1,quad_affine_uv=2,quad_painter=4,
    quad_transparent_index0=8,quad_lit_palette=16,quad_blend_75=32,
    sprite_transparent_index0=1,sprite_solid_color=2,sprite_additive=4,
    sprite_palette_ramp=8,sprite_texel_alpha=16};
struct DrawList {
    std::optional<pxa::game::Frame> frame;
    void begin(pxa::game::Renderer& renderer,uint8_t* bytes,uint32_t capacity,uint64_t id) {
        frame.emplace(renderer,std::span{reinterpret_cast<std::byte*>(bytes),capacity},id);
    }
};
inline int submit(pxa::game::Renderer&,DrawList* list) {
    auto result=list->frame->submit();
    if(!result&&result.error()!=pxa::Error::would_block){
        char line[96];std::snprintf(line,sizeof(line),"C++ draw rejected error=%d bytes=%u",int(result.error()),unsigned(list->frame->bytes_used()));
        log(4,line);
    }
    return result?static_cast<int>(list->frame->bytes_used()):static_cast<int>(result.error());
}
inline void clear(DrawList* list,uint16_t color){list->frame->clear({color});}
inline void flat_quad(DrawList* list,const int16_t* xy,uint16_t color) {
    std::array<int16_t,8> points;std::copy_n(xy,8,points.begin());list->frame->quad(points,{color});
}
inline pxa::game::PolygonOptions polygon_options(unsigned flags) {
    return {.affine_uv=bool(flags&2),.painter=bool(flags&4),
        .transparent_index0=bool(flags&8),.lit_palette=bool(flags&16),.blend_75=bool(flags&32)};
}
inline void triangle_batch_flags(DrawList* list,const pxa::game::Vertex* vertices,
                                 uint16_t count,uint8_t slot,uint8_t flags,uint16_t color) {
    if(flags&quad_solid_color)list->frame->palette_triangles({vertices,size_t(count)*3},uint8_t(color));
    else list->frame->triangles({slot},{vertices,size_t(count)*3},polygon_options(flags));
}
inline void textured_quad_flags(DrawList* list,const pxa::game::Vertex* vertices,uint8_t slot,unsigned flags) {
    list->frame->textured_quad({slot},{vertices[0],vertices[1],vertices[2],vertices[3]},polygon_options(flags));
}
inline void sprite_batch(DrawList* list,uint8_t slot,uint8_t flags,uint32_t,
                         const pxa::game::Sprite* sprites,uint16_t count,uint16_t color) {
    list->frame->sprites({slot},{sprites,count},{.transparent_index0=bool(flags&1),
       .solid_color=bool(flags&2),.additive=bool(flags&4),.palette_ramp=bool(flags&8),
       .texel_alpha=bool(flags&16),.color={color}});
}
inline void sprite(DrawList* list,uint8_t slot,uint8_t flags,uint32_t capabilities,int16_t x,int16_t y,
    uint16_t width,uint16_t height,uint16_t sx,uint16_t sy,uint16_t sw,uint16_t sh,uint16_t color) {
    const pxa::game::Sprite item{x,y,width,height,sx,sy,sw,sh};sprite_batch(list,slot,flags,capabilities,&item,1,color);
}
inline int upload_texture_index8(pxa::game::Renderer& renderer,uint8_t slot,uint16_t width,
    uint16_t height,const uint8_t* pixels,uint8_t* scratch,uint32_t capacity) {
    auto result=pxa::game::Upload(renderer,{reinterpret_cast<std::byte*>(scratch),capacity})
        .texture({slot},width,height,{reinterpret_cast<const std::byte*>(pixels),size_t(width)*height});
    return result?int(20+size_t(width)*height):static_cast<int>(result.error());
}
inline int upload_lit_palette_rgb565(pxa::game::Renderer& renderer,uint16_t levels,const uint16_t* colors,
    uint8_t* scratch,uint32_t capacity) {
    auto result=pxa::game::Upload(renderer,{reinterpret_cast<std::byte*>(scratch),capacity}).palette({colors,size_t(levels)*256},levels);
    return result?int(20+size_t(levels)*512):static_cast<int>(result.error());
}
inline int upload_palette_rgb565(pxa::game::Renderer& renderer,const uint16_t* colors,uint8_t* scratch,uint32_t capacity) {
    return upload_lit_palette_rgb565(renderer,1,colors,scratch,capacity);
}
} // namespace arcade
