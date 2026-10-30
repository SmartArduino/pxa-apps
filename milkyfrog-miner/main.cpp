// SPDX-License-Identifier: MIT
#include "../common/milky-frog/media.hpp"
#include "sprites.hpp"
#include "heroes.hpp"
#include "core.hpp"
#include <stdio.h>
#include <string.h>
namespace {
using namespace pocket;using namespace milky;
frogminer::Engine engine;unsigned page,difficulty,highest=1,best,accumulator,dex_page;uint64_t collection;bool dragging,first_frog;int down_x,down_y;unsigned laugh_id,laugh_time;
const char *const recordings[]={"assets/laugh-1-normal.s16","assets/laugh-2-normal.s16","assets/laugh-3-normal.s16","assets/laugh-4-normal.s16","assets/laugh-1-low.s16","assets/laugh-2-low.s16","assets/laugh-3-low.s16","assets/laugh-4-low.s16","assets/laugh-1-high.s16","assets/laugh-2-high.s16","assets/laugh-3-high.s16","assets/laugh-4-high.s16","assets/ice-1.s16","assets/ice-2.s16","assets/ice-3.s16","assets/andy.s16","assets/lose.s16"};
const Sprite *const rare[]={
    &art::rare_1,&art::rare_2,&art::rare_3,&art::rare_4,&art::rare_5,&art::rare_6,&art::rare_7,&art::rare_8,&art::rare_9,&art::rare_10,&art::rare_11,&art::rare_12,&art::rare_13,&art::rare_14,&art::rare_15,&art::rare_16,&art::rare_17,&art::rare_18,&art::rare_19,&art::rare_20,&art::rare_21,&art::rare_22,&art::rare_23,&art::rare_24,&art::rare_25,&art::rare_26,&art::rare_27,&art::rare_28,&art::rare_29,&art::rare_30,&art::rare_31,&art::rare_32,&art::rare_33,&art::rare_34,&art::rare_35,&art::rare_36,&art::rare_37,&art::rare_38,&art::rare_39,&art::rare_40,&art::rare_41,&art::rare_42,&art::rare_43,&art::rare_44,&art::rare_45,&art::rare_46,&art::rare_47};
const Sprite *const laughs[]={&art::laugh_0,&art::laugh_1,&art::laugh_2,&art::laugh_3,&art::laugh_4,&art::laugh_5,&art::laugh_6,&art::laugh_7,&art::laugh_8,&art::laugh_9,&art::laugh_10,&art::laugh_11,&art::laugh_12,&art::laugh_13,&art::laugh_14,&art::laugh_15,&art::laugh_16,&art::laugh_17,&art::laugh_18,&art::laugh_19};
unsigned collected(){unsigned n=0;for(unsigned i=0;i<47;++i)n+=bool(collection&(uint64_t(1)<<i));return n;}
void start(unsigned stage=1){engine.reset(pocket::random,stage,difficulty,collection);page=1;first_frog=false;accumulator=0;laugh_time=0;silence();log("MilkyMiner: started");redraw();}
void action(){if(page==1)engine.activate();else if(page==2)start(engine.level+1);else if(page==3)start(engine.level);else if(page==0)start();}
void tap(int x,int y){
    if(page==5){int result=settings_touch(x,y);if(result==2)page=0;if(result)save();redraw();return;}
    if(page==4){auto l=gallery_layout();unsigned capacity=unsigned(l.columns*l.rows),pages=(47+capacity-1)/capacity;
        if(back_button().contains(x,y))page=0;
        else if(l.previous.contains(x,y))dex_page=(dex_page+pages-1)%pages;
        else if(l.next.contains(x,y))dex_page=(dex_page+1)%pages;
        redraw();return;}
    auto l=home_layout(page==0);
    if(page==0){if(l.difficulty.contains(x,y)){difficulty=std::min(2,(x-l.difficulty.x)*3/l.difficulty.w);save();}
        else if(l.start.contains(x,y))start();else if(l.secondary[0].contains(x,y)){page=4;dex_page=0;}else if(l.secondary[1].contains(x,y))page=5;
    }else if(page==1)action();else if(l.start.contains(x,y))action();else if(l.secondary[0].contains(x,y)||l.secondary[1].contains(x,y))page=0;redraw();
}
const Sprite &item_sprite(const frogminer::Item &item){switch(item.kind){case frogminer::Small:case frogminer::Tiny:return art::frog_small;case frogminer::Big:return art::frog_big;case frogminer::Diamond:return art::diamond_small;case frogminer::Rare:return *rare[item.rare];case frogminer::Ice:return item.cubes==1?art::ice_1:item.cubes==2?art::ice_2:art::ice_3;default:return art::frog_small;}}
}
namespace game {
const char *title(){return "奶蛙工厂";}const char *app_id(){return "pxa-milkyfrog-miner";}
void init(){sounds(recordings,17);volume(100,20);page=0;difficulty=0;highest=1;best=0;collection=0;save();}
void reset(){start();}
void key(Key key){if(key==Key::B){page=0;silence();redraw();}else if(key==Key::Y){page=5;silence();redraw();}else if(key==Key::X){page=4;dex_page=0;silence();redraw();}else if(page==0&&(key==Key::LEFT||key==Key::RIGHT)){difficulty=(difficulty+1)%3;save();redraw();}else if(key==Key::A){if(page==5)audio_test();else action();}}
void pointer(Phase phase,int x,int y){if(phase==Phase::DOWN){down_x=x;down_y=y;dragging=true;}
    else if(phase==Phase::CANCEL)dragging=false;
    else if(phase==Phase::UP&&dragging){dragging=false;if(abs(x-down_x)<16&&abs(y-down_y)<16)tap(x,y);}}
void tick(uint32_t elapsed){if(page!=1)return;accumulator+=elapsed;engine.events=0;
    for(unsigned step=0;accumulator>=20&&step<5;++step){accumulator-=20;engine.step(.02f);}
    if(engine.events&frogminer::Grabbed){const auto &it=engine.items[engine.last_item];
        if(it.kind==frogminer::Small||it.kind==frogminer::Tiny||it.kind==frogminer::Big||it.kind==frogminer::Rare){laugh_id=pocket::random()%4+(it.kind==frogminer::Big?4:it.kind==frogminer::Tiny?8:0);play(laugh_id,0,true);laugh_time=1;
            if(!first_frog){play(15,2);first_frog=true;}}
        else if(it.kind==frogminer::Ice)play(11+it.cubes,1);
        else if(effects_volume())tone(it.kind==frogminer::Diamond?1400:180,.15f);
        log("MilkyMiner: grabbed");
    }
    if(laugh_time)laugh_time+=elapsed;
    if(engine.events&frogminer::Delivered){finish(0);laugh_time=0;collection=engine.collection;if(engine.score>best)best=engine.score;save();if(effects_volume())tone(900,.08f);log("MilkyMiner: delivered");}
    if(engine.events&frogminer::Cleared){page=2;highest=std::max(highest,engine.level+1);play(15,2);save();log("MilkyMiner: level cleared");}
    if(engine.events&frogminer::Lost){page=3;stop(0);play(16);log("MilkyMiner: time up");}
    redraw();
}
void draw(){clear(CREAM);auto b=bounds();int g=gap();
    if(page==5){settings_draw();return;}
    if(page==4){header(tr("Frog collection","奶蛙图鉴"),true);auto l=gallery_layout();unsigned capacity=unsigned(l.columns*l.rows),pages=(47+capacity-1)/capacity;dex_page%=pages;
        for(unsigned k=0;k<capacity;++k){unsigned i=dex_page*capacity+k;if(i>=47)break;auto r=gallery_cell(l,int(k));bool known=collection&(uint64_t(1)<<i);panel(r,known?PAPER:SOFT,false);
            const int caption=text_height()+2;if(known)thumbnail(*rare[i],{r.x+4,r.y+3,r.w-8,r.h-caption-3});else{int size=std::min(28,std::min(r.w-8,r.h-caption-4));icon(Icon::Lock,{r.x+(r.w-size)/2,r.y+(r.h-caption-size)/2,size,size},SUBTLE);}
            char value[24];snprintf(value,sizeof(value),"NO.%02u",i+1);label({r.x+3,r.bottom()-caption,r.w-6,caption},value,known?SAGE:SUBTLE,1,1);
        }
        action(l.previous,tr("Previous","上一页"),false,Icon::Back);char value[40];snprintf(value,sizeof(value),"%s %u/%u",tr("Next","下一页"),dex_page+1,pages);action(l.next,value,false,Icon::Next);return;}
    if(page==0){header(tr("Frog Factory","奶蛙工厂"));auto l=home_layout(true);portrait(art::hero_frog,l.hero);
        const int title=text_height(2),caption=text_height(),height=title+caption+g/2,top=l.copy.y+(l.copy.h-height)/2;
        label({l.copy.x,top,l.copy.w,title},tr("Let's go mining!","开工，抓奶蛙！"),BROWN,2,1);
        char value[48];snprintf(value,sizeof(value),"%s %u/47",tr("Collected","已收集"),collected());label({l.copy.x,top+title+g/2,l.copy.w,caption},value,SAGE,1,1);
        const char *modes[]={tr("Easy","简单"),tr("Normal","中等"),tr("Hard","困难")};int cw=(l.difficulty.w-8)/3;
        for(int i=0;i<3;++i)milky::action({l.difficulty.x+i*(cw+4),l.difficulty.y,cw,l.difficulty.h},modes[i],difficulty==unsigned(i));
        milky::action(l.start,tr("Start mining","开始挖矿"),true);milky::action(l.secondary[0],tr("Collection","奶蛙图鉴"));milky::action(l.secondary[1],tr("Sound","声音设置"));return;}
    if(page==2||page==3){header(page==2?tr("Great catch!","这班干得漂亮！"):tr("Time for a break","收工啦，下次加油"));auto l=home_layout(false);portrait(page==2?art::hero_frog:art::hero_angry,l.hero);
        int caption=text_height(),big=text_height(3),top=l.copy.y+(l.copy.h-caption-big-caption-g)/2;
        label({l.copy.x,top,l.copy.w,caption},tr("Score / goal","得分 / 目标"),SAGE,1,1);char value[40];snprintf(value,sizeof(value),"%u",engine.score);label({l.copy.x,top+caption,l.copy.w,big},value,BROWN,3,1);
        snprintf(value,sizeof(value),"%s %u",tr("Goal","目标"),engine.target);label({l.copy.x,top+caption+big+g/2,l.copy.w,caption},value,SUBTLE,1,1);
        milky::action(l.start,page==2?tr("Next level","下一关"):tr("Try again","再试一次"),true);milky::action(l.secondary[0],tr("Home","回工厂"));milky::action(l.secondary[1],tr("Home","返回首页"));return;}
    const int hud_w=menu_button().x-b.x-2*g,cw=(hud_w-10)/3,hud_h=touch_height();
    metric({b.x+g,b.y+g/2,cw,hud_h},tr("Level","关卡"),engine.level);metric({b.x+g+cw+5,b.y+g/2,cw,hud_h},tr("Goal","目标"),engine.target);metric({b.x+g+2*(cw+5),b.y+g/2,cw,hud_h},tr("Score","得分"),engine.score,true);
    Rect f{b.x+g,header_bottom()+g/2,b.w-2*g,b.bottom()-g-text_height()-header_bottom()-g/2};
    panel(f,pocket::rgb(121,87,54),false);int pivot_y=f.y+22;float sx=f.w/800.f,sy=(f.h-26)/472.f;
    rounded(f.x+5,f.y+23,f.w-10,3,pocket::rgb(184,150,89),1);
    for(unsigned i=0;i<28;++i){int x=f.x+6+(i*43+17)%std::max(1,f.w-12),y=f.y+32+(i*61)%std::max(1,f.h-40);rect(x,y,2,2,pocket::rgb(154,118,70));}
    char value[40];snprintf(value,sizeof(value),"%s %us",tr("Time","时间"),unsigned(engine.time)+1);label({f.x+7,f.y+2,std::min(100,f.w/3),20},value,PAPER);
    portrait(engine.misses>=3?art::od_angry:art::od_boss,{f.right()-32,f.y+1,26,22});
    int pivot_x=f.x+int(engine.pivot*sx);portrait(art::dandan_head,{pivot_x-10,f.y+1,20,21});
    int hx=f.x+int(engine.tip_x()*sx),hy=pivot_y+int((engine.tip_y()-128)*sy);line(pivot_x,pivot_y,hx,hy,SOFT);line(hx-3,hy-2,hx,hy+3,HONEY);line(hx,hy+3,hx+3,hy-2,HONEY);
    for(unsigned i=0;i<engine.count;++i){const auto &it=engine.items[i];if(it.gone)continue;int x=f.x+int(it.x*sx),y=pivot_y+int((it.y-128)*sy);float scale=std::min(sx,sy);int w=std::max(9,int(it.w*scale)),h=std::max(9,int(it.h*scale));
        if(it.kind==frogminer::Rock){rounded(x-w/2,y-h/2,w,h,pocket::rgb(165,150,122),4);line(x-w/3,y-h/4,x+w/4,y+h/4,SOFT);continue;}
        if(it.kind==frogminer::Ice){w=std::max(9,int(w*it.ratio));h=std::max(9,int(h*it.ratio));}
        const Sprite *s=&item_sprite(it);if(it.grabbed&&laugh_time&&it.kind!=frogminer::Ice)s=laughs[(laugh_time/65)%20];portrait(*s,{x-w/2,y-h/2,w,h});
        if(it.kind==frogminer::Big&&difficulty)portrait(art::diamond_small,{x,y+3,10,10});
    }
    label({b.x+g,b.bottom()-text_height()-g/2,b.w-2*g,text_height()},engine.hook==0?tr("Tap anywhere to lower the hook","点一下放钩，再点提前收钩"):tr("Hang on, bringing them home!","收钩中，奶蛙正在回家"),SUBTLE,1,1);
}
size_t serialize(uint8_t *p,size_t n){if(n<32)return 0;memset(p,0,32);memcpy(p,"MFMN",4);p[4]=1;p[5]=uint8_t(difficulty);p[6]=uint8_t(effects_volume());p[7]=uint8_t(music_volume());put32(p+8,best);put32(p+12,highest);put32(p+16,uint32_t(collection));put32(p+20,uint32_t(collection>>32));put32(p+28,hash(p,28));return 32;}
void load(const uint8_t *p,size_t n){if(n!=32||memcmp(p,"MFMN",4)||p[4]!=1||p[5]>2||p[6]>100||p[7]>100||get32(p+24)||get32(p+28)!=hash(p,28)||!get32(p+12)||get32(p+12)>100000||(get32(p+20)&0xffff8000u))return;difficulty=p[5];best=get32(p+8);highest=get32(p+12);collection=uint64_t(get32(p+16))|uint64_t(get32(p+20))<<32;volume(p[6],p[7]);redraw();}
}
