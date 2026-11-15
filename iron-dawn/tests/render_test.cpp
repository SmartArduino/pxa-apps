// SPDX-License-Identifier: MIT
#include "../art.hpp"
#include <cassert>
#include <vector>
#include <iostream>
using namespace iron;
int main(){for(int h:{320,389,480,854,1038}){std::vector<uint16_t> pixels(480*h+2,0xa55a);Raster r(pixels.data()+1,480,h,480);for(int ch=0;ch<3;++ch){World world;world.init(ch);Battlefield f;f.width=480;f.height=h;f.bottom=h-113;world.select_all();world.order({16,16});for(int frame=0;frame<120;++frame){world.tick(.05f);if(frame%20)continue;f.clock=frame*.05f;f.zoom=frame%40?1.3f:.72f;f.focus(frame<60?Vec{7,10}:Vec{20,19});f.draw(r,world);r.text(10,0,"钢铁黎明 / 建造与撤离 90 秒",cream,15);r.label({10,h-40,460,30},"林岚：北境的灯重新亮了。",cyan,17);assert(pixels.front()==0xa55a&&pixels.back()==0xa55a);}}}std::cout<<"Renderer bounds and font sampling passed at five viewport ratios\n";}
