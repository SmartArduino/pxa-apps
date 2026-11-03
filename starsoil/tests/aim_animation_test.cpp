// SPDX-License-Identifier: MIT
// Render production poses and verify their relation to real projectile travel.
#include "../actor_renderer.hpp"
#include "../core.hpp"
#include <array>
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <vector>
using namespace starsoil;
using namespace starsoil_actor;
struct Pixel { uint16_t color=0; Tag tag=Tag::Empty; };
using Canvas=std::array<Pixel,64*64>;
Canvas render(const Appearance &a) {
  Canvas c{};
  draw(a,[&](int x,int y,uint16_t color,Tag tag,uint8_t) {
    assert(x>=0 && y>=0 && x<64 && y<64);
    c[y*64+x]={color,tag};
  });
  return c;
}
void save(const std::filesystem::path &p,const std::vector<uint16_t> &image,int w,int h) {
  FILE *f=std::fopen(p.c_str(),"wb");assert(f);
  std::fprintf(f,"P6\n%d %d\n255\n",w,h);
  for(auto c:image) {
    unsigned char rgb[]={uint8_t((c>>11)*255/31),uint8_t(((c>>5)&63)*255/63),uint8_t((c&31)*255/31)};
    assert(std::fwrite(rgb,1,3,f)==3);
  }
  std::fclose(f);
}
int main(int argc,char **argv) {
  const std::array<std::array<uint8_t,8>,4> looks={{{},
    {StrawHat,0,0,RedTunic,TealCape,0,0,0},
    {IronHelmet,IronMail,IronGreaves,SageRobe,TealCape,SwiftBoots,0,Magnet},
    {CrystalHelmet,CrystalMail,CrystalGreaves,RedTunic,EmberCape,CloudBoots,0,GlowCharm}}};
  const int frames[]={art::StandFrame,art::JumpFrame,art::ApexFrame,art::FallFrame};
  std::vector<uint16_t> gallery(7*64*8*64,rgb(23,34,44));
  for(int outfit=0;outfit<4;++outfit)for(int f=0;f<art::HumanFrames;++f)for(int draw:{0,128,256})
    for(int angle=-90;angle<=90;angle+=15) {
      float rad=angle*3.14159265359f/180;
      Appearance a{32,48,f,1,255,0,true,false,looks[outfit].data(),HunterBow};
      a.aimed=true;a.aim_x=std::cos(rad);a.aim_y=std::sin(rad);a.bow_draw=draw;
      auto c=render(a);
      for(auto j:{CameraHand,Offhand}) {
        auto p=world(a,joint(a,j));assert(c[p.y*64+p.x].tag==Tag::Grip);
      }
      for(bool rear:{false,true}) {
        auto sh=joint(a,rear?FarShoulder:Shoulder),el=joint(a,rear?FarElbow:Elbow),
             hand=joint(a,rear?Offhand:CameraHand);
        float upper=std::hypot(float(el.x-sh.x),float(el.y-sh.y)),
              lower=std::hypot(float(hand.x-el.x),float(hand.y-el.y));
        assert(std::fabs(upper-(rear?3.5f:4.25f))<1.05f);
        assert(std::fabs(lower-(rear?3.8f:4.25f))<1.05f);
      }
      if(outfit==0 && f==art::StandFrame && draw==256) {
        auto h=joint(a,Offhand);
        auto tip=world(a,{h.x+int(std::lround(7*a.aim_x)),h.y+int(std::lround(7*a.aim_y))});
        assert(c[tip.y*64+tip.x].tag==Tag::Weapon);
        a.bow_loaded=false;auto empty=render(a);a.bow_loaded=true;
        assert(empty[tip.y*64+tip.x].tag!=Tag::Weapon);
      }
      a.face=false;a.aim_x=-a.aim_x;auto mirrored=render(a);
      for(int y=0;y<64;++y)for(int x=0;x<64;++x) {
        assert(c[y*64+x].color==mirrored[y*64+63-x].color);
        assert(c[y*64+x].tag==mirrored[y*64+63-x].tag);
      }
      if(f==art::StandFrame && draw==256 && angle%30==0) {
        for(int face=0;face<2;++face) {
          auto &canvas=face?mirrored:c;int col=(angle+90)/30,row=outfit*2+face;
          for(int y=0;y<64;++y)for(int x=0;x<64;++x)
            if(canvas[y*64+x].color)gallery[(row*64+y)*7*64+col*64+x]=canvas[y*64+x].color;
        }
      }
    }
  puts("PASS aimed bow: both wrists stay on grips; stable two-bone arms, clothing/armor and exact facing mirrors across 13 angles, 3 draw states and all air poses");
  assert(locomotion_frame(false,true,0,-200)==art::JumpFrame);
  assert(locomotion_frame(false,true,0,0)==art::ApexFrame);
  assert(locomotion_frame(false,true,0,200)==art::FallFrame);
  assert(locomotion_frame(true,false,0,200)==art::StandFrame);
  std::vector<uint16_t> air(4*64*4*64,rgb(23,34,44));
  for(int outfit=0;outfit<4;++outfit) {
    std::array<Canvas,4> poses;
    for(int i=0;i<4;++i) {
      Appearance a{32,48,frames[i],1,255,0,true,false,looks[outfit].data(),0};
      poses[i]=render(a);
      for(int y=0;y<64;++y)for(int x=0;x<64;++x)
        if(poses[i][y*64+x].color)air[(outfit*64+y)*4*64+i*64+x]=poses[i][y*64+x].color;
    }
    for(int i=0;i<4;++i)for(int j=i+1;j<4;++j) {
      bool different=false;
      for(int y=39;y<48;++y)for(int x=16;x<48;++x)
        different|=poses[i][y*64+x].color!=poses[j][y*64+x].color;
      assert(different);
    }
  }
  puts("PASS distinct standing/ascent/apex/descent leg silhouettes with cloth, greaves, boots and capes");
  State s;
  for(int dx:{-4,0,4})for(int dy:{-4,0,4}) {
    if(!dx&&!dy)continue;
    new_game(s,314159,WorldSize::Small);
    std::fill_n(s.tiles,Cells,uint8_t(Air));std::fill_n(s.walls,Cells,uint8_t(0));
    for(int x=1;x<s.width-1;++x)set_tile(s,x,60,Stone);
    s.player={1608,960,0,0,100,0,1};s.player.grounded=true;
    for(auto &slot:s.bag)slot={};s.bag[0]={HunterBow,1};add(s,Arrows,3);s.selected=0;
    for(auto &m:s.mobs)m={};
    Input input;input.target_x=100+dx;input.target_y=59+dy;input.use=true;
    tick(s,input,1);
    auto &shot=s.shots[0];assert(shot.life && count(s,Arrows)==2);
    auto d=starsoil_aim::direction(s.player.x,s.player.y,input.target_x*16+8,input.target_y*16+8,s.player.face);
    assert(std::fabs(shot.vx-d.x*220)<.001f && std::fabs(shot.vy-.11f-d.y*220)<.001f);
    assert((dx<0 && !s.player.face) || (dx>=0 && s.player.face));
  }
  puts("PASS real bow projectiles share visual aim in eight directions; pointer aim turns the player and consumes exactly one arrow");
  if(argc==2) {
    std::filesystem::create_directories(argv[1]);
    save(std::filesystem::path(argv[1])/"bow-angles.ppm",gallery,7*64,8*64);
    save(std::filesystem::path(argv[1])/"air-poses.ppm",air,4*64,4*64);
  }
}
