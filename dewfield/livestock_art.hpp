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
inline void ranch_shed(Raster&r,int x,int y,int feed) {
  r.rect(x-21,y-29,42,26,Ink);r.rect(x-20,y-28,40,24,rgb(160,113,64));
  for(int n=-19;n<20;n+=6){r.rect(x+n,y-26,1,20,rgb(116,82,47));r.rect(x+n+1,y-25,1,17,rgb(191,144,77));}
  r.rect(x-15,y-18,20,17,Ink);r.rect(x-14,y-17,18,16,rgb(73,68,42));
  r.line(x-14,y-8,x+3,y-16,rgb(119,89,44));
  for(int yy=0;yy<12;++yy)r.rect(x-26+yy,y-41+yy,52-yy*2,2,yy%3?rgb(166,94,49):rgb(204,141,65));
  r.line(x-26,y-29,x,y-44,Ink);r.line(x,y-44,x+26,y-29,Ink);
  r.rect(x+7,y-24,9,10,Ink);r.rect(x+8,y-23,7,8,Cream);r.rect(x+11,y-23,1,8,rgb(123,83,44));
  r.rect(x-22,y-3,44,3,rgb(105,77,43));
  r.rect(x+8,y-5,15,9,Ink);r.rect(x+9,y-4,13,7,rgb(143,106,61));
  if(feed)for(int n=0;n<6;++n)r.line(x+9+n*2,y-2,x+11+n*2,y-4,Gold);
  r.rect(x+9,y+2,13,1,rgb(190,144,74));
}
