// SPDX-License-Identifier: MIT
// Native contact sheet; not a simulator or marketplace screenshot.
#include "art.hpp"
#include <fstream>
#include <string>
#include <vector>
using namespace iron;
int main(int argc,char** argv){if(argc!=2)return 2;constexpr int w=1024,h=1040;std::vector<uint16_t> pixels(w*h);Raster r(pixels.data(),w,h,w);for(int y=0;y<h;++y)for(int x=0;x<w;++x)pixels[y*w+x]=art::ground_material[art::ground_texels[(y&255)*256+(x&255)]];
 for(int kind=tank;kind<=harvester;++kind)for(int pose=0;pose<2;++pose)for(unsigned face=0;face<8;++face){int x=int(face)*128+64,y=((kind-tank)*2+pose)*110+85;unsigned id=art::unit_first[kind]+pose*8+face;ShadowArt::base(r,id,x,y,1);Sprites::anchor(r,id,x,y);r.text(face*128+4,y-80,std::to_string(kind)+" / "+std::to_string(face)+" / "+std::to_string(pose),cream,12);}
 for(unsigned k=0;k<8;++k){int x=k*128+64,y=785;unsigned id=art::buildings_first+k;ShadowArt::base(r,id,x,y,1,true);if(k==5)TurretArt::draw(r,x,y,1,0,false);else Sprites::anchor(r,id,x,y);}
 for(unsigned k=0;k<8;++k){int x=k*128+64,y=885;unsigned id=art::environment_first+k;ShadowArt::base(r,id,x,y,1);Sprites::anchor(r,id,x,y);}
 for(int k=0;k<2;++k)for(unsigned face=0;face<8;++face){int x=face*128+64,y=950+k*65;ShadowArt::infantry(r,k,face,x,y,1,.125f,1);CombatArt::infantry(r,k,face,x,y,1,.125f,1,0);}
 std::ofstream f(argv[1],std::ios::binary);f<<"P6\n"<<w<<" "<<h<<"\n255\n";for(auto p:pixels){char c[]={char((p>>11)*255/31),char(((p>>5)&63)*255/63),char((p&31)*255/31)};f.write(c,3);}return !f;
}
