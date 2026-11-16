// SPDX-License-Identifier: MIT
// Native renderer QA preview, not a simulator/store screenshot.
#include "art.hpp"
#include "ui.hpp"
#include "hud_layout.hpp"
#include <vector>
#include <cstdio>
using namespace iron;
int main(int argc,char** argv){if(argc<2)return 1;constexpr int w=480,h=480;std::vector<uint16_t> pixels(w*h,navy);Raster r(pixels.data(),w,h,w);World world;world.init(0);world.add(artillery,0,{11,12});world.select_all();Battlefield field;field.top=50;field.bottom=HudLayout(h).top;field.focus({8,10});field.clock=1;field.draw(r,world);HudLayout layout(h);r.rect(0,0,w,50,navy);Sprites::fit(r,art::badge_first,{8,7,28,30});r.text(43,8,"钢铁黎明",cream,24);r.text(174,8,"1100",gold,24);r.text(260,8,"+20",cyan,24);r.rect(0,layout.top,w,h-layout.top,navy);
 for(int i=0;i<5;++i){auto a=layout.card(i);Skin::panel_frame(r,a);auto pic=layout.image(i);Sprites::fit(r,Sprites::unit_frame(i,1.5707963f,i==harvester),pic);r.label(layout.name(i),specs[i].name,cream,12);char price[20];std::snprintf(price,sizeof price,"%d",specs[i].cost);r.label(layout.price(i),price,gold,12);}
 Skin::button(r,{8,layout.top+8,56,25},true,false,true);r.label({8,layout.top+8,56,25},"部队",cyan,12);Skin::button(r,{8,layout.top+38,56,25},false,false,true);r.label({8,layout.top+38,56,25},"基地",cream,12);Sprites::fit(r,art::buildings_first,{9,layout.top+73,54,40});Sprites::fit(r,art::badge_first,{409,layout.top+73,56,40});
 FILE* file=fopen(argv[1],"wb");if(!file)return 2;fprintf(file,"P6\n%d %d\n255\n",w,h);for(auto c:pixels){unsigned char color[]={uint8_t(((c>>11)&31)*255/31),uint8_t(((c>>5)&63)*255/63),uint8_t((c&31)*255/31)};fwrite(color,1,3,file);}fclose(file);return 0;
}
