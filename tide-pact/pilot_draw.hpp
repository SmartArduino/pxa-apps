#pragma once

#include "chart_style.hpp"
#include "pilot_atlas.hpp"

namespace tide {

struct PilotRaster {
    Rect view;
    float west,north,dx,dy;

    PilotRaster(Rect view,float center_x,float center_y,float x_scale,float y_scale)
        : view(view), west(center_x-(view.w/2)/x_scale), north(center_y-(view.h/2)/y_scale),
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

// The regional chart blends over eight terrain cells on each edge.
inline constexpr int PilotDetailBlend=8;
inline constexpr int PilotStripWidth=512;
inline Rect opaque_pilot_detail(Rect target) {
    if (target.w<=0 || target.h<=0) return {};
    const auto project=[](int cell,int pixels,int count) {
        return int((std::int64_t(cell)*pixels+count-1)/count);
    };
    const int left=project(PilotDetailBlend,target.w,PilotWidth);
    const int right=project(PilotWidth-PilotDetailBlend,target.w,PilotWidth);
    const int top=project(PilotDetailBlend,target.h,PilotHeight);
    const int bottom=project(PilotHeight-PilotDetailBlend,target.h,PilotHeight);
    return {target.x+left,target.y+top,right-left,bottom-top};
}

inline void draw_pilot_world(Draw& d,const PilotRaster& raster,const PilotAtlas& atlas,Rect covered,
                             std::span<const int> columns) {
    const auto rect=raster.view;
    const auto r=d.clipped(rect);
    covered=d.clipped(covered);
    const int gap_left=std::max(r.x,covered.x), gap_right=std::min(r.right(),covered.right());
    const Image overview{art::chart};
    constexpr auto coast=rgb(0x31767c), beyond=rgb(0x142b37);
    struct CoastRows {
        int patch=-1, sample=-1;
        PilotAtlas::Row middle,above,below;
    };
    std::array<CoastRows,4> coast_rows;
    for (int y=r.y;y<r.bottom();++y) {
        const bool masked=gap_left<gap_right && y>=covered.y && y<covered.bottom();
        if (masked && gap_left==r.x && gap_right==r.right()) continue;
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
        const CoastRows* rows=nullptr;
        const auto segment=[&](int left,int right) {
            for (int x=left;x<right;++x) {
                const int u=columns[x-r.x];
                if (u<0 || u>=PilotAtlasWidth) { pixels[x]=beyond; continue; }
                const int column=u/PilotPatchWidth;
                if (column!=current_column) {
                    const int id=tile_row*PilotTilesX+column;
                    origin=pilot_patch_origin(id);
                    const int sample=(v-origin.y*8)/PilotSubcell;
                    auto& cache=coast_rows[column%coast_rows.size()];
                    if (cache.patch!=id || cache.sample!=sample) {
                        cache={id,sample,atlas.row(id,sample),atlas.row(id,sample-1),atlas.row(id,sample+1)};
                    }
                    rows=&cache;
                    current_column=column;
                }
                const int cell=(u-origin.x*8)/PilotSubcell;
                const int gx=u/PilotAtlasUnit;
                const bool loaded=rows->middle.loaded;
                const int coarse=loaded?0:overview.index(gx,gy);
                const bool land=loaded?rows->middle.land(cell):coarse>=2;
                if (land) {
                    const int height=Draw::relief_level(gx,gy);
                    const int grain=chart_style::texture(height>=10?3:2,u>>2,v>>2);
                    pixels[x]=chart_style::Palette[3+height][grain];
                } else {
                    const bool bank=loaded?(rows->middle.land(cell-1) || rows->middle.land(cell+1) ||
                                            rows->above.land(cell) || rows->below.land(cell)):coarse==1;
                    const auto deep=raster.sea(u,v);
                    pixels[x]=bank?Draw::mix(deep,coast,8):deep;
                }
            }
        };
        segment(r.x,masked?gap_left:r.right());
        if (masked) segment(gap_right,r.right());
    }
}

inline void draw_pilot_detail(Draw& d,PilotWater water,Rect target,const PilotRaster& raster,
                              std::span<const int> columns,int opacity) {
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
    const int width=PilotWidth*4;
    const auto offset=std::int64_t(r.x-target.x)*width;
    const int step=width/target.w, remainder=width%target.w;
    std::array<std::uint16_t,PilotStripWidth> texture_x;
    int u=int(offset/target.w), error=int(offset%target.w);
    for (int i=0;i<r.w;++i) {
        texture_x[i]=u;
        u+=step;
        error+=remainder;
        if (error>=target.w) { error-=target.w; ++u; }
    }
    const Rect opaque=opacity==15?d.clipped(opaque_pilot_detail(target)):Rect{};
    for (int y=r.y;y<r.bottom();++y) {
        auto pixels=*d.pixels.row(y);
        const int v=int(std::int64_t(y-target.y)*PilotHeight*4/target.h);
        const int latitude=raster.y(y);
        const int vertical_edge=std::min(v/4,PilotHeight-1-v/4);
        const auto* terrain_row=water.bytes+4+(v/4)*(PilotWidth/2);
        const auto segment=[&]<bool Blend>(int left,int right) {
            for (int x=left;x<right;++x) {
                const int tex=texture_x[x-r.x], cell=tex/4;
                const int terrain=(terrain_row[cell/2]>>(4*(cell&1)))&15;
                const int material=terrain<5?1:terrain<=9?2:3;
                const auto shaded=terrain==0?raster.sea(columns[x-d.clip.x],latitude):
                    palette[terrain][chart_style::texture(material,tex,v)];
                if constexpr (Blend) {
                    const int edge=std::min({cell,PilotWidth-1-cell,vertical_edge});
                    pixels[x]=Draw::mix(pixels[x],shaded,std::min(opacity,edge*2));
                } else pixels[x]=shaded;
            }
        };
        if (opaque.w && y>=opaque.y && y<opaque.bottom()) {
            segment.operator()<true>(r.x,opaque.x);
            segment.operator()<false>(opaque.x,opaque.right());
            segment.operator()<true>(opaque.right(),r.right());
        } else segment.operator()<true>(r.x,r.right());
    }
}

inline void draw_pilot_terrain(Draw& d,const PilotRaster& raster,const PilotAtlas& atlas,
                               PilotWater detail,Rect target,int opacity) {
    const Rect previous=d.clip, visible=d.clipped(raster.view);
    const Rect covered=detail.bytes && opacity==15?opaque_pilot_detail(target):Rect{};
    std::array<int,PilotStripWidth> columns;
    for (int left=visible.x;left<visible.right();) {
        const int width=std::min(visible.right()-left,int(columns.size()));
        d.clip={left,visible.y,width,visible.h};
        // The atlas source column is shared by every scanline and both terrain layers.
        for (int i=0;i<width;++i) columns[i]=raster.x(left+i);
        const auto samples=std::span(columns).first(width);
        draw_pilot_world(d,raster,atlas,covered,samples);
        if (opacity && detail.bytes) draw_pilot_detail(d,detail,target,raster,samples,opacity);
        left+=width;
    }
    d.clip=previous;
}

} // namespace tide
