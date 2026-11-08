// SPDX-License-Identifier: MIT
// Included in art. Integer pixels and existing RGB565 spans only.
#pragma once
inline void mine_floor_art(Raster&r,int depth) {
 constexpr uint16_t floor[]={rgb(107,96,76),rgb(77,91,94),rgb(77,75,99)};
 constexpr uint16_t wall[]={rgb(132,117,88),rgb(105,117,120),rgb(105,99,129)};
 constexpr uint16_t dark[]={rgb(68,64,52),rgb(52,64,69),rgb(51,49,72)};
 int theme=(depth-1)/10;
 r.rect(0,0,512,384,dark[theme]);
 for(int ty=0;ty<24;++ty)for(int tx=0;tx<32;++tx) {
  int x=tx*16,y=ty*16;auto h=hash(tx+depth*7,ty);
  if(farm::mine_floor(depth,tx,ty)) {
   r.rect(x,y,16,16,floor[theme]);
   for(int i=0;i<4;++i){int xx=(h>>(i*5))%14,yy=(h>>(i*4+2))%14;r.rect(x+xx,y+yy,1+(i&1),1,i&1?wall[theme]:dark[theme]);}
   if(!farm::mine_floor(depth,tx,ty-1))r.rect(x,y,16,2,dark[theme]);
   if(!farm::mine_floor(depth,tx-1,ty))r.rect(x,y,1,16,dark[theme]);
  } else {
   r.rect(x,y,16,16,wall[theme]);r.rect(x,y+11,16,5,dark[theme]);
   r.line(x+1,y+2,x+10,y+1,rgb(159,151,123));
   r.line(x+9,y+4,x+12,y+8,dark[theme]);r.line(x+12,y+8,x+15,y+6,dark[theme]);
   r.rect(x+2+(h%3),y+6,4,2,dark[theme]);
   if(!farm::mine_floor(depth,tx,ty+1))r.rect(x,y+13,16,3,wall[theme]);
  }
 }
}
inline void mine_entrance(Raster&r,int x,int y) {
 for(int yy=0;yy<42;++yy) {
  int side=yy<8?8-yy:0;r.rect(x-26+side,y-43+yy,53-side*2,2,yy%7<2?rgb(140,143,107):rgb(109,119,88));
 }
 r.rect(x-10,y-27,21,28,Ink);r.rect(x-8,y-28,17,2,Ink);r.rect(x-7,y-25,15,24,rgb(40,45,43));
 for(int xx:{-14,11}) {r.rect(x+xx,y-29,4,30,rgb(103,73,42));r.rect(x+xx+1,y-28,1,28,rgb(182,132,68));}
 r.rect(x-15,y-31,31,5,rgb(101,72,40));r.rect(x-14,y-30,29,1,Gold);
 r.rect(x-11,y,23,3,rgb(157,142,99));r.rect(x-10,y+1,21,1,Cream);
 r.line(x-22,y-32,x-18,y-35,Leaf);r.line(x+19,y-18,x+24,y-22,Leaf);
}
inline void mine_ore(Raster&r,int x,int y,int item) {
 grounded(r,rock_sprite(x,y),x,y);
 uint16_t c=item==38?rgb(62,66,60):item==39?rgb(190,116,66):item==40?rgb(134,174,175):item==41?rgb(224,175,68):rgb(166,130,189);
 for(int i=0;i<3;++i) {
  int xx=x-6+i*5,yy=y-9-(i%2)*5;
  r.rect(xx,yy,4,4,Ink);r.rect(xx+1,yy,3,3,c);r.rect(xx+1,yy,2,1,item==38?rgb(110,121,112):Cream);
 }
}
template<class Canvas> inline void mine_ladder(Canvas&r,int x,int y,bool down,bool found=true) {
 if(!found){r.rect(x-7,y-5,15,4,rgb(75,70,57));r.line(x-5,y-4,x+4,y-4,rgb(155,139,99));return;}
 r.rect(x-9,y-14,19,15,Ink);r.rect(x-7,y-12,15,11,rgb(43,49,51));
 for(int side:{-5,5}){r.rect(x+side,y-12,2,12,Gold);r.rect(x+side,y-12,1,12,Cream);}
 for(int n=0;n<4;++n)r.rect(x-4,y-11+n*3,9,1,rgb(192,148,76));
 r.rect(x-10,y-1,21,2,rgb(139,129,94));
 if(!down){r.rect(x-3,y-20,7,3,Cream);r.rect(x-1,y-22,3,7,Cream);}
}
inline void mine_torch(Raster&r,int x,int y,int frame) {
 r.rect(x-3,y-11,7,13,Ink);r.rect(x-2,y-9,5,9,rgb(128,88,45));r.rect(x-1,y-8,2,9,Gold);
 r.rect(x-4,y-14,9,4,rgb(158,90,48));r.rect(x-2,y-18-(frame&1),5,8,Gold);r.rect(x-1,y-16,3,6,Cream);r.rect(x,y-19,1,5,rgb(240,199,109));
}
inline void mine_monster_shadow(Raster&r,int x,int y,int type) {
 if(type==1)shadow_strip(r,x-6,y,13,1);
 else if(type==3){shadow_strip(r,x-10,y,6,2);shadow_strip(r,x+4,y,7,2);}
 else shadow_strip(r,x-(type==2?10:6),y,type==2?21:13,2);
}
template<class Canvas> inline void mine_monster(Canvas&r,int x,int y,int type,int frame,int dir,bool angry) {
 uint16_t outline=angry&&(frame&1)?rgb(202,91,69):Ink;
 if(type==0) {
  int lift=(frame%4==1);r.rect(x-6,y-7-lift,13,7,outline);r.rect(x-4,y-11-lift,9,5,outline);
  r.rect(x-5,y-7-lift,11,6,rgb(115,151,90));r.rect(x-3,y-10-lift,7,5,rgb(151,175,107));r.rect(x-2,y-9-lift,3,1,Cream);
  for(int sx:{-2,3})r.rect(x+sx,y-5-lift,1,2,Ink);r.rect(x,y-2-lift,2,1,Ink);
 } else if(type==1) {
  int yy=y-12,wing=(frame&1)?-4:2;
  for(int side:{-1,1}) {
   r.line(x+side*2,yy-1,x+side*10,yy+wing-4,outline);r.line(x+side*10,yy+wing-4,x+side*8,yy+wing+3,outline);
   for(int i=2;i<9;++i)r.line(x+side*i,yy+(i>5?wing:-1),x+side*i,yy+3,rgb(93,102,131));
  }
  r.rect(x-3,yy-4,7,8,outline);r.rect(x-2,yy-3,5,6,rgb(111,119,146));r.rect(x-3,yy-6,2,3,outline);r.rect(x+2,yy-6,2,3,outline);
  r.rect(x-2,yy-1,1,1,Gold);r.rect(x+2,yy-1,1,1,Gold);
 } else if(type==2) {
  for(int sx:{-1,1})for(int i=0;i<3;++i){int xx=x+sx*(6+i*2);r.line(xx,y-5,x+sx*(10+i),y-1-(frame+i)%2,outline);}
  r.rect(x-8,y-11,17,9,outline);r.rect(x-6,y-14,13,5,outline);r.rect(x-7,y-10,15,7,rgb(118,134,133));r.rect(x-5,y-13,11,5,rgb(157,166,149));
  r.line(x-4,y-10,x+4,y-7,rgb(87,107,107));r.rect(x-1,y-12,4,4,Gold);
  r.rect(x-5,y-3,2,2,outline);r.rect(x+4,y-3,2,2,outline);
 } else {
  for(int sx:{-9,5}){r.rect(x+sx,y-6,6,6,outline);r.rect(x+sx+1,y-5,4,4,rgb(109,122,130));}
  r.rect(x-10,y-23,21,18,outline);r.rect(x-8,y-25,17,21,rgb(124,137,140));
  r.line(x-7,y-20,x+6,y-18,rgb(174,179,160));r.line(x-4,y-24,x-2,y-5,rgb(79,95,105));
  for(int sx:{-15,11}){r.rect(x+sx,y-19,5,13,outline);r.rect(x+sx+1,y-18,3,10,rgb(117,127,135));}
  r.rect(x-3,y-17,7,8,outline);r.rect(x-2,y-16,5,6,rgb(174,128,168));r.rect(x,y-15,2,4,Cream);
  r.rect(x-6,y-22,3,2,Gold);r.rect(x+4,y-22,3,2,Gold);
 }
 (void)dir;
}
