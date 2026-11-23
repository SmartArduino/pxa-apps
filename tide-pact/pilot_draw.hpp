#pragma once

#include "chart_style.hpp"
#include "pilot_atlas.hpp"

#include <memory>

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

    PilotRaster(Rect view,float center_x,float center_y,float x_scale,float y_scale,int density=1)
        : view(view), west(center_x*x_scale-view.w/2), north(center_y*y_scale-view.h/2),
          dx(1.f/x_scale), dy(1.f/y_scale),
          wave_strength(int(std::clamp(9.f-std::max(dx,dy)*density,0.f,8.f))),
          grid_x(dx*density>=3.f?int(std::ceil(dx*density))/density:0),
          grid_y(grid_x?int(std::ceil(dy*density))/density:0) {}

    PilotRaster(Rect view,Point centre,float x_scale,float y_scale,int density=1)
        : PilotRaster(view,0.f,0.f,x_scale,y_scale,density) {
        west=float(centre.x-view.w/2);
        north=float(centre.y-view.h/2);
    }

    // Water samples are twice as fine as the shoreline coordinates.
    int x(int screen) const { return int(std::floor(2.f*(west+screen-view.x+.5f)*dx)); }
    int y(int screen) const { return int(std::floor(2.f*(north+screen-view.y+.5f)*dy)); }
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

inline constexpr int PilotDetailBlend=8;
inline constexpr int PilotRoadsteadBlend=24;
inline constexpr int PilotStripWidth=512;
inline Rect opaque_pilot_detail(Rect target,int blend) {
    if (target.w<=0 || target.h<=0) return {};
    const auto project=[](int cell,int pixels,int count) {
        return int((std::int64_t(cell)*pixels+count-1)/count);
    };
    const int left=project(blend,target.w,PilotWidth);
    const int right=project(PilotWidth-blend,target.w,PilotWidth);
    const int top=project(blend,target.h,PilotHeight);
    const int bottom=project(PilotHeight-blend,target.h,PilotHeight);
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
        auto* pixels=d.row(y);
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

namespace pilot_detail {

inline constexpr int Fraction=16, Bands=8, BandScale=Fraction*Fraction/Bands;
inline constexpr int DepthShift=9, HeightShift=19;
inline constexpr unsigned Unsurveyed=1U<<31;
using Palette=std::array<std::array<Color,16>,16*Bands>;

inline constexpr auto Shades=[] {
    constexpr std::array<unsigned,16> colors{
        0x153d53,0x1c4f64,0x286a7b,0x388080,0xa99869,0x82905e,0x758556,0x6f8052,
        0x707b51,0x7b7b53,0x8a8057,0x9b8962,0xa38c6c,0xb2977e,0xc1ab8e,0xd2c6ac
    };
    std::array<Palette,2> palettes{};
    for (int band=0;band<16*Bands;++band) {
        const int base=band/Bands, part=band%Bands;
        const int next=std::min(base+1,base<4?3:15);
        const auto channel=[&](int shift) {
            return (((colors[base]>>shift)&255)*(Bands-part)+((colors[next]>>shift)&255)*part+Bands/2)/Bands;
        };
        const auto color=channel(16)<<16 | channel(8)<<8 | channel(0);
        for (int shade=0;shade<16;++shade) {
            palettes[0][band][shade]=chart_style::tint(color,(shade-8)*(base<4?1:2));
            if (base<4) {
                constexpr int shelf[]{0,1,4,10};
                const int bank=(shelf[base]*(Bands-part)+shelf[next]*part+Bands/2)/Bands;
                palettes[1][band][shade]=pilot_sea::Palette[bank][shade];
            } else
                palettes[1][band][shade]=Draw::mix(chart_style::Palette[base-1][shade],
                                                    chart_style::Palette[next-1][shade],(15*part+Bands/2)/Bands);
        }
    }
    return palettes;
}();

// After two 16-point interpolations the three fields occupy 9, 10, and 12 bits.
// Keeping them in separate lanes lets one interpolation carry the shoreline,
// water depth, and land elevation together.
inline constexpr auto TerrainFields=[] {
    std::array<unsigned,16> values{};
    for (unsigned terrain=0;terrain<values.size();++terrain)
        values[terrain]=unsigned(terrain>=4) | (std::min(terrain,3U)<<DepthShift) |
                        ((std::max(terrain,4U)-4)<<HeightShift);
    return values;
}();

} // namespace pilot_detail

inline void draw_pilot_detail(Draw& d,PilotWater water,Rect target,const PilotRaster& raster,
                              std::span<const int> columns,int opacity,
                              const std::array<std::uint32_t,PilotSurveyRows>& surveyed) {
    using namespace pilot_detail;
    const auto r=d.clipped(target);
    if (r.w<=0 || r.h<=0 || !water.bytes || target.w<=0 || target.h<=0) return;
    const auto& shades=Shades[water.roadstead];
    const int blend=water.roadstead?PilotRoadsteadBlend:PilotDetailBlend;
    const int width=PilotWidth*Fraction;
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
    const int first_cell=std::max(0,(texture_x.front()+Fraction/2)/Fraction-1);
    const int last_cell=std::min(PilotWidth-1,(texture_x[r.w-1]+Fraction/2)/Fraction);
    struct TerrainRow { int index=-1; std::array<unsigned,PilotWidth> fields; };
    std::array<TerrainRow,2> row_cache;
    const auto fields_at=[&](int row) -> const std::array<unsigned,PilotWidth>& {
        auto& cached=row_cache[row&1];
        if (cached.index==row) return cached.fields;
        cached.index=row;
        const auto known=surveyed[row/PilotSurveyCell];
        const auto* bytes=water.bytes+4+row*(PilotWidth/2);
        for (int cell=first_cell;cell<=last_cell;++cell)
            cached.fields[cell]=known&(1U<<(cell/PilotSurveyCell))?
                TerrainFields[(bytes[cell/2]>>(4*(cell&1)))&15]:Unsurveyed;
        return cached.fields;
    };
    const Rect opaque=opacity==15?d.clipped(opaque_pilot_detail(target,blend)):Rect{};
    for (int y=r.y;y<r.bottom();++y) {
        auto* pixels=d.row(y);
        const int v=int(std::int64_t(y-target.y)*PilotHeight*Fraction/target.h);
        const PilotSeaLine sea{raster,raster.y(y)};
        const int row=v/Fraction;
        const int vertical_edge=std::min(row,PilotHeight-1-row);
        const int north=(v+Fraction/2)/Fraction-1, fy=(v+Fraction/2)&(Fraction-1);
        const int top=std::max(0,north), bottom=std::min(PilotHeight-1,north+1);
        const auto known=surveyed[row/PilotSurveyCell];
        const auto& north_row=fields_at(top);
        const auto& south_row=fields_at(bottom);
        int last_pair=-1;
        unsigned left_fields=0,right_fields=0;
        int west_light=8*Fraction, east_light=8*Fraction;
        const auto segment=[&]<bool Blend>(int left,int right) {
            for (int x=left;x<right;++x) {
                const int tex=texture_x[x-r.x], cell=tex/Fraction;
                Color shaded=chart_style::Uncharted;
                if (known&(std::uint32_t{1}<<(cell/PilotSurveyCell))) {
                    const int pair=(tex+Fraction/2)/Fraction;
                    if (pair!=last_pair) {
                        const int west=std::max(0,pair-1), east=std::min(PilotWidth-1,pair);
                        unsigned nw=north_row[west], ne=north_row[east];
                        unsigned sw=south_row[west], se=south_row[east];
                        // Do not blend colours or terrain from behind the survey fog.
                        if ((nw|ne|sw|se)&Unsurveyed) {
                            const unsigned fallback=nw!=Unsurveyed?nw:ne!=Unsurveyed?ne:sw!=Unsurveyed?sw:se;
                            if (nw==Unsurveyed) nw=fallback;
                            if (ne==Unsurveyed) ne=fallback;
                            if (sw==Unsurveyed) sw=fallback;
                            if (se==Unsurveyed) se=fallback;
                        }
                        left_fields=nw*(Fraction-fy)+sw*fy;
                        right_fields=ne*(Fraction-fy)+se*fy;
                        if ((nw|ne|sw|se)&1U) {
                            // Light the north-west slopes and soften the gradient across each pair.
                            const int east_rise=int(right_fields>>HeightShift)-int(left_fields>>HeightShift);
                            west_light=8*Fraction+east_rise+8*(int(sw>>HeightShift)-int(nw>>HeightShift));
                            east_light=8*Fraction+east_rise+8*(int(se>>HeightShift)-int(ne>>HeightShift));
                        }
                        last_pair=pair;
                    }
                    const int fx=(tex+Fraction/2)&(Fraction-1);
                    const unsigned fields=left_fields*(Fraction-fx)+right_fields*fx;
                    const int coast=fields&((1U<<DepthShift)-1);
                    const auto land=[&] {
                        const int band=4*Bands+int(((fields>>HeightShift)+BandScale/2)/BandScale);
                        const int material=band<5*Bands?1:band<=9*Bands?2:3;
                        const int grain=chart_style::texture(material,tex/2+v/64,v/2+tex/64);
                        const int light=(west_light*(Fraction-fx)+east_light*fx)/(Fraction*Fraction);
                        return shades[band][std::clamp(light+(grain-8)*3/4,0,15)];
                    };
                    const auto ocean=[&] {
                        const int band=(((fields>>DepthShift)&1023)+BandScale/2)/BandScale;
                        const int u=columns[x-d.clip.x];
                        return band?shades[band][sea.shade(u)]:sea.color(u);
                    };
                    if (coast<=108) shaded=ocean();
                    else if (coast>=148) shaded=land();
                    else shaded=Draw::mix(ocean(),land(),(coast-108)*15/40);
                }
                if constexpr (Blend) {
                    const int edge=std::min({cell,PilotWidth-1-cell,vertical_edge});
                    const int feather=water.roadstead?edge*15/PilotRoadsteadBlend:edge*2;
                    pixels[x]=Draw::mix(pixels[x],shaded,std::min(opacity,feather));
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
    const int blend=detail.roadstead?PilotRoadsteadBlend:PilotDetailBlend;
    const Rect covered=draw_detail && opacity==15?opaque_pilot_detail(target,blend):Rect{};
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

class ChartTerrainCache {
public:
    void reset() {
        bytes.reset();
        capacity=0;
        width=0;
    }
    float stable_longitude_scale(float candidate,float vertical,int visible_width) const {
        // Reuse a projection until its change would move the map edge by half a pixel.
        return mode==Pilot && visible_width==width && vertical==scale_y &&
               std::abs(candidate-scale_x)*width<=scale_x?scale_x:candidate;
    }

    void draw(Draw& out,Rect view,Point centre,float sx,float sy,const PilotAtlas& atlas,const Game& game,
              int region,PilotWater detail,Rect target,int opacity,int density=1) {
        const auto revisions=atlas.revisions();
        const auto& coast=game.s.coast;
        const auto survey=region>=0 && region<WorldPilotIndex?game.s.pilot_chart[region]:decltype(surveyed){};
        const bool aligned=mode==Pilot && sx==scale_x && sy==scale_y &&
                           region==detail_region && detail.bytes==detail_bytes && opacity==detail_opacity &&
                           coast==charted && survey==surveyed &&
                           same_pilot_tiles(view,centre,sx,sy,density,revisions);
        draw_scrolled(out,view,centre,density,aligned,[&](Draw& d,Point offset) {
            const Rect projection{view.x+offset.x,view.y+offset.y,view.w,view.h};
            const Rect detail_view{target.x+offset.x,target.y+offset.y,target.w,target.h};
            draw_pilot_terrain(d,{projection,centre,sx,sy,density},atlas,game,region,detail,detail_view,opacity);
        });
        mode=Pilot;
        scale_x=sx;
        scale_y=sy;
        detail_region=region;
        detail_bytes=detail.bytes;
        detail_opacity=opacity;
        atlas_revision=revisions;
        charted=coast;
        surveyed=survey;
    }

    void draw_chart(Draw& out,Rect view,Rect world,const Game& game,int density,bool helm,
                    bool reveal_all,bool frame) {
        const int style=int(helm)|(int(reveal_all)<<1)|(int(frame)<<2);
        const bool aligned=mode==Chart && world.w==chart_width && world.h==chart_height &&
                           style==chart_style && (reveal_all || charted==game.s.coast);
        draw_scrolled(out,view,{view.x-world.x,view.y-world.y},density,aligned,[&](Draw& d,Point offset) {
            draw_chart_background(d,{world.x+offset.x,world.y+offset.y,world.w,world.h},
                                  game,helm,reveal_all,frame);
        });
        mode=Chart;
        chart_width=world.w;
        chart_height=world.h;
        chart_style=style;
        charted=game.s.coast;
    }

private:
    bool same_pilot_tiles(Rect view,Point centre,float sx,float sy,int density,
                          const PilotAtlas::Revisions& revisions) const {
        if (revisions==atlas_revision) return true;
        const Point shift{centre.x-anchor.x,centre.y-anchor.y};
        const int left=view.x+std::max(0,-shift.x), right=view.right()-std::max(0,shift.x);
        const int top=view.y+std::max(0,-shift.y), bottom=view.bottom()-std::max(0,shift.y);
        if (left>=right || top>=bottom) return false;
        const PilotRaster projection{view,centre,sx,sy,density};
        const int west=projection.x(left)/(2*PilotPatchWidth), east=projection.x(right-1)/(2*PilotPatchWidth);
        const int north=projection.y(top)/(2*PilotPatchHeight), south=projection.y(bottom-1)/(2*PilotPatchHeight);
        const auto touches=[&](int patch) {
            if (patch<0) return false;
            const int x=patch%PilotTilesX,y=patch/PilotTilesX;
            return x>=west && x<=east && y>=north && y<=south;
        };
        for (std::size_t i=0;i<revisions.size();++i) {
            if (revisions[i]!=atlas_revision[i] &&
                (touches(revisions[i].patch) || touches(atlas_revision[i].patch))) return false;
        }
        return true;
    }

    template<class Paint>
    void draw_scrolled(Draw& out,Rect view,Point centre,int density,bool aligned,Paint paint_pixels) {
        const auto clip=out.clipped(view);
        if (!clip.w || !clip.h) return;
        const auto pixel_count=std::size_t(view.w)*view.h;
        if (!capacity) {
            capacity=BufferPixels*density*density*sizeof(Color);
            bytes.reset(new (std::nothrow) std::byte[capacity]);
        }
        if (!bytes || pixel_count*sizeof(Color)>capacity || clip.w!=view.w || clip.h!=view.h) {
            width=0;
            const auto previous=out.clip;
            out.clip=clip;
            paint_pixels(out,{});
            out.clip=previous;
            return;
        }
        const int stride=view.w*sizeof(Color);
        auto frame=std::span(bytes.get(),pixel_count*sizeof(Color));
        auto pixel_view=pxa::Rgb565Pixels::from_bytes(frame,view.w,view.h,stride);
        if (!pixel_view) return;
        Draw canvas(*pixel_view,frame,stride);
        const auto paint=[&](Rect strip) {
            for (int y=strip.y;y<strip.bottom();) {
                const int row=(y+ring.y)%view.h;
                const int rows=std::min(strip.bottom()-y,view.h-row);
                for (int x=strip.x;x<strip.right();) {
                    const int column=(x+ring.x)%view.w;
                    const int columns=std::min(strip.right()-x,view.w-column);
                    canvas.clip={column,row,columns,rows};
                    paint_pixels(canvas,{column-x-view.x,row-y-view.y});
                    x+=columns;
                }
                y+=rows;
            }
        };
        const Point shift{centre.x-anchor.x,centre.y-anchor.y};
        if (!aligned || width!=view.w || height!=view.h || std::abs(shift.x)>=view.w || std::abs(shift.y)>=view.h) {
            ring={};
            paint({0,0,view.w,view.h});
        } else if (shift.x || shift.y) {
            // Shift the sample origin and fill the exposed edges without moving
            // the overlapping pixels. Each scanline is copied in at most two spans.
            ring.x=(ring.x+shift.x+view.w)%view.w;
            ring.y=(ring.y+shift.y+view.h)%view.h;
            const int left=std::max(0,-shift.x), top=std::max(0,-shift.y);
            const int right=view.w-std::max(0,shift.x), bottom=view.h-std::max(0,shift.y);
            paint({0,0,view.w,top});
            paint({0,bottom,view.w,view.h-bottom});
            paint({0,top,left,bottom-top});
            paint({right,top,view.w-right,bottom-top});
        }
        width=view.w;
        height=view.h;
        anchor=centre;
        const int wrap=ring.x*sizeof(Color);
        const int first=stride-wrap;
        for (int y=0,row=ring.y;y<view.h;++y) {
            const auto* source=frame.data()+row*stride;
            auto* dest=out.frame_bytes.data()+(view.y+y)*out.stride_bytes+view.x*sizeof(Color);
            std::memcpy(dest,source+wrap,first);
            if (wrap) std::memcpy(dest+first,source,wrap);
            if (++row==view.h) row=0;
        }
    }

    static constexpr std::size_t BufferPixels=64*1024;
    std::unique_ptr<std::byte[]> bytes;
    std::size_t capacity=0;
    decltype(State::coast) charted{};
    std::array<std::uint32_t,PilotSurveyRows> surveyed{};
    Point anchor{},ring{};
    enum { Empty, Pilot, Chart } mode=Empty;
    float scale_x=0, scale_y=0;
    int width=0,height=0,detail_region=-1,detail_opacity=0;
    int chart_width=0,chart_height=0,chart_style=0;
    const unsigned char* detail_bytes=nullptr;
    PilotAtlas::Revisions atlas_revision{};
};

} // namespace tide
