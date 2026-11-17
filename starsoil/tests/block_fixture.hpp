// SPDX-License-Identifier: MIT
#pragma once
#include "../core.hpp"
#include <cstring>
inline bool block_fixture(starsoil::State &s,const char *name) {
  using namespace starsoil;
  const bool soil=std::strcmp(name,"block-soil")==0,roof=std::strcmp(name,"block-roof")==0;
  if(!soil && !roof)return false;
  for(auto &m:s.mobs)m={};
  s.clock=100000;s.invulnerable=60000;
  const Slot contents[]={{WoodPick,1},{Soil,24},{Boards,999},{Wood,99},{Rock,2},{Glass,100},{WaterBucket,5},{Torches,10}};
  for(int i=0;i<8;++i)s.bag[i]=contents[i];
  s.bag[8]={Soil,999};s.bag[9]={Boards,24};s.chest[0]={Wood,999};s.selected=0;
  if(soil) {
    for(int x=85;x<119;++x) {
      int ground=x<96?42:x<104?44:45;s.heights[x]=uint8_t(ground);
      for(int y=20;y<59;++y) {
        set_tile(s,x,y,y<ground?Air:y<48?Dirt:Stone);s.walls[y*WorldW+x]=Air;
      }
    }
    // A real, enclosed water pocket sits beside dry dirt for comparison.
    for(int x=106;x<=112;++x)for(int y=43;y<=46;++y)
      set_tile(s,x,y,y==46 || x==106 || x==112?Stone:y>=44?Water:Air);
    s.player.x=100*16+8;s.player.y=44*16;s.home_x=100;s.home_y=44;
  } else {
    for(int x=38;x<=57;++x)for(int y=19;y<32;++y) {
      set_tile(s,x,y,y==31?Grass:Air);s.walls[y*WorldW+x]=Air;s.heights[x]=31;
    }
    for(int x=40;x<=54;++x)set_tile(s,x,25,Plank);
    for(int x=41;x<=53;++x)for(int y=26;y<=30;++y) {
      s.walls[y*WorldW+x]=Plank;
      if(x==41 || x==53)set_tile(s,x,y,Plank);
    }
    set_tile(s,53,29,DoorTop);set_tile(s,53,30,Door);
    set_tile(s,44,30,Bench);set_tile(s,46,29,Torch);set_tile(s,49,30,Bed);
    s.player.x=48*16+8;s.player.y=31*16;s.home_x=48;s.home_y=31;
  }
  return true;
}
