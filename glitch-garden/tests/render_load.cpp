// SPDX-License-Identifier: MIT
#include "scene.hpp"
#include "tests/dense_scene.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
int main(int argc,char** argv){if(argc!=2)return 2;using namespace garden;std::filesystem::path out=argv[1];std::filesystem::create_directories(out);
 FilteredArt art;art.bytes.resize(garden_art::pixel_bytes);if(!unpack(garden_art::packed_pixels,art.bytes))return 3;
 Layout layout;layout.x=8;layout.y=8;layout.w=280;layout.h=273;layout.bx=38;layout.by=93;layout.cell=25;
 constexpr int width=480,height=480;std::vector<uint16_t> pixels(width*height);Game game;game.start(0);std::vector<unsigned> times;uint64_t hash=1469598103934665603ull;
 for(int frame=0;frame<240;++frame){dense_scene(game,frame/30.f,frame<120);auto start=std::chrono::steady_clock::now();
  std::fill(pixels.begin(),pixels.end(),rgb(109,149,66));Paint paint{pixels.data(),width,height,width,art,480.f/296};draw_board(paint,game,layout);
  times.push_back(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count());
  for(auto p:pixels){hash^=p;hash*=1099511628211ull;}}
 std::ofstream json(out/"render.json");json<<"{\"hash\":"<<hash<<",\"cache_bytes\":"<<art.cache_bytes<<",\"hits\":"<<art.hits<<",\"misses\":"<<art.misses<<",\"us\":[";
 for(unsigned i=0;i<times.size();++i)json<<(i?",":"")<<times[i];json<<"]}\n";
 std::ofstream ppm(out/"last.ppm",std::ios::binary);ppm<<"P6\n480 480\n255\n";for(auto p:pixels){char rgb[3]={char((p>>11)*255/31),char(((p>>5)&63)*255/63),char((p&31)*255/31)};ppm.write(rgb,3);}
 std::cout<<"render hash="<<hash<<" cache="<<art.cache_bytes<<" hits="<<art.hits<<" misses="<<art.misses<<"\n";
}
