// SPDX-License-Identifier: MIT
#pragma once
// Included inside namespace art. All maps share the original compiled atlas.
// Small hand-authored foods share the scene palette and an exact ground edge.
// y is the bottom baseline, so backpack, carried and forage drawings agree.
template<class Canvas> inline void food(Canvas &r, int x, int y, int item) {
  if(item>=38) {
    if(item==43||item==44) {
      r.rect(x-2,y-6,4,6,Ink);r.rect(x-1,y-5,2,4,Cream);
      if(item==43){r.rect(x-6,y-10,12,5,Ink);r.rect(x-4,y-12,8,7,rgb(168,122,137));r.rect(x-5,y-9,10,2,rgb(194,151,156));r.rect(x-2,y-11,2,2,Cream);r.rect(x+3,y-8,2,1,Cream);}
      else {r.rect(x-3,y-6,6,5,rgb(205,179,122));r.line(x,y-7,x-5,y-12,Leaf);r.line(x,y-7,x+5,y-13,Leaf);r.rect(x-5,y-13,4,2,rgb(179,206,133));r.rect(x+2,y-14,4,2,rgb(179,206,133));}
    } else {
      uint16_t c=item==38?rgb(83,91,83):item==39?rgb(192,118,66):item==40?rgb(137,173,173):item==41?Gold:item==42?rgb(176,143,193):rgb(124,158,106);
      r.rect(x-5,y-7,10,7,Ink);r.rect(x-3,y-10,7,9,Ink);r.rect(x-4,y-6,8,5,c);r.rect(x-2,y-9,5,7,c);r.line(x-1,y-9,x-1,y-5,Cream);r.rect(x+2,y-3,2,2,Ink);
    }
    return;
  }
  if(item>=29) {
    if(item==29||item==30||item==35||item==36) {
      r.rect(x-3,y-8,6,8,Ink);r.rect(x-4,y-6,8,5,Ink);
      r.rect(x-2,y-8,4,1,Cream);r.rect(x-3,y-7,6,6,item==30||item==36?rgb(187,207,164):Cream);
      r.rect(x-2,y-7,2,4,rgb(250,235,194));r.rect(x+1,y-3,2,2,rgb(187,163,108));
      if(item>=35){r.rect(x+2,y-10,3,2,Leaf);r.rect(x+3,y-11,1,4,Dark);}
    } else if(item==31||item==32) {
      r.rect(x-3,y-11,6,2,Ink);r.rect(x-4,y-8,8,8,Ink);r.rect(x-3,y-7,6,6,Cream);
      r.rect(x-2,y-10,4,3,rgb(218,207,161));r.rect(x-3,y-6,6,2,item==31?rgb(109,151,161):rgb(150,153,95));r.rect(x-2,y-5,1,3,rgb(251,237,193));
    } else if(item==33) {
      r.rect(x-5,y-6,10,5,Ink);r.rect(x-4,y-8,8,8,Cream);r.rect(x-2,y-9,4,1,Cream);
      for(int i=0;i<3;++i){r.line(x-4+i*3,y-6,x-1+i*3,y-3,rgb(184,164,119));r.line(x-4+i*3,y-3,x-1+i*3,y-6,rgb(207,189,142));}
    } else {
      r.rect(x-5,y-6,10,5,Ink);r.rect(x-4,y-8,8,8,rgb(178,99,74));r.rect(x-3,y-7,6,5,rgb(222,153,116));r.rect(x-1,y-5,3,2,Cream);
    }
    return;
  }
  if(item>=24&&item<=28) {
    if(item==26||item==28) {
      r.rect(x-5,y-7,11,6,Ink);r.rect(x-4,y-9,9,8,item==26?Earth:Cream);
      for(int i=-2;i<=2;i+=2)r.line(x+i,y-7,x+i/2,y-2,item==26?Gold:rgb(175,144,113));
    } else {
      r.rect(x-2,y-6,5,6,Ink);r.rect(x-1,y-7,3,6,item==27?Cream:Green);
      for(int i=-1;i<=1;++i){r.line(x,y-5,x+i*4,y-12,Leaf);r.rect(x+i*4-1,y-13,3,4,item==25?rgb(127,137,102):Green);}
    }
  } else if (item == 19) {
    r.rect(x-4,y-9,9,7,Ink); r.rect(x-3,y-10,7,10,rgb(172,69,43));
    r.rect(x-4,y-8,9,6,rgb(199,82,45)); r.rect(x-2,y-9,3,2,rgb(238,149,85));
    r.rect(x,y-13,1,4,Earth); r.rect(x+1,y-12,4,2,Leaf);
    r.rect(x-2,y-1,5,1,Earth);
  } else if (item == 20) {
    r.rect(x-2,y-7,5,7,Ink); r.rect(x-1,y-6,3,5,Cream);
    r.rect(x-6,y-9,13,3,Ink); r.rect(x-4,y-12,9,6,rgb(150,92,52));
    r.rect(x-5,y-9,11,3,rgb(188,126,69));
    r.rect(x-2,y-11,2,2,Cream); r.rect(x+2,y-8,2,1,Gold);
  } else if (item == 21) {
    for (int i=0;i<3;++i) {
      int xx=x+(i-1)*4, yy=y-(i==1?3:0);
      r.rect(xx-2,yy-5,5,5,Ink);r.rect(xx-1,yy-5,3,4,rgb(139,76,93));
      r.rect(xx-1,yy-5,1,1,rgb(210,140,143));
    }
    r.rect(x-2,y-10,5,2,Leaf);r.rect(x,y-9,1,3,Green);
  } else if (item == 22) {
    r.rect(x-7,y-10,15,5,Ink);r.rect(x-6,y-9,13,4,Earth);
    r.rect(x-5,y-4,11,3,Ink);r.rect(x-6,y-6,13,3,Water);
    r.rect(x-4,y-3,9,2,Cream);r.rect(x-4,y-8,9,2,Gold);
    r.rect(x-2,y-8,2,1,Leaf);r.rect(x+2,y-7,2,1,Cream);
  } else if (item == 23) {
    r.rect(x-5,y-11,11,10,Ink);r.rect(x-4,y-10,9,8,rgb(165,80,53));
    r.rect(x-5,y-13,11,3,Gold);r.rect(x-4,y-12,9,1,Cream);
    r.rect(x-3,y-7,7,3,Cream);r.rect(x-1,y-6,3,1,rgb(167,85,50));
    r.rect(x-3,y-9,1,2,rgb(223,158,102));
  }
}
inline void world_stove(Raster &r, int x, int y) {
  // Masonry oven, enamel hob and copper cookware share the cottage palette.
  r.rect(x-21,y-38,43,36,Ink);r.rect(x-19,y-36,39,32,rgb(172,132,91));
  for(int row=0;row<4;++row)for(int col=0;col<5;++col){
    int xx=x-18+col*8-(row%2?4:0),yy=y-34+row*7;
    r.rect(xx,yy,7,6,(row+col)%3?rgb(181,138,94):rgb(156,111,74));
    r.rect(xx+1,yy,5,1,rgb(205,166,115));
  }
  r.rect(x-22,y-40,45,6,Ink);r.rect(x-21,y-40,43,3,rgb(95,111,101));
  r.rect(x-19,y-40,39,1,rgb(175,188,160));
  for(int xx:{-12,9}){r.rect(x+xx-5,y-39,11,3,Ink);r.rect(x+xx-3,y-40,7,2,rgb(71,78,68));}
  r.rect(x-12,y-28,25,21,Ink);r.rect(x-10,y-26,21,17,rgb(65,49,36));
  r.rect(x-8,y-23,17,11,rgb(130,71,37));r.rect(x-5,y-21,3,8,Gold);
  r.rect(x,y-24,4,12,rgb(224,139,54));r.rect(x-6,y-15,13,3,Earth);
  r.rect(x+7,y-19,2,3,farm_palette::Brass);r.rect(x-8,y-9,17,2,rgb(122,82,51));
  r.rect(x-18,y-4,37,3,farm_palette::WoodDark);r.rect(x-18,y-1,5,1,Ink);r.rect(x+14,y-1,5,1,Ink);
  r.rect(x-9,y-47,17,6,Ink);r.rect(x-8,y-47,15,4,rgb(182,112,58));
  r.rect(x-6,y-46,4,2,rgb(227,176,101));r.rect(x-7,y-49,13,2,Ink);
  r.line(x+8,y-46,x+12,y-46,Ink);r.line(x+12,y-46,x+12,y-43,Ink);
}
inline const sprites::Sprite &villager_sprite(int id, int dir = 2) {
  const sprites::Sprite *people[4][4] = {
      {&sprites::villager_0_2, &sprites::villager_0_1, &sprites::villager_0_0,
       &sprites::villager_0_3},
      {&sprites::villager_1_2, &sprites::villager_1_1, &sprites::villager_1_0,
       &sprites::villager_1_3},
      {&sprites::villager_2_2, &sprites::villager_2_1, &sprites::villager_2_0,
       &sprites::villager_2_3},
      {&sprites::villager_3_2, &sprites::villager_3_1, &sprites::villager_3_0,
       &sprites::villager_3_3}};
  return *people[id % 4][dir & 3];
}
inline const sprites::Sprite &villager_walking_sprite(int id,int dir,int) {
  return villager_sprite(id,dir);
}
inline const sprites::Rig &villager_rig(int id,int dir) {
  const sprites::Rig *all[4][4]={
{&sprites::villager_0_2_rig,&sprites::villager_0_1_rig,&sprites::villager_0_0_rig,&sprites::villager_0_3_rig},
{&sprites::villager_1_2_rig,&sprites::villager_1_1_rig,&sprites::villager_1_0_rig,&sprites::villager_1_3_rig},
{&sprites::villager_2_2_rig,&sprites::villager_2_1_rig,&sprites::villager_2_0_rig,&sprites::villager_2_3_rig},
{&sprites::villager_3_2_rig,&sprites::villager_3_1_rig,&sprites::villager_3_0_rig,&sprites::villager_3_3_rig}};
  return *all[id%4][dir&3];
}
inline void world_bed(Raster &r, int x, int y, int style=0) {
  const uint16_t quilts[]={rgb(116,139,88),rgb(156,101,77),rgb(113,142,143),rgb(164,143,87)};
  auto quilt=quilts[style&3];
  r.rect(x-19,y-42,38,39,Ink);r.rect(x-17,y-41,34,37,farm_palette::Wood);
  r.rect(x-17,y-43,34,4,farm_palette::WoodDark);r.rect(x-16,y-43,32,1,Gold);
  for(int side:{-1,1}){int xx=x+side*17-1;r.rect(xx,y-45,3,42,Ink);r.rect(xx,y-44,2,39,farm_palette::Brass);r.rect(xx,y-4,3,4,Ink);}
  r.rect(x-14,y-37,28,33,rgb(212,184,132));r.rect(x-13,y-36,26,31,Cream);
  r.rect(x-11,y-35,22,9,rgb(205,190,151));r.rect(x-10,y-36,20,8,rgb(246,225,180));
  r.rect(x-8,y-35,16,1,farm_palette::Highlight);r.rect(x-9,y-29,18,1,rgb(218,198,151));
  r.rect(x-14,y-25,28,21,quilt);r.rect(x-13,y-24,26,3,rgb(194,194,133));
  r.rect(x-12,y-21,2,15,blend(quilt,Cream,2));r.rect(x+11,y-21,2,15,blend(quilt,Ink,2));
  for(int yy=y-19;yy<y-5;yy+=6)for(int xx=x-7;xx<x+9;xx+=6){
    r.rect(xx,yy,3,2,blend(quilt,Cream,1));r.rect(xx+2,yy+2,2,1,blend(quilt,Ink,1));
  }
  r.rect(x-15,y-5,30,4,farm_palette::WoodDark);r.rect(x-15,y-5,30,1,Gold);
  r.rect(x-11,y-3,22,1,farm_palette::Grain);
}
inline void world_table(Raster &r, int x, int y) {
  for(int side:{-1,1}){int xx=x+side*14-1;r.rect(xx,y-12,4,12,Ink);r.rect(xx+1,y-11,2,10,farm_palette::WoodDark);r.rect(xx,y-1,5,1,Ink);}
  r.rect(x-21,y-20,42,15,Ink);r.rect(x-20,y-19,40,11,farm_palette::Wood);
  r.rect(x-19,y-19,38,2,Gold);r.rect(x-20,y-9,40,3,farm_palette::WoodDark);
  for(int xx=x-11;xx<x+18;xx+=10){r.rect(xx,y-17,1,7,farm_palette::Grain);r.line(xx+2,y-14,xx+6,y-14,farm_palette::Grain);}
  r.rect(x-9,y-20,17,9,rgb(224,198,142));r.rect(x-8,y-20,15,1,Cream);
  r.rect(x-3,y-24,6,7,Ink);r.rect(x-2,y-23,4,5,rgb(174,107,61));
  r.rect(x-1,y-25,2,4,Leaf);r.rect(x-4,y-27,3,3,Green);r.rect(x+1,y-28,3,3,rgb(198,126,88));
  r.rect(x+10,y-19,5,4,Ink);r.rect(x+11,y-19,3,3,Cream);r.rect(x+15,y-18,1,2,Cream);
}

template<class Canvas> inline void world_appliance(Canvas&r,int x,int y,bool tv) {
  if(tv) {
    r.line(x-8,y-36,x-2,y-30,Ink);r.line(x+8,y-37,x+2,y-30,Ink);
    r.rect(x-15,y-30,31,24,Ink);r.rect(x-14,y-29,29,22,farm_palette::WoodDark);
    r.rect(x-13,y-29,27,2,Gold);r.rect(x-13,y-26,25,17,farm_palette::Wood);
    r.rect(x-12,y-26,20,16,Ink);r.rect(x-11,y-25,18,14,rgb(64,91,91));
    r.rect(x-10,y-24,16,12,rgb(119,158,139));r.rect(x-8,y-23,12,4,rgb(158,185,151));
    r.rect(x-8,y-23,3,1,rgb(191,207,167));r.rect(x-7,y-19,3,3,rgb(218,193,123));
    r.line(x-10,y-13,x-4,y-18,Leaf);r.line(x-4,y-18,x+5,y-13,Green);
    r.rect(x+10,y-23,3,3,Ink);r.rect(x+10,y-23,2,2,Gold);
    r.rect(x+10,y-18,3,2,Ink);r.rect(x+10,y-18,2,1,farm_palette::Brass);
    for(int yy=y-14;yy<y-10;yy+=2)r.rect(x+10,yy,3,1,Ink);
    r.rect(x-14,y-6,29,4,farm_palette::Wood);r.rect(x-13,y-6,27,1,Gold);
    r.rect(x-11,y-2,4,2,Ink);r.rect(x+8,y-2,4,2,Ink);
  } else {
    r.rect(x-12,y-31,25,29,Ink);r.rect(x-11,y-30,23,27,farm_palette::Wood);
    r.rect(x-10,y-29,21,1,Gold);r.rect(x-9,y-27,19,23,Cream);
    r.rect(x-9,y-27,19,5,rgb(173,91,57));r.rect(x-7,y-28,2,3,Ink);r.rect(x+6,y-28,2,3,Ink);
    r.rect(x-7,y-20,15,1,rgb(211,184,135));
    for(int yy=0;yy<3;++yy)for(int xx=0;xx<4;++xx)r.rect(x-6+xx*4,y-17+yy*4,2,2,xx==2&&yy==1?rgb(178,87,56):rgb(160,133,88));
    r.rect(x-10,y-3,21,1,farm_palette::WoodDark);
  }
}
inline void world_cabinet(Raster&r,int x,int y,int width=38) {
  int left=x-width/2;
  r.rect(left,y-34,width,32,Ink);r.rect(left+1,y-33,width-2,30,farm_palette::WoodDark);
  r.rect(left+1,y-34,width-2,3,farm_palette::Wood);r.rect(left+2,y-34,width-4,1,Gold);
  for(int col=0;col<2;++col){int xx=left+3+col*(width/2-1),w=width/2-5;
    r.rect(xx,y-29,w,20,farm_palette::Wood);r.rect(xx+2,y-27,w-4,12,rgb(84,94,70));
    r.rect(xx+3,y-26,w-6,4,rgb(122,133,94));r.rect(xx+3,y-19,3,4,Cream);r.rect(xx+7,y-21,3,6,rgb(179,134,72));
    r.rect(xx+w-3,y-14,2,2,Gold);r.rect(xx+2,y-7,w-4,2,farm_palette::Wood);
  }
  r.rect(left+2,y-3,3,3,Ink);r.rect(left+width-5,y-3,3,3,Ink);
}
inline void world_fireplace(Raster&r,int x,int y,int frame) {
  r.rect(x-18,y-45,37,43,Ink);r.rect(x-16,y-44,33,40,rgb(151,128,94));
  for(int yy=0;yy<5;++yy)for(int xx=0;xx<4;++xx){int bx=x-15+xx*8-(yy%2?3:0),by=y-42+yy*8;
    r.rect(bx,by,7,7,(xx+yy)%2?rgb(178,151,108):rgb(164,137,99));r.rect(bx+1,by,5,1,rgb(205,177,128));}
  r.rect(x-20,y-40,41,5,Ink);r.rect(x-19,y-40,39,2,Gold);r.rect(x-18,y-37,37,2,farm_palette::WoodDark);
  r.rect(x-12,y-27,25,23,Ink);r.rect(x-9,y-30,19,3,Ink);r.rect(x-10,y-26,21,21,rgb(61,49,37));
  r.rect(x-8,y-12,17,7,rgb(129,65,35));r.line(x-9,y-7,x+7,y-10,Earth);r.line(x-5,y-11,x+9,y-7,farm_palette::WoodDark);
  int f=frame%3;r.rect(x-6,y-17-f,4,9+f,rgb(217,123,46));r.rect(x,y-20+f,5,12-f,Gold);r.rect(x-2,y-14,4,6,rgb(248,211,119));
  r.rect(x-21,y-4,43,4,Ink);r.rect(x-20,y-4,41,2,rgb(194,167,120));
}
inline void world_chair(Raster&r,int x,int y) {
  r.rect(x-9,y-25,18,16,Ink);r.rect(x-7,y-24,14,12,farm_palette::Wood);
  r.rect(x-6,y-23,12,2,Gold);r.rect(x-5,y-20,2,6,farm_palette::Grain);r.rect(x+3,y-20,2,6,farm_palette::Grain);
  r.rect(x-10,y-12,20,6,Ink);r.rect(x-8,y-12,16,4,rgb(143,153,93));r.rect(x-7,y-12,14,1,rgb(193,190,126));
  r.rect(x-8,y-6,3,6,Ink);r.rect(x+5,y-6,3,6,Ink);r.rect(x-7,y-5,1,4,farm_palette::Wood);
}
inline void world_shelf(Raster&r,int x,int y) {
  int left=x-128;
  r.rect(left,y-36,256,34,Ink);r.rect(left+2,y-35,252,31,farm_palette::WoodDark);
  for(int level=0;level<2;++level){int yy=y-31+level*14;
    for(int i=0;i<12;++i){int xx=left+8+i*20;uint16_t c=i%3==0?rgb(168,96,61):i%3==1?rgb(157,161,101):rgb(212,181,114);
      r.rect(xx,yy,13,11,Ink);r.rect(xx+1,yy-1,11,10,c);r.rect(xx+1,yy-1,11,2,Gold);r.rect(xx+3,yy+3,7,4,Cream);r.rect(xx+6,yy+4,2,2,Leaf);
    }
    r.rect(left+1,yy+11,254,3,farm_palette::Wood);r.rect(left+2,yy+11,252,1,Gold);
  }
  for(int xx=left+3;xx<left+256;xx+=62)r.rect(xx,y-35,3,32,farm_palette::Wood);
  r.rect(left,y-3,256,3,Ink);r.rect(left+1,y-3,254,1,farm_palette::Wood);
}
inline void world_counter(Raster&r,int x,int y) {
  r.rect(x,y-23,176,23,Ink);r.rect(x+2,y-21,172,19,farm_palette::Wood);
  for(int xx=5;xx<170;xx+=28){r.rect(x+xx,y-17,24,13,farm_palette::WoodDark);r.rect(x+xx+2,y-16,20,10,farm_palette::Wood);r.rect(x+xx+3,y-15,18,1,Gold);r.rect(x+xx+4,y-7,16,1,farm_palette::Grain);}
  r.rect(x-2,y-26,180,7,Ink);r.rect(x-1,y-26,178,4,rgb(208,158,91));r.rect(x,y-26,176,1,Gold);r.rect(x,y-21,176,2,farm_palette::WoodDark);
  for(int xx=7;xx<118;xx+=31){r.line(x+xx,y-24,x+xx+20,y-24,farm_palette::Grain);}
  // Wicker seed display and an open ledger.
  r.rect(x+10,y-33,39,8,Ink);r.rect(x+11,y-32,37,6,rgb(172,123,63));
  for(int xx=12;xx<48;xx+=4)r.rect(x+xx,y-31,2,4,Gold);
  for(int xx:{15,27,39}){r.rect(x+xx,y-42,9,10,farm_palette::WoodDark);r.rect(x+xx+1,y-41,7,8,Cream);r.rect(x+xx+3,y-39,3,3,Leaf);}
  r.rect(x+71,y-32,23,6,Ink);r.rect(x+72,y-32,21,4,Cream);r.rect(x+82,y-32,1,4,farm_palette::WoodDark);
  for(int xx:{74,85})r.rect(x+xx,y-30,6,1,rgb(148,133,89));
  // Brass and dark-green mechanical till: screen, key rows and coin drawer.
  r.rect(x+137,y-38,26,13,Ink);r.rect(x+139,y-37,22,10,rgb(102,119,93));
  r.rect(x+141,y-43,17,7,Ink);r.rect(x+142,y-42,15,4,rgb(68,86,69));r.rect(x+145,y-41,7,1,Gold);
  for(int yy=0;yy<2;++yy)for(int xx=0;xx<4;++xx)r.rect(x+142+xx*4,y-35+yy*3,2,2,Cream);
  r.rect(x+136,y-28,28,3,farm_palette::WoodDark);r.rect(x+147,y-27,7,1,Gold);
}
inline void world_signpost(Raster&r,int x,int y,int direction) {
  r.rect(x-2,y-13,5,13,Ink);r.rect(x-1,y-12,2,11,farm_palette::Wood);
  r.rect(x-12,y-27,25,15,Ink);r.rect(x-11,y-26,23,13,farm_palette::Wood);
  r.rect(x-10,y-26,21,1,Gold);r.rect(x-10,y-14,21,1,farm_palette::WoodDark);
  int dx=(direction==1)-(direction==3),dy=(direction==2)-(direction==0);
  r.line(x-4*dx,y-20-4*dy,x+4*dx,y-20+4*dy,Cream);
  r.line(x+4*dx,y-20+4*dy,x+dx+2*dy,y-20+dy-2*dx,Cream);
  r.line(x+4*dx,y-20+4*dy,x+dx-2*dy,y-20+dy+2*dx,Cream);
}

inline void world_window(Raster&r,int x,int y) {
  r.rect(x,y,34,23,Ink);r.rect(x+1,y+1,32,21,farm_palette::WoodDark);
  r.rect(x+2,y+1,30,1,Gold);r.rect(x+4,y+4,26,14,rgb(92,136,140));
  r.rect(x+5,y+5,24,4,rgb(146,176,157));r.rect(x+6,y+5,4,1,rgb(194,207,172));
  for(int i=0;i<4;++i){int xx=x+6+i*6;r.line(xx,y+16,xx+3,y+10,rgb(96,130,89));r.line(xx+3,y+10,xx+5,y+16,rgb(96,130,89));}
  r.rect(x+16,y+4,2,15,farm_palette::Brass);r.rect(x+4,y+11,26,1,farm_palette::Brass);
  r.rect(x+2,y+3,3,16,Cream);r.rect(x+29,y+3,3,16,Cream);
  r.rect(x-1,y+21,36,3,farm_palette::Wood);r.rect(x,y+21,34,1,Gold);
}
inline void world_rug(Raster&r,int x,int y,int w,int h) {
  r.rect(x,y,w,h,rgb(106,112,68));r.rect(x+1,y+1,w-2,h-2,rgb(146,153,91));
  r.outline(x+3,y+3,w-6,h-6,rgb(206,180,115));
  for(int xx=x+6;xx<x+w-5;xx+=6){r.rect(xx,y+5,2,1,rgb(192,181,116));r.rect(xx+1,y+h-6,2,1,rgb(192,181,116));}
  for(int xx=x+2;xx<x+w-1;xx+=4){r.rect(xx,y-1,2,1,rgb(200,183,128));r.rect(xx,y+h,2,1,rgb(200,183,128));}
}
inline void world_room(Raster&r,bool shop,bool lodge) {
  r.rect(105,47,302,293,Ink);r.rect(107,49,298,289,farm_palette::WoodDark);
  r.rect(112,50,288,286,rgb(193,146,91));
  for(int yy=80;yy<336;yy+=12)for(int xx=112;xx<400;xx+=48) {
    int width=std::min(48,400-xx),height=std::min(12,336-yy);
    auto h=hash(xx/48,yy/12);
    r.rect(xx,yy,width,height,h%3?rgb(191,143,87):rgb(199,151,93));
    r.rect(xx,yy,width,1,rgb(167,119,70));r.rect(xx,yy+1,width,1,rgb(210,163,103));
    int seam=xx+int((yy/12)%2?24:0);r.rect(seam,yy+1,1,height-1,rgb(167,119,70));
    if(width>26){r.line(xx+5,yy+5,xx+16,yy+5,rgb(179,130,76));r.line(xx+20,yy+8,xx+31,yy+8,rgb(201,152,90));}
  }
  r.rect(112,50,288,30,rgb(221,191,135));
  for(int xx=118;xx<396;xx+=16){r.rect(xx,55,2,2,rgb(205,177,121));r.rect(xx-1,57,4,1,rgb(210,181,125));}
  r.rect(112,49,288,3,Gold);r.rect(112,77,288,4,farm_palette::WoodDark);r.rect(113,77,286,1,Gold);
  r.rect(108,50,5,286,farm_palette::Wood);r.rect(108,50,1,286,Gold);
  r.rect(400,50,5,286,farm_palette::WoodDark);r.rect(400,50,1,286,farm_palette::Wood);
  world_window(r,170,54);world_window(r,315,54);
  world_rug(r,240,278,48,28);
  if(!shop&&!lodge)world_rug(r,280,223,72,48);
  r.rect(112,334,136,3,farm_palette::WoodDark);r.rect(281,334,119,3,farm_palette::WoodDark);
  r.rect(248,332,33,4,rgb(151,112,67));
}
