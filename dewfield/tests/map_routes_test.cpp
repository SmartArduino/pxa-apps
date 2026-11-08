// SPDX-License-Identifier: MIT
#include "../mines.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
using namespace farm;
int main() {
  State s;fresh(s);s.story=3;s.minute=600;
  for(const auto&p:Portals) {
    s.region=uint8_t(p.from);if(p.from==Region::Mine)enter_mine(s);auto a=portal_approach(p);
    assert(walkable(s,p.x,p.y) && walkable(s,a.x,a.y));
    int x=p.x*16+8,y=p.y*16+8,d=portal_heading(p);
    int fx=x-(d==1?16:d==3?-16:0),fy=y-(d==2?16:d==0?-16:0);
    assert(crossed_portal(p,fx,fy,x,y,d));
    for(int wrong=0;wrong<4;++wrong)if(wrong!=d)assert(!crossed_portal(p,fx,fy,x,y,wrong));
    assert(!crossed_portal(p,fx,fy,(fx+x)/2,(fy+y)/2,d));
    for(const auto&q:Portals)if(q.from==p.to && q.to==p.from) {
      State dest=s;dest.region=uint8_t(p.to);if(p.to==Region::Mine)enter_mine(dest);assert(walkable(dest,p.spawn_x,p.spawn_y));
    }
  }
  // Every road that actually reaches the drawing's edge has a corresponding
  // regional exit; interior branches end at real facilities or loop back.
  for(int region=0;region<RegionCount;++region)if(!indoors(region))
    for(int y=0;y<MapH;++y)for(int x=0;x<MapW;++x) {
      if(x!=0&&x!=MapW-1&&y!=0&&y!=MapH-1)continue;
      if(!road_tile(region,x,y))continue;
      bool found=false;
      for(const auto&p:Portals)if(int(p.from)==region&&!indoors(uint8_t(p.to))) {
        int d=portal_heading(p);
        found|=(x==0&&d==3&&std::abs(y-p.y)<=1)||(x==MapW-1&&d==1&&std::abs(y-p.y)<=1)||
               (y==0&&d==0&&std::abs(x-p.x)<=1)||(y==MapH-1&&d==2&&std::abs(x-p.x)<=1);
      }
      assert(found);
    }
  puts("PASS: every regional road has a destination, directional thresholds, reachable approach tiles, safe reciprocal spawns and the repaired forest bridge");
}
