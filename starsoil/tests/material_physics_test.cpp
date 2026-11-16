// SPDX-License-Identifier: MIT
#include "../core.hpp"
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
using namespace starsoil;
static State s,loaded;
static void put(int x,int y,int t){s.tiles[y*WorldW+x]=uint8_t(t);}
static void fresh(bool chamber=true) {
  std::memset(&s,0,sizeof(s));s.width=256;s.height=96;s.day=1;s.clock=100000;s.random=123;
  s.hp=100;s.mana=60;s.player.x=8*16+8;s.player.y=50*16;s.home_x=8;s.home_y=50;
  std::fill(std::begin(s.tiles),std::end(s.tiles),uint8_t(Stone));
  std::fill(std::begin(s.heights),std::end(s.heights),uint8_t(60));
  if(chamber)for(int y=16;y<=50;++y)for(int x=32;x<64;++x)put(x,y,Air);
}
static std::array<int,3> amounts() {
  std::array<int,3> n{};
  for(int y=0;y<s.height;++y)for(int x=0;x<s.width;++x) {
    int t=tile(s,x,y);if(t==Sand)++n[0];else if(t==Water)++n[1];else if(t==Lava)++n[2];
  }
  return n;
}
static void run(int n) {
  for(int i=0;i<n;++i) {
    step_materials(s,50);
    assert(s.materials.moves<=64 && s.materials.processed_cells<=1024);
  }
}
int main() {
  fresh();put(40,20,Sand);run(160);assert(tile(s,40,50)==Sand && amounts()[0]==1);
  auto moves=s.materials.total_moves;run(40);assert(s.materials.total_moves==moves);
  for(auto bits:s.materials.active)assert(!bits);
  assert(set_tile(s,40,51,Air));run(15);assert(tile(s,40,51)==Sand);
  puts("PASS sand falls, rests, sleeps and wakes when its support is mined");
  fresh();for(int y=20;y<25;++y)put(40,y,Sand);run(200);
  assert(amounts()[0]==5);
  for(int y=16;y<=50;++y)for(int x=32;x<64;++x)
    if(tile(s,x,y)==Sand)assert(tile(s,x,y+1)!=Air);
  puts("PASS sand piles along diagonal slopes without material loss");
  fresh();for(int y=18;y<24;++y)put(40,y,Water);run(240);
  assert(amounts()[1]==6 && s.random==123);
  for(int y=16;y<50;++y)for(int x=32;x<64;++x)assert(tile(s,x,y)!=Water);
  moves=s.materials.total_moves;run(40);assert(s.materials.total_moves==moves);
  puts("PASS finite water falls, spreads into a level basin and rests; game RNG unchanged");
  for(int t:{Water,Lava}) {
    fresh();put(40,20,t);s.materials.initialized=true;s.materials.active[1]=uint64_t(1)<<2;
    run(3);assert(tile(s,40,t==Water?23:21)==t);
  }
  fresh();put(40,31,Sand);s.materials.initialized=true;
  s.materials.active[1]=s.materials.active[2]=uint64_t(1)<<2;
  s.player.x=40*16+8;s.player.y=32*16+100;run(1);
  assert(tile(s,40,32)==Sand && tile(s,40,33)==Air);
  puts("PASS slower lava cadence and exactly one movement across chunk boundaries per step");
  fresh(false);put(40,20,Sand);put(40,21,Water);run(80);
  assert(tile(s,40,21)==Sand && tile(s,40,20)==Water && amounts()==(std::array<int,3>{1,1,0}));
  fresh(false);put(40,20,Sand);for(int y=21;y<=23;++y)put(40,y,Air);
  s.player.x=40*16+8;s.player.y=22*16;run(50);assert(tile(s,40,20)==Sand);
  s.player.x=8*16+8;run(30);assert(tile(s,40,23)==Sand);
  fresh(false);put(40,20,Water);put(40,21,Chest);run(50);
  assert(tile(s,40,20)==Water && tile(s,40,21)==Chest);
  fresh(false);put(40,20,Water);put(40,21,Platform);put(40,22,Air);run(50);
  assert(tile(s,40,22)==Water && tile(s,40,21)==Platform);
  fresh(false);put(40,20,Water);put(40,21,Lava);run(50);
  assert(tile(s,40,20)==Water && tile(s,40,21)==Lava);
  puts("PASS sand displaces water, protects actors, preserves furniture/platforms and keeps liquid types separate");
  fresh();for(int y=18;y<25;++y)for(int x=34;x<61;++x)put(x,y,(x+y)%3==0?Sand:(x+y)%3==1?Water:Lava);
  auto initial=amounts();
  for(int i=0;i<30;++i){run(20);assert(amounts()==initial);}
  std::vector<uint8_t> bytes(SaveBytes);size_t n=encode(s,bytes.data(),bytes.size());
  assert(n==save_size(s.width,s.height) && decode(loaded,bytes.data(),n));
  assert(!loaded.materials.initialized);s=loaded;assert(amounts()==initial);run(100);assert(amounts()==initial);
  puts("PASS mixed-material conservation, bounded work and unchanged save format with flow resumed after reload");
  fresh();s.width=1024;s.height=256;
  for(int y=1;y<s.height-1;++y)for(int x=1;x<s.width-1;++x)put(x,y,y%2?Sand:Air);
  run(40);assert(s.materials.moves<=64 && s.materials.processed_cells<=1024);
  puts("PASS maximum world dimensions keep fixed chunk and movement budgets");
}
