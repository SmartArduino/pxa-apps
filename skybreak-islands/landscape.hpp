#pragma once
#include "draw.hpp"

namespace sky {

struct GroundLight {
    const std::array<std::uint8_t,World::map_size*World::map_size>& shadow;
    int offset_x,offset_z;
    GroundLight(decltype(shadow) map,float wind_x,float wind_z)
        : shadow(map),offset_x(int((wind_x+World::cell*.5f)*GroundScan::units)),
          offset_z(int((wind_z+World::cell*.5f)*GroundScan::units)) {}

    int sample(int x,int altitude_quarters,int z) const {
        constexpr int tile_bits=14,weights=64;
        constexpr int sun_x=int(shadow_x*GroundScan::units*.25f+.5f);
        constexpr int sun_z=int(shadow_z*GroundScan::units*.25f+.5f);
        static_assert((1<<tile_bits)==World::cell*GroundScan::units);
        const int sx=x-offset_x+altitude_quarters*sun_x;
        const int sz=z-offset_z+altitude_quarters*sun_z;
        const int tx=sx>>tile_bits,tz=sz>>tile_bits;
        const int shade=shadow[World::index(tx,tz)];
        const int east=shadow[World::index(tx+1,tz)];
        const int north=shadow[World::index(tx,tz+1)];
        const int corner=shadow[World::index(tx+1,tz+1)];
        if ((shade|east|north|corner)<9) return 0;
        const int fx=(sx>>(tile_bits-6))&(weights-1),fz=(sz>>(tile_bits-6))&(weights-1);
        const int a=shade*(weights-fx)+east*fx,b=north*(weights-fx)+corner*fx;
        return std::min(15,(a*(weights-fz)+b*fz)/(10*weights*weights));
    }
    int operator()(float x,float altitude,float z) const {
        return sample(int(World::wrap(x)*GroundScan::units),int(altitude*4),
                      int(World::wrap(z)*GroundScan::units));
    }
};

void draw_woodland(Scene& scene,const World& world,const GroundLight& light);

} // namespace sky
