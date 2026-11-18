// SPDX-License-Identifier: MIT
#include "../model.hpp"
#include <cassert>
#include <cstdio>
using namespace garden;
int main(){Game g;g.start(0);assert(!g.place(-1,0,Defender::cactus));assert(!g.place(0,9,Defender::cactus));assert(!g.place(0,0,Defender(255)));assert(g.place(0,0,Defender::trophy));assert(g.stars==stages[0].stars-40);assert(!g.place(0,0,Defender::cactus));g.next_spawn=1000;for(int n=0;n<322;++n)g.step(.025f);assert(g.generated==1);assert(g.remove(0,0));assert(!g.remove(0,0));
 g.start(1);g.next_spawn=1000;assert(g.place(0,3,Defender::stone));assert(g.spawn(0,true));g.enemies[0].x=3.8f;g.step(.025f);assert(g.enemies[0].jump>0);for(int i=0;i<40;++i)g.step(.025f);assert(g.enemies[0].x<3&&g.plants[3].hp==health[int(Defender::stone)]);
 g.start(0);g.next_spawn=1000;assert(g.place(2,0,Defender::frost));assert(g.spawn(2,EnemyKind::snail));g.enemies[0].x=2;for(int i=0;i<40;++i)g.step(.025f);assert(g.enemies[0].slow>2&&g.enemies[0].hp<160);
 g.start(0);g.next_spawn=1000;assert(g.place(2,0,Defender::ember));for(int r:{2,3,0}){assert(g.spawn(r,EnemyKind::dust));g.enemies[r==2?0:r==3?1:2].x=3;}for(int i=0;i<40;++i)g.step(.025f);assert(g.enemies[0].hp<55&&g.enemies[1].hp<55&&g.enemies[2].hp==55);
 g.start(0);g.next_spawn=1000;assert(g.place(2,1,Defender::clover));assert(g.place(2,2,Defender::cactus));g.plants[2*columns+2].hp=50;for(int i=0;i<165;++i)g.step(.025f);assert(g.plants[2*columns+2].hp==64);assert(g.plants[2*columns+1].hp==85);
 g.start(0);for(int n=0;n<1500&&g.outcome==Outcome::playing;++n)g.step(.025f);assert(g.outcome==Outcome::lost);
 for(int stage=0;stage<stage_count;++stage){g.start(stage);for(int r=0;r<rows;++r){assert(g.place(r,0,Defender::cactus));assert(g.place(r,1,Defender::cactus));}for(int n=0;n<5000&&g.outcome==Outcome::playing;++n)g.step(.025f);std::printf("stage=%d result=%d kills=%d shots=%d elapsed=%.2f\n",stage+1,int(g.outcome),g.kills,g.shots_fired,g.elapsed);assert(g.outcome==Outcome::won&&g.kills>5&&g.shots_fired>30);}
 g.start(11);bool kinds[4]={};for(int n=0;n<1000;++n){g.stars=999;for(int r=0;r<5;++r){if(g.plants[r*9].kind==Defender::none)g.place(r,0,Defender::gnome);}g.step(.025f);for(auto e:g.enemies)if(e.active)kinds[int(e.kind)]=true;}for(bool k:kinds)assert(k);
 g.start(0);for(int i=0;i<48;++i)assert(g.spawn(i%5,false));assert(!g.spawn(0,false));assert(!g.spawn(-1,false));auto before=g.elapsed;g.step(NAN);assert(g.elapsed==before);g.start(999);assert(g.level==stage_count-1);std::puts("12-stage campaign, seven plants, four enemies, slow/splash/heal, bounds and outcomes passed");}
