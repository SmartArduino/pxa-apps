// SPDX-License-Identifier: MIT
// Render raw poses through the exact game sampler for before/after inspection.
#include "paint.hpp"
#include <filesystem>
#include <fstream>
#include <vector>
#include <cstdio>
int main(int argc,char**argv){if(argc<2||argc>3)return 2;bool walk=argc==3;std::filesystem::create_directories(argv[1]);garden::FilteredArt art;art.bytes.resize(garden_art::pixel_bytes);if(!garden::unpack(garden_art::packed_pixels,art.bytes))return 3;
 int width=walk?280:140,height=190;std::vector<uint16_t> pixels(width*height);
 for(int frame=0;frame<12;++frame){std::fill(pixels.begin(),pixels.end(),garden::rgb(74,96,53));garden::Paint p{pixels.data(),width,height,width,art,480.f/296};float phase=(frame+.05f)/12;
  if(walk){std::array clips{garden_art::dustimp_idle,garden_art::copperbeetle_idle,garden_art::armorsnail_idle,garden_art::swiftmoth_idle};for(int i=0;i<4;++i){int x=43+(i%2)*86,y=45+(i/2)*55;p.shadow(x,y,25);p.actor(clips[i],phase,x,y,25);}}
  else{p.shadow(43,45,25);p.actor(garden_art::coralflower_idle,phase,43,45,25);p.shadow(43,100,25);p.actor(garden_art::dustimp_idle,phase,43,100,25);}
  char name[40];std::snprintf(name,sizeof name,"frame-%02d.ppm",frame);std::ofstream out(std::filesystem::path(argv[1])/name,std::ios::binary);out<<"P6\n"<<width<<" "<<height<<"\n255\n";for(auto pixel:pixels){char c[3]={char((pixel>>11)*255/31),char(((pixel>>5)&63)*255/63),char((pixel&31)*255/31)};out.write(c,3);}}
 }
