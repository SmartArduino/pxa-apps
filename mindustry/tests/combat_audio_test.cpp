// SPDX-License-Identifier: GPL-3.0-only
#include "../core.hpp"
#include <cstdio>
#include <cstdlib>
#include <array>
#include <cstring>
using namespace md;
unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); std::exit(1); } } while(0)
int main() {
  State s; s.generate(154,0,true);
  for (auto &b:s.buildings) if(b.type!=Core)b={};s.rebuild();
  s.playerX=s.targetX=24.5f;s.playerY=s.targetY=30.5f;
  s.enemies[0]={24.5f,25.5f,1000};s.enemies[1]={24.5f,18.5f,1000};
  for(int i=0;i<6;++i)s.step();
  CHECK(s.enemies[0].hp==989 && s.enemies[1].hp==1000);
  for(int i=0;i<28;++i)s.step();
  CHECK(s.enemies[0].hp==934 && s.enemies[1].hp==1000); // 6 alternating shots in 102 frames.
  s.enemies[0]={};s.enemies[1]={24.5f,12.5f,1000};
  for(int i=0;i<6;++i)s.step();CHECK(s.enemies[1].hp==989); // Original range extends beyond 6 tiles.
  s.enemies[1]={24.5f,5.5f,1000};for(int i=0;i<6;++i)s.step();CHECK(s.enemies[1].hp==1000);
  s.soundCount=0;
  for(unsigned i=0;i<MaxSoundEvents;++i)s.emit(Sound::DuoShot,float(i*4),0);
  CHECK(s.soundCount==MaxSoundEvents);
  s.emit(Sound::Wave,0,0);s.emit(Sound::Explosion,0,0);
  unsigned waves=0,explosions=0;for(auto &e:s.soundEvents){waves+=e.kind==Sound::Wave;explosions+=e.kind==Sound::Explosion;}
  CHECK(waves==1 && explosions==1);
  s.soundCount=0;s.emit(Sound::Place,10,10);s.emit(Sound::Place,10.5f,10);CHECK(s.soundCount==1);
  for(auto &e:s.enemies)e={};
  std::array<uint8_t,3900> data{};
  for(unsigned mode=0;mode<3;++mode){s.musicMode=mode;s.muted=true;auto n=s.save(data.data(),data.size());State loaded;CHECK(loaded.load(data.data(),n));CHECK(loaded.musicMode==mode && loaded.muted && loaded.soundCount==0);}
  // MND7 migration retains the world while defaulting to original music rules.
  s.musicMode=0;auto n=s.save(data.data(),data.size());data[3]='7';uint32_t hash=2166136261u;
  for(size_t i=0;i<n-4;++i)hash=(hash^data[i])*16777619u;for(int i=0;i<4;++i)data[n-4+i]=uint8_t(hash>>(i*8));
  State old;CHECK(old.load(data.data(),n));CHECK(old.musicMode==0 && old.muted && old.wave==s.wave);
  std::printf("%u combat/audio-state checks passed\n",checks);
}
