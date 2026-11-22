#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace sky {

constexpr float pi = 3.14159265f;
constexpr float shadow_x=.34f,shadow_z=.25f;
struct Vec {
    float x = 0, y = 0, z = 0;
    Vec operator+(Vec b) const { return {x+b.x, y+b.y, z+b.z}; }
    Vec operator-(Vec b) const { return {x-b.x, y-b.y, z-b.z}; }
    Vec operator*(float s) const { return {x*s, y*s, z*s}; }
    Vec& operator+=(Vec b) { return *this = *this + b; }
};
inline float dot(Vec a, Vec b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
inline float norm2(Vec v) { return dot(v,v); }
inline Vec unit(Vec v) { return v * (1.f / std::sqrt(std::max(.0001f, norm2(v)))); }
inline float mix(float a, float b, float t) { return a + (b-a)*t; }
inline float saturate(float v) { return std::clamp(v,0.f,1.f); }

struct Basis {
    Vec right, up, front;
    Vec apply(Vec p) const { return right*p.x + up*p.y + front*p.z; }
};
inline Basis attitude(float yaw, float pitch = 0, float roll = 0) {
    const float s = std::sin(yaw), c = std::cos(yaw);
    const float p = std::sin(pitch), q = std::cos(pitch);
    const float r = std::sin(roll), t = std::cos(roll);
    const Vec right{c,0,-s}, up{-s*p,q,-c*p};
    return {right*t + up*r, up*t - right*r, {s*q,p,c*q}};
}

using Color = std::uint16_t;
constexpr Color rgb(unsigned hex) {
    return Color(((hex >> 8)&0xf800) | ((hex >> 5)&0x07e0) | ((hex >> 3)&0x001f));
}
inline Color blend(Color a, Color b, int t) {
    const unsigned inv = 32-t;
    const unsigned rb = (((a&0xf81f)*inv + (b&0xf81f)*t) >> 5)&0xf81f;
    const unsigned g = (((a&0x07e0)*inv + (b&0x07e0)*t) >> 5)&0x07e0;
    return Color(rb | g);
}
constexpr std::uint32_t hash(std::uint32_t x, std::uint32_t z) {
    std::uint32_t h = x*0x9e3779b1u ^ z*0x85ebca6bu;
    h ^= h >> 16;
    h *= 0x7feb352du;
    return h ^ (h >> 15);
}

struct WaterMotion {
    int swell_x,swell_z,cross_x,cross_z,shore;

    static WaterMotion at(float seconds) {
        const int tick=int(seconds*16.f)&4095;
        return {tick>>1,tick>>3,-(tick>>2),tick>>1,tick>>1};
    }
};

struct World {
    static constexpr int map_size = 256, cell = 16;
    static constexpr float period = map_size * cell;
    static constexpr int highest_land = 46;
    // The cover byte stores forest in bit 0, river in bit 1 and waterline in meters above them.
    static constexpr std::uint8_t forest_cover=1;
    static constexpr int river_flag=2;
    static constexpr int sea_size=map_size*2;
    std::array<std::uint8_t,map_size*map_size> heights{};
    std::array<Color,map_size*map_size> colors{};
    std::array<std::uint8_t,map_size*map_size> terrain_features{};
    static constexpr int shoreline=63;
    static constexpr int inland_margin=128;
    static constexpr int bed_mask=inland_margin-1;
    std::array<std::uint8_t,sea_size*sea_size> relief_map{};
    static constexpr int wave_size=128;
    std::array<std::uint8_t,wave_size*wave_size> water_texture{};
    std::array<std::array<Color,24>,64> water_palette{};

    struct Surface {
        int altitude=0, freshwater_depth=0;
        bool flowing=false;
        bool inland() const { return freshwater_depth>0; }
        bool water() const { return !altitude || inland(); }
        float height() const { return altitude*.25f; }
    };

    static int index(int x, int z) {
        return (z & (map_size-1))*map_size + (x & (map_size-1));
    }
    static int tile(float x) { return int(x / cell + 4096.f) - 4096; }
    static int sea_index(int x,int z) {
        return (z&(sea_size-1))*sea_size+(x&(sea_size-1));
    }
    int tile_at(float x,float z) const { return index(tile(x),tile(z)); }
    bool forest(int at) const { return (terrain_features[at]&forest_cover)!=0; }
    static int coast_index(int u,int v) { return sea_index((u-2)>>2,(v-2)>>2); }
    static int wave_index(int u,int v) {
        return (v&(wave_size-1))*wave_size+(u&(wave_size-1));
    }
    Color surface_color(int u,int v,float altitude,int detail) const {
        const int x=(u-4)>>3,z=(v-4)>>3;
        const int fx=((u-4)&7)*4,fz=((v-4)&7)*4;
        const Color a=blend(colors[index(x,z)],colors[index(x+1,z)],fx);
        const Color b=blend(colors[index(x,z+1)],colors[index(x+1,z+1)],fx);
        const Color base=blend(a,b,fz);
        if (altitude<4.5f) {
            const int moisture=std::clamp(int(24-altitude*6.f),0,19);
            return blend(base,rgb(0x928373),moisture);
        }
        if (!detail) return base;
        const int texel_mask=map_size*cell/2-1;
        const int grain_x=u&texel_mask,grain_z=v&texel_mask;
        const auto grain=hash(grain_x,grain_z);
        const auto patch=hash(((grain_x+(grain_z>>2))&texel_mask)>>2,grain_z>>2);
        if (forest(index(u>>3,v>>3))) {
            return (patch&7)<3 ? blend(base,rgb(0x9aba6d),(6+int(grain&3))*detail/32)
                               : blend(base,rgb(0x1f4f41),int((grain>>8)&7)*detail/32);
        }
        if ((patch&31)<5) return blend(base,rgb(0xdeca8c),(7+int((grain>>7)&7))*detail/32);
        if ((grain&127)==11) return blend(base,rgb(0xf1dcab),12*detail/32);
        return (grain&31)<5 ? blend(base,rgb(0x416245),7*detail/32) : base;
    }
    Color distant_water(int level) const {
        return water_palette[std::max(0,shoreline-(level&bed_mask))][8];
    }
    int contour_level(int u,int v,int coast,int level) const {
        const int x=(u-2)&3,z=(v-2)&3;
        const int east=(coast&~(sea_size-1))|((coast+1)&(sea_size-1));
        const int below=(coast+sea_size)&(sea_size*sea_size-1);
        const int diagonal=(east+sea_size)&(sea_size*sea_size-1);
        const int top=(level&bed_mask)*(4-x)+(relief_map[east]&bed_mask)*x;
        const int bottom=(relief_map[below]&bed_mask)*(4-x)+(relief_map[diagonal]&bed_mask)*x;
        return (top*(4-z)+bottom*z+2)>>2;
    }
    int surf_depth(int u,int v,int coast,int level) const {
        return std::max(0,shoreline*4-contour_level(u,v,coast,level));
    }
    Surface sample(int u,int v,int coast,int level) const {
        if (level<shoreline-12) return {};
        const int bed=contour_level(u,v,coast,level)-shoreline*4;
        if (level&inland_margin) {
            const int x=(u-2)>>3,z=(v-2)>>3;
            const int fx=(u-2)&7,fz=(v-2)&7;
            const int a=terrain_features[index(x,z)],b=terrain_features[index(x+1,z)];
            const int c=terrain_features[index(x,z+1)],d=terrain_features[index(x+1,z+1)];
            const int north=(a>>2)*(8-fx)+(b>>2)*fx;
            const int south=(c>>2)*(8-fx)+(d>>2)*fx;
            const int waterline=(north*(8-fz)+south*fz+8)>>4;
            if (waterline>0 && waterline>bed) {
                const int nearest=fz<4 ? (fx<4 ? a : b) : (fx<4 ? c : d);
                return {waterline,waterline-bed,(nearest&river_flag)!=0};
            }
        }
        return {std::max(0,bed)};
    }
    Surface surface_at(float x,float z) const {
        const int u=tile(x*8),v=tile(z*8),coast=coast_index(u,v);
        return sample(u,v,coast,relief_map[coast]);
    }
    float visual_height(float x,float z) const { return surface_at(x,z).height(); }
    Color inland_texel(int u,int v,Surface s,const WaterMotion& motion,int detail) const {
        const auto shallow=s.flowing ? rgb(0x78b6a8) : rgb(0x87bda2);
        const auto deep=s.flowing ? rgb(0x277d8d) : rgb(0x246f66);
        auto color=blend(shallow,deep,std::min(32,s.freshwater_depth*(s.flowing ? 2 : 1)));
        if (!detail) return color;
        // The lake catches small wind ripples. Long, broken streaks read as moving water in the river.
        const int shift=s.flowing ? motion.swell_x : motion.swell_z;
        const int noise=water_texture[wave_index((s.flowing ? u/2 : u)-shift,v+motion.cross_x/2)];
        const int wave=(noise&15)-7,cross=(noise>>4)-7;
        const int glimmer=std::max(0,wave+cross/2-3);
        if (s.freshwater_depth<13) {
            const int bank=std::max(0,10-s.freshwater_depth)+std::max(0,2+cross);
            color=blend(color,rgb(0xb1bc85),bank*detail/32);
        }
        if (glimmer) {
            const int crest=s.flowing && cross>=2 ? glimmer*2 : glimmer;
            color=blend(color,rgb(0xe2f5d5),std::min(24,crest*detail/(s.flowing ? 22 : 31)));
        } else if (wave<-2) color=blend(color,rgb(0x245360),(-wave-2)*detail/22);
        return color;
    }
    Color water_texel(int u,int v,int coast,int level,const WaterMotion& motion,int glint) const {
        const int depth=std::max(0,shoreline-(level&bed_mask));
        const int swell=(water_texture[wave_index(u-motion.swell_x,v-motion.swell_z)]&15)-7;
        const int cross=(water_texture[wave_index(u-motion.cross_x,v-motion.cross_z)]>>4)-7;
        int wave=(swell*2+cross+2)/5;
        if (depth<11) {
            const int shoal=surf_depth(u,v,coast,level);
            const int phase=(shoal+motion.shore+(cross>>1))&31;
            wave=(wave*std::min(16,shoal+5))>>4;
            auto color=water_palette[depth][8+wave];
            // Small highlights show sand through the moving surface in sheltered shallows.
            const int fleck=cross-swell;
            if (shoal>5 && shoal<34 && cross>0) {
                const unsigned sand=hash(unsigned(u>>2)&1023u,unsigned(v>>2)&1023u);
                if ((sand&15)<4) color=blend(color,rgb(0x3d8d83),6);
                if (fleck>=5 && fleck<=8) color=blend(color,rgb(0xd2ecc4),(37-shoal)>>2);
            }
            // Constant depth traces the shore. Advancing the phase moves the
            // broken band into shallower water, even around sheltered bends.
            if (shoal<38 && phase<4 && swell+cross>=0)
                return blend(color,rgb(0xffefe0),7+2*(4-phase)+std::max(0,swell));
            if (shoal>=9 && shoal<46 && phase>=14 && phase<=16 && swell>=4 && cross>0)
                color=blend(color,rgb(0xc3e8df),5+cross);
            return color;
        }
        if ((swell==8)!=(cross==8)) {
            const int support=swell==8 ? cross : swell;
            if (support>=-2 && support<=2) {
                const unsigned fleck=hash(unsigned(u-motion.swell_x)&127u,unsigned(v-motion.swell_z)&127u);
                wave=(fleck&3) ? 8 : 11+(support+2)/2;
            }
        }
        auto color=water_palette[depth][8+wave];
        if (glint && wave>1 && cross>-2) {
            const int light=(glint*(wave-1+std::max(0,cross)/3))>>3;
            if (light) color=blend(color,rgb(0xffe9ba),std::min(23,light));
        }
        return color;
    }
    float ground(Vec p) const { return visual_height(p.x,p.z); }
    static float wrap(float x) { return x - std::floor(x/period + .5f)*period; }

    void generate();
};

} // namespace sky
