// SPDX-License-Identifier: MIT
#include "../camera.hpp"
#include <cassert>
#include <cstdio>
using namespace ash;
bool near(float a,float b){return std::abs(a-b)<.001f;}
int main(){
    unsigned cases=0;
    // Physics origins must coincide with palm pixels after crop and UV mirroring.
    for(int c=0;c<characterCount;++c)for(int face:{-1,1})
    for(float x:{19.f,91.f,301.f})for(float y:{0.f,35.f,90.f}){
        Fighter p;p.character=c;p.face=face;p.x=x;p.y=y;
        auto pose=actorPose(p,fireReleaseFrame);auto offset=art::fighter_offsets[c][fireReleaseFrame];
        auto pixel=firePalms[c];auto origin=fireOrigin(p);
        float localX=pixel.x+.5f-offset.x,localY=pixel.y+.5f-offset.y;
        assert(localX>=0&&localX<pose.tile.w&&localY>=0&&localY<pose.tile.h);
        assert(near(origin.x,pose.x+(face>0?localX:pose.tile.w-localX)));
        assert(near(193-origin.y,pose.y+localY));
        ++cases;
    }
    for(int c=0;c<characterCount;++c)for(int face:{-1,1}){
        Arena a;a.reset(true,c,(c+1)%4);a.ai=false;a.phase=Phase::fight;
        a.f[0].x=face>0?50:270;a.f[1].x=face>0?270:50;a.f[0].face=face;
        a.beginMove(0,Action::fire);int startup=moveFor(a.f[0]).startup;
        for(int tick=1;tick<startup;++tick){a.step();for(auto& s:a.shots)assert(!s.active);}
        a.step();auto& shot=a.shots[0];auto palm=fireOrigin(a.f[0]);
        assert(shot.active&&shot.age==1&&a.sprite(a.f[0])==fireReleaseFrame);
        assert(near(shot.x,palm.x)&&near(shot.y,palm.y)&&projectileFrame(shot)==0);
        float birth=shot.x;a.step();assert(near(shot.x,birth+shot.speed));
        // A distant impact belongs at the target, not halfway from the caster.
        for(int tick=0;tick<140&&a.damageEvents==0;++tick)a.step();
        assert(a.damageEvents==1&&std::abs(a.hit.x-a.f[1].x)<17&&near(a.hit.y,palm.y));
        ++cases;
    }
    // Pushbox resolution on the emission tick cannot detach the new shot.
    {Arena a;a.reset(true);a.ai=false;a.phase=Phase::fight;
     a.f[0].x=131;a.f[1].x=140;a.f[0].set(Action::fire);
     a.f[0].age=moveFor(a.f[0]).startup-1;a.f[1].invulnerable=5;
     a.step();auto palm=fireOrigin(a.f[0]);assert(a.shots[0].active);
     assert(near(a.shots[0].x,palm.x)&&near(a.shots[0].y,palm.y));++cases;}
    // The visible, higher flight path clears a crouched opponent.
    {Arena a;a.reset();a.ai=false;a.phase=Phase::fight;a.f[0].x=60;a.f[1].x=230;
     a.feed(1,Down);a.beginMove(0,Action::fire);for(int i=0;i<110;++i)a.step();
     assert(a.damageEvents==0&&a.blocks==0);++cases;}
    // Opposing shots can only clash while their vertical volumes overlap.
    for(bool aligned:{false,true}){
        Arena a;a.reset(true);a.ai=false;a.phase=Phase::fight;a.f[0].x=20;a.f[1].x=300;
        a.shots[0]={true,0,12,100,100,40,0};a.shots[1]={true,1,12,100,105,aligned?40.f:100.f,0};
        a.step();assert(a.shots[0].active==!aligned&&a.shots[1].active==!aligned);++cases;
    }
    std::printf("%u projectile cases passed: palm pixels, both facings, spawn timing, travel, impacts, pushboxes, crouching and clashes.\n",cases);
}
