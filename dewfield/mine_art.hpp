// SPDX-License-Identifier: MIT
// Included in art. Integer pixels and existing RGB565 spans only.
#pragma once
inline void mine_floor_art(Raster&r,int depth) {
 // Cached once on entry: shared strata and exposed faces, never a wall grid.
 constexpr uint16_t floor[]={rgb(134,112,78),rgb(101,119,111),rgb(104,98,122)};
 constexpr uint16_t stone[]={rgb(106,94,73),rgb(78,98,94),rgb(78,74,101)};
 constexpr uint16_t face[]={rgb(81,72,59),rgb(60,79,79),rgb(58,54,80)};
 constexpr uint16_t dark[]={rgb(56,53,46),rgb(44,61,62),rgb(44,41,65)};
 constexpr uint16_t lip[]={rgb(165,143,98),rgb(138,157,139),rgb(143,132,161)};
 int theme=std::clamp((depth-1)/10,0,2);
 r.rect(0,0,512,384,stone[theme]);
 for(int ty=0;ty<24;++ty)for(int tx=0;tx<32;++tx) {
  int x=tx*16,y=ty*16;auto h=hash(tx+depth*7,ty);
  bool open=farm::mine_floor(depth,tx,ty);
  bool n=farm::mine_floor(depth,tx,ty-1),s=farm::mine_floor(depth,tx,ty+1);
  bool w=farm::mine_floor(depth,tx-1,ty),e=farm::mine_floor(depth,tx+1,ty);
  if(open) {
   r.rect(x,y,16,16,floor[theme]);
   // Small disconnected pebbles, with no repeated tile border or long planks.
   for(int i=0;i<3;++i) {
    int xx=(h>>(i*7))%13+1,yy=(h>>(i*6+3))%12+2;
    r.rect(x+xx,y+yy,2+(i&1),1,i==1?lip[theme]:stone[theme]);
    if(i==1)r.rect(x+xx+1,y+yy+1,2,1,stone[theme]);
   }
   if(!n){r.rect(x,y,16,2,dark[theme]);r.rect(x,y+2,16,1,face[theme]);}
   if(!w)r.rect(x,y,2,16,face[theme]);
   if(!e)r.rect(x+15,y,1,16,stone[theme]);
   // Stair-step inner corners visually soften the cave's collision tiles.
   if(!n&&!w){r.rect(x,y,4,3,dark[theme]);r.rect(x,y+3,3,2,face[theme]);}
   if(!n&&!e){r.rect(x+12,y,4,3,dark[theme]);r.rect(x+13,y+3,3,2,face[theme]);}
  } else {
   // Broad clustered shelves cross tiles; only exposed edges have a rim.
   if(h%5==0){int xx=x+3+(h%5),yy=y+2+(h>>8)%6;
    r.line(xx,yy,xx+5,yy-1,lip[theme]);r.line(xx+5,yy-1,xx+8,yy+2,face[theme]);}
   if((h>>5)%4==0){r.rect(x+3,y+10,7,2,face[theme]);r.rect(x+8,y+9,4,1,face[theme]);}
   if(s) {
    int left=w?3:0,right=e?3:0;
    r.rect(x+left,y+8,16-left-right,8,face[theme]);
    r.rect(x+left,y+7,16-left-right,2,lip[theme]);
    r.rect(x+left+1,y+9,14-left-right,1,stone[theme]);
    r.rect(x+left,y+15,16-left-right,1,dark[theme]);
    int seam=3+(h%9);r.line(x+seam,y+10,x+seam+1,y+13,dark[theme]);
    if(w){r.rect(x,y+11,3,5,face[theme]);r.rect(x,y+10,3,1,lip[theme]);}
    if(e){r.rect(x+13,y+11,3,5,face[theme]);r.rect(x+13,y+10,3,1,lip[theme]);}
   }
   if(w){r.rect(x,y,2,s?8:16,lip[theme]);r.rect(x+2,y+2,1,s?5:14,face[theme]);}
   if(e){r.rect(x+14,y,2,s?8:16,dark[theme]);r.rect(x+13,y+2,1,s?5:14,face[theme]);}
   if(n)r.rect(x,y,16,2,lip[theme]);
   if(!n&&!s&&!w&&!e&&h%13==0) {
    // Occasional small mineral flecks remain decorative and unobtrusive.
    r.rect(x+7,y+5,3,2,lip[theme]);r.rect(x+8,y+4,1,1,floor[theme]);
   }
  }
 }
}
inline void mine_entrance(Raster&r,int x,int y) {
 for(int yy=0;yy<42;++yy) {
  int side=yy<8?8-yy:0;r.rect(x-26+side,y-43+yy,53-side*2,2,yy%7<2?rgb(140,143,107):rgb(109,119,88));
 }
 r.rect(x-10,y-27,21,27,Ink);r.rect(x-8,y-28,17,2,Ink);r.rect(x-7,y-25,15,24,rgb(40,45,43));
 for(int xx:{-14,11}) {r.rect(x+xx,y-29,4,29,rgb(103,73,42));r.rect(x+xx+1,y-28,1,28,rgb(182,132,68));}
 r.rect(x-15,y-31,31,5,rgb(101,72,40));r.rect(x-14,y-30,29,1,Gold);
 r.rect(x-11,y-2,23,2,rgb(157,142,99));r.rect(x-10,y-2,21,1,Cream);
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
