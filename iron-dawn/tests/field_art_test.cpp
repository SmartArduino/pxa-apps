// SPDX-License-Identifier: MIT
#include "art.hpp"
#include "mission_skin.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
using namespace iron;
int main(){
 constexpr int w=128,h=128;std::vector<uint16_t> a(w*h,navy),b(w*h,navy);Raster ra(a.data(),w,h,w),rb(b.data(),w,h,w);GroundTexture x,y;x.prepare(w,0,h,-40,180,1);y.prepare(w,0,h,-23,180,1);x.paint(ra,w,0,h,-40,180,1);y.paint(rb,w,0,h,-23,180,1);for(int j=0;j<h;++j)for(int i=0;i<w-17;++i)assert(a[j*w+i+17]==b[j*w+i]);assert(GroundTexture::mirror(-1)==GroundTexture::mirror(0)&&GroundTexture::mirror(255)==GroundTexture::mirror(256));
 for(unsigned i=0;i<16;++i){auto f=art::frames[art::turret_heads_first+i];auto p=art::turret_muzzles[i];assert(p.x>=f.bx-2&&p.x<=f.bx+f.bw+2&&p.y>=f.by-2&&p.y<=f.by+f.bh+2);auto socket=TurretArt::muzzle(i%8,i>=8);assert(socket.x==p.x-f.ox&&socket.y==p.y-f.oy-art::turret_mount_height);}
 for(auto area:{Rect{13,17,96,96},Rect{31,42,20,35},Rect{0,0,128,70}}){std::fill(a.begin(),a.end(),navy);TurretArt::fit(ra,area);unsigned count=0;for(int yy=0;yy<h;++yy)for(int xx=0;xx<w;++xx)if(a[yy*w+xx]!=navy){assert(area.has(xx,yy));++count;}assert(count>20);}
 for(unsigned id=0;id<4;++id)for(int width:{1,9,24,48,119})for(int height:{1,5,17,30}){
  std::fill(a.begin(),a.end(),navy);b=a;Rect area{-3,7,width,height};MissionSkin::draw(ra,area,id);auto& tile=MissionSkin::cache().get(id);int ex=std::min(8,(width-1)/2),ey=std::min(3,(height-1)/2);
  for(int yy=0;yy<height;++yy)for(int xx=0;xx<width;++xx){int sx=xx<ex?xx:xx>=width-ex?144-width+xx:8+(xx-ex)%128,sy=yy<ey?yy:yy>=height-ey?20-height+yy:3+(yy-ey)%14,at=sy*144+sx;if(tile.alpha[at/8]&(1u<<(at%8)))rb.dot(area.x+xx,area.y+yy,tile.pixels[at]);}assert(a==b);
 }
 World world;world.terrain.fill(iron::ground);world.fog.fill(1);world.seen.fill(1);Battlefield field;field.width=w;field.top=0;field.bottom=h;field.focus({8,10});field.draw(ra,world);auto misses=field.ground.misses;field.clock=.6f;field.draw(ra,world);assert(field.ground.misses==misses);world.terrain[cell({8,10})]=water;field.draw(ra,world);misses=field.ground.misses;field.clock=1.1f;field.draw(ra,world);assert(field.ground.misses==misses+1);
 std::puts("Continuous world-coordinate material, wrap edges, 16 registered turret sockets, full turret thumbnails, transparent nine-slice strips and visible-water invalidation passed");
}
