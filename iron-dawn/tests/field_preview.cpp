// SPDX-License-Identifier: MIT
#include "art.hpp"
#include "ui.hpp"
#include <fstream>
#include <string>
#include <vector>
using namespace iron;
static void save(const std::string& name,const std::vector<uint16_t>& pixels,int w,int h){std::ofstream f(name,std::ios::binary);f<<"P6\n"<<w<<" "<<h<<"\n255\n";for(auto p:pixels){char c[]={char((p>>11)*255/31),char(((p>>5)&63)*255/63),char((p&31)*255/31)};f.write(c,3);}}
int main(int argc,char** argv){if(argc!=2)return 2;std::string prefix=argv[1];
 {constexpr int w=960,h=270;std::vector<uint16_t> pixels(w*h,navy);Raster r(pixels.data(),w,h,w);for(int state=0;state<2;++state)for(unsigned d=0;d<8;++d){int x=d*120+60,y=state*135+110;ShadowArt::base(r,TurretArt::base,x,y,1);TurretArt::draw(r,x,y,1,d,state);r.text(d*120+4,state*135+4,std::to_string(d),cream,12);}save(prefix+"-turrets.ppm",pixels,w,h);}
 {constexpr int w=800,h=480;std::vector<uint16_t> pixels(w*h,navy);Raster r(pixels.data(),w,h,w);World world;world.init(1);world.seen.fill(1);world.fog.fill(1);Battlefield field;field.width=w;field.top=0;field.bottom=h;field.focus({14,7});field.draw(r,world);save(prefix+"-bridge.ppm",pixels,w,h);}
 {constexpr int w=600,h=220;std::vector<uint16_t> pixels(w*h,navy);Raster r(pixels.data(),w,h,w);for(unsigned i=0;i<4;++i){MissionSkin::draw(r,{12,10+int(i)*50,576,32},i);r.text(28,18+int(i)*50,i==0?"作战目标：保护中继站":i==1?"指挥官，前方发现敌军。":i==2?"敌军增援正在靠近":"升级完成",cream,12);}save(prefix+"-mission.ppm",pixels,w,h);}
}
