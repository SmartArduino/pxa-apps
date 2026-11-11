// SPDX-License-Identifier: MIT
#include "../scene.hpp"
#include <cassert>
#include <cstdio>
#include <set>
#include <vector>
using namespace farm::walls;
static unsigned pixel(unsigned n,int x,int y){int p=y*Tile+x;return (Images[n][p/2]>>((p&1)*4))&15;}
int main(){
 using namespace room_wall_sprites;
 // Reference-derived materials must remain pixel textures, with transparent
 // capital corners and enough distinct tones to retain carved wood/paper.
 for(const auto*m:{&BeamH,&BeamV,&Post,&Collar,&Jamb,&room_wall_sprites::Plaster,&Skirting,&Window,&Door,&Mat}){
  std::set<uint16_t> colors;for(int y=0;y<m->h;++y)for(int x=0;x<m->w;++x){auto c=sample(*m,x,y);if(c)colors.insert(c);}
  assert(colors.size()>=5&&colors.size()<=15);
 }
 assert(!sample(Post,0,0)&&!sample(Collar,0,0));
 // The concept has full timber jambs and a wider moulded capital, rather than
 // the low flat strip or a pale plaster pillar used in previous revisions.
 assert(pixel(North,8,20)==Wood&&pixel(North,8,8)==Capital);
 assert(pixel(North|East|West,8,4)==Capital);
 for(int n=0;n<16;++n){
  std::vector<uint16_t> frame(64*80);art::Raster r{frame.data(),64,80};art::room_wall(r,1,2,n,-1000,-1000);
  for(int y=0;y<80;++y)for(int x=0;x<64;++x)if(frame[y*64+x])assert(x>=16&&x<32&&y>=4&&y<44);
 }
 constexpr int W=512,H=384;constexpr auto ground=art::rgb(82,107,71);
 for(int region:{4,6,7,9,10,11,12}){
  std::vector<uint16_t> full(W*H,ground);art::Raster r{full.data(),W,H};art::room_shell(r,region);
  assert(full[46*W+140]==sample(BeamH,(140-104)%64,2));
  assert(full[320*W+404]==sample(BeamV,4,(320-44)%64));
  assert(full[350*W+220]==sample(room_wall_sprites::Plaster,(220-112)%40,8));
  assert(full[346*W+264]==sample(Door,16,12));
  farm::State s;farm::fresh(s);s.region=region;farm_scene::Queue q;int tx=-1,ty=-1;
  assert(q.pick(264,350,s,tx,ty)&&tx==16&&ty==20);
  assert(q.pick(264,322,s,tx,ty)&&tx==16&&ty==20); // Visible doormat remains touchable above the belt.
  PostSite posts[24];auto count=shell_posts(region,posts);assert(count>=4&&count<=24);
  for(unsigned i=0;i<count;++i){q.add(farm_scene::Kind::RoomPost,posts[i].x,posts[i].y,posts[i].depth);auto b=farm_scene::bounds(q.entries[q.count-1]);assert(b.w==12&&b.h==12);}
  for(auto p:{farm::Point{101,41},farm::Point{395,43},farm::Point{102,331},farm::Point{246,339}}){
   constexpr int cw=19,ch=37;std::vector<uint16_t> frame(cw*ch+32,0xabcd);std::fill(frame.begin()+16,frame.end()-16,ground);
   art::Raster clipped{frame.data()+16,cw,ch,p.x,p.y};art::room_shell(clipped,region);
   for(int y=0;y<ch;++y)for(int x=0;x<cw;++x)assert(frame[16+y*cw+x]==full[(p.y+y)*W+p.x+x]);
   for(int i=0;i<16;++i)assert(frame[i]==0xabcd&&frame[16+cw*ch+i]==0xabcd);
  }
 }
 puts("PASS: reference wood/paper materials, carved capitals, full timber jambs, south facade, visible door picking and clipped rendering");
}
