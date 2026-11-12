// SPDX-License-Identifier: MIT
#pragma once
#include "settlement.hpp"
#include "palette.hpp"
#include "room_wall_sprites.hpp"
#include <array>
#include <algorithm>
#include <cstdlib>

namespace farm::walls {
// Orthographic cutaway: a surface at height Rise projects exactly Rise pixels
// north of its floor footprint. Every orientation uses the same eight-pixel
// band. Connections extend that band to the adjacent cell, including the shell.
constexpr int Tile=16, Inset=4, Band=8, Rise=28, Front=Inset+Band;
constexpr int ImageHeight=Rise+Front;
constexpr int ReturnRise=10;
constexpr int ShellLeft=104,ShellRight=400,ShellBack=80,ShellBottom=336;
enum Link:unsigned {North=1,East=2,South=4,West=8,DoorCorner=16};
inline unsigned links(uint8_t region,int x,int y) {
 unsigned n=0;
 if(partition_wall(region,x,y-1)||y==5)n|=North;
 if(partition_wall(region,x+1,y)||x==24)n|=East;
 if(partition_wall(region,x,y+1))n|=South;
 if(partition_wall(region,x-1,y)||x==7)n|=West;
 // The lintel joins the transverse wall here. Preserve the elbow instead of
 // hiding it under a square capital. This is overhead trim, not floor solid.
 if(region!=unsigned(Region::Lodge)&&y==12&&(x==15||x==18))n|=DoorCorner;
 return n;
}
constexpr unsigned painted_links(unsigned n){return (n&15)|((n&DoorCorner)?North:0);}
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
constexpr bool timber_face(unsigned n,int x) {
 bool framed=n!=(North|South)&&n!=(East|West)&&x>=Inset&&x<Front;
 return framed||!footprint(n,x-1,Front-1)||!footprint(n,x+1,Front-1);
}
constexpr bool capital(unsigned n,int x,int y) {
 if(n&DoorCorner)return false;
 bool straight=n==(North|South)||n==(East|West);
 if(straight)return false;
 int left=n==North?3:2,top=n==North?4:2;
 const auto&m=n==North?room_wall_sprites::JambCap:room_wall_sprites::Post;
 return x>=left&&x<left+m.w&&y>=top&&y<top+m.h&&room_wall_sprites::index(m,x-left,y-top)!=0;
}
constexpr int cap_height(unsigned){return Tile;}
constexpr bool cap_footprint(unsigned n,int x,int y){return footprint(n,x,y)||capital(n,x,y);}
constexpr bool cap_shape(unsigned n,int x,int y){
 if(n&DoorCorner){
  if(y<-ReturnRise)return false;
  if(y==-ReturnRise&&(x==Inset||x==Front-1))return false;
 }
 return footprint(painted_links(n),x,y)||capital(n,x,y);
}
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
 if(y<Tile&&cap_shape(n,x,y))return cap_color([n](int xx,int yy){return cap_shape(n,xx,yy);},x,y);
 if(y<Front||!front_face(n,x))return Clear;
 return face_color(timber_face(n,x),y-Front);
}
// Front cutaway face sits outside the walkable floor, as in the concept. The
// same rectangle is used by painting and picking, including the visible door.
inline constexpr TileBox FrontDoor{248,316,33,58};
inline const Portal*front_exit(uint8_t region) {
 for(const auto&p:Portals)if(unsigned(p.from)==region&&p.x==16&&p.y==20)return &p;
 return nullptr;
}
struct PostSite {int x,y,depth;};
struct DoorSite {int x,y,side;};
inline constexpr DoorSite RoomDoors[]={{248,144,-1},{296,144,1}};
inline unsigned door_count(uint8_t region){return partitioned(region)&&region!=unsigned(Region::Lodge)?2:0;}
inline unsigned door_open(const DoorSite&door,int px,int py){
 int dx=std::abs(px-door.x),dy=std::max({door.y-py,py-(door.y+48),0});
 int distance=std::max(dx,dy);
 return unsigned(std::clamp((56-distance)/3,0,8));
}
inline unsigned shell_posts(uint8_t region,PostSite*out){
 unsigned count=0;
 for(int x:{108,404}){out[count++]={x,48,93};out[count++]={x,338,337};
  if(partitioned(region))out[count++]={x,172,205};}
 for(int x=7;x<=24;++x)if(partition_wall(region,x,5))out[count++]={x*Tile+8,48,93};
 return count;
}
constexpr auto images() {
 std::array<std::array<uint8_t,Tile*ImageHeight/2>,18> bank{};
 for(unsigned image=0;image<18;++image)for(int y=0;y<ImageHeight;++y)for(int x=0;x<Tile;++x) {
  unsigned n=image<16?image:DoorCorner|(image==16?East:West);
  int p=y*Tile+x;bank[image][p/2]|=color_at(n,x,y)<<((p&1)*4);
 }
 return bank;
}
inline constexpr auto Images=images();
// Only the two door returns extend above their tile image. Store their tiny
// crests separately instead of padding every wall tile or adding a layer.
constexpr auto crests(){
 std::array<std::array<uint8_t,Tile*ReturnRise/2>,2> bank{};
 for(unsigned i=0;i<2;++i)for(int y=0;y<ReturnRise;++y)for(int x=0;x<Tile;++x){
  int p=y*Tile+x;bank[i][p/2]|=color_at(DoorCorner|(i?West:East),x,y-ReturnRise)<<((p&1)*4);
 }
 return bank;
}
inline constexpr auto Crests=crests();
static_assert(sizeof Images==5760);
static_assert(sizeof Crests==160);
inline constexpr const auto&image(unsigned n){return Images[(n&DoorCorner)?((n&West)?17:16):(n&15)];}
}
