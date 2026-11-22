#pragma once

#include "chart_style.hpp"
#include "pilot_atlas.hpp"

namespace tide {

struct PilotRaster {
    Rect view;
    float west,north,dx,dy;

    PilotRaster(Rect view,Point center,float x_scale,float y_scale)
        : view(view), west(center.x-(view.w/2.f)/x_scale), north(center.y-(view.h/2.f)/y_scale),
          dx(1.f/x_scale), dy(1.f/y_scale) {}

    int x(int screen) const { return int(std::floor(west+(screen-view.x+.5f)*dx)); }
    int y(int screen) const { return int(std::floor(north+(screen-view.y+.5f)*dy)); }

    Color sea(int u,int v) const {
        constexpr auto shades=[] {
            std::array<Color,16> palette{};
            for (int i=0;i<16;++i) palette[i]=chart_style::tint(0x103f54,i-8);
            return palette;
        }();
        const auto color=shades[chart_style::texture(0,u,v)];
        if (dx>=3.f && (u%(ChartLongitudeScale*PilotAtlasUnit)<dx ||
                        v%(ChartLatitudeScale*PilotAtlasUnit)<dy))
            return Draw::mix(color,rgb(0x31767c),3);
        return color;
    }
};

inline void draw_pilot_world(Draw& d,const PilotRaster& raster,const PilotAtlas& atlas) {
    const auto rect=raster.view;
    const auto r=d.clipped(rect);
    const Image overview{art::chart};
    constexpr auto coast=rgb(0x31767c), beyond=rgb(0x142b37);
    for (int y=r.y;y<r.bottom();++y) {
        auto pixels=*d.pixels.row(y);
        const int v=raster.y(y);
        if (v<0 || v>=PilotAtlasHeight) {
            for (int x=r.x;x<r.right();++x) pixels[x]=beyond;
            continue;
        }
        const int tile_row=v/PilotPatchHeight;
        const int gy=v/PilotAtlasUnit;
        int current_column=-1;
        Point origin{};
        PilotAtlas::Row middle,above,below;
        for (int x=r.x;x<r.right();++x) {
            const int u=raster.x(x);
            if (u<0 || u>=PilotAtlasWidth) { pixels[x]=beyond; continue; }
            const int column=u/PilotPatchWidth;
            if (column!=current_column) {
                const int id=tile_row*PilotTilesX+column;
                origin=pilot_patch_origin(id);
                const int sample=(v-origin.y*8)/PilotSubcell;
                middle=atlas.row(id,sample);
                above=atlas.row(id,sample-1);
                below=atlas.row(id,sample+1);
                current_column=column;
            }
            const int cell=(u-origin.x*8)/PilotSubcell;
            const int gx=u/PilotAtlasUnit;
            const int coarse=overview.index(gx,gy);
            const bool land=middle.loaded?middle.land(cell):coarse>=2;
            if (land) {
                const int height=Draw::relief_level(gx,gy);
                const int grain=chart_style::texture(height>=10?3:2,u>>2,v>>2);
                pixels[x]=chart_style::Palette[3+height][grain];
            } else {
                const bool bank=middle.loaded?(middle.land(cell-1) || middle.land(cell+1) ||
                                                above.land(cell) || below.land(cell)):coarse==1;
                const auto deep=raster.sea(u,v);
                pixels[x]=bank?Draw::mix(deep,coast,8):deep;
            }
        }
    }
}

inline void draw_pilot_detail(Draw& d,PilotWater water,Rect target,const PilotRaster& raster) {
    const auto r=d.clipped(target);
    if (r.w<=0 || r.h<=0 || !water.bytes || target.w<=0 || target.h<=0) return;
    constexpr std::array<unsigned,16> colors{
        0x0e4057,0x15576b,0x227986,0x419997,0xc5b37d,0xa49a63,0x82925c,0x6e8350,
        0x687749,0x7b774b,0x8a7f57,0xa18d64,0x9d8365,0xad9377,0xc1ab8e,0xd2c6ac
    };
    constexpr auto palette=[colors] {
        std::array<std::array<Color,16>,16> result{};
        for (int terrain=0;terrain<16;++terrain)
            for (int shade=0;shade<16;++shade)
                result[terrain][shade]=chart_style::tint(colors[terrain],(shade-8)*(terrain<4?1:3));
        return result;
    }();
    for (int y=r.y;y<r.bottom();++y) {
        auto pixels=*d.pixels.row(y);
        const int v=(y-target.y)*PilotHeight*4/target.h;
        const int latitude=raster.y(y);
        for (int x=r.x;x<r.right();++x) {
            const int u=(x-target.x)*PilotWidth*4/target.w;
            const Point cell{u/4,v/4};
            const int terrain=water.at(cell);
            const int material=terrain<5?1:terrain<=9?2:3;
            const auto shaded=terrain==0?raster.sea(raster.x(x),latitude):
                palette[terrain][chart_style::texture(material,u,v)];
            const int edge=std::min({cell.x,PilotWidth-1-cell.x,cell.y,PilotHeight-1-cell.y});
            const int fade=std::min(16,edge*2);
            pixels[x]=fade>=16?shaded:Draw::mix(pixels[x],shaded,fade);
        }
    }
}

} // namespace tide
