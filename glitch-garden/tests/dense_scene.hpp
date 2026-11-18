// SPDX-License-Identifier: MIT
#pragma once
#include "../model.hpp"
namespace garden {
// Fixed capacities and deterministic phases for comparable real-renderer load.
// Used only by benchmark builds/native tools; never writes player progress.
inline void dense_scene(Game& g,float time,bool maximum){
 g.outcome=Outcome::playing;g.elapsed=std::fmod(time,50.f);g.next_spawn=10000;g.stars=999;
 for(unsigned i=0;i<g.plants.size();++i){auto& p=g.plants[i];p={};if(i>=unsigned(maximum?45:20))continue;
  p.kind=Defender(i%7+1);p.hp=100000;p.animation=time+i*.071f;p.attacking=true;p.cycle=std::fmod(time+i*.137f,1.7f);}
 for(unsigned i=0;i<g.enemies.size();++i){auto& e=g.enemies[i];e={};if(i>=unsigned(maximum?48:12))continue;
  e.active=true;e.row=i%5;e.kind=EnemyKind(i%4);e.hp=100000;e.x=8.8f-std::fmod(time*(.4f+.2f*(i%4))+i*.731f,6.8f);
  e.animation=time+i*.063f;e.attacking=i%4==0;e.cycle=std::fmod(time+i*.031f,.6f);}
 for(unsigned i=0;i<g.shots.size();++i){auto& s=g.shots[i];s={};if(i>=unsigned(maximum?96:24))continue;
  s.active=true;s.row=i%5;s.kind=ShotKind(i%4);s.axe=s.kind==ShotKind::leaf;s.x=std::fmod(time*2.3f+i*.271f,9.f);}
 for(auto& e:g.effects)e={};
}
}
