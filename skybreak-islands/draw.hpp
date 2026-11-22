#pragma once
#include "world.hpp"

namespace sky {

struct Rect {
    int x=0,y=0,w=0,h=0;
    bool contains(int a,int b) const { return a>=x && a<x+w && b>=y && b<y+h; }
};
struct Point { int x=0,y=0; };
struct Canvas {
    static constexpr int max_pixels=104000, max_width=512;
    Color* pixels;
    std::uint16_t* depth;
    int width,height,stride;

    void pixel(int x,int y,Color c) {
        if (unsigned(x)<unsigned(width) && unsigned(y)<unsigned(height))
            pixels[y*stride+x]=c;
    }
    void zpixel(int x,int y,float z,Color c,int opacity=32) {
        if (unsigned(x)>=unsigned(width) || unsigned(y)>=unsigned(height) || z<4 || z>=8191) return;
        auto& current=depth[y*width+x];
        const auto d=std::uint16_t(z*8);
        if (d>current) return;
        auto& pixel=pixels[y*stride+x];
        if (opacity==32) { current=d; pixel=c; }
        else pixel=blend(pixel,c,opacity);
    }
    void shade_ground(int x,int y,float z,Color color,int opacity) {
        const auto ground=depth[y*width+x];
        if (ground==65535 || std::abs(ground-z*8)>std::max(104.f,z*.78f)) return;
        auto& pixel=pixels[y*stride+x];
        pixel=blend(pixel,color,opacity);
    }
    void fill(Rect rect,Color c) {
        int left=std::clamp(rect.x,0,width), right=std::clamp(rect.x+rect.w,0,width);
        for (int y=std::max(0,rect.y);y<std::min(height,rect.y+rect.h);++y)
            std::fill(pixels+y*stride+left,pixels+y*stride+right,c);
    }
    void tint(Rect rect,Color c,int alpha) {
        int left=std::clamp(rect.x,0,width), right=std::clamp(rect.x+rect.w,0,width);
        for (int y=std::max(0,rect.y);y<std::min(height,rect.y+rect.h);++y)
            for (int x=left;x<right;++x) pixels[y*stride+x]=blend(pixels[y*stride+x],c,alpha);
    }
    void line(Point a,Point b,Color c) {
        const int dx=std::abs(b.x-a.x), sx=a.x<b.x?1:-1;
        const int dy=-std::abs(b.y-a.y), sy=a.y<b.y?1:-1;
        int err=dx+dy;
        for (;;) {
            pixel(a.x,a.y,c);
            if (a.x==b.x && a.y==b.y) break;
            const int e=2*err;
            if (e>=dy) { err+=dy; a.x+=sx; }
            if (e<=dx) { err+=dx; a.y+=sy; }
        }
    }
    void frame(Rect r,Color c) {
        fill({r.x,r.y,r.w,1},c);
        fill({r.x,r.y+r.h-1,r.w,1},c);
        fill({r.x,r.y,1,r.h},c);
        fill({r.x+r.w-1,r.y,1,r.h},c);
    }
    void disk(Point center,int radius,Color c,int alpha=32) {
        for (int y=-radius;y<=radius;++y) {
            const int edge=int(std::sqrt(float(radius*radius-y*y)));
            tint({center.x-edge,center.y+y,2*edge+1,1},c,alpha);
        }
    }
};

struct Camera {
    Vec pos;
    Basis axes;
    float focal, cx, horizon;
    bool overhead=false;
    Vec local(Vec p) const {
        const auto d=p-pos;
        return {dot(d,axes.right),dot(d,axes.up),dot(d,axes.front)};
    }
    float scale_at(float depth) const { return overhead ? focal : focal/depth; }
    Vec project(Vec p) const {
        const float scale=scale_at(p.z);
        return {cx+scale*p.x,horizon-scale*p.y,p.z};
    }
};

struct GroundScan {
    static constexpr int fractional_bits=11;
    static constexpr float units=1024.f;
    int x,z,step_x,step_z;

    GroundScan(Vec start,Vec step)
        : x(int(std::floor(World::wrap(start.x)*units))),
          z(int(std::floor(World::wrap(start.z)*units))),
          step_x(int(std::round(step.x*units))),step_z(int(std::round(step.z*units))) {}
    void advance() { x+=step_x; z+=step_z; }
    int u() const { return x>>fractional_bits; }
    int v() const { return z>>fractional_bits; }
};

struct SlopeSteps {
    std::uint16_t color, reciprocal;
};
inline constexpr auto slope_steps=[] {
    std::array<SlopeSteps,65> result{};
    for (unsigned span=2;span<result.size();++span)
        result[span]={std::uint16_t(8192/span),std::uint16_t(65536/span)};
    return result;
}();

struct Scene {
    Canvas& out;
    Camera camera;
    static constexpr float near=4.f;
    Color fog=rgb(0xa0c8d6);
    static int land_detail(float depth) {
        return int(32.f*saturate((560.f-depth)/380.f));
    }

    void terrain_overhead(const World& world,float time,const auto& shadow) {
        const auto right=camera.axes.right*(1/camera.focal);
        const auto down=camera.axes.up*(-1/camera.focal);
        const auto corner=camera.pos+right*(.5f-camera.cx)+down*(.5f-camera.horizon);
        const auto waves=WaterMotion::at(time);
        const int detail=int(32.f*saturate(camera.focal*1.45f));
        for (int y=0;y<out.height;++y) {
            GroundScan scan(corner+down*float(y),right);
            for (int x=0;x<out.width;++x,scan.advance()) {
                const int u=scan.u(),v=scan.v();
                const int coast=World::coast_index(u,v);
                const int level=world.relief_map[coast];
                const auto surface=world.sample(u,v,coast,level);
                auto color=surface.inland() ? world.inland_texel(u,v,surface,waves,detail)
                    : surface.altitude ? world.surface_color(u,v,surface.height(),detail)
                                       : world.water_texel(u,v,coast,level,waves,7);
                const int shade=shadow.sample(scan.x,surface.altitude,scan.z);
                if (shade) color=blend(color,rgb(0x183d59),shade);
                out.pixels[y*out.stride+x]=color;
                out.depth[y*out.width+x]=std::uint16_t(camera.pos.y*8-surface.altitude*2);
            }
        }
    }

    void terrain(const World& world,float time,const auto& shadow) {
        if (camera.overhead) {
            terrain_overhead(world,time,shadow);
            return;
        }
        std::fill_n(out.depth,out.width*out.height,std::uint16_t(65535));
        std::array<int,Canvas::max_width> top;
        std::fill_n(top.begin(),out.width,out.height);
        const float inverse_focal=1.f/camera.focal;
        const auto across=camera.axes.right*inverse_focal;
        const auto ray=camera.axes.front+across*(.5f-camera.cx);
        const auto waves=WaterMotion::at(time);
        const float first=std::max(near,camera.focal*(camera.pos.y-World::highest_land)/
                                         std::max(1.f,out.height-camera.horizon));
        constexpr float far=3420;
        int land_top=out.height;
        // Raised surfaces use the heightfield. Sea samples use the exact plane
        // intersection later. Reuse the same depth bands as altitude changes.
        const auto next_depth=[](float z) {
            // Additional near samples keep a single ground texel from stretching down a slope.
            if (z<164.f) return z+std::max(3.5f,z*.043f);
            return z+std::max(7.f,z*.069f);
        };
        float start_depth=near;
        while (next_depth(start_depth)<first) start_depth=next_depth(start_depth);
        for (float z=start_depth;z<far;z=next_depth(z)) {
            const float projected=camera.focal/z;
            const int tide=std::clamp(int(camera.horizon+camera.pos.y*projected+.75f),0,out.height);
            if (tide==0) break;
            const auto start=camera.pos+ray*z;
            const auto delta=across*z;
            const auto depth=std::uint16_t(z*8);
            const int haze=int(saturate((z-430)/3020.f)*29);
            const int detail=land_detail(z);
            const int water_detail=int(32.f*saturate((1050.f-z)/520.f));
            const bool smooth_slope=z<540.f;
            const int tolerance=std::max(116,int(z*.96f));
            const int highest=std::max(0,int(camera.horizon+(camera.pos.y-World::highest_land)*projected));
            GroundScan scan(start,delta);
            for (int x=0;x<out.width;++x,scan.advance()) {
                const int bottom=std::min(top[x],tide);
                if (bottom<=highest) continue;
                const int u=scan.u(),v=scan.v(),coast=World::coast_index(u,v);
                const auto surface=world.sample(u,v,coast,world.relief_map[coast]);
                if (!surface.altitude) continue;
                const float screen_y=camera.horizon+(camera.pos.y-surface.height())*projected;
                if (screen_y>=bottom) continue;
                const int y0=std::max(0,int(screen_y));
                auto color=surface.inland() ? world.inland_texel(u,v,surface,waves,water_detail)
                                            : world.surface_color(u,v,surface.height(),detail);
                const int shade=z<1850.f ? shadow.sample(scan.x,surface.altitude,scan.z) : 0;
                if (shade) color=blend(color,rgb(0x244358),shade);
                if (haze) color=blend(color,fog,haze);
                // Connect nearby samples along a slope to soften marching bands.
                const int span=bottom-y0;
                const auto nearer=smooth_slope && !surface.inland() && span>1 && bottom<out.height
                    ? out.depth[bottom*out.width+x] : std::uint16_t(65535);
                if (surface.inland() && !surface.flowing && span>1 && z<860.f &&
                    camera.pos.y>surface.height()+2.f) {
                    // Project the ends of this short span onto the lake plane,
                    // then step through its texels without stretching a march sample.
                    const float numerator=camera.focal*(camera.pos.y-surface.height());
                    const Vec direction=ray+across*float(x);
                    float distance=std::min(z*1.08f,numerator/std::max(.5f,y0+.5f-camera.horizon));
                    const float step=(numerator/std::max(.5f,bottom-.5f-camera.horizon)-distance)/(span-1);
                    const Vec advance=direction*step;
                    Vec point=camera.pos+direction*distance;
                    for (int y=y0;y<bottom;y+=2,point+=advance*2,distance+=step*2) {
                        const int su=World::tile(point.x*8),sv=World::tile(point.z*8);
                        const int at=World::coast_index(su,sv);
                        const int level=world.relief_map[at];
                        const int bank=world.terrain_features[World::index((su+2)>>3,(sv+2)>>3)];
                        Color pixel=color;
                        auto pixel_depth=depth;
                        if (level&World::inland_margin && (bank>>2)*4==surface.altitude && !(bank&World::river_flag)) {
                            const int bed=world.contour_level(su,sv,at,level)-World::shoreline*4;
                            if (const int lake_depth=surface.altitude-bed; lake_depth>0) {
                                pixel=world.inland_texel(su,sv,{surface.altitude,lake_depth},waves,water_detail);
                                if (shade) pixel=blend(pixel,rgb(0x244358),shade);
                                if (haze) pixel=blend(pixel,fog,haze);
                                pixel_depth=std::uint16_t(std::clamp(distance,near,far)*8);
                            }
                        }
                        out.depth[y*out.width+x]=pixel_depth;
                        out.pixels[y*out.stride+x]=pixel;
                        if (y+1<bottom) {
                            out.depth[(y+1)*out.width+x]=pixel_depth;
                            out.pixels[(y+1)*out.stride+x]=pixel;
                        }
                    }
                } else if (nearer<depth && depth-nearer<=tolerance) {
                    const auto near_color=out.pixels[bottom*out.stride+x];
                    const int depth_difference=depth-nearer;
                    const bool cached=span<int(slope_steps.size());
                    const int color_step=cached ? slope_steps[span].color : 8192/span;
                    const int depth_step=cached ? (depth_difference*slope_steps[span].reciprocal)>>8
                                                : depth_difference*256/span;
                    int weight=color_step/2,distance=depth*256-depth_step/2;
                    for (int y=y0;y<bottom;++y,weight+=color_step,distance-=depth_step) {
                        out.depth[y*out.width+x]=std::uint16_t(distance>>8);
                        out.pixels[y*out.stride+x]=blend(color,near_color,weight>>8);
                    }
                } else {
                    for (int y=y0;y<bottom;++y) {
                        out.depth[y*out.width+x]=depth;
                        out.pixels[y*out.stride+x]=color;
                    }
                }
                top[x]=y0;
                land_top=std::min(land_top,y0);
            }
        }

        constexpr Color zenith=rgb(0x3c80b5),mid=rgb(0x7fbbd9),skyline=rgb(0xd2e4dc);
        const Vec sunshine=unit({-shadow_x,0,-shadow_z});
        const float facing=dot(camera.axes.front,sunshine);
        const int light_x=facing>.17f
            ? int(camera.cx+camera.focal*dot(camera.axes.right,sunshine)/facing) : -out.width;
        const float falloff=22.f/(out.width*.28f+1);
        std::array<std::uint8_t,Canvas::max_width> reflection{};
        if (facing>.17f) {
            for (int x=0;x<out.width;++x)
                reflection[x]=std::uint8_t(std::max(0.f,22.f-std::abs(x-light_x)*falloff));
        }
        const float sky_span=1.f/std::max(16.f,camera.horizon+13.f);
        for (int y=0;y<out.height;++y) {
            const float angle=y+.5f-camera.horizon;
            const float z=angle>0 ? camera.pos.y*camera.focal/angle : far+1;
            auto* row=out.pixels+y*out.stride;
            auto* depths=out.depth+y*out.width;
            if (z>=far || z<near) {
                const float t=saturate(y*sky_span);
                const float weight=(t<.5f ? t : t-.5f)*64;
                const auto a=t<.5f ? zenith : mid, b=t<.5f ? mid : skyline;
                const int level=int(weight);
                const Color s0=blend(a,b,level), s1=blend(a,b,std::min(32,level+1));
                const bool alternate=weight-level>(y&1 ? .54f : .29f);
                if (y<land_top && !alternate) std::fill_n(row,out.width,s0);
                else if (y<land_top) {
                    for (int x=0;x<out.width;++x) row[x]=((x+y)&1) ? s1 : s0;
                } else {
                    for (int x=0;x<out.width;++x)
                        if (depths[x]==65535) row[x]=alternate && ((x+y)&1) ? s1 : s0;
                }
                continue;
            }
            const auto base=camera.pos+ray*z, delta=across*z;
            const auto depth=std::uint16_t(z*8);
            const int haze=int(saturate((z-510)/3100.f)*29);
            const int sea_detail=int(32.f*saturate((1480.f-z)/372.f));
            const int land_grain=land_detail(z);
            const bool shadows=z<1720.f;
            GroundScan scan(base,delta);
            for (int x=0;x<out.width;++x,scan.advance()) {
                if (depths[x]<=depth) continue;
                const int u=scan.u(),v=scan.v();
                const int coast=World::coast_index(u,v);
                const int level=world.relief_map[coast];
                const auto surface=world.sample(u,v,coast,level);
                // Keep the rising shoreline where discrete heightfield samples
                // and the exact sea intersection fall on opposite sides.
                if (surface.altitude && depths[x]!=65535) continue;
                Color color;
                if (surface.inland()) color=world.inland_texel(u,v,surface,waves,sea_detail);
                else if (surface.altitude) color=world.surface_color(u,v,surface.height(),land_grain);
                else if (sea_detail==32) color=world.water_texel(u,v,coast,level,waves,reflection[x]);
                else {
                    color=world.distant_water(level);
                    if (sea_detail)
                        color=blend(color,world.water_texel(u,v,coast,level,waves,reflection[x]),sea_detail);
                }
                if (shadows) {
                    const int shade=shadow.sample(scan.x,surface.altitude,scan.z);
                    if (shade) color=blend(color,rgb(0x193957),shade);
                }
                if (haze) color=blend(color,fog,haze);
                depths[x]=depth;
                row[x]=color;
            }
        }
    }

    void raster(Vec a,Vec b,Vec c,Color color,int opacity) {
        const float min_x=std::min({a.x,b.x,c.x}),max_x=std::max({a.x,b.x,c.x});
        const float min_y=std::min({a.y,b.y,c.y}),max_y=std::max({a.y,b.y,c.y});
        if (max_x<0 || max_y<0 || min_x>=out.width || min_y>=out.height) return;
        const float area=(b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);
        if (std::abs(area)<.07f) return;
        const int left=int(std::clamp(min_x,0.f,float(out.width-1)));
        const int right=int(std::clamp(max_x,0.f,float(out.width-1)));
        const int top=int(std::clamp(min_y,0.f,float(out.height-1)));
        const int bottom=int(std::clamp(max_y,0.f,float(out.height-1)));
        const float inverse=1.f/area;
        const float dx=(b.y-c.y)*inverse, dy=(c.x-b.x)*inverse;
        const float ex=(c.y-a.y)*inverse, ey=(a.x-c.x)*inverse;
        const float az=camera.overhead ? a.z : 1/a.z;
        const float bz=camera.overhead ? b.z : 1/b.z;
        const float cz=camera.overhead ? c.z : 1/c.z;
        float s=(left+.5f-b.x)*dx+(top+.5f-b.y)*dy;
        float t=(left+.5f-c.x)*ex+(top+.5f-c.y)*ey;
        for (int y=top;y<=bottom;++y,s+=dy,t+=ey) {
            float u=s,v=t;
            for (int x=left;x<=right;++x,u+=dx,v+=ex) {
                const float w=1-u-v;
                if (u>=0 && v>=0 && w>=0) {
                    const float z=az*u+bz*v+cz*w;
                    const float depth=camera.overhead ? z : 1/z;
                    if (opacity==32) out.zpixel(x,y,depth,color);
                    else out.shade_ground(x,y,depth,color,opacity);
                }
            }
        }
    }
    void triangle_local(Vec a,Vec b,Vec c,Color color,int opacity=32) {
        const std::array<Vec,3> vertices{a,b,c};
        if (vertices[0].z>=near && vertices[1].z>=near && vertices[2].z>=near) {
            raster(camera.project(vertices[0]),camera.project(vertices[1]),camera.project(vertices[2]),color,opacity);
            return;
        }
        if (vertices[0].z<near && vertices[1].z<near && vertices[2].z<near) return;
        std::array<Vec,4> polygon;
        int count=0;
        for (int i=0;i<3;++i) {
            const auto first=vertices[i],second=vertices[(i+1)%3];
            if (first.z>=near) polygon[count++]=camera.project(first);
            if ((first.z>=near)!=(second.z>=near)) {
                auto p=first+(second-first)*((near-first.z)/(second.z-first.z));
                p.z=near;
                polygon[count++]=camera.project(p);
            }
        }
        for (int i=1;i+1<count;++i) raster(polygon[0],polygon[i],polygon[i+1],color,opacity);
    }
    void triangle(Vec a,Vec b,Vec c,Color color,int opacity=32) {
        triangle_local(camera.local(a),camera.local(b),camera.local(c),color,opacity);
    }
    void quad(Vec a,Vec b,Vec c,Vec d,Color color,int opacity=32) {
        triangle(a,b,c,color,opacity); triangle(a,c,d,color,opacity);
    }
    void stroke(Vec from,Vec to,Color c,int weight=1,int opacity=32) {
        Vec a=camera.local(from),b=camera.local(to);
        if (a.z<near && b.z<near) return;
        if (a.z<near) a=a+(b-a)*((near-a.z)/(b.z-a.z));
        if (b.z<near) b=b+(a-b)*((near-b.z)/(a.z-b.z));
        a=camera.project(a); b=camera.project(b);
        const float dx=b.x-a.x, dy=b.y-a.y;
        float begin=0,end=1;
        // Clip in screen space before stepping, including near-plane crossings.
        const auto clip=[&](float origin,float slope,float limit) {
            if (std::abs(slope)<.0001f) return origin>=0 && origin<=limit;
            const float low=-origin/slope,high=(limit-origin)/slope;
            begin=std::max(begin,std::min(low,high));
            end=std::min(end,std::max(low,high));
            return begin<=end;
        };
        if (!clip(a.x,dx,float(out.width-1)) || !clip(a.y,dy,float(out.height-1))) return;
        const int steps=std::max(1,int(std::max(std::abs(dx),std::abs(dy))*(end-begin)));
        for (int i=0;i<=steps;++i) {
            const float t=mix(begin,end,float(i)/steps);
            const int x=int(a.x+dx*t),y=int(a.y+dy*t);
            const float z=camera.overhead ? mix(a.z,b.z,t) : 1/mix(1/a.z,1/b.z,t);
            out.zpixel(x,y,z,c,opacity);
            if (weight>1) out.zpixel(x+1,y,z,c,opacity);
        }
    }
};

} // namespace sky
