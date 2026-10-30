// SPDX-License-Identifier: MIT
// Bounded C++ adaptation of nai wa milk rain's public engine.js.
#pragma once
#include <algorithm>
#include <math.h>
#include <stdint.h>
namespace frograin {
enum Type:uint8_t{Plain,Ice,Rich,Double,Coin};
enum Event:unsigned{Caught=1,Missed=2,Burst=4,Healed=8,Ended=16};
struct Item{float x,y,vx,vy;Type type;bool safe;};
struct Engine{
    Item items[12]{};unsigned count=0,score=0,hearts=3,combo=0,max_combo=0,catches=0,spawned=0,size=2,events=0;
    float time=0,spawn_in=.7f,x=240,target=240,vx=0,ice_until=0,rich_until=0,double_until=0,burst_until=0,laugh_until=0,hurt_until=0;
    bool over=false;Type last_type=Plain;
    unsigned (*random)()=nullptr;
    float unit(){return float(random()&0xffffff)/16777216.f;}
    unsigned dimension_size()const{return std::min(6u,size+(rich_until>time?1u:0u));}
    float width()const{return 70.f+dimension_size()*8;}
    float height()const{return 132.f+dimension_size()*8;}
    unsigned multiplier()const{return (combo>=5?4:combo>=3?2:1)*(double_until>time?2:1);}
    static unsigned points(Type type){return type==Coin?10:type==Plain?1:2;}
    void reset(unsigned (*rng)()){*this=Engine{};random=rng;}
    void spawn(){if(count>=12)return;Type type=Plain;
        if(spawned>=3){float pick=unit()*100;const int weights[]={55,12,12,12,9};for(int i=0;i<5;++i){pick-=weights[i];if(pick<=0){type=Type(i);break;}}}
        float speed=std::min(150.f+score*3.6f,455.f)*(time<15?.7f:1.f)*(burst_until>time?.85f:1.f);
        items[count++]={34+unit()*412,-32,unit()<.22f?(unit()-.5f)*58:0,speed*(.93f+unit()*.14f),type,time<15};++spawned;
    }
    void caught(Item item){
        score+=points(item.type)*multiplier();++catches;++combo;max_combo=std::max(max_combo,combo);events|=Caught;last_type=item.type;
        if(item.type==Ice)ice_until=time+5;if(item.type==Rich)rich_until=time+5;if(item.type==Double)double_until=time+5;
        if(item.type==Coin){if(hearts<3){++hearts;events|=Healed;}else ice_until=std::max(ice_until,time+3);}
        if(catches%3==0&&size<6)++size;
        if(combo==5){for(unsigned i=0;i<count;++i)score+=points(items[i].type)*multiplier();count=0;burst_until=time+4;laugh_until=time+4.3f;events|=Burst;}
    }
    void miss(Item item){if(item.safe||time<15)return;combo=0;rich_until=0;--hearts;size=size?size-1:0;hurt_until=time+.8f;events|=Missed;
        if(!hearts){over=true;events|=Ended;}}
    void step(float dt,int direction=0){
        if(over||dt<=0||dt>.051f)return;time+=dt;
        static constexpr float speed_by_size[]={1.25f,1.15f,1,.92f,.85f,.78f,.72f};
        const float speed=430*speed_by_size[dimension_size()]*(ice_until>time?1.5f:1)*(burst_until>time?1.3f:1);
        if(direction)target=std::clamp(target+direction*speed*dt,0.f,480.f);
        const float desired=std::clamp((target-x)*11,-speed,speed);
        vx+=std::clamp(desired-vx,-3500*dt,3500*dt);x=std::clamp(x+vx*dt,width()/2+7,480-width()/2-7);
        spawn_in-=dt;if(spawn_in<=0&&count<12){spawn();spawn_in=std::max(.39f,1.13f-score*.003f)*(burst_until>time?1.18f:1);}
        for(unsigned i=0;i<count&&!over;){
            auto &item=items[i];const unsigned steps=unsigned(ceilf((fabsf(item.vy)+fabsf(item.vx))*dt/12))+1;
            bool hit=false;
            for(unsigned k=0;k<steps;++k){
                item.x+=item.vx*dt/steps;item.y+=item.vy*dt/steps;
                if(item.x<25||item.x>455){item.vx=-item.vx;item.x=std::clamp(item.x,25.f,455.f);}
                float near_x=std::clamp(item.x,x-(68+dimension_size()*10)/2,x+(68+dimension_size()*10)/2);
                float near_y=std::clamp(item.y,793-height()*.87f,789.f);
                hit=(item.x-near_x)*(item.x-near_x)+(item.y-near_y)*(item.y-near_y)<=19.09f*19.09f;if(hit)break;
            }
            if(hit||item.y+23>805){Item removed=item;for(unsigned j=i+1;j<count;++j)items[j-1]=items[j];--count;if(hit)caught(removed);else miss(removed);}
            else ++i;
        }
    }
};
}
