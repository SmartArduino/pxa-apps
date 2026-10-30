// SPDX-License-Identifier: MIT
#include "../../../milkyfrog-2048/core.hpp"
#include "../../../milkyfrog-rain/core.hpp"
#include "../../../milkyfrog-miner/core.hpp"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned seed=0x12345;
static unsigned rng(){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
static unsigned sum(const uint8_t *p){unsigned result=0;for(int i=0;i<16;++i)if(p[i])result+=1u<<p[i];return result;}
int main(){
    uint8_t board[16]={1,1,1,1};auto result=frog2048::move(board,3);assert(result.changed&&result.gained==8&&board[0]==2&&board[1]==2&&board[2]==0);
    uint8_t capped[16]={12,12};result=frog2048::move(capped,3);assert(!result.changed&&capped[0]==12&&capped[1]==12);
    for(unsigned trial=0;trial<8000;++trial){for(auto &v:board)v=uint8_t(rng()%13);unsigned before=sum(board);frog2048::move(board,rng()%4);assert(sum(board)==before);for(auto v:board)assert(v<=12);}
    frograin::Engine rain;rain.reset(rng);
    rain.time=1;rain.miss({240,800,0,100,frograin::Plain,true});assert(rain.hearts==3);
    rain.time=16;rain.count=2;rain.items[0]={20,0,0,100,frograin::Plain,false};rain.items[1]={30,0,0,100,frograin::Coin,false};
    rain.combo=4;rain.caught({240,700,0,100,frograin::Plain,false});assert(rain.count==0&&rain.events&frograin::Burst&&rain.score==46&&rain.combo==5);
    rain.reset(rng);rain.time=16;rain.count=1;rain.items[0]={240,700,0,0,frograin::Ice,false};rain.step(.02f);assert(rain.catches==1&&rain.score==2&&rain.ice_until>rain.time);
    for(int i=0;i<10000&&!rain.over;++i){rain.target=240;rain.step(.02f);assert(rain.count<=12&&rain.hearts<=3&&rain.x>=0&&rain.x<=480);}
    for(unsigned i=1;i<=300;++i){seed=i;frogminer::Engine miner;miner.reset(rng,1+i%8,i%3,0);unsigned total=0;
        assert(miner.count<=40&&miner.count>3);for(unsigned j=0;j<miner.count;++j)if(miner.items[j].kind!=frogminer::Ice)total+=miner.items[j].value;assert(miner.target<=total);
    }
    frogminer::Engine miner;miner.reset(rng,1,0,0);miner.count=1;miner.items[0]={};auto &frog=miner.items[0];frog.x=400;frog.y=240;frog.r=30;frog.w=58;frog.h=56;frog.kind=frogminer::Rare;frog.rare=17;frog.value=700;
    miner.angle=0;miner.activate();for(int i=0;i<200&&!frog.gone;++i)miner.step(.02f);assert(frog.gone&&miner.score==700&&(miner.collection&(uint64_t(1)<<17)));
    miner.reset(rng,1,0,0);miner.time=.01f;miner.step(.02f);assert(miner.over&&(miner.events&frogminer::Lost));
    puts("PASS: 2048 conservation/cap; rain collision/combo/bonuses; 300 miner generations, delivery, collection and timeout.");
}
