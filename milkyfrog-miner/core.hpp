// SPDX-License-Identifier: MIT
// Bounded PXA adaptation of 66970010-boop/frog-miner (MIT).
#pragma once
#include <algorithm>
#include <math.h>
#include <stdint.h>
namespace frogminer {
enum Kind:uint8_t{Small,Tiny,Big,Diamond,Rock,Rare,Ice};
enum Event:unsigned{Grabbed=1,Delivered=2,Missed=4,Cleared=8,Lost=16};
struct Item{float x,y,r,w,h,ratio=1,initial_ratio=1,home=0,speed=0;unsigned value=0,original_value=0;Kind kind=Small;uint8_t rare=0,cubes=1;bool gone=false,grabbed=false;};
struct Engine{
    Item items[40]{};unsigned count=0,level=1,score=0,target=800,difficulty=0,events=0,misses=0;
    float time=45,angle=0,length=54,phase=0,pivot=400,clock=0,melt_rate=0;
    int hook=0,carried=-1,last_item=-1;bool clear=false,over=false;uint64_t collection=0;
    unsigned (*random)()=nullptr;
    float unit(){return float(random()&0xffffff)/16777216.f;}
    float range(float a,float b){return a+(b-a)*unit();}
    void reset(unsigned(*rng)(),unsigned stage,unsigned mode,uint64_t dex){*this=Engine{};random=rng;level=stage;difficulty=mode;collection=dex;phase=range(0,6.283f);generate();}
    float tip_x()const{return pivot+sinf(angle)*length;}float tip_y()const{return 128+cosf(angle)*length;}
    void place(Kind kind,unsigned number){
        for(unsigned n=0;n<number&&count<40;++n)for(unsigned attempt=0;attempt<400;++attempt){
            const float a=kind==Diamond?(unit()<.5f?-1:1)*range(.62f,.98f)*1.05f:range(-.9975f,.9975f);
            const float maximum=std::min(446.f,426/std::max(.52f,cosf(a)));
            const float len=kind==Diamond?range(maximum*.82f,maximum*.98f):range(160,maximum);
            Item it;it.x=400+sinf(a)*len;it.y=128+cosf(a)*len;it.kind=kind;
            if(it.y<237)continue;
            switch(kind){
                case Small:it.r=30;it.w=58;it.h=56;it.value=unsigned(range(8,12)+.5f)*10;break;
                case Tiny:it.r=24;it.w=42;it.h=40;it.value=60;break;
                case Big:it.r=50;it.w=90;it.h=126;it.value=unsigned(range(45,60)+.5f)*10;break;
                case Diamond:it.r=18;it.w=it.h=26;it.value=250;break;
                case Rock:it.r=38;it.w=56;it.h=54;it.value=1+random()%10;break;
                case Rare:{it.r=36;it.w=72;it.h=80;unsigned available[47],k=0;for(unsigned i=0;i<47;++i)if(!(collection&(uint64_t(1)<<i)))available[k++]=i;it.rare=uint8_t(k?available[random()%k]:random()%47);it.value=it.rare<9?600+it.rare*50:560+(it.rare*37)%280;break;}
                case Ice:it.cubes=uint8_t(1+random()%3);it.r=25+it.cubes*10;it.w=55+it.cubes*25;it.h=75;it.ratio=it.initial_ratio=range(.55f,1);it.value=it.cubes*50;break;
            }
            bool valid=true;for(unsigned j=0;j<count;++j){const float dx=items[j].x-it.x,dy=items[j].y-it.y;float distance=items[j].r+it.r+14;if(dx*dx+dy*dy<distance*distance){valid=false;break;}}
            if(!valid)continue;
            if(kind==Big&&difficulty){it.home=it.x;it.speed=range(30,85)*(unit()<.5f?-1:1);it.value+=400;}
            it.original_value=it.value;items[count++]=it;break;
        }
    }
    void generate(){
        place(Small,2+std::min(level,6u));place(Tiny,level>=4?2:1);place(Big,std::min(1+(level+1)/2,5u));place(Diamond,level>=2?2:1);place(Rock,2+(level*2+2)/3);place(Rare,1);
        unsigned rocks=0,rock_value=0;for(unsigned i=0;i<count;++i)if(items[i].kind==Rock){++rocks;rock_value+=items[i].value;}
        place(Ice,(rocks+1)/2);
        unsigned total=0;float maximum=0;for(unsigned i=0;i<count;++i){auto &it=items[i];if(it.kind!=Ice)total+=it.value;else{it.original_value=it.value=it.cubes*10*(rocks?rock_value/rocks:5);maximum=std::max(maximum,it.ratio);}}
        target=std::max(500u,unsigned(total*.62f/50+.5f)*50);melt_rate=maximum/45;
    }
    void activate(){if(over||clear)return;if(hook==0)hook=1;else if(hook==1){hook=2;++misses;events|=Missed;}}
    void step(float dt){
        if(over||clear||dt<=0||dt>.051f)return;time=std::max(0.f,time-dt);clock+=dt;
        if(difficulty==2)pivot=400+sinf(clock*1.4f)*40;
        for(unsigned i=0;i<count;++i){auto &it=items[i];if(it.gone||it.grabbed)continue;
            if(it.kind==Big&&difficulty){it.x+=it.speed*dt;if(fabsf(it.x-it.home)>80){it.speed=-it.speed;it.x=std::clamp(it.x,it.home-80,it.home+80);}}
            if(it.kind==Ice){it.ratio=std::max(.08f,it.ratio-melt_rate*dt);it.value=unsigned(it.original_value*it.ratio);}
        }
        if(hook==0){phase+=dt*3.14159265f;angle=1.05f*sinf(phase);length=54;}
        else if(hook==1){
            const unsigned steps=unsigned(ceilf(330*dt/8));
            for(unsigned k=0;k<steps&&hook==1;++k){length+=330*dt/steps;
                if(length>=470||tip_x()<12||tip_x()>788||tip_y()>587){hook=2;++misses;events|=Missed;break;}
                for(unsigned i=0;i<count;++i){auto &it=items[i];if(it.gone||it.grabbed)continue;float dx=tip_x()-it.x,dy=tip_y()-it.y;float radius=(it.kind==Ice?it.r*it.ratio:it.r)+12;
                    if(dx*dx+dy*dy<radius*radius){carried=last_item=int(i);hook=2;it.grabbed=true;events|=Grabbed;if(it.kind!=Rock)misses=0;else ++misses;break;}}
            }
        }else{
            const float speeds[]={250,285,70,330,90,260,120};float speed=carried<0?420:speeds[items[carried].kind];
            if(carried>=0&&items[carried].kind==Ice)speed=90+210*items[carried].ratio;
            if(carried>=0&&items[carried].kind==Big&&fmodf(clock,.8f)>.32f)speed*=.05f;
            length-=speed*dt;
            if(carried>=0){items[carried].x=tip_x();items[carried].y=tip_y();}
            if(length<=54){length=54;hook=0;if(carried>=0){auto &it=items[carried];score+=it.value;it.gone=true;last_item=carried;if(it.kind==Rare)collection|=uint64_t(1)<<it.rare;events|=Delivered;carried=-1;
                    if(score>=target){clear=true;events|=Cleared;}}}
        }
        if(time<=0&&!clear){over=true;events|=Lost;}
    }
};
}
