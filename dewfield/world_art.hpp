// SPDX-License-Identifier: MIT
#pragma once
// Included inside namespace art. All maps share the original compiled atlas.
// Small hand-authored foods share the scene palette and an exact ground edge.
// y is the bottom baseline, so backpack, carried and forage drawings agree.
template<class Canvas> inline void food(Canvas &r, int x, int y, int item) {
  if(item==46||item==47) {
    r.rect(x-5,y-10,11,9,Ink);r.rect(x-4,y-9,9,7,item==46?rgb(164,116,74):rgb(110,132,120));
    r.rect(x-4,y-11,9,3,item==46?Gold:rgb(165,181,158));
    if(item==46){r.rect(x-2,y-7,4,2,rgb(225,153,117));r.rect(x+1,y-6,2,3,rgb(225,153,117));}
    else {r.line(x-3,y-8,x+3,y-4,Ink);r.rect(x-2,y-2,5,1,Ink);}
    return;
  }
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
  furniture(r,furniture_sprites::Stove,x,y);
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
  furniture(r,furniture_sprites::Bed,x,y,style);
}
inline void world_table(Raster &r, int x, int y,int style=0) {
  furniture(r,furniture_sprites::Table,x,y);
  if(style==4) {
    r.rect(x-4,y-24,8,7,Cream);r.rect(x-2,y-26,4,2,Gold);r.rect(x+4,y-23,3,2,Cream);r.rect(x-6,y-22,2,4,Cream);r.rect(x-1,y-21,2,2,rgb(180,114,105));
  } else if(style==2) {
    r.rect(x-10,y-21,20,8,rgb(120,150,160));r.outline(x-8,y-20,13,5,Cream);r.line(x-4,y-20,x+5,y-16,Gold);
  } else if(style==3) {
    r.rect(x-7,y-20,14,6,Cream);r.rect(x-4,y-19,8,3,rgb(111,145,145));r.rect(x+3,y-20,2,4,Gold);
  } else {
    r.rect(x-3,y-24,6,7,Ink);r.rect(x-2,y-23,4,5,rgb(174,107,61));
    r.rect(x-1,y-25,2,4,Leaf);r.rect(x-4,y-27,3,3,Green);r.rect(x+1,y-28,3,3,rgb(198,126,88));
  }
  r.rect(x+10,y-19,5,4,Ink);r.rect(x+11,y-19,3,3,Cream);r.rect(x+15,y-18,1,2,Cream);
}

template<class Canvas> inline void world_appliance(Canvas&r,int x,int y,bool tv) {
  furniture(r,tv?furniture_sprites::Television:furniture_sprites::Calendar,x,y);
}
inline void world_cabinet(Raster&r,int x,int y,int width=38) {
  (void)width;furniture(r,furniture_sprites::Cabinet,x,y);
}
inline void world_fireplace(Raster&r,int x,int y,int frame) {
  furniture(r,furniture_sprites::Fireplace,x,y);
  int f=frame%3;
  r.rect(x-4,y-11-f,2,4+f,rgb(211,120,48));r.rect(x+1,y-13+f,2,5-f,Gold);
  r.rect(x-1,y-9,2,2,rgb(246,204,118));
}
inline void world_chair(Raster&r,int x,int y,int style=0) {
  furniture(r,furniture_sprites::Chair,x,y,style);
}
inline void world_shelf(Raster&r,int x,int y) {
  for(int bay=0;bay<4;++bay)furniture(r,furniture_sprites::ShelfBay,x-96+bay*64,y);
}
inline void world_counter(Raster&r,int x,int y) {
  for(int bay=0;bay<4;++bay)furniture(r,furniture_sprites::CounterBay,x+22+bay*44,y);
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

inline void room_material(Raster&r,const room_wall_sprites::Material&m,int x,int y,int w,int h,int alpha=8) {
 int x0=std::max(x,r.ox),x1=std::min(x+w,r.ox+r.w),y0=std::max(y,r.oy),y1=std::min(y+h,r.oy+r.h);
 for(int yy=y0;yy<y1;++yy)for(int xx=x0;xx<x1;++xx){
  auto color=room_wall_sprites::sample(m,(xx-x)%m.w,(yy-y)%m.h);if(!color)continue;
  if(r.tint)color=r.tint[color];auto&dst=r.p[(yy-r.oy)*r.w+xx-r.ox];dst=alpha==8?color:blend(dst,color,alpha);
 }
}
inline void room_post(Raster&r,int x,int y,int alpha=8){
 const auto&m=room_wall_sprites::Post;room_material(r,m,x-m.w/2,y-m.h/2,m.w,m.h,alpha);
}
inline void world_window(Raster&r,int x,int y) {
 const auto&m=room_wall_sprites::Window;room_material(r,m,x,y,m.w,m.h);
}
inline void world_rug(Raster&r,int x,int y,int w,int h) {
  r.rect(x,y,w,h,rgb(106,112,68));r.rect(x+1,y+1,w-2,h-2,rgb(146,153,91));
  r.outline(x+3,y+3,w-6,h-6,rgb(206,180,115));
  for(int xx=x+6;xx<x+w-5;xx+=6){r.rect(xx,y+5,2,1,rgb(192,181,116));r.rect(xx+1,y+h-6,2,1,rgb(192,181,116));}
  for(int xx=x+2;xx<x+w-1;xx+=4){r.rect(xx,y-1,2,1,rgb(200,183,128));r.rect(xx,y+h,2,1,rgb(200,183,128));}
}
// Kitchen tiles and cutaway walls use exactly the collision layout. These
// static pixels are baked once on room entry, with no additional framebuffer.
inline void room_floors(Raster&r,uint8_t region) {
 if(!farm::partitioned(region))return;
 for(int ty=5;ty<=12;++ty)for(int tx=7;tx<=24;++tx) {
  if(farm::partition_wall(region,tx,ty))continue;
  auto room=farm::room_kind(region,tx,ty);
  if(room==farm::RoomKind::GuestRoom){
    int x=tx*16,y=ty*16;auto c=rgb(187,145,94);r.rect(x,y,16,16,c);r.rect(x,y,16,1,blend(c,Ink,1));
    if((tx+ty)%2)r.rect(x,y+1,1,15,blend(c,Ink,1));r.rect(x+4,y+7,7,1,blend(c,Cream,1));continue;
  }
  if(room!=farm::RoomKind::Kitchen)continue;
  auto c=region==unsigned(farm::Region::FisherHome)?rgb(184,194,173):rgb(205,198,165);
  if((tx+ty)%2)c=blend(c,Cream,1);
  int x=tx*16,y=ty*16;r.rect(x,y,16,16,c);
  r.rect(x,y,16,1,blend(c,Ink,1));r.rect(x,y,1,16,blend(c,Ink,1));
 }
}
inline void room_partitions(Raster&r,uint8_t region) {
 if(!farm::partitioned(region))return;
 using namespace farm::walls;
 // Contact follows the actual eight-pixel wall band, rather than a whole
 // collision tile. This cache is rebuilt only when entering another room.
 for(int ty=5;ty<=12;++ty)for(int tx=7;tx<=24;++tx)if(farm::partition_wall(region,tx,ty)) {
  auto n=links(region,tx,ty);int x=tx*Tile,y=ty*Tile;
  for(int col=0;col<Tile;){
   if(!front_face(n,col)){++col;continue;}int first=col;
   while(col<Tile&&front_face(n,col))++col;
   shadow_strip(r,x+first,y+Front,col-first,1);
  }
  for(int row=0;row<Tile;++row)if(footprint(n,Front-1,row)&&!footprint(n,Front,row))
   shadow_strip(r,x+Front,y+row,2,1);
 }
 auto threshold=[&](int x,int y,int w,int h){
  r.rect(x,y,w,h,rgb(184,142,86));
  if(w>h){r.rect(x,y,w,1,rgb(212,173,109));r.rect(x,y+h-1,w,1,rgb(145,101,55));}
  else {r.rect(x,y,1,h,rgb(212,173,109));r.rect(x+w-1,y,1,h,rgb(145,101,55));}
 };
 if(region==unsigned(farm::Region::Lodge))for(int x:{9,14,20})threshold(x*Tile,12*Tile+Inset,2*Tile,Band);
 else for(int x:{15,18})threshold(x*Tile+Inset,10*Tile,Band,2*Tile);
}
inline void room_wall(Raster&r,int tx,int ty,unsigned links,int viewer_x,int viewer_y) {
 using namespace farm::walls;
 int x=tx*Tile,y=ty*Tile-Rise;
 int left=std::max(0,r.ox-x),right=std::min(Tile,r.ox+r.w-x);
 int top=std::max(0,r.oy-y),bottom=std::min(ImageHeight,r.oy+r.h-y);
 if(left>=right||top>=bottom)return;
 const auto&image=Images[links&15];
 bool fade=viewer_y<ty*Tile+Front&&viewer_y>ty*Tile-Rise-24
   &&viewer_x>x-18&&viewer_x<x+Tile+18;
 for(int row=top;row<bottom;++row){
  auto*dst=r.p+(y+row-r.oy)*r.w+x+left-r.ox;
  for(int col=left;col<right;++col,++dst){
   int p=row*Tile+col;auto index=(image[p/2]>>((p&1)*4))&15;
   if(!index)continue;
   uint16_t color=0;
   using namespace room_wall_sprites;
   if(capital(links&15,col,row)){
    const auto&m=(links&15)==North?Collar:Post;int sx=(links&15)==North?3:2,sy=(links&15)==North?8:2;
    color=sample(m,col-sx,row-sy);
   }else if(row<Tile&&footprint(links&15,col,row)){
    bool vertical=(links&15)==(North|South)||row<Inset||row>=Front;
    color=vertical?sample(BeamV,std::clamp(col-Inset,0,Band-1),(ty*Tile+row)%BeamV.h)
                  :sample(BeamH,(tx*Tile+col)%BeamH.w,std::clamp(row-Inset,0,Band-1));
   }else{
    int f=row-Front;
    if(f>=Rise-Skirting.h)color=sample(Skirting,(tx*Tile+col)%Skirting.w,f-(Rise-Skirting.h));
    else if(index==Wood||index==WoodLight||index==Edge)color=sample(Jamb,std::clamp(col-Inset,0,Band-1),std::clamp(f,0,Rise-1));
    else color=sample(room_wall_sprites::Plaster,(tx*Tile+col)%room_wall_sprites::Plaster.w,std::max(0,f)%room_wall_sprites::Plaster.h);
   }
   if(r.tint)color=r.tint[color];int alpha=8;
   if(fade){
    int margin=std::min({5,x+col-(viewer_x-18),viewer_x+18-1-(x+col),
                         y+row-(viewer_y-38),viewer_y+4-1-(y+row)});
    if(margin>0)alpha-=margin;
   }
   *dst=alpha==8?color:blend(*dst,color,alpha);
  }
 }
}
// The perimeter uses the same cap thickness and elevation as the partitions.
// It is static; only interior walls need to join the moving-object depth queue.
inline void room_shell(Raster&r,uint8_t region) {
 using namespace farm::walls;using namespace room_wall_sprites;
 // The approved drawing is a timber-framed cutaway. The south wall presents
 // its outside face below the floor, keeping the entrance and lounge readable.
 room_material(r,room_wall_sprites::Plaster,112,52,288,Rise-6);
 room_material(r,Skirting,112,74,288,6);
 for(int x:{ShellLeft,ShellRight})room_material(r,BeamV,x,44,Band,330);
 room_material(r,BeamH,ShellLeft,44,ShellRight-ShellLeft+Band,Band);
 for(auto part:{farm::TileBox{112,342,136,26},farm::TileBox{281,342,119,26}}){
  room_material(r,room_wall_sprites::Plaster,part.x,part.y,part.w,part.h);
  room_material(r,Skirting,part.x,368,part.w,6);
 }
 room_material(r,BeamH,104,334,144,8);room_material(r,BeamH,281,334,127,8);
 world_window(r,165,343);world_window(r,322,343);
 room_material(r,Door,248,334,Door.w,Door.h);
 // The small threshold is on the walkable floor in front of the actual door.
 r.rect(252,330,24,3,rgb(188,137,73));r.rect(252,330,24,1,Gold);
}
inline void world_room(Raster&r,bool shop,bool lodge,int owner=-1) {
  int theme=shop?1:lodge?2:owner>=0?owner+3:0;
  constexpr uint16_t floors[]={rgb(193,146,91),rgb(199,159,106),rgb(166,158,133),rgb(198,158,101),rgb(172,125,78),rgb(153,145,115),rgb(199,152,101)};
  auto floor=floors[theme];auto seam=blend(floor,Ink,2),light=blend(floor,Cream,2);
  r.rect(102,42,308,333,Ink);
  r.rect(112,50,288,286,floor);
  int plank=theme==2?24:48,rows=theme==2?16:12;
  for(int yy=80;yy<336;yy+=rows)for(int xx=112;xx<400;xx+=plank) {
    int width=std::min(plank,400-xx),height=std::min(rows,336-yy);auto h=hash(xx/plank,yy/rows);
    r.rect(xx,yy,width,height,h%3?floor:light);
    r.rect(xx,yy,width,1,seam);r.rect(xx,yy+1,width,1,light);
    int join=xx+int((yy/rows)%2?plank/2:0);r.rect(join,yy+1,1,height-1,seam);
    if(width>26){r.line(xx+5,yy+5,xx+13+h%8,yy+5,blend(floor,Ink,1));r.line(xx+23,yy+8,xx+31,yy+8,light);}
  }
  uint8_t region=unsigned(shop?farm::Region::SeedShop:lodge?farm::Region::Lodge:owner>=0?farm::Region(unsigned(farm::Region::SeedHome)+owner):farm::Region::Home);
  room_floors(r,region);
  room_shell(r,region);
  for(int xx=118;xx<396;xx+=16){r.rect(xx,55,2,2,rgb(205,177,121));r.rect(xx-1,57,4,1,rgb(210,181,125));}
  if(lodge){world_window(r,134,54);world_window(r,230,54);world_window(r,337,54);}
  else {world_window(r,170,54);world_window(r,315,54);}
  if(owner<0&&!shop&&!lodge){world_rug(r,176,282,60,37);world_rug(r,286,256,84,41);world_rug(r,144,163,68,27);}
  if(lodge){world_rug(r,248,231,33,86);world_rug(r,147,265,75,39);world_rug(r,307,265,75,39);}
}
