// SPDX-License-Identifier: MIT
// Included in art. Details follow art/reference/village.png and interior-mine.png.
#pragma once
inline const building_sprites::Image &building_image(int kind){return *building_sprites::Images[std::clamp(kind,0,5)];}
inline const sprites::Sprite &building_shape(int kind){return building_image(kind).shape;}
inline void village_house(Raster&r,int x,int y,int kind) {
  const auto &image=building_image(kind);const auto &s=image.shape;
  int left=x+40-s.anchor_x-r.ox,top=y+61-s.anchor_y-r.oy;
  // Fixed 4-bit colour lookup within opaque spans: no image allocation, PNG
  // decoding, per-pixel clipping, or recolouring of a reused cottage model.
  for(unsigned i=0;i<s.count;++i) {
    const auto &span=s.spans[i];int yy=top+span.y;if(yy<0||yy>=r.h)continue;
    int begin=std::max(0,left+span.x),end=std::min(r.w,left+span.x+span.count);
    unsigned at=span.offset+begin-left-span.x;
    for(int xx=begin;xx<end;++xx,++at) {
      auto packed=image.indices[at/2];auto c=image.palette[(packed>>((at&1)*4))&15];
      r.p[yy*r.w+xx]=r.tint?r.tint[c]:c;
    }
  }
}
inline void room_workbench(Raster&r,int x,int y,int owner) {
  furniture(r,furniture_sprites::Workbench,x,y);
  if(owner==0) {
    for(int i=0;i<4;++i){int xx=x-25+i*14;r.rect(xx,y-28,10,5,Ink);r.rect(xx+1,y-27,8,3,Earth);leaf(r,xx+2,y-29);}
  } else if(owner==2) {
    r.rect(x-20,y-28,24,6,Cream);r.rect(x-14,y-27,14,3,Water);r.rect(x-1,y-28,2,4,Gold);
    for(int dx:{10,22}){r.rect(x+dx,y-31,8,8,Ink);r.rect(x+dx+1,y-30,6,6,Earth);r.rect(x+dx+1,y-31,6,2,Cream);}
  } else {
    r.rect(x-23,y-25,24,5,Gold);r.line(x-21,y-25,x-5,y-27,Cream);
    r.line(x+7,y-24,x+19,y-31,Ink);r.rect(x+16,y-32,9,4,Ink);r.rect(x+17,y-31,7,2,rgb(154,164,151));
    for(int i=0;i<4;++i)r.rect(x-19+i*8,y-29+i%3,5,1,Cream);
  }
}
inline void room_bookcase(Raster&r,int x,int y,int owner) {
  (void)owner;furniture(r,furniture_sprites::Bookcase,x,y);
}
inline void room_woodrack(Raster&r,int x,int y,int owner) {
  furniture(r,owner==2?furniture_sprites::Barrels:furniture_sprites::WoodRack,x,y);
}
inline void room_divider(Raster&r,int x,int y,int owner) {
  (void)owner;furniture(r,furniture_sprites::Divider,x,y);
}
inline void room_armchair(Raster&r,int x,int y) {
  furniture(r,furniture_sprites::Armchair,x,y);
}
inline void home_rug(Raster&r,int x,int y,int w,int h,uint16_t c,bool oval=false) {
  for(int yy=0;yy<h;++yy) {
    int corner=std::min(yy,h-1-yy),cut=std::max(0,(oval?9:3)-corner);
    r.rect(x+cut,y+yy,w-cut*2,1,blend(c,Ink,2));
    if(yy&&yy<h-1)r.rect(x+cut+1,y+yy,w-cut*2-2,1,c);
    if(yy==3||yy==h-4)r.rect(x+cut+3,y+yy,w-cut*2-6,1,Cream);
    if(yy>3&&yy<h-4){r.rect(x+cut+3,y+yy,1,1,Cream);r.rect(x+w-cut-4,y+yy,1,1,Cream);}
  }
  for(int xx=x+8;xx<x+w-7;xx+=8){r.rect(xx,y+6,2,1,Gold);r.rect(xx+1,y+h-7,2,1,Gold);}
}
inline void home_details(Raster&r,int owner) {
  uint16_t accent[]={Leaf,rgb(115,139,148),rgb(111,154,149),rgb(194,138,119)};
  if(owner==0) {
    for(int i=0;i<3;++i){int x=126+i*14;r.line(x,52,x,66,Earth);leaf(r,x,68);r.rect(x-2,73,8,6,Earth);}
    home_rug(r,141,163,52,27,accent[0]);home_rug(r,126,254,82,38,accent[0]);home_rug(r,302,265,78,26,accent[0]);
  } else if(owner==1) {
    home_rug(r,140,258,88,43,accent[1]);home_rug(r,283,245,88,36,accent[1]);
    for(int i=0;i<7;++i)r.rect(146+i*10,295+(i%3)*4,4,1,Gold);
  } else if(owner==2) {
    for(int i=0;i<3;++i){int x=360+i*14;r.line(x,54,x+4,73,Gold);r.line(x+4,73,x+2,77,Cream);}
    for(int i=0;i<5;++i){r.line(126+i*7,53,151+i*3,74,Earth);r.line(128,55+i*4,163,55+i*4,Earth);}
    home_rug(r,300,254,86,39,accent[2],true);home_rug(r,128,278,82,29,accent[2]);
  } else {
    for(int x:{140,210}){r.rect(x,53,24,24,Ink);r.rect(x+2,55,20,20,Cream);leaf(r,x+9,67);flower(r,x+13,62,rgb(191,132,110));}
    home_rug(r,282,269,88,37,accent[3]);
    home_rug(r,144,281,72,40,accent[0],true);
    for(int i=0;i<3;++i)flower(r,161+i*10,296+i%2*4,Cream);
  }
  // A small framed local landscape, wall-mounted behind all moving furniture.
  int px=owner==3?365:211;
  r.rect(px,54,30,21,Ink);r.rect(px+2,56,26,17,Cream);r.rect(px+5,59,20,10,rgb(176,194,153));
  for(int i=0;i<20;++i){int hill=3+std::abs(i-9)/3;r.rect(px+5+i,59+hill,1,10-hill,Leaf);}
  r.rect(px+5,67,20,2,Water);r.rect(px+18,60,2,2,Gold);
}
inline void paving(Raster&r,const paving_sprites::Image&image,int x,int y) {
  int left=x-r.ox,top=y-r.oy;
  int x0=std::max(0,left),x1=std::min(r.w,left+image.w);
  for(int yy=std::max(0,top);yy<std::min(r.h,top+image.h);++yy) {
    unsigned at=(yy-top)*image.w+x0-left;
    for(int xx=x0;xx<x1;++xx,++at) {
      auto packed=image.indices[at/2];unsigned c=(packed>>((at&1)*4))&15;
      if(c){auto color=paving_sprites::Palette[c];r.p[yy*r.w+xx]=r.tint?r.tint[color]:color;}
    }
  }
}
inline void settlement_ground(Raster&r,const farm::State&s) {
  if(s.region==unsigned(farm::Region::Town))paving(r,paving_sprites::Plaza,194,146);
  if(s.region==unsigned(farm::Region::Farm)) {
    // Loose stepping flags follow the actual narrow path, not a 3-column grid
    // stamped over the surrounding lawn. Spacing, width and outlines vary.
    paving(r,*paving_sprites::Stones[0],97,128);
    paving(r,*paving_sprites::Stones[1],99,142);
    paving(r,*paving_sprites::Stones[2],96,154);
    paving(r,*paving_sprites::Stones[5],99,170);
    paving(r,*paving_sprites::Stones[3],87,179);
    paving(r,*paving_sprites::Stones[3],116,115);
  }
  for(const auto&site:farm::Buildings)if(unsigned(site.area)==s.region) {
    auto p=site.door();int x=p.x*16+8,y=p.y*16-3;
    if(site.kind==5) {
      for(int i=0;i<3;++i){r.rect(x-12,y+i*3,25,3,farm_palette::WoodDark);r.rect(x-11,y+i*3,23,2,Earth);r.rect(x-9,y+i*3,19,1,Gold);}
    } else {
      paving(r,*paving_sprites::Stones[5],x-13,y);
      paving(r,*paving_sprites::Stones[0],x+1,y+1);
      paving(r,*paving_sprites::Stones[1],x-5,y+11);
      if(site.kind==1)paving(r,*paving_sprites::Stones[2],x+25,y+1);
    }
    if(site.kind==3)for(int i=0;i<7;++i) {
      int xx=x-28+(i*11)%51,yy=y+15+(i%3)*4;
      if(farm::walkable(s,xx/16,yy/16))r.rect(xx,yy,3,1,Gold);
    }
  }
  if(s.region==unsigned(farm::Region::Lake))for(auto p:{farm::Point{22,8},farm::Point{23,16},farm::Point{24,19}}) {
    int x=p.x*16+8,y=p.y*16+13;
    for(int i=0;i<3;++i){r.line(x+i*3,y,x+i*3-1,y-6-i%2*2,Green);r.rect(x+i*3-2,y-7-i%2*2,3,2,Gold);}
  }
}
