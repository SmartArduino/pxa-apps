#pragma once

#include "chart_style.hpp"
#include "pilot_atlas.hpp"

namespace tide {

namespace pilot_sea {

inline constexpr int SurfaceWidth=128, SurfaceHeight=128;

constexpr int wave(int phase) {
    const int p=phase&63;
    const int t=p<32?p:64-p;
    return (t*t*(96-2*t)+512)/1024;
}

// Bake intersecting swells and short, curved crests into a small seamless
// texture. Drawing a water pixel needs a single texel and a palette lookup.
inline constexpr auto Surface=[] {
    std::array<std::array<std::uint8_t,SurfaceWidth>,SurfaceHeight> texture{};
    for (int y=0;y<SurfaceHeight;++y) {
        const int drift=wave(3*y/2);
        for (int x=0;x<SurfaceWidth;++x) {
            const int bend=wave(x+y/2);
            const int swell=wave(x-y/2+bend/2)*2+wave(x/2+2*y+drift);
            const int crest=wave(3*x-2*y+bend+drift);
            const int breaks=wave(3*x/2+y+bend);
            const int glint=std::max(0,crest-25)*breaks/32;
            const int shimmer=wave(7*x+8*y+drift)>=28 && swell>=46?2:0;
            texture[y][x]=std::min(15,2+swell/12+glint+shimmer);
        }
    }
    return texture;
}();

inline constexpr auto Palette=[] {
    constexpr unsigned deep=0x153d53, shallows=0x347f80;
    std::array<std::array<Color,16>,16> colors{};
    for (int bank=0;bank<16;++bank) {
        const auto channel=[&](int shift) {
            return (((deep>>shift)&255)*(15-bank)+((shallows>>shift)&255)*bank+7)/15;
        };
        const auto base=channel(16)<<16 | channel(8)<<8 | channel(0);
        for (int ripple=0;ripple<16;++ripple) {
            const int glint=std::max(0,ripple-11)*(8+bank)/16;
            colors[bank][ripple]=chart_style::tint(base,ripple-8+glint);
        }
    }
    return colors;
}();

// Four columns of land bits from each of four rows, with a row per nibble.
inline unsigned coast_window(const std::array<PilotAtlas::Row,5>& rows,int y,int x) {
    // The overlapping tile collar leaves space for all four columns.
    const int byte=x/8, shift=12-(x&7);
    unsigned window=0;
    for (int row=0;row<4;++row) {
        const auto& bits=rows[y+row].bits;
        const auto cells=((unsigned(bits[byte])<<8 | bits[byte+1])>>shift)&15;
        window|=cells<<(4*row);
    }
    return window;
}

// Three adjacent columns in three of the packed rows, centred on the middle cell.
inline int coastal_weight(unsigned window) {
    const auto middle=(window>>4)&7;
    if (middle&2) return 34;
    constexpr int neighbouring_row[]{0,3,8,11,3,6,11,14};
    const int side=(middle&1?8:0)+(middle&4?8:0);
    return side+neighbouring_row[window&7]+neighbouring_row[(window>>8)&7];
}

// Four weights in atlas order, with 16 fractional positions per axis.
inline int interpolate(const std::array<int,4>& values,int x,int y) {
    const int top=16*values[0]+(values[1]-values[0])*x;
    const int bottom=16*values[2]+(values[3]-values[2])*x;
    return 16*top+(bottom-top)*y;
}

} // namespace pilot_sea

struct PilotRaster {
    Rect view;
    float west,north,dx,dy;
    int wave_strength, grid_x, grid_y;

    PilotRaster(Rect view,float center_x,float center_y,float x_scale,float y_scale)
        : view(view), west(center_x-(view.w/2)/x_scale), north(center_y-(view.h/2)/y_scale),
          dx(1.f/x_scale), dy(1.f/y_scale),
          wave_strength(int(std::clamp(9.f-std::max(dx,dy),0.f,8.f))),
          grid_x(dx>=3.f?int(std::ceil(dx)):0), grid_y(grid_x?int(std::ceil(dy)):0) {}

    // Water samples are twice as fine as the shoreline coordinates.
    int x(int screen) const { return int(std::floor(2.f*(west+(screen-view.x+.5f)*dx))); }
    int y(int screen) const { return int(std::floor(2.f*(north+(screen-view.y+.5f)*dy))); }
};

struct PilotSeaLine {
    int v, strength, shade_bias, grid_x;
    bool latitude_line;

    PilotSeaLine(const PilotRaster& raster,int sample_y)
        : v(sample_y), strength(raster.wave_strength), shade_bias(68-8*strength), grid_x(raster.grid_x),
          latitude_line((unsigned(v>>1)%(ChartLatitudeScale*PilotAtlasUnit))<unsigned(raster.grid_y)) {}

    int shade(int u) const {
        const auto& row=pilot_sea::Surface[unsigned(v+(u>>2))&(pilot_sea::SurfaceHeight-1)];
        const int ripple=row[unsigned(u)&(pilot_sea::SurfaceWidth-1)];
        return (ripple*strength+shade_bias)>>3;
    }

    Color color(int u,int shelf=0) const {
        const auto water=pilot_sea::Palette[shelf][shade(u)];
        return grid_x && (latitude_line || (u>>1)%(ChartLongitudeScale*PilotAtlasUnit)<grid_x)
            ?Draw::mix(water,rgb(0x31767c),3):water;
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

inline void draw_pilot_world(Draw& d,const PilotRaster& raster,const PilotAtlas& atlas,const Game& game,Rect covered,
                             std::span<const int> columns) {
    const auto rect=raster.view;
    const auto r=d.clipped(rect);
    covered=d.clipped(covered);
    const int gap_left=std::max(r.x,covered.x), gap_right=std::min(r.right(),covered.right());
    const Image overview{art::chart};
    constexpr auto beyond=rgb(0x142b37);
    struct CoastRows {
        int patch=-1, sample=-1;
        std::array<PilotAtlas::Row,5> terrain;
    };
    std::array<CoastRows,4> coast_rows;
    for (int y=r.y;y<r.bottom();++y) {
        const bool masked=gap_left<gap_right && y>=covered.y && y<covered.bottom();
        if (masked && gap_left==r.x && gap_right==r.right()) continue;
        auto pixels=*d.pixels.row(y);
        const int sample_y=raster.y(y);
        const int v=sample_y>>1;
        if (v<0 || v>=PilotAtlasHeight) {
            for (int x=r.x;x<r.right();++x) pixels[x]=beyond;
            continue;
        }
        const PilotSeaLine sea{raster,sample_y};
        const int tile_row=v/PilotPatchHeight;
        const int gy=v/PilotAtlasUnit;
        int current_column=-1;
        int chart_column=-1;
        bool charted=false;
        Point origin{};
        const CoastRows* rows=nullptr;
        int last_sample=-1;
        std::array<int,4> bank{};
        bool near_coast=false;
        unsigned corners=0;
        const int north=(v&15)<8?1:2, fy=(v+8)&15;
        const auto segment=[&](int left,int right) {
            for (int x=left;x<right;++x) {
                const int sample_x=columns[x-r.x];
                const int u=sample_x>>1;
                if (u<0 || u>=PilotAtlasWidth) { pixels[x]=beyond; continue; }
                const int cell_column=u/(PilotAtlasUnit*ChartCell);
                if (cell_column!=chart_column) {
                    chart_column=cell_column;
                    charted=game.chart_cell(chart_column,gy/ChartCell);
                }
                if (!charted) { pixels[x]=chart_style::Uncharted; continue; }
                const int column=u/PilotPatchWidth;
                if (column!=current_column) {
                    const int id=tile_row*PilotTilesX+column;
                    origin=pilot_patch_origin(id);
                    const int sample=(v-origin.y*8)/PilotSubcell;
                    auto& cache=coast_rows[column%coast_rows.size()];
                    if (cache.patch!=id || cache.sample!=sample)
                        cache={id,sample,{atlas.row(id,sample-2),atlas.row(id,sample-1),atlas.row(id,sample),
                                          atlas.row(id,sample+1),atlas.row(id,sample+2)}};
                    rows=&cache;
                    current_column=column;
                    last_sample=-1;
                }
                const int local_x=u-origin.x*8;
                const int gx=u/PilotAtlasUnit;
                const bool loaded=rows->terrain[2].loaded;
                const int coarse=loaded?0:overview.index(gx,gy);
                int coverage=coarse>=2?256:0;
                int shelf_alpha=coarse==1?8:0;
                if (loaded) {
                    const int sample=(local_x+8)/16;
                    if (sample!=last_sample) {
                        const unsigned window=pilot_sea::coast_window(rows->terrain,north-1,sample-2);
                        const unsigned top=(window>>4)&6, bottom=(window>>8)&6;
                        corners=(top>>2)|(top&2)|(bottom&4)|((bottom&2)<<2);
                        near_coast=window!=0;
                        if (near_coast) {
                            const bool consecutive=last_sample>=0 && sample==last_sample+1;
                            bank={consecutive?bank[1]:pilot_sea::coastal_weight(window>>1),
                                  pilot_sea::coastal_weight(window),
                                  consecutive?bank[3]:pilot_sea::coastal_weight(window>>5),
                                  pilot_sea::coastal_weight(window>>4)};
                        } else bank={};
                        last_sample=sample;
                    }
                    const int fx=(local_x+8)&15;
                    if (corners==15) coverage=256;
                    else if (corners) {
                        const int a=(corners&1?16-fx:0)+(corners&2?fx:0);
                        const int b=(corners&4?16-fx:0)+(corners&8?fx:0);
                        coverage=a*(16-fy)+b*fy;
                    }
                    if (coverage<168 && near_coast)
                        shelf_alpha=std::min(15,pilot_sea::interpolate(bank,fx,fy)/512);
                } else if (coarse==1 || coarse==3) {
                    const auto edge=chart_style::coast(overview,gx,gy,2*(u%PilotAtlasUnit),2*(v%PilotAtlasUnit),coarse);
                    coverage=edge.land;
                    shelf_alpha=edge.shelf/24;
                }
                if (coverage>=168) {
                    const int height=Draw::relief_level(gx,gy);
                    const int grain=chart_style::texture(height>=10?3:2,u>>2,v>>2);
                    pixels[x]=chart_style::Palette[3+height][grain];
                } else {
                    const auto shelf=sea.color(sample_x,shelf_alpha);
                    if (coverage<=86) pixels[x]=shelf;
                    else {
                        const int height=Draw::relief_level(gx,gy);
                        const int grain=chart_style::texture(2,u>>2,v>>2);
                        pixels[x]=Draw::mix(shelf,chart_style::Palette[3+height][grain],(coverage-86)*15/82);
                    }
                }
            }
        };
        segment(r.x,masked?gap_left:r.right());
        if (masked) segment(gap_right,r.right());
    }
}

inline void draw_pilot_detail(Draw& d,PilotWater water,Rect target,const PilotRaster& raster,
                              std::span<const int> columns,int opacity,
                              const std::array<std::uint32_t,PilotSurveyRows>& surveyed) {
    const auto r=d.clipped(target);
    if (r.w<=0 || r.h<=0 || !water.bytes || target.w<=0 || target.h<=0) return;
    constexpr std::array<unsigned,16> colors{
        0x153d53,0x1c4f64,0x286a7b,0x388080,0xc5b37d,0xa49a63,0x82925c,0x6e8350,
        0x687749,0x7b774b,0x8a7f57,0xa18d64,0x9d8365,0xad9377,0xc1ab8e,0xd2c6ac
    };
    constexpr auto palette=[colors] {
        std::array<std::array<Color,16>,16> result{};
        for (int terrain=0;terrain<16;++terrain)
            for (int shade=0;shade<16;++shade)
                result[terrain][shade]=chart_style::tint(colors[terrain],(shade-8)*(terrain<4?1:3));
        return result;
    }();
    constexpr int fraction=16;
    const int width=PilotWidth*fraction;
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
        const int v=int(std::int64_t(y-target.y)*PilotHeight*fraction/target.h);
        const PilotSeaLine sea{raster,raster.y(y)};
        const int row=v/fraction;
        const int vertical_edge=std::min(row,PilotHeight-1-row);
        const int dy=2*(v&(fraction-1))-(fraction-1);
        const int neighbour_row=std::clamp(row+(dy<0?-1:1),0,PilotHeight-1);
        const auto known=surveyed[row/PilotSurveyCell];
        const auto neighbour_known=surveyed[neighbour_row/PilotSurveyCell];
        const auto* terrain_row=water.bytes+4+row*(PilotWidth/2);
        const auto* adjacent_row=water.bytes+4+neighbour_row*(PilotWidth/2);
        const auto terrain_at=[](int cell,std::uint32_t mask,const unsigned char* data) {
            return mask&(std::uint32_t{1}<<(cell/PilotSurveyCell)) ? (data[cell/2]>>(4*(cell&1)))&15 : -1;
        };
        const auto segment=[&]<bool Blend>(int left,int right) {
            for (int x=left;x<right;++x) {
                const int tex=texture_x[x-r.x], cell=tex/fraction;
                Color shaded=chart_style::Uncharted;
                if (known&(std::uint32_t{1}<<(cell/PilotSurveyCell))) {
                    const auto color=[&](int terrain) {
                        if (terrain<0) return chart_style::Uncharted;
                        if (!terrain) return sea.color(columns[x-d.clip.x]);
                        if (terrain<4) return palette[terrain][sea.shade(columns[x-d.clip.x])];
                        const int material=terrain==4?1:terrain<=9?2:3;
                        return palette[terrain][chart_style::texture(material,tex/4,v/4)];
                    };
                    const int terrain=(terrain_row[cell/2]>>(4*(cell&1)))&15;
                    shaded=color(terrain);
                    const int dx=2*(tex&(fraction-1))-(fraction-1);
                    const int edge=std::max(std::abs(dx),std::abs(dy));
                    if (edge>3) {
                        const int adjacent=std::abs(dx)>std::abs(dy)?
                            terrain_at(std::clamp(cell+(dx<0?-1:1),0,PilotWidth-1),known,terrain_row):
                            terrain_at(cell,neighbour_known,adjacent_row);
                        if (adjacent!=terrain) shaded=Draw::mix(shaded,color(adjacent),edge/2);
                    }
                }
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

inline void draw_pilot_terrain(Draw& d,const PilotRaster& raster,const PilotAtlas& atlas,const Game& game,
                               int region,PilotWater detail,Rect target,int opacity) {
    const Rect previous=d.clip, visible=d.clipped(raster.view);
    const bool draw_detail=region>=0 && region<WorldPilotIndex && opacity && detail.bytes;
    const Rect covered=draw_detail && opacity==15?opaque_pilot_detail(target):Rect{};
    std::array<int,PilotStripWidth> columns;
    for (int left=visible.x;left<visible.right();) {
        const int width=std::min(visible.right()-left,int(columns.size()));
        d.clip={left,visible.y,width,visible.h};
        // The atlas source column is shared by every scanline and both terrain layers.
        for (int i=0;i<width;++i) columns[i]=raster.x(left+i);
        const auto samples=std::span(columns).first(width);
        draw_pilot_world(d,raster,atlas,game,covered,samples);
        if (draw_detail) draw_pilot_detail(d,detail,target,raster,samples,opacity,game.s.pilot_chart[region]);
        left+=width;
    }
    d.clip=previous;
}

} // namespace tide
