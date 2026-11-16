// SPDX-License-Identifier: MIT
#include "raster.hpp"
#include <cassert>
#include <vector>
#include <cstdio>
using namespace iron;
int main(int argc,char** argv){
 assert(font_strikes.size()==1&&font_strikes[0].width==12&&font_strikes[0].height==12);
 assert(font_style(9,false).scale==1&&font_style(24,false).scale==2);
 std::vector<uint16_t> one(180*32,navy),two(360*64,navy);Raster a(one.data(),180,32,180),b(two.data(),360,64,360);a.text(0,0,"钢铁黎明 /0123",cream,12);b.text(0,0,"钢铁黎明 /0123",cream,24);
 for(int y=0;y<64;++y)for(int x=0;x<360;++x)assert(two[y*360+x]==one[(y/2)*180+x/2]);
 for(int width:{296,320,480,800}){int height=240;std::vector<uint16_t> original(480*389,navy),physical(width*height+2,0xa55a),reference(width*height+2,0xa55a);std::fill(physical.begin()+1,physical.end()-1,navy);std::fill(reference.begin()+1,reference.end()-1,navy);NativeText text;text.reset(width,height,480,389);Raster logical(original.data(),480,389,480);logical.native_text=&text;char message[]="先锋坦克";logical.text(20,20,message,cream,12);message[0]='?';Raster target(physical.data()+1,width,height,width);draw_native_text(text,target);Raster expected(reference.data()+1,width,height,width);auto style=font_style(pixel_scale(12,width,480),false);expected.bitmap_text(pixel_scale(20,width,480),pixel_scale(20,height,389),"先锋坦克",cream,style);assert(physical==reference);assert(!text.overflow);assert(physical.front()==0xa55a&&physical.back()==0xa55a);
  text.reset(width,height,480,389);logical.clip={30,30,80,50};logical.text(-20,10,"钢铁黎明：长弓火炮 / 123",gold,26);std::fill(physical.begin()+1,physical.end()-1,navy);draw_native_text(text,target);for(int y=0;y<height;++y)for(int x=0;x<width;++x)if(physical[1+y*width+x]!=navy)assert(x>=pixel_scale(30,width,480)&&x<pixel_scale(110,width,480)&&y>=pixel_scale(30,height,389)&&y<pixel_scale(80,height,389));
  text.reset(width,height,480,389);logical.clip={0,0,480,389};logical.label({20,40,130,24},"拖动框选部队，然后点地面下达移动指令；保护矿车，修建电站、兵营和工厂，朝着黎明前进。",cream,12,0);assert(text.count==1);auto command=text.commands[0];std::string_view shortened(text.strings.data()+command.offset,command.length);assert(shortened.ends_with("..."));auto remaining=shortened;while(!remaining.empty())assert(Raster::next(remaining)!='?');std::fill(physical.begin()+1,physical.end()-1,navy);draw_native_text(text,target);for(int y=0;y<height;++y)for(int x=0;x<width;++x)if(physical[1+y*width+x]!=navy)assert(x>=pixel_scale(20,width,480)&&x<pixel_scale(150,width,480));
 }
 if(argc>1){const int w=480,h=240;std::vector<uint16_t> p(w*h,navy);Raster r(p.data(),w,h,w);r.text(18,10,"钢铁黎明",cream,24);r.text(18,57,"框选部队 · 长弓火炮 · 聚变电站",cyan,12);r.text(18,84,"先造兵，再点全军；点击战场下达指令。",cream,12);r.text(18,115,"补给接收！矿石 +280，附近部队已维修",gold,12);r.text(18,156,"0123456789  30 FPS  +280",cream,16);FILE* f=fopen(argv[1],"wb");fprintf(f,"P6\n%d %d\n255\n",w,h);for(auto c:p){unsigned char pixel[]={uint8_t(((c>>11)&31)*255/31),uint8_t(((c>>5)&63)*255/63),uint8_t((c&31)*255/31)};fwrite(pixel,1,3,f);}fclose(f);}
 std::puts("Integer glyph scale, native screen grid, copied UTF-8 commands and clipping passed");
}
