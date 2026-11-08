// SPDX-License-Identifier: MIT
// Included inside art, using native RGB565 animal poses.
#pragma once
inline const sprites::Sprite &animal_sprite(const farm::Animal&a,int heading,int frame) {
  return *animal_sprites::animals[std::clamp(int(a.kind)-1,0,4)][farm::animal_stage(a)][(heading==3?2:0)+(frame&1)];
}
inline void animal_shadow(Raster&r,int x,int y,const farm::Animal&a,int heading,int frame) {
  const auto &s=animal_sprite(a,heading,frame);
  // Read only the baked planted sole row; lifted feet never cast contact strips.
  for(int i=0;i<s.count;++i) {
    const auto &span=s.spans[i];
    if(span.y==26)shadow_strip(r,x-s.anchor_x+span.x,y,span.count,2);
  }
}
inline void animal(Raster&r,int x,int y,const farm::Animal&a,int heading,int frame,bool badge=true) {
  grounded(r,animal_sprite(a,heading,frame),x,y);
  if(a.kind==1&&a.male&&farm::animal_stage(a)==2) {
    int dx=heading==3?-1:1;r.rect(x+dx*5-1,y-17,3,2,rgb(166,76,48));
  }
  if(badge&&farm::animal_product(a)) {
    r.rect(x-3,y-32,6,3,Ink);r.rect(x-2,y-31,4,1,Gold);
    r.rect(x,y-29,1,2,Ink);
  }
}
inline void ranch_shed(Raster&r,int x,int y,int feed,bool reinforced=false,bool lit=false) {
  r.rect(x-22,y-27,45,27,Ink);r.rect(x-21,y-26,43,25,farm_palette::Wood);
  for(int n=-20;n<21;n+=7){r.rect(x+n,y-24,1,22,farm_palette::WoodDark);r.rect(x+n+1,y-22,1,19,rgb(191,144,77));}
  // One visible roof plane: the far ridge recedes and the near eave widens.
  // Draw complete scanlines so shingle seams never expose the wall/terrain.
  for(int row=0;row<21;++row) {
    int half=20+row/2,yy=y-47+row;
    r.rect(x-half-1,yy,half*2+3,1,Ink);
    r.rect(x-half,yy,half*2+1,1,row%5==4?rgb(125,77,48):row%5==0?rgb(205,146,78):rgb(171,106,57));
    r.rect(x-half,yy,2,1,rgb(218,164,91));r.rect(x+half-2,yy,3,1,rgb(137,82,49));
    if(row%5>0&&row%5<4)for(int col=-26+(row/5%2)*5;col<29;col+=10)
      if(col>-half+2&&col<half-3)r.rect(x+col,yy,1,1,rgb(143,85,48));
  }
  r.rect(x-21,y-49,43,3,Ink);r.rect(x-20,y-48,41,1,Gold);
  r.rect(x-31,y-26,63,3,Ink);r.rect(x-30,y-26,61,1,rgb(205,146,78));
  r.rect(x-22,y-23,45,2,farm_palette::WoodDark);
  r.rect(x-16,y-21,21,21,Ink);r.rect(x-14,y-19,17,18,rgb(73,68,42));
  r.rect(x-13,y-5,15,4,rgb(123,100,55));
  for(int i=0;i<4;++i)r.line(x-12+i*3,y-2,x-10+i*3,y-5,Gold);
  r.rect(x-16,y-21,21,2,rgb(196,149,81));r.rect(x-16,y-19,2,18,farm_palette::WoodDark);
  r.rect(x+7,y-20,11,11,Ink);r.rect(x+8,y-19,9,9,rgb(105,135,118));
  if(lit){Raster light=r;light.tint=nullptr;light.rect(x+8,y-19,9,9,rgb(227,178,93));light.rect(x+9,y-18,3,3,Cream);}
  r.rect(x+12,y-19,1,9,farm_palette::WoodDark);r.rect(x+8,y-15,9,1,farm_palette::WoodDark);
  r.rect(x+6,y-9,13,2,farm_palette::WoodDark);
  if(reinforced)for(int side:{-20,19}){r.rect(x+side,y-23,2,5,rgb(92,105,88));r.rect(x+side,y-22,1,1,Cream);}
  r.rect(x-22,y-2,45,2,farm_palette::WoodDark);
  // The trough shares the wall's ground anchor; no feet below its shadow.
  r.rect(x+7,y-8,17,8,Ink);r.rect(x+8,y-7,15,6,rgb(143,106,61));
  r.rect(x+8,y-7,15,2,rgb(78,78,47));
  if(feed)for(int n=0;n<6;++n)r.line(x+9+n*2,y-5,x+11+n*2,y-7,Gold);
  r.rect(x+8,y-2,15,1,rgb(190,144,74));
  if(reinforced) {
    r.rect(x-23,y-21,4,2,Ink);r.rect(x-25,y-20,5,7,Ink);
    r.rect(x-24,y-19,3,5,rgb(139,126,78));
    if(lit){Raster light=r;light.tint=nullptr;light.rect(x-24,y-19,3,5,Gold);light.rect(x-23,y-18,1,3,Cream);}
    r.rect(x-25,y-14,5,1,farm_palette::WoodDark);
  }
}
