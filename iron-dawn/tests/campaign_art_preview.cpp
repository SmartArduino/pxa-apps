// SPDX-License-Identifier: MIT
// Native art review sheet; not a simulator/marketplace screenshot.
#include "art.hpp"
#include <fstream>
#include <vector>
using namespace iron;
int main(int argc,char** argv){if(argc!=2)return 2;constexpr int w=720,h=840;std::vector<uint16_t> pixels(w*h,navy);Raster r(pixels.data(),w,h,w);for(int y=0;y<650;++y)for(int x=0;x<w;++x)pixels[y*w+x]=art::ground_material[art::ground_texels[(y&255)*256+(x&255)]];
 for(int kind=power;kind<=turret;++kind)for(int stage=0;stage<5;++stage){unsigned id=stage==4?art::buildings_first+kind-hq:art::construction_first+(kind-power)*4+stage;int x=stage*144+72,y=(kind-power)*125+108;ShadowArt::base(r,id,x,y,1,true);if(stage==4&&kind==turret)TurretArt::draw(r,x,y,1,0,false);else Sprites::anchor(r,id,x,y);}
 for(unsigned i=0;i<4;++i)Sprites::fit(r,art::story_portraits_first+i,{12+int(i)*112,665,100,100});for(unsigned i=0;i<2;++i)Sprites::fit(r,art::story_standing_first+i,{470+int(i)*118,655,100,180});
 std::ofstream f(argv[1],std::ios::binary);f<<"P6\n"<<w<<" "<<h<<"\n255\n";for(auto p:pixels){char c[]={char((p>>11)*255/31),char(((p>>5)&63)*255/63),char((p&31)*255/31)};f.write(c,3);}return !f;
}
