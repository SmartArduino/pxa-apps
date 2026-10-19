#include "classic-raster.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>

int main() {
    std::array<uint8_t,65536> tile{};
    std::array<uint8_t,8192> light{};
    std::array<uint16_t,256> palette{};
    for(unsigned i=0;i<light.size();++i)light[i]=i&255;
    palette[1]=0xf800;palette[2]=0x001f;
    std::array<uint16_t,16> a{},b{},za,zb;
    za.fill(65535);zb.fill(65535);
    auto face=[&](std::array<uint16_t,16>&color,std::array<uint16_t,16>&depth,
                  int z,int dz,unsigned index,bool write=true) {
        tile.fill(index);
        ol_classic_depth_span(color.data(),depth.data(),16,z,dz,0,0,0,0,0,0,
            tile.data(),light.data(),palette.data(),write);
    };
    // Intersecting sloped faces must choose the closest pixel regardless of
    // submission order, even when their average distances are equal.
    face(a,za,100000000,10000000,1);face(a,za,255000000,-10000000,2);
    face(b,zb,255000000,-10000000,2);face(b,zb,100000000,10000000,1);
    assert(a==b && za==zb);
    assert(a[0]==palette[1] && a[15]==palette[2]);
    // Transparent foreground texels leave background color and depth intact.
    auto previous=a,previousDepth=za;
    face(a,za,0,0,0);assert(a==previous && za==previousDepth);
    // A depth-read-only pass may draw, but cannot hide later opaque geometry.
    face(a,za,0,0,2,false);assert(za==previousDepth && a[0]==palette[2]);
    face(a,za,1000000,0,1);assert(a[0]==palette[1]);
    // The largest representable projected depth stays below the clear value.
    za.fill(65535);face(a,za,2147418112,0,2);
    assert(za[0]==65534 && a[0]==palette[2]);
    puts("depth: crossing faces, transparency, read-only pass, far depth passed");
}
