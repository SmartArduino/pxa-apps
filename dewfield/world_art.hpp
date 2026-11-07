// SPDX-License-Identifier: MIT
#pragma once
// Included inside namespace art. All maps share the original compiled atlas.
// Small hand-authored foods share the scene palette and an exact ground edge.
// y is the bottom baseline, so backpack, carried and forage drawings agree.
template<class Canvas> inline void food(Canvas &r, int x, int y, int item) {
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
  r.rect(x-20,y-31,41,30,Ink);r.rect(x-18,y-28,37,24,rgb(166,146,105));
  r.rect(x-19,y-32,39,5,Earth);r.rect(x-17,y-31,35,2,Cream);
  r.rect(x-12,y-24,24,17,Ink);r.rect(x-10,y-22,20,13,rgb(75,66,49));
  r.rect(x-7,y-16,14,5,rgb(187,83,36));r.rect(x-3,y-20,6,9,Gold);
  r.rect(x-5,y-34,11,4,Ink);r.rect(x-4,y-36,9,4,rgb(119,134,111));
  r.rect(x-2,y-37,5,1,Cream);
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
inline void world_bed(Raster &r, int x, int y) {
  r.rect(x - 17, y - 35, 34, 34, Ink);
  r.rect(x - 15, y - 34, 30, 32, rgb(166, 112, 61));
  r.rect(x - 13, y - 30, 26, 28, Cream);
  r.rect(x - 10, y - 29, 20, 7, rgb(240, 213, 161));
  r.rect(x - 13, y - 19, 26, 17, rgb(131, 150, 104));
  r.rect(x - 11, y - 18, 2, 15, rgb(163, 173, 118));
  r.rect(x - 16, y - 3, 3, 3, Ink);
  r.rect(x + 13, y - 3, 3, 3, Ink);
}
inline void world_table(Raster &r, int x, int y) {
  r.rect(x - 18, y - 18, 36, 15, Ink);
  r.rect(x - 16, y - 17, 32, 11, rgb(189, 137, 72));
  r.rect(x - 15, y - 16, 30, 2, Gold);
  r.rect(x - 14, y - 3, 3, 3, Ink);
  r.rect(x + 11, y - 3, 3, 3, Ink);
  r.rect(x - 4, y - 20, 8, 5, Cream);
  r.rect(x - 2, y - 19, 4, 2, rgb(112, 139, 88));
}

template<class Canvas> inline void world_appliance(Canvas&r,int x,int y,bool tv) {
  r.rect(x-10,y-24,21,22,Ink);r.rect(x-9,y-23,19,20,Earth);
  r.rect(x-8,y-22,17,17,tv?rgb(73,106,111):Cream);
  if(tv) {r.rect(x-6,y-20,11,11,rgb(139,177,159));r.rect(x-4,y-18,3,3,Gold);
    r.line(x-6,y-11,x-2,y-15,Leaf);r.line(x-2,y-15,x+4,y-11,Leaf);r.rect(x+7,y-18,1,2,Gold);}
  else {r.rect(x-8,y-22,17,4,rgb(160,83,54));for(int yy=0;yy<3;++yy)for(int xx=0;xx<4;++xx)r.rect(x-6+xx*4,y-15+yy*4,2,2,xx==2&&yy==1?Gold:Earth);}
  r.rect(x-8,y-2,3,2,Ink);r.rect(x+6,y-2,3,2,Ink);
}
