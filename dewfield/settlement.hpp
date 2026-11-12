// SPDX-License-Identifier: MIT
#pragma once
#include "world.hpp"
namespace farm {
struct TileBox {
 int x,y,w,h;
 constexpr bool contains(int xx,int yy)const{return xx>=x&&yy>=y&&xx<x+w&&yy<y+h;}
};
struct BuildingSite {
 Region area,inside;uint8_t kind,x,y;
 constexpr Point door()const{return {x+2,y+4};}
 constexpr TileBox footprint()const{return {x,y,5,4};}
};
inline constexpr BuildingSite Buildings[]={
 {Region::Farm,Region::Home,0,4,3},{Region::Town,Region::SeedShop,1,5,3},
 {Region::Town,Region::Lodge,2,22,3},{Region::Town,Region::CarpenterHome,3,23,14},
 {Region::Town,Region::ElderHome,4,9,14},{Region::Lake,Region::FisherHome,5,24,5}};
inline bool building_solid(uint8_t region,int x,int y){
 for(const auto&b:Buildings)if(unsigned(b.area)==region&&b.footprint().contains(x,y))return true;
 return false;
}
// Shared cutaway architecture: every solid wall tile has a matching painted
// partition; two-tile openings connect private rooms to the entrance hall.
enum class RoomKind:uint8_t {Living,Bedroom,Kitchen,Hall,GuestRoom,Reading};
inline bool partitioned(uint8_t r){return r==unsigned(Region::Home)||r==unsigned(Region::Lodge)||(r>=unsigned(Region::SeedHome)&&r<RegionCount);}
inline bool partition_wall(uint8_t r,int x,int y){
 if(!partitioned(r))return false;
 if(r==unsigned(Region::Lodge)) {
  if(y>=5&&y<=12&&(x==12||x==18))return true;
  return y==12&&x>=7&&x<=24&&x!=9&&x!=10&&x!=14&&x!=15&&x!=20&&x!=21;
 }
 if(y>=5&&y<=12&&(x==15||x==18)&&(y<9||y>11))return true;
 return y==12&&((x>=7&&x<=15)||(x>=18&&x<=24));
}
inline RoomKind room_kind(uint8_t r,int x,int y){
 if(y>=13)return RoomKind::Living;
 if(r==unsigned(Region::Lodge))return x<18?RoomKind::GuestRoom:RoomKind::Reading;
 if(x>=16&&x<=17)return RoomKind::Hall;
 bool right=x>=19,fish=r==unsigned(Region::FisherHome);
 return right!=fish?RoomKind::Kitchen:RoomKind::Bedroom;
}
inline constexpr Point NpcRest[]={{12,10},{12,10},{19,10},{12,10}};
enum class HomeProp:uint8_t {Bed,Cabinet,Stove,Table,Chair,Fireplace,Planter,Workbench,Bookcase,WoodRack,Divider,Armchair,Television,Calendar,KitchenCounter};
struct HomePiece {HomeProp kind;uint8_t x,y;uint8_t facing=2;};
inline constexpr HomePiece PlayerRoom[]={
 {HomeProp::Bed,10,9},{HomeProp::Cabinet,13,7,3},{HomeProp::Cabinet,23,6},{HomeProp::KitchenCounter,20,6},{HomeProp::Stove,21,10},
 {HomeProp::Fireplace,8,14},{HomeProp::Television,12,14},{HomeProp::Calendar,13,5},
 {HomeProp::Table,20,16},{HomeProp::Chair,18,16,1},{HomeProp::Chair,22,16,3},
 {HomeProp::Armchair,12,18,0},{HomeProp::Planter,8,19},{HomeProp::Planter,13,19}};
inline constexpr HomePiece SeedRoom[]={
 {HomeProp::Bed,10,9},{HomeProp::Cabinet,13,7,3},{HomeProp::Cabinet,23,6},{HomeProp::KitchenCounter,20,6},{HomeProp::Stove,22,10},
 {HomeProp::Workbench,21,16},{HomeProp::Table,10,16},{HomeProp::Chair,8,16,1},{HomeProp::Chair,12,16,3},
 {HomeProp::Planter,8,18},{HomeProp::Planter,23,19}};
inline constexpr HomePiece CarpenterRoom[]={
 {HomeProp::Bed,10,9},{HomeProp::Cabinet,13,7,3},{HomeProp::Cabinet,23,6},{HomeProp::KitchenCounter,20,6},{HomeProp::Stove,22,10},
 {HomeProp::Workbench,11,16},{HomeProp::Chair,11,18,0},{HomeProp::Table,20,15},
 {HomeProp::Chair,18,15,1},{HomeProp::Chair,22,15,3},{HomeProp::WoodRack,22,18},{HomeProp::Planter,8,19}};
inline constexpr HomePiece FisherRoom[]={
 {HomeProp::Bed,21,9},{HomeProp::Cabinet,23,6,3},{HomeProp::Cabinet,13,6},{HomeProp::KitchenCounter,10,6},{HomeProp::Stove,10,10},
 {HomeProp::Workbench,10,17},{HomeProp::Table,21,16},{HomeProp::Chair,19,16,1},{HomeProp::Chair,23,16,3},
 {HomeProp::WoodRack,10,19},{HomeProp::Planter,23,19}};
inline constexpr HomePiece ElderRoom[]={
 {HomeProp::Bed,10,9},{HomeProp::Cabinet,13,7,3},{HomeProp::Cabinet,23,6},{HomeProp::KitchenCounter,20,6},{HomeProp::Stove,22,10},
 {HomeProp::Fireplace,8,14},{HomeProp::Bookcase,12,15},{HomeProp::Table,20,17},
 {HomeProp::Chair,18,17,1},{HomeProp::Chair,22,17,3},{HomeProp::Armchair,11,18,1},
 {HomeProp::Planter,8,19},{HomeProp::Planter,14,18}};
inline constexpr HomePiece LodgeRoom[]={
 {HomeProp::Bed,10,9},{HomeProp::Bed,14,9},{HomeProp::Bookcase,21,7},{HomeProp::Fireplace,22,14},
 {HomeProp::Table,11,17},{HomeProp::Table,21,17},
 {HomeProp::Chair,9,17,1},{HomeProp::Chair,13,17,3},{HomeProp::Chair,19,17,1},{HomeProp::Chair,23,17,3}};
struct HomePlan {const HomePiece *pieces;unsigned count;constexpr const HomePiece*begin()const{return pieces;}constexpr const HomePiece*end()const{return pieces+count;}};
template<unsigned N>constexpr HomePlan plan(const HomePiece(&p)[N]){return {p,N};}
inline constexpr HomePlan Homes[]={plan(SeedRoom),plan(CarpenterRoom),plan(FisherRoom),plan(ElderRoom)};
inline constexpr HomePlan home_plan(uint8_t region){return region==unsigned(Region::Home)?plan(PlayerRoom):region==unsigned(Region::Lodge)?plan(LodgeRoom):Homes[region-unsigned(Region::SeedHome)];}
inline constexpr TileBox home_footprint(const HomePiece&p){
 int x=p.x,y=p.y;
 switch(p.kind){
 case HomeProp::Calendar:return {x,y,0,0}; // Wall-mounted, not a floor obstacle.
 case HomeProp::Bed:case HomeProp::Stove:return {x-1,y-2,3,3};
 case HomeProp::Cabinet:if(p.facing==3)return {x,y-1,2,3};if(p.facing==1)return {x-1,y-1,2,3};return {x-1,y-1,3,2};
 case HomeProp::KitchenCounter:case HomeProp::Fireplace:case HomeProp::Table:case HomeProp::WoodRack:return {x-1,y-1,3,2};
 case HomeProp::Workbench:case HomeProp::Bookcase:return {x-2,y-1,5,2};
 case HomeProp::Divider:return {x-2,y,5,1};
 case HomeProp::Armchair:return {x-1,y-1,2,2};
 default:return {x,y,1,1};
 }
}
inline bool furnished(uint8_t region){return partitioned(region);}
inline bool home_solid(uint8_t region,int x,int y){
 if(!furnished(region))return false;
 if(partition_wall(region,x,y))return true;
 for(const auto&p:home_plan(region))if(home_footprint(p).contains(x,y))return true;
 return false;
}
}
