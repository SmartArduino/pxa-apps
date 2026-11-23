#pragma once

#include "draw.hpp"

namespace tide {

namespace chart_style {

inline constexpr Color Uncharted=rgb(0x293c3c);
inline constexpr int TileSize=32;
inline constexpr std::array<unsigned,16> Land{
    0x536947,0x5a7049,0x61744c,0x68784d,0x707b4d,0x788050,0x838451,0x8d8952,
    0x968e55,0x9e925a,0xa79863,0xb1a16f,0xb8aa79,0xc0b286,0xc8bc94,0xd3c8a7
};

constexpr Color tint(unsigned color,int offset) {
    const auto channel=[&](int shift) { return unsigned(std::clamp(int((color>>shift)&255)+offset,0,255)); };
    return rgb(channel(16)<<16 | channel(8)<<8 | channel(0));
}

inline constexpr auto Palette=[] {
    std::array<std::array<Color,16>,19> table{};
    for (std::size_t i=0;i<table.size();++i) {
        const unsigned base=i==0?0x123d49:i==1?0x20515b:i==2?0xa2976b:Land[i-3];
        for (int shade=0;shade<16;++shade)
            table[i][shade]=tint(base,(shade-8)*(i==0?1:2));
    }
    return table;
}();

inline int texture(int material,int u,int v) {
    const int offset=material*TileSize*TileSize + (v&(TileSize-1))*TileSize + (u&(TileSize-1));
    return (art::chart_materials[offset/2]>>(4*(offset&1)))&15;
}

// Reconstruct the contour between centres of the existing navigation cells.
struct Coast {
    int land, shelf;
};

inline Coast coast(const Image& source,int sx,int sy,int u,int v,unsigned terrain) {
    const int left=std::max(0,sx-int(u<128)), right=std::min(ChartWidth-1,sx+int(u>=128));
    const int top=std::max(0,sy-int(v<128)), bottom=std::min(ChartHeight-1,sy+int(v>=128));
    const int fx=(u+128)&255, fy=(v+128)&255;
    const int weights[]{(256-fx)*(256-fy),fx*(256-fy),(256-fx)*fy,fx*fy};
    const unsigned corners[]{source.index(left,top),source.index(right,top),
                             source.index(left,bottom),source.index(right,bottom)};
    int land=0, shelf=0;
    for (int i=0;i<4;++i) {
        if (corners[i]>=2) land+=weights[i];
        if (corners[i]>=1) shelf+=weights[i];
    }
    // Retain islands and individual water pixels in narrow passages such as Gibraltar.
    if (terrain>=2) land=std::max(land,10240);
    else land=std::min(land,55296);
    return {land>>8,shelf>>8};
}

inline Color surface(const Image& atlas,int u,int v,bool close) {
    const int x=u>>8, y=v>>8;
    const auto terrain=atlas.index(x,y);
    const int u_tex=u>>5, v_tex=v>>5;
    const auto edge=close && (terrain==1 || terrain==3)?coast(atlas,x,y,u&255,v&255,terrain):
                         Coast{terrain>=2?256:0,terrain?256:0};
    const auto shade=[&](int material) {
        const int pattern=texture(material,u_tex,v_tex);
        return close?pattern:8+(pattern-8)/2;
    };
    if (edge.land<118) {
        const auto deep=Palette[0][shade(0)];
        if (edge.shelf<20) return deep;
        const auto shelf=Palette[1][shade(1)];
        if (edge.land>=72) return Draw::mix(shelf,rgb(0x658f7a),(edge.land-72)/4);
        return edge.shelf>=246?shelf:Draw::mix(deep,shelf,edge.shelf>>4);
    }
    const int height=Draw::relief_level(x,y);
    const auto land=Palette[height+3][shade(height>=10?3:2)];
    const auto beach=Palette[2][shade(1)];
    if (edge.land<=143) return Draw::mix(Palette[1][shade(1)],beach,(edge.land-118)/2);
    if (edge.land<=171) return Draw::mix(beach,land,(edge.land-143)/2);
    return land;
}

} // namespace chart_style

inline void draw_helm_chart(Draw& out,Rect destination) {
    destination=out.pixel_rect(destination);
    auto d=out.native_pixels();
    if (destination.w<=0 || destination.h<=0) return;
    const auto visible=d.clipped(destination);
    if (!visible.w || !visible.h) return;
    const bool close=destination.w>ChartWidth;
    constexpr Image atlas{art::chart};
    // 8-bit fractional atlas coordinates, accumulated without a division per pixel.
    constexpr int width=ChartWidth*256, height=ChartHeight*256;
    const auto start=std::int64_t(visible.x-destination.x)*width;
    const auto first_row=std::int64_t(visible.y-destination.y)*height;
    const int x_step=width/destination.w, remainder=width%destination.w;
    const int y_step=height/destination.h, y_remainder=height%destination.h;
    const int initial=int(start/destination.w), phase=int(start%destination.w);
    int v=int(first_row/destination.h), y_error=int(first_row%destination.h);
    for (int y=visible.y;y<visible.bottom();++y) {
        auto* row=d.row(y);
        int u=initial, error=phase;
        for (int x=visible.x;x<visible.right();++x) {
            row[x]=chart_style::surface(atlas,u,v,close);
            u+=x_step;
            error+=remainder;
            if (error>=destination.w) { error-=destination.w; ++u; }
        }
        v+=y_step;
        y_error+=y_remainder;
        if (y_error>=destination.h) { y_error-=destination.h; ++v; }
    }
}

inline void draw_chart_background(Draw& d,Rect world,const Game& game,bool helm,bool reveal_all,
                                  bool frame) {
    constexpr Image atlas{art::chart};
    constexpr auto sea=rgb(0x173d46), grid=rgb(0x374b49);
    d.fill(d.clip,sea);
    const auto outer=d.clip;
    const auto visible=d.clipped(world);
    if (!visible.w || !visible.h) return;
    if (reveal_all) {
        d.blit(atlas,{0,0,ChartWidth,ChartHeight},world,false,false);
        return;
    }
    d.fill(visible,chart_style::Uncharted);
    const auto project=[](int cell,int count,int size) {
        return int((std::int64_t(std::min(cell*ChartCell,count))*size+count/2)/count);
    };
    const auto column=[&](int x) { return world.x+project(x,ChartWidth,world.w); };
    const auto row=[&](int y) { return world.y+project(y,ChartHeight,world.h); };
    const int left=std::max(0,int(std::int64_t(visible.x-world.x)*ChartWidth/world.w)/ChartCell-1);
    const int right=std::min(ChartColumns-1,int(std::int64_t(visible.right()-world.x)*ChartWidth/world.w)/ChartCell+1);
    const int top=std::max(0,int(std::int64_t(visible.y-world.y)*ChartHeight/world.h)/ChartCell-1);
    const int bottom=std::min(ChartRows-1,int(std::int64_t(visible.bottom()-world.y)*ChartHeight/world.h)/ChartCell+1);
    d.clip=visible;
    for (int x=(left+3)/4*4;x<=right;x+=4)
        d.fill({column(x),visible.y,1,visible.h},grid);
    for (int y=(top+3)/4*4;y<=bottom;y+=4)
        d.fill({visible.x,row(y),visible.w,1},grid);
    for (int y=top;y<=bottom;++y) {
        const int north=row(y), south=row(y+1);
        for (int x=left;x<=right;) {
            if (!game.chart_cell(x,y)) { ++x; continue; }
            const int first=x++;
            while (x<=right && game.chart_cell(x,y)) ++x;
            const int west=column(first);
            d.clip=visible;
            d.clip=d.clipped({west,north,column(x)-west,south-north});
            if (d.clip.w && d.clip.h) {
                if (helm && world.w>ChartWidth) draw_helm_chart(d,world);
                else d.blit(atlas,{0,0,ChartWidth,ChartHeight},world,false,false);
            }
        }
    }
    d.clip=visible;
    if (frame) d.frame(world,rgb(0x497172));
    d.clip=outer;
}

} // namespace tide
