// SPDX-License-Identifier: MIT
// Production ground-reference composition: overlaps, tint, water and clipping.
#include "../scene.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
using namespace farm_scene;
constexpr int W=144,H=144;
static const uint16_t Floor=art::rgb(183,154,112);
static void bind(art::Raster&r,const uint16_t*base){r.shadow_ground=base;r.shadow_width=W;r.shadow_height=H;}
int main(){
 farm::State s;farm::fresh(s);s.story=5;s.plots[0]={1,1,2,1};s.animals[0]={3,0,8,65};
 Queue q;q.viewport={0,0,W,H};
 for(auto kind:{Kind::Tree,Kind::Rock,Kind::Grass,Kind::House,Kind::Well,Kind::Chest,Kind::Crop,Kind::Animal,Kind::WorldNpc,Kind::Player,Kind::Sprinkler,Kind::Planter,Kind::Fence,Kind::Table,Kind::Workbench,Kind::Bed,Kind::Cabinet})
  q.add(kind,kind==Kind::House?32:72,kind==Kind::House?39:100,100);
 std::vector<uint16_t> base(W*H,Floor),a=base,b=base,c=base;
 std::vector<uint16_t> tint(65536);for(unsigned i=0;i<tint.size();++i)tint[i]=art::blend(i,art::rgb(40,53,70),3);
 for(bool night:{false,true}){
  if(night)std::fill(base.begin(),base.end(),tint[Floor]);
  a=b=c=base;art::Raster r{a.data(),W,H};bind(r,base.data());r.tint=night?tint.data():nullptr;r.shadow_diffuse=night;r.shadow_limit=night?2:3;
  q.shadows(r,s);q.shadows(r,s);b=a;r.p=c.data();
  for(unsigned i=0;i<q.count/2;++i)std::swap(q.entries[i],q.entries[q.count-1-i]);
  q.shadows(r,s);assert(c==a);r.p=a.data();q.shadows(r,s);assert(a==b);
  auto deepest=art::blend(base[0],night?tint[art::ShadowTint]:art::ShadowTint,night?2:3);
  int shaded=0;for(unsigned i=0;i<a.size();++i){assert(a[i]>=deepest&&a[i]<=base[i]);shaded+=a[i]!=base[i];}assert(shaded>200);
 }
 // Even overlapping actors/props cannot shade water or read beyond the map
 // when a taller viewport includes the clear area below its last row.
 std::vector<uint8_t> materials(W*H/4,0);
 for(int y=0;y<H;++y)for(int x=80;x<W;++x){unsigned at=y*W+x;materials[at/4]|=3<<((at&3)*2);}
 a=base;art::Raster water{a.data(),W,H};bind(water,base.data());water.shadow_materials=materials.data();q.shadows(water,s);
 for(int y=0;y<H;++y)for(int x=80;x<W;++x)assert(a[y*W+x]==base[y*W+x]);
 for(auto origin:{farm::Point{-4,-5},farm::Point{136,136},farm::Point{300,300}}){
  constexpr uint16_t Guard=0xabcd;std::vector<uint16_t> frame(33*27+32,Guard);
  art::Raster clip{frame.data()+16,33,27,origin.x,origin.y};bind(clip,base.data());q.shadows(clip,s);
  for(int i=0;i<16;++i)assert(frame[i]==Guard&&frame[16+33*27+i]==Guard);
 }
 // Large vegetation has a visible but shallow cast; seedling casts stay small.
 int coverage[2]{};
 for(int j=0;j<2;++j){Queue tree;tree.viewport={0,0,W,H};tree.add(Kind::Tree,72,100,100,0,false,j?2:0);a=base;art::Raster r{a.data(),W,H};bind(r,base.data());tree.shadows(r,s);for(unsigned i=0;i<a.size();++i)coverage[j]+=a[i]!=base[i];}
 assert(coverage[1]>coverage[0]*3);
 for(int j=0;j<2;++j){Queue animals;animals.viewport={0,0,W,H};animals.add(Kind::Animal,72,100,100);s.animals[0].age=j?8:0;a=base;art::Raster r{a.data(),W,H};bind(r,base.data());animals.shadows(r,s);coverage[j]=0;for(unsigned i=0;i<a.size();++i)coverage[j]+=a[i]!=base[i];}
 assert(coverage[1]>coverage[0]*2);
 // Packed paving must clip safely at every edge, including negative cameras.
 for(int offset:{-110,-1,0,139}){
  std::vector<uint16_t> frame(17*19+32,0xabcd);art::Raster r{frame.data()+16,17,19,3,5};
  art::paving(r,paving_sprites::Plaza,offset,offset);
  for(int i=0;i<16;++i)assert(frame[i]==0xabcd&&frame[16+17*19+i]==0xabcd);
 }
 puts("PASS: ground shadow order/idempotence, opacity bounds, night tint, water exclusion, clipped map edges, tree stages and paving bounds");
}
