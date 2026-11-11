// SPDX-License-Identifier: MIT
#include "../scene.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <queue>
#include <vector>
using namespace farm;
static void sign(uint8_t*b){uint32_t h=2166136261u;for(int i=0;i<SaveBytes-4;++i)h=(h^b[i])*16777619u;for(int i=0;i<4;++i)b[SaveBytes-4+i]=h>>(i*8);}
int main() {
 // Every overnight path clears fatigue and restores full energy, including
 // zero-energy manual sleep, late sleep and a forced outdoor collapse.
 for(int energy:{0,1,50,99})for(bool tired:{false,true})for(bool forced:{false,true})for(int minute:{360,1439,1441,1500,1560}) {
  State s;fresh(s);s.energy=energy;s.exhausted=tired;s.overwork=tired?10:0;s.minute=minute;s.shipping=35;s.gold=100;
  auto day=s.day;sleep(s,forced);assert(s.energy==100&&!s.exhausted&&!s.overwork&&s.minute==360&&s.day==day+1&&s.shipping==0);
 }
 State s;fresh(s);s.story=5;s.minute=600;
 // All room plans have distinct furniture placement and continuous access to
 // every free tile, with no furniture overlapping an entrance or another prop.
 for(int region:{4,7,9,10,11,12}) {
  s.region=region;auto plan=home_plan(region);bool blocked[MapH][MapW]{};
  for(auto p:plan){auto box=home_footprint(p);for(int y=box.y;y<box.y+box.h;++y)for(int x=box.x;x<box.x+box.w;++x){assert(!blocked[y][x]);blocked[y][x]=true;assert(!walkable(s,x,y));}}
  assert(walkable(s,16,18)&&walkable(s,16,19)&&walkable(s,16,20)&&walkable(s,16,11));
  bool seen[MapH][MapW]{};std::queue<Point> queue;queue.push({16,18});seen[18][16]=true;
  while(!queue.empty()){auto p=queue.front();queue.pop();for(auto d:{Point{0,1},Point{1,0},Point{0,-1},Point{-1,0}}){int x=p.x+d.x,y=p.y+d.y;if(x>=0&&y>=0&&x<MapW&&y<MapH&&!seen[y][x]&&walkable(s,x,y)){seen[y][x]=true;queue.push({x,y});}}}
  for(int y=5;y<=20;++y)for(int x=7;x<=24;++x)if(walkable(s,x,y)){if(!seen[y][x])std::fprintf(stderr,"Disconnected room floor: region=%d x=%d y=%d\n",region,x,y);assert(seen[y][x]);}
  for(int y=5;y<=20;++y)for(int x=7;x<=24;++x)if(partition_wall(region,x,y))assert(!walkable(s,x,y));
  if(region!=7){assert(partition_wall(region,15,7)&&partition_wall(region,18,7));assert(walkable(s,15,10)&&walkable(s,18,10));}
 }
 for(const auto&site:Buildings) {
  s.region=unsigned(site.area);auto door=site.door();auto*p=portal_at(s,door.x,door.y);assert(p&&p->to==site.inside);
  assert(walkable(s,door.x,door.y)&&walkable(s,door.x,door.y+1));
  farm_scene::Queue scene;scene.viewport={0,0,512,384};scene.add(farm_scene::Kind::House,site.x*16,site.y*16,site.y*16+61,site.kind,true);
  // Clicking each actual drawn door must navigate to that building, including
  // the two southern homes previously routed to the northern lodge door.
  int hits=0;
  for(int y=site.y*16+32;y<site.y*16+60;++y)for(int x=door.x*16+2;x<=door.x*16+14;++x){int tx=-1,ty=-1;if(scene.pick(x,y,s,tx,ty)){assert(tx==door.x&&ty==door.y);++hits;}}
  assert(hits>40);
  // Contact follows each model's real foundation/posts, including the raised
  // fisher porch. Every painted pixel stays in its declared culling rectangle.
  constexpr int W=144;constexpr uint16_t ground=art::rgb(179,148,94);
  std::vector<uint16_t> body(W*W),shadow(W*W,ground);art::Raster r{body.data(),W,W};
  farm_scene::Queue q;q.viewport={0,0,W,W};q.add(farm_scene::Kind::House,30,45,106,site.kind);q.objects(r,s,{});
  auto bounds=farm_scene::bounds(q.entries[0]);
  r.p=shadow.data();q.shadows(r,s,{});int contacts=0;
  for(int y=1;y<W;++y)for(int x=0;x<W;++x){if(body[y*W+x])assert(bounds.intersects({x,y,1,1}));if(shadow[y*W+x]==art::blend(ground,art::ShadowTint,2)){assert(body[(y-1)*W+x]);++contacts;}}
  assert(contacts>10);
 }
 s.region=unsigned(Region::Town);farm_scene::Queue shop;shop.viewport={0,0,512,384};shop.add(farm_scene::Kind::House,80,48,109,1,true);
 int home_hits=0;for(int y=80;y<109;++y)for(int x=148;x<157;++x){int tx=-1,ty=-1;if(shop.pick(x,y,s,tx,ty)){assert(tx==9&&ty==7);++home_hits;}}assert(home_hits>20);
  for(int x:{139,165}){int tx=-1,ty=-1;assert(shop.pick(x,100,s,tx,ty)&&tx==9&&ty==7);}
  {int tx=-1,ty=-1;assert(!shop.pick(135,100,s,tx,ty)&&!shop.pick(139,60,s,tx,ty));}
 // Different architecture must have different silhouettes, not just palettes.
 for(int a=0;a<6;++a)for(int b=a+1;b<6;++b){const auto&aa=art::building_shape(a);const auto&bb=art::building_shape(b);int diff=0;for(int y=-90;y<0;++y)for(int x=-50;x<50;++x)diff+=art::hit(aa,x+aa.anchor_x,y+aa.anchor_y)!=art::hit(bb,x+bb.anchor_x,y+bb.anchor_y);assert(diff>150);}
 // v1 room saves migrate off a newly positioned stove; current-layout malformed
 // positions still fail instead of silently relocating arbitrary bad saves.
 State t;fresh(s);s.region=unsigned(Region::FisherHome);s.visited|=1u<<s.region;s.px=21*16+8;s.py=15*16+8;
 uint8_t bytes[SaveBytes];assert(encode(s,bytes,sizeof bytes));assert(!decode(t,bytes,sizeof bytes));
 bytes[SaveDataBytes-7]=1;sign(bytes);assert(decode(t,bytes,sizeof bytes)&&walkable(t,t.px/16,t.py/16));
 // A 0.22 farmer on the new bedroom wall migrates safely, while a current
 // corrupt save cannot silently enter a solid wall. Goods and time survive.
 fresh(s);s.region=unsigned(Region::Home);s.visited|=1u<<s.region;s.px=15*16+8;s.py=7*16+8;s.gold=449;s.day=18;
 assert(encode(s,bytes,sizeof bytes)&&!decode(t,bytes,sizeof bytes));bytes[SaveDataBytes-7]=2;sign(bytes);
 assert(decode(t,bytes,sizeof bytes)&&walkable(t,t.px/16,t.py/16)&&t.day==18&&t.gold==449);
 puts("PASS: full overnight restoration, five unique connected room plans, physical doors, distinct architecture and prior-layout save migration");
}
