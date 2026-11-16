// SPDX-License-Identifier: MIT
#include "art.hpp"
#include <fstream>
#include <string>
#include <vector>
using namespace iron;
void save(const std::string& path,const std::vector<uint16_t>& pixels,int w,int h){std::ofstream f(path,std::ios::binary);f<<"P6\n"<<w<<" "<<h<<"\n255\n";for(auto p:pixels){char rgb[]={char((p>>11)*255/31),char(((p>>5)&63)*255/63),char((p&31)*255/31)};f.write(rgb,3);}}
int main(int argc,char** argv){if(argc!=2)return 2;std::string prefix=argv[1];constexpr int cellw=64,cellh=64,w=cellw*8,h=cellh*8;for(int kind=0;kind<2;++kind){std::vector<uint16_t> pixels(w*h,navy);Raster r(pixels.data(),w,h,w);for(unsigned direction=0;direction<8;++direction)for(int phase=0;phase<8;++phase){int x=phase*cellw+32,y=direction*cellh+49;r.line(phase*cellw,y+2,phase*cellw+cellw-1,y+2,edge);CombatArt::infantry(r,kind,direction,x,y,1,phase/8.f,1,0);}save(prefix+(kind?"-rocket.ppm":"-rifle.ppm"),pixels,w,h);}
 {int ww=8*100,hh=4*110;std::vector<uint16_t> pixels(ww*hh,navy);Raster r(pixels.data(),ww,hh,ww);for(int k=0;k<4;++k)for(unsigned d=0;d<8;++d){int x=d*100+50,y=k*110+90;if(k<2)CombatArt::infantry(r,k,d,x,y,1,0,0,0);else Sprites::anchor(r,art::unit_first[k]+d,x,y);auto p=CombatArt::socket(k,d,x,y,1);r.line(p.x-3,p.y,p.x+3,p.y,red);r.line(p.x,p.y-3,p.x,p.y+3,red);}save(prefix+"-muzzles.ppm",pixels,ww,hh);}
}
