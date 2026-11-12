// SPDX-License-Identifier: MIT
#include "../scene.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
using namespace farm_scene;
int main(){
 using namespace farm::walls;
 constexpr int W=512,H=384;
 constexpr auto ground=art::rgb(193,146,91);
 farm::State s;farm::fresh(s);s.region=4;s.px=264;s.py=296;
 for(int region:{4,9,10,11,12}){
  assert(door_count(region)==2);
  for(auto d:RoomDoors){
   assert(door_open(d,d.x,d.y+16)==8);
   assert(door_open(d,d.x,d.y+120)==0);
   assert(!farm::partition_wall(region,d.x/16,9));
   assert(!farm::partition_wall(region,d.x/16,10));
   assert(!farm::partition_wall(region,d.x/16,11));
   auto n=links(region,d.x/16,12);assert(n&DoorCorner);
  }
 }
 for(int region:{0,1,6,7,8})assert(!door_count(region));
 // The cutaway opens into visible floor, without displaying a front-facing
 // door panel across a side passage or requiring a second tap.
 for(auto d:RoomDoors)for(unsigned phase=0;phase<=8;++phase){
  std::vector<uint16_t> full(W*H,ground);art::Raster r{full.data(),W,H};
  art::room_door(r,d.x,d.y,phase);
  int painted=0;
  for(int y=0;y<H;++y)for(int x=0;x<W;++x)if(full[y*W+x]!=ground){
   ++painted;assert(x>=d.x-2&&x<=d.x+2&&y>=d.y-Rise&&y<d.y+48);
  }
  assert(phase==8?painted==0:painted>=300);
  for(auto p:{farm::Point{d.x-34,140},farm::Point{d.x-3,154},farm::Point{d.x+10,144}}){
   constexpr int cw=17,ch=31;std::vector<uint16_t> clipped(cw*ch+32,0xabcd);
   std::fill(clipped.begin()+16,clipped.end()-16,ground);
   art::Raster crop{clipped.data()+16,cw,ch,p.x,p.y};art::room_door(crop,d.x,d.y,phase);
   for(int y=0;y<ch;++y)for(int x=0;x<cw;++x)assert(clipped[16+y*cw+x]==full[(p.y+y)*W+p.x+x]);
   for(int i=0;i<16;++i)assert(clipped[i]==0xabcd&&clipped[16+cw*ch+i]==0xabcd);
  }
 }
 // Nearby remote farmers open the same door even when the local farmer is across the room.
 for(bool remote:{false,true}){
  s.px=remote?264:248;s.py=remote?296:176;
  RemoteFarmer peer;peer.x=248;peer.y=176;View v;
  if(remote){v.farmers=&peer;v.farmer_count=1;}
  Queue q;q.build(s,v,{0,0,W,H});int leaves=0;
  for(unsigned i=0;i<q.count;++i){auto&e=q.entries[i];
   if(e.kind==Kind::RoomDoor){++leaves;if(!e.id)assert(e.growth==8&&e.depth==192);}
  }
  assert(leaves==2);
 }
 // Open doorways retain a visible floor gap above the raised lower return.
 // Jamb ends are
 // sealed. The lower caps turn into the transverse walls rather than posts.
 s.px=264;s.py=176;Queue q;q.build(s,{}, {0,0,W,H});Queue architecture;architecture.viewport=q.viewport;
 for(unsigned i=0;i<q.count;++i)if(q.entries[i].kind==Kind::RoomWall||q.entries[i].kind==Kind::RoomDoor)
  architecture.entries[architecture.count++]=q.entries[i];
 std::vector<uint16_t> frame(W*H,ground);art::Raster r{frame.data(),W,H};architecture.objects(r,s,{});
 for(int x:{248,296})for(int y=140;y<154;++y)assert(frame[y*W+x]==ground);
 puts("PASS: wide cutaway doorways, sealed timber jambs, visible wall returns, automatic local/remote opening and clipped rendering");
}
