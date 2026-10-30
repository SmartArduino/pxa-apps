// SPDX-License-Identifier: MIT
#include "../common/milky-frog/media.hpp"
#include "sprites.hpp"
#include "heroes.hpp"
#include "core.hpp"
#include <stdio.h>
#include <string.h>
namespace {
using namespace pocket;using namespace milky;
frograin::Engine engine;uint32_t best;unsigned page=0,accumulator;bool dragging;int down_x,down_y;
const char *const recordings[]={"assets/laugh.s16","assets/ding.s16","assets/andy.s16"};
const Sprite *const laughs[]={&art::laugh_0,&art::laugh_1,&art::laugh_2,&art::laugh_3,&art::laugh_4,&art::laugh_5,&art::laugh_6,&art::laugh_7,&art::laugh_8,&art::laugh_9,&art::laugh_10,&art::laugh_11,&art::laugh_12,&art::laugh_13,&art::laugh_14,&art::laugh_15,&art::laugh_16,&art::laugh_17,&art::laugh_18,&art::laugh_19};
void start(){engine.reset(pocket::random);page=1;accumulator=0;dragging=false;silence();redraw();log("MilkyRain: started");}
Rect field(){auto b=bounds();int g=gap(),top=header_bottom()+g/2;return {b.x+g,top,b.w-2*g,b.bottom()-g-text_height()-top};}
void tap(int x,int y){
    if(page==3){int result=settings_touch(x,y);if(result==2)page=0;if(result)save();redraw();return;}
    if(page==4){if(back_button().contains(x,y)||settings_layout().back.contains(x,y))page=0;redraw();return;}
    if(page==0||page==2){auto l=home_layout(false);if(l.start.contains(x,y))start();else if(l.secondary[0].contains(x,y))page=3;else if(l.secondary[1].contains(x,y))page=4;}
    redraw();
}
void heart(int x,int y,int size,bool active){
    const auto color=active?pocket::rgb(215,113,99):BORDER;
    for(int row=0;row<8;++row){int width=row<2?6:row<4?8:10-row;int left=(8-width)/2;
        rect(x+left*size/8,y+row*size/8,std::max(1,width*size/8),std::max(1,size/8+1),color);
        if(row==0)rect(x+3*size/8,y,size/4,size/8+1,CREAM);
    }
}

}
namespace game {
const char *title(){return "奶蛙的奶块雨";}const char *app_id(){return "pxa-milkyfrog-rain";}
void init(){sounds(recordings,3);volume(100,20);engine.reset(pocket::random);best=0;page=0;save();}
void reset(){start();}
void key(Key key){if(key==Key::B){page=0;silence();redraw();}else if(key==Key::Y){page=3;silence();redraw();}else if(key==Key::A&&page!=1){if(page==3)audio_test();else start();}}
void pointer(Phase phase,int x,int y){if(phase==Phase::DOWN){dragging=true;down_x=x;down_y=y;if(page==1)engine.target=std::clamp((x-field().x)*480.f/field().w,0.f,480.f);}
    else if(phase==Phase::MOVE&&dragging&&page==1)engine.target=std::clamp((x-field().x)*480.f/field().w,0.f,480.f);
    else if(phase==Phase::CANCEL)dragging=false;
    else if(phase==Phase::UP&&dragging){dragging=false;if(page!=1&&abs(x-down_x)<12&&abs(y-down_y)<12)tap(x,y);}}
void tick(uint32_t elapsed){if(page!=1)return;accumulator+=elapsed;engine.events=0;
    for(unsigned step=0;accumulator>=20&&step<5;++step){accumulator-=20;engine.step(.02f,int(held(Key::RIGHT))-int(held(Key::LEFT)));}
    if(engine.events&frograin::Caught){if(effects_volume())tone(700+engine.combo*40,.06f);if(engine.last_type==frograin::Ice)play(1,1);if(engine.last_type==frograin::Coin)play(2,2);}
    if(engine.events&frograin::Burst){play(0);log("MilkyRain: five-combo burst");}
    if(engine.events&frograin::Missed){play(1,1);log("MilkyRain: miss");}
    if(engine.score>best){best=engine.score;save();}
    if(engine.events&frograin::Ended){page=2;play(0);save();log("MilkyRain: game over");}
    redraw();
}
void draw(){clear(CREAM);auto b=bounds();int g=gap();
    if(page==3){settings_draw();return;}
    if(page==4){header(tr("How to play","怎么玩"),true);auto l=settings_layout();
        panel(l.effects);label(l.effects,tr("Drag left and right to catch","左右拖动，接住奶块"),BROWN,1,1);
        panel(l.music);label(l.music,tr("Five catches: laugh & clear","五连击，大笑清屏"),BROWN,1,1);
        panel(l.preview);label(l.preview,tr("Gold coins restore hearts","金币补心，漏接扣心"),BROWN,1,1);
        action(l.back,tr("Got it","知道啦"),true);return;}
    if(page==0||page==2){header(page==2?tr("A lovely shower!","快乐接住啦！"):tr("Milky Rain","奶蛙的奶块雨"));auto l=home_layout(false);
        portrait(page==2?art::hero_heart:art::hero_frog,l.hero);
        int caption=text_height(),big=text_height(3),cy=l.copy.y+(l.copy.h-caption-big-caption-g)/2;
        label({l.copy.x,cy,l.copy.w,caption},page==2?tr("This round","本局得分"):tr("Personal best","我的最高分"),SAGE,1,1);
        char value[24];snprintf(value,sizeof(value),"%u",page==2?engine.score:best);label({l.copy.x,cy+caption,l.copy.w,big},value,BROWN,3,1);
        label({l.copy.x,cy+caption+big+g/2,l.copy.w,caption},page==2?tr("Thanks for playing","奶蛙吃饱啦"):tr("Five catches, one big laugh","五连击，一起大笑"),SUBTLE,1,1);
        action(l.start,page==2?tr("One more shower","再接一场奶块雨"):tr("Start catching","开始接奶块"),true);
        action(l.secondary[0],tr("Sound","声音设置"));action(l.secondary[1],tr("How to play","怎么玩"));return;}
    auto f=field();int hud_w=menu_button().x-b.x-2*g,hud_h=touch_height();
    metric({b.x+g,b.y+g/2,hud_w*55/100,hud_h},tr("Score","得分"),engine.score,true);
    metric({b.x+g+hud_w*55/100+5,b.y+g/2,hud_w*45/100-5,hud_h},tr("Best","最高"),best);
    panel(f,pocket::rgb(245,245,222),false);
    for(int i=0;i<22;++i){int x=f.x+7+(i*67+19)%std::max(1,f.w-14),y=f.y+8+(i*47+17)%std::max(1,f.h-16);rect(x,y,2,2,BORDER);}
    int floor=f.bottom()-4,block=std::clamp(f.w/15,16,26);
    for(unsigned i=0;i<engine.count;++i){const auto &item=engine.items[i];int x=f.x+int(item.x*f.w/480),y=f.y+int(item.y*f.h/854);
        if(y<f.y||y>floor)continue;
        const uint16_t colors[]={HONEY,pocket::rgb(164,218,224),pocket::rgb(237,183,71),pocket::rgb(193,177,214),HONEY};
        if(item.type==frograin::Coin){portrait(art::coin,{x-block/2,y-block/2,block,block});continue;}
        rounded(x-block/2,y-block/2,block,block,colors[item.type],4);portrait(art::face,{x-block/2+2,y-block/2+2,block-4,block-4});
    }
    int height=std::clamp(f.h*27/100+int(engine.dimension_size()),42,82),width=height*3/4,px=f.x+int(engine.x*f.w/480);
    rounded(px-width/2,floor-2,width,4,SOFT,2);const Sprite *frog=&art::frog;
    if(engine.hurt_until>engine.time)frog=&art::angry;
    else if(engine.laugh_until>engine.time){unsigned frame=unsigned((4.3f-(engine.laugh_until-engine.time))/4.3f*20);frog=laughs[std::min(frame,19u)];}
    portrait(*frog,{px-width/2,floor-height,width,height});
    int heart_size=std::clamp(f.w/22,12,18);for(unsigned i=0;i<3;++i)heart(f.right()-g-(3-int(i))*(heart_size+4),f.y+g,heart_size,i<engine.hearts);
    char status[64];
    if(engine.time<15)snprintf(status,sizeof(status),"%s %us",tr("Practice","练习保护"),unsigned(15-engine.time)+1);
    else snprintf(status,sizeof(status),"%s %u  x%u",tr("Combo","连击"),engine.combo,engine.multiplier());
    label({f.x+8,f.y+5,f.w-3*(heart_size+4)-2*g,text_height()},status,SAGE);
    label({b.x+g,b.bottom()-text_height()-g/2,b.w-2*g,text_height()},tr("Drag to move · arrows also work","左右拖动接奶块 · 方向键也可操作"),SUBTLE,1,1);
}
size_t serialize(uint8_t *p,size_t n){if(n<20)return 0;memset(p,0,20);memcpy(p,"MFRN",4);p[4]=1;p[5]=uint8_t(effects_volume());p[6]=uint8_t(music_volume());put32(p+8,best);put32(p+12,engine.max_combo);put32(p+16,hash(p,16));return 20;}
void load(const uint8_t *p,size_t n){if(n!=20||memcmp(p,"MFRN",4)||p[4]!=1||p[5]>100||p[6]>100||p[7]||get32(p+16)!=hash(p,16))return;best=get32(p+8);volume(p[5],p[6]);redraw();}
}
