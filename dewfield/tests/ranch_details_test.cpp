// SPDX-License-Identifier: MIT
#include "../scene.hpp"
#include <array>
#include <cassert>
#include <cstring>
#include <cstdio>
using namespace farm;
constexpr int W=320,H=112,X=160,Y=80;
using Pixels=std::array<uint16_t,W*H>;
static void unchanged(const State&a,const State&b) {
 uint8_t x[SaveBytes],y[SaveBytes];assert(encode(a,x,sizeof(x))&&encode(b,y,sizeof(y)));assert(!std::memcmp(x,y,sizeof(x)));
}
int main() {
 State s;fresh(s);begin_ranch(s);s.ranch_chapter=4;sync_inventory(s);
 assert(ranch_contact(s)==1&&!ranch_story_ready(s,1));
 assert(change_item(s,17,6)&&change_item(s,40,3));sync_inventory(s);
 State before=s;assert(!ranch_story(s,1));unchanged(s,before);
 assert(change_item(s,40,1));sync_inventory(s);before=s;
 assert(ranch_story_ready(s,1)&&!ranch_story(s,0));unchanged(s,before);
 auto gold=s.gold;assert(ranch_story(s,1)&&s.ranch_chapter==5&&s.gold==gold+500);
 assert(!item_count(s,17)&&!item_count(s,40)&&!ranch_story(s,1));
 assert(change_item(s,42,1)&&change_item(s,33,1)&&change_item(s,31,2));sync_inventory(s);
 before=s;assert(!ranch_story(s,3));unchanged(s,before); // The guardian's heart is required.
 s.mine_rewards|=1;assert(ranch_story_ready(s,3));gold=s.gold;
 assert(ranch_story(s,3)&&s.ranch_chapter==6&&s.gold==gold+600);
 assert(!item_count(s,42)&&!item_count(s,33)&&!item_count(s,31)&&ranch_contact(s)==-1);
 before=s;for(int npc=0;npc<4;++npc)assert(!ranch_story(s,npc));unchanged(s,before);
 for(int minute:{419,420,1079,1080,1500}){s.minute=minute;assert(ranch_lit(s)==(minute<420||minute>=1080));}
 uint8_t raw[SaveBytes],compact[StorageSaveBytes];State restored;
 assert(encode(s,raw,sizeof(raw))&&decode(restored,raw,sizeof(raw))&&restored.ranch_chapter==6);
 assert(encode_storage(s,compact,sizeof(compact))==StorageSaveBytes&&decode(restored,compact,sizeof(compact)));
 assert(restored.ranch_chapter==6&&restored.mine_rewards==s.mine_rewards&&restored.gold==s.gold);
 // Every new shed state remains inside its culling bounds and above the ground.
 for(bool reinforced:{false,true})for(bool lit:{false,true})for(int feed:{0,24}) {
  Pixels p{};art::Raster r{p.data(),W,H};art::ranch_shed(r,X,Y,feed,reinforced,lit);
  farm_scene::Entry e{farm_scene::Kind::RanchShed,X,Y,Y};auto b=farm_scene::bounds(e);
  for(int y=0;y<H;++y)for(int x=0;x<W;++x)if(p[y*W+x]){assert(y<Y);assert(b.intersects({x,y,1,1}));}
  for(int x=X-22;x<=X+23;++x)assert(p[(Y-1)*W+x]);
  int far=0,near=0;
  for(int x=0;x<W;++x){far+=p[(Y-43)*W+x]!=0;near+=p[(Y-28)*W+x]!=0;}
  assert(near>far); // The near eave widens; the old inverted roof narrowed.
  for(int y=Y-46;y<=Y-27;++y) {
   int left=0,right=W-1;while(!p[y*W+left])++left;while(!p[y*W+right])--right;
   for(int x=left;x<=right;++x)assert(p[y*W+x]); // No sky/terrain leaks inside the plane.
  }
  // Viewport clipping is identical to cropping the full render.
  std::array<uint16_t,39*47> clipped{};art::Raster clip{clipped.data(),39,47,X-17,Y-40};
  art::ranch_shed(clip,X,Y,feed,reinforced,lit);
  for(int y=0;y<47;++y)for(int x=0;x<39;++x)assert(clipped[y*39+x]==p[(Y-40+y)*W+X-17+x]);
 }
 Pixels day{},night{};std::array<uint16_t,65536> tint;tint.fill(0x1111);
 art::Raster rd{day.data(),W,H,0,0,tint.data()},rn{night.data(),W,H,0,0,tint.data()};
 art::ranch_shed(rd,X,Y,0,true,false);art::ranch_shed(rn,X,Y,0,true,true);
 int glowing=0;for(int y=0;y<H;++y)for(int x=0;x<W;++x)if(day[y*W+x]!=night[y*W+x]) {
  assert(day[y*W+x]);assert((x>=X+8&&x<=X+16&&y>=Y-19&&y<=Y-11)||(x>=X-24&&x<=X-22&&y>=Y-19&&y<=Y-15));++glowing;
 }
 assert(glowing>30); // Light belongs to the panes, never detached pixels.
 for(int prop=0;prop<2;++prop) {
  Pixels p{};art::Raster r{p.data(),W,H};
  if(prop)art::mine_entrance(r,X,Y);else art::world_shelf(r,X,Y);
  for(int y=Y;y<H;++y)for(int x=0;x<W;++x)assert(!p[y*W+x]);
  for(int x=X-(prop?25:128);x<X+(prop?26:128);++x)assert(p[(Y-1)*W+x]);
 }
 puts("PASS: roof perspective, culling/clipping, supported prop contacts, contained night light, transactional ranch epilogue and 960-byte persistence");
}
