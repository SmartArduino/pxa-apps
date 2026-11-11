// SPDX-License-Identifier: MIT
#pragma once
#include "settlement.hpp"
#include "palette.hpp"
#include "room_wall_sprites.hpp"
#include <array>

namespace farm::walls {
// Orthographic cutaway: a surface at height Rise projects exactly Rise pixels
// north of its floor footprint. Every orientation uses the same eight-pixel
// band. Connections extend that band to the adjacent cell, including the shell.
constexpr int Tile=16, Inset=4, Band=8, Rise=28, Front=Inset+Band;
constexpr int ImageHeight=Rise+Front;
constexpr int ShellLeft=104,ShellRight=400,ShellBack=80,ShellBottom=336;
enum Link:unsigned {North=1,East=2,South=4,West=8};
inline unsigned links(uint8_t region,int x,int y) {
 unsigned n=0;
 if(partition_wall(region,x,y-1)||y==5)n|=North;
 if(partition_wall(region,x+1,y)||x==24)n|=East;
 if(partition_wall(region,x,y+1))n|=South;
 if(partition_wall(region,x-1,y)||x==7)n|=West;
 return n;
}
constexpr bool footprint(unsigned n,int x,int y) {
 // One sample outside a cell is sufficient to resolve joined outlines. An
 // active link guarantees the reciprocal band in the adjoining wall cell.
 if(x<0)return (n&West)&&y>=Inset&&y<Front;
 if(x>=Tile)return (n&East)&&y>=Inset&&y<Front;
 if(y<0)return (n&North)&&x>=Inset&&x<Front;
 if(y>=Tile)return (n&South)&&x>=Inset&&x<Front;
 bool center=x>=Inset&&x<Front&&y>=Inset&&y<Front;
 return center||((n&North)&&x>=Inset&&x<Front&&y<Inset)
   ||((n&South)&&x>=Inset&&x<Front&&y>=Front)
   ||((n&West)&&y>=Inset&&y<Front&&x<Inset)
   ||((n&East)&&y>=Inset&&y<Front&&x>=Front);
}
constexpr bool front_face(unsigned n,int x) {
 return footprint(n,x,Front-1)&&!footprint(n,x,Front);
}
constexpr bool capital(unsigned n,int x,int y) {
 bool straight=n==(North|South)||n==(East|West);
 if(straight)return false;
 int left=n==North?3:2,top=n==North?8:2;
 const auto&m=n==North?room_wall_sprites::Collar:room_wall_sprites::Post;
 return x>=left&&x<left+m.w&&y>=top&&y<top+m.h&&room_wall_sprites::index(m,x-left,y-top)!=0;
}
constexpr int cap_height(unsigned){return Tile;}
constexpr bool cap_footprint(unsigned n,int x,int y){return footprint(n,x,y)||capital(n,x,y);}
// A single packed image contains cap, plaster and skirting. Connected pixels
// are shaded once, so fading a junction cannot create dark double-blended seams.
enum Color:uint8_t {Clear,Edge,Cap,CapLight,CapShade,Plaster,PlasterLight,
                    PlasterSoft,Wood,WoodLight,Capital};
inline constexpr uint16_t Colors[]={0,farm_palette::rgb(89,46,24),
 farm_palette::rgb(177,105,48),farm_palette::rgb(231,170,83),
 farm_palette::rgb(130,69,29),farm_palette::rgb(239,217,177),
 farm_palette::rgb(250,233,199),farm_palette::rgb(226,196,147),
 farm_palette::rgb(164,90,36),farm_palette::rgb(211,143,63),farm_palette::rgb(177,105,48)};
// Shade the boundary of the union, never the ends of individual rectangles.
// The same rule closes free ends, mitres corners and leaves connected tops open.
template<class Occupancy>
constexpr uint8_t cap_color(const Occupancy&filled,int x,int y) {
 if(!filled(x,y))return Clear;
 if(!filled(x-1,y)||!filled(x+1,y)||!filled(x,y-1)||!filled(x,y+1))return Edge;
 if(!filled(x-2,y)||!filled(x,y-2))return CapLight;
 if(!filled(x+2,y)||!filled(x,y+2))return CapShade;
 return Cap;
}
constexpr uint8_t face_color(bool side,int row) {
 if(row<0||row>=Rise)return Clear;
 if(row==Rise-1||row==Rise-4)return Edge;
 if(row==Rise-3)return WoodLight;
 if(row==Rise-2)return Wood;
 if(side)return Wood;
 if(row==0)return PlasterLight;
 if(row==Rise-5)return PlasterSoft;
 return Plaster;
}
constexpr uint8_t color_at(unsigned n,int x,int y) {
 if(capital(n,x,y))return Capital;
 if(y<Tile&&footprint(n,x,y))return cap_color([n](int xx,int yy){return cap_footprint(n,xx,yy);},x,y);
 if(y<Front||!front_face(n,x))return Clear;
 bool framed=n!=(North|South)&&n!=(East|West)&&x>=Inset&&x<Front;
 return face_color(framed||!footprint(n,x-1,Front-1)||!footprint(n,x+1,Front-1),y-Front);
}
// Front cutaway face sits outside the walkable floor, as in the concept. The
// same rectangle is used by painting and picking, including the visible door.
inline constexpr TileBox FrontDoor{248,316,33,58};
inline const Portal*front_exit(uint8_t region) {
 for(const auto&p:Portals)if(unsigned(p.from)==region&&p.x==16&&p.y==20)return &p;
 return nullptr;
}
struct PostSite {int x,y,depth;};
inline unsigned shell_posts(uint8_t region,PostSite*out){
 unsigned count=0;
 for(int x:{108,404}){out[count++]={x,48,93};out[count++]={x,338,337};
  if(partitioned(region))out[count++]={x,172,205};}
 for(int x=7;x<=24;++x)if(partition_wall(region,x,5))out[count++]={x*Tile+8,48,93};
 return count;
}
constexpr auto images() {
 std::array<std::array<uint8_t,Tile*ImageHeight/2>,16> bank{};
 for(unsigned n=0;n<16;++n)for(int y=0;y<ImageHeight;++y)for(int x=0;x<Tile;++x) {
  int p=y*Tile+x;bank[n][p/2]|=color_at(n,x,y)<<((p&1)*4);
 }
 return bank;
}
inline constexpr auto Images=images();
static_assert(sizeof Images==5120);
}
