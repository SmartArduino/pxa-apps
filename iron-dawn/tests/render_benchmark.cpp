// SPDX-License-Identifier: MIT
// Render-only, repeatable workload. Timings exclude fixture setup and readback.
#include "art.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <vector>
using namespace iron;
int main(int argc,char** argv){int frames=argc>1?std::atoi(argv[1]):180;uint64_t guard=0;std::printf("height,scene,frames,wall_mean_us,wall_p95_us,pixel_hash,cpu_mean_us\n");
 for(int height:{480,854})for(int scene=0;scene<3;++scene){World world;world.init(scene?2:0);if(scene){world.fog.fill(1);world.seen.fill(1);for(int i=0;i<28;++i)world.add(i%4,0,{14.f+(i%7)*.6f,14.f+(i/7)*.65f});world.select_all();}Battlefield field;field.height=height;field.bottom=height-113;field.focus(scene?Vec{17,17}:Vec{8,10});std::vector<uint16_t> pixels(480*height);Raster r(pixels.data(),480,height,480);std::vector<double> samples;double cpu=0;
  for(int frame=-10;frame<frames;++frame){field.clock=(frame+10)/30.f;if(scene==2){int k=0;for(auto& b:world.blasts){b.p={15.f+(k%4)*1.1f,15.f+(k/4)*.7f};b.age=(frame%27+27)*.025f;b.type=k%3;b.alive=k<12;++k;}}auto cpu_begin=std::clock();auto begin=std::chrono::steady_clock::now();field.draw(r,world);r.rect(0,0,480,43,navy);r.text(15,7,"钢铁黎明",cream,18);r.text(165,7,"1580",gold,20);for(int i=0;i<5;++i){r.box({72+i*65,height-105,62,55},panel);r.label({74+i*65,height-78,58,20},specs[i].name,cream,11);r.label({74+i*65,height-58,58,15},"330",gold,10);}r.shade({8,height-147,464,27},navy,14);r.text(14,height-143,"部队晋升！火力增强，战地修复已完成",cream,13);auto end=std::chrono::steady_clock::now();if(frame>=0)cpu+=double(std::clock()-cpu_begin)*1e6/CLOCKS_PER_SEC;if(frame>=0)samples.push_back(std::chrono::duration<double,std::micro>(end-begin).count());}
  double total=0;for(auto t:samples)total+=t;std::sort(samples.begin(),samples.end());uint32_t hash=2166136261u;for(auto p:pixels)hash=(hash^p)*16777619u;guard+=hash;std::printf("%d,%d,%d,%.2f,%.2f,%08x,%.2f\n",height,scene,frames,total/frames,samples[frames*95/100],hash,cpu/frames);
 }return guard==0;}
