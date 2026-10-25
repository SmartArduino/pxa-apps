// SPDX-License-Identifier: CC0-1.0
#include "../common/pocket-play/runtime.hpp"
#include "core.hpp"
#include "../common/pocket-play/music.hpp"
#include <stdio.h>
#include <string.h>
namespace {
using pocket::Key;using pocket::Phase;using pocket::tr;using namespace elements;
using pocket_music::Cue;
State state;
pocket_music::Effects effects;
bool captured,valid_point,stroke_changed,last_a;
int last_x,last_y;
uint32_t paint_clock,save_clock,reaction_clock;
constexpr uint16_t BG=pocket::rgb(16,28,42),PANEL=pocket::rgb(28,46,59),INK=pocket::rgb(240,242,216),MINT=pocket::rgb(127,232,189),GOLD=pocket::rgb(255,211,112),FAINT=pocket::rgb(125,153,163);
constexpr uint8_t colors[MATERIALS][3]={{23,40,51},{234,186,98},{63,158,216},{115,135,145},{163,111,68},{255,132,69},{176,217,225},{86,191,117},{191,157,91},{242,87,58}};
int clamp(int n,int a,int b){return n<a?a:n>b?b:n;}
void cue(Cue value){effects.set_enabled(!state.muted);effects.cue(value);}
void release(){captured=valid_point=false;stroke_changed=false;last_a=false;paint_clock=0;}
void changed(bool save=true){pocket::redraw();if(save)pocket::save();}
void save_stroke(){if(stroke_changed)pocket::save();release();}
void toggle_sound(){state.muted^=1;effects.set_enabled(!state.muted);if(!state.muted)cue(Cue::Confirm);pocket::log(state.muted?"Elements: sound off":"Elements: sound on");changed();}
const char *material_name(unsigned m){static const char *const zh[]={"擦除","沙","水","石","木","火","蒸汽","植物","种子","熔岩"};return tr(name(m),m<MATERIALS?zh[m]:"?");}
const char *scene_label(unsigned s){static const char *const zh[]={"自由创作","装满水罐","浇灌森林","守住防火带","冷却熔岩"};return tr(scene_name(s),s<5?zh[s]:"?");}
void label(int x,int y,const char *text,uint16_t color=INK){pocket::text(x,y,text,color);}
void center(int y,const char *text,uint16_t color=INK,int scale=1){pocket::text((288-pocket::text_width(text,scale))/2,y,text,color,scale);}
void button(int x,int y,int width,int height,const char *text,bool selected=false){pocket::rect(x,y,width,height,selected?pocket::rgb(54,101,113):PANEL);pocket::outline(x,y,width,height,selected?MINT:FAINT);label(x+(width-pocket::text_width(text))/2,y+(height-10)/2,text);}
void choose_material(unsigned m){if(allowed(state.scene,m)){state.material=uint8_t(m);state.page=m/5;cue(Cue::Select);pocket::log("Elements: material selected");changed();}else cue(Cue::Error);}
void cycle_material(int direction){for(int i=1;i<=MATERIALS;++i){int m=(int(state.material)+direction*i+MATERIALS*2)%MATERIALS;if(allowed(state.scene,unsigned(m))){choose_material(unsigned(m));return;}}}
void ask(unsigned pending,unsigned target){save_stroke();state.ui=CONFIRM;state.pending=uint8_t(pending);state.target=uint8_t(target);state.confirm_yes=0;cue(Cue::Select);changed();}
void confirm(bool yes){if(yes){if(state.pending==1){memset(state.cells,0,sizeof(state.cells));state.scene_start=state.steps;state.burned=state.cooled=0;state.paused=1;}else reset(state,state.target);save_clock=0;cue(Cue::Clear);pocket::log("Elements: scene replaced after confirmation");}else cue(Cue::Select);state.ui=WORLD;state.pending=0;release();changed();}
bool in_grid(int x,int y){return x>=30&&x<246&&y>=40&&y<184;}
void stamp(int x,int y,bool interpolate){bool painted=interpolate&&valid_point?stroke(state,last_x,last_y,x,y,state.material,state.brush):paint(state,x,y,state.material,state.brush);stroke_changed=painted||stroke_changed;if(painted)cue(Cue::Paint);last_x=x;last_y=y;valid_point=true;state.cx=uint8_t(x);state.cy=uint8_t(y);pocket::redraw();}
void tasks(){save_stroke();state.selected=state.scene;state.ui=TASKS;cue(Cue::Select);changed();}
void tool(int row){save_stroke();if(row==0)state.paused^=1;else if(row==1)state.brush=uint8_t((state.brush+1)%3);else if(row==2){choose_material(EMPTY);return;}else if(row==3){tasks();return;}else if(row==4){state.ui=HELP;}else if(row==5){ask(1,state.scene);return;}cue(Cue::Select);changed();}
void pixel_world(){
    uint16_t *fb=pocket::framebuffer();if(!fb)return;
    for(int y=0;y<H;++y)for(int x=0;x<W;++x){const unsigned m=at(state,x,y);const uint32_t h=uint32_t(x*173+y*401+int(m)*71);int light=int((h^(h>>4))%15)-7;
        if(m==FIRE||m==LAVA)light+=int((h+state.steps*7)%24);
        const int red=clamp(int(colors[m][0])+light,0,255),green=clamp(int(colors[m][1])+light,0,255),blue=clamp(int(colors[m][2])+light,0,255);
        uint16_t base=pocket::rgb(unsigned(red),unsigned(green),unsigned(blue));
        const uint16_t bright=pocket::rgb(unsigned(clamp(red+17,0,255)),unsigned(clamp(green+17,0,255)),unsigned(clamp(blue+17,0,255)));
        const uint16_t shade=pocket::rgb(unsigned(clamp(red-14,0,255)),unsigned(clamp(green-14,0,255)),unsigned(clamp(blue-14,0,255)));
        const int px=30+3*x,py=40+3*y;
        for(int dy=0;dy<3;++dy)for(int dx=0;dx<3;++dx)fb[(py+dy)*288+px+dx]=m==EMPTY?base:dy==0&&dx==0?bright:dy==2?shade:base;
        if(m==FIRE&&((h+state.steps)%7)==0)fb[py*288+px+1]=GOLD;
    }
}
void help(){pocket::rect(22,22,256,189,BG);pocket::outline(22,22,256,189,MINT);center(26,tr("TINY ELEMENTS","微型元素"),MINT,2);
    const char *lines[]={tr("HOLD / DRAG TO PAINT A WORLD","按住或拖动，画出粒子世界"),tr("SAND FALLS. WATER FLOWS.","沙会下落，水会流动"),tr("SEED + WATER GROWS PLANTS","种子遇水会长成植物"),tr("FIRE BURNS WOOD AND PLANTS","火能烧掉木头和植物"),tr("WATER + LAVA = STONE + STEAM","水遇熔岩，生成石头和蒸汽"),tr("STEAM COOLS BACK INTO WATER","蒸汽冷却后又会变成水"),tr("TASKS START PAUSED: BUILD FIRST","挑战先暂停，布置好再开始"),tr("RIGHT: PAUSE SIZE ERASE TASKS","右侧：暂停、笔刷、擦除、挑战"),tr("BOTTOM: MATERIALS / NEXT PAGE","底部选材料，右下角翻页"),tr("D-PAD CURSOR / HOLD A TO PAINT","方向键移动，按住A绘制"),tr("L/R MATERIAL   B ERASE","L/R切换材料，B擦除"),tr("X PAUSE  Y SIZE  SELECT TASKS","X暂停，Y笔刷，SELECT挑战"),tr("START MENU / Y SOUND HERE","START菜单；此页Y切换声音")};
    for(int i=0;i<13;++i)label(30,51+10*i,lines[i],i<6?INK:FAINT);
    button(40,187,110,18,tr("A CREATE","A 开始创作"),true);
    button(155,187,110,18,state.muted?tr("SOUND OFF","声音 关"):tr("SOUND ON","声音 开"),!state.muted);
}
void draw_tasks(){pocket::rect(22,22,256,189,BG);center(27,tr("FIVE LITTLE WORLDS","五个小小世界"),MINT);for(int i=0;i<5;++i){button(30,43+29*i,238,25,scene_label(unsigned(i)),state.selected==i);if(i&&state.badges&(1u<<(i-1)))label(252,52+29*i,"*",GOLD);}label(30,194,tr("A SELECT   B BACK   ARROWS MOVE","A选择  B返回  方向键移动"),FAINT);}
void draw_confirm(){pocket::rect(27,61,238,140,BG);pocket::outline(27,61,238,140,GOLD);center(73,tr("REPLACE THIS WORLD?","替换当前世界？"),GOLD);center(94,state.pending==1?tr("CLEAR ALL PARTICLES","清除所有粒子"):scene_label(state.target));center(114,tr("THIS CANNOT BE UNDONE.","此操作无法撤销"));center(130,tr("YOUR FOUR BADGES STAY.","已获得的徽章会保留"),MINT);button(36,168,98,23,tr("REPLACE","确认替换"),state.confirm_yes);button(154,168,98,23,tr("CANCEL","取消"),!state.confirm_yes);}
}
namespace game {
const char *title(){return tr("Tiny Elements","微型元素");}
const char *app_id(){return "pxa-tiny-elements";}
void init(){elements::init(state);release();save_clock=reaction_clock=0;effects.reset(!state.muted);pocket::log("Elements: ready 72x48 20Hz");pocket::redraw();}
void reset(){ask(2,state.scene);}
void key(Key key){
    if(state.ui==HELP){if(key==Key::Y)toggle_sound();else if(key==Key::A||key==Key::B){state.ui=WORLD;cue(Cue::Confirm);changed();}return;}
    if(state.ui==CONFIRM){if(key==Key::LEFT)state.confirm_yes=1;else if(key==Key::RIGHT)state.confirm_yes=0;else if(key==Key::B){confirm(false);return;}else if(key==Key::A){confirm(state.confirm_yes!=0);return;}changed();return;}
    if(state.ui==TASKS){if(key==Key::B){state.ui=WORLD;changed();}else if(key==Key::A)ask(2,state.selected);else{state.selected=uint8_t(clamp(int(state.selected)+(key==Key::UP?-1:key==Key::DOWN?1:0),0,4));cue(Cue::Select);changed();}return;}
    if(key==Key::SELECT){tasks();return;}if(key==Key::X){tool(0);return;}if(key==Key::Y){tool(1);return;}if(key==Key::B){tool(2);return;}if(key==Key::L){cycle_material(-1);return;}if(key==Key::R){cycle_material(1);return;}
    if(key==Key::A){const bool painted=paint(state,state.cx,state.cy,state.material,state.brush);stroke_changed=painted||stroke_changed;if(painted)cue(Cue::Paint);changed(false);return;}
    state.cx=uint8_t(clamp(int(state.cx)+(key==Key::LEFT?-1:key==Key::RIGHT?1:0),0,W-1));state.cy=uint8_t(clamp(int(state.cy)+(key==Key::UP?-1:key==Key::DOWN?1:0),0,H-1));pocket::redraw();
}
void pointer(Phase phase,int x,int y){
    if(phase==Phase::CANCEL||phase==Phase::UP){save_stroke();return;}
    if(phase==Phase::MOVE){if(captured){if(in_grid(x,y))stamp((x-30)/3,(y-40)/3,true);else valid_point=false;}return;}
    save_stroke();
    if(state.ui==HELP){if(y>=182&&y<211){if(x>=150)toggle_sound();else{state.ui=WORLD;cue(Cue::Confirm);changed();}}return;}
    if(state.ui==CONFIRM){if(y>=164&&y<196){if(x>=36&&x<134)confirm(true);else if(x>=154&&x<252)confirm(false);}return;}
    if(state.ui==TASKS){if(x>=30&&x<268&&y>=43&&y<188){const unsigned n=unsigned((y-43)/29);if(n<5){state.selected=uint8_t(n);ask(2,n);}}else if(y>=190){state.ui=WORLD;changed();}return;}
    if(in_grid(x,y)){captured=true;stamp((x-30)/3,(y-40)/3,false);return;}
    if(x>=252&&x<282){if(y>=40&&y<184)tool((y-40)/24);else if(y>=188&&y<212){state.page^=1;cue(Cue::Select);changed();}return;}
    if(y>=188&&y<212&&x>=30&&x<245){const unsigned n=unsigned((x-30)/43)+state.page*5u;if(n<MATERIALS)choose_material(n);}
}
void tick(uint32_t elapsed){
    elapsed=elapsed>100?100:elapsed;
    if(state.ui!=WORLD)return;
    const bool a=pocket::held(Key::A);
    if(last_a&&!a&&stroke_changed){pocket::save();stroke_changed=false;}last_a=a;
    paint_clock+=elapsed;
    if(paint_clock>=50){paint_clock%=50;if(captured&&valid_point)stamp(last_x,last_y,false);if(a){const bool painted=paint(state,state.cx,state.cy,state.material,state.brush);stroke_changed=painted||stroke_changed;if(painted)cue(Cue::Paint);pocket::redraw();}}
    if(state.paused)return;
    state.accumulator=uint16_t(state.accumulator+elapsed);const uint8_t badges=state.badges;const uint32_t burned=state.burned,cooled=state.cooled;
    for(int i=0;i<2&&state.accumulator>=50;++i){state.accumulator=uint16_t(state.accumulator-50);step(state);pocket::redraw();}
    reaction_clock+=elapsed;
    if((state.burned!=burned||state.cooled!=cooled)&&reaction_clock>=900){reaction_clock=0;cue(Cue::Reaction);}
    save_clock+=elapsed;if(save_clock>=15000){save_clock%=15000;pocket::save();}
    if(state.badges!=badges){cue(Cue::Goal);pocket::log("Elements: challenge badge earned");changed();}
}
void draw(){
    pocket::clear(BG);label(30,20,tr("TINY ELEMENTS","微型元素"),MINT);char text[96];snprintf(text,sizeof(text),"%s %s",state.paused?tr("PAUSED","暂停"):tr("LIVE","运行"),material_name(state.material));label(155,20,text,GOLD);
    if(state.scene){snprintf(text,sizeof(text),"%s %u/%u%s",state.scene==1?tr("JAR","水罐"):state.scene==2?tr("PLANTS","植物"):state.scene==3?tr("SAFE STEPS","守护步数"):tr("COOLED","已冷却"),progress(state),goal(state.scene),(state.badges&(1u<<(state.scene-1)))?" *":"");label(30,30,text,INK);}else label(30,30,tr(objective(0),"自由探索：绘制、流动、燃烧、生长"),FAINT);
    pocket::outline(28,38,220,148,FAINT);pixel_world();
    const char *tools[]={"II",nullptr,tr("ER","擦"),tr("TSK","任务"),"?",tr("CLR","清空")};
    for(int i=0;i<6;++i){char size[8];snprintf(size,sizeof(size),"S%u",state.brush+1);button(252,40+24*i,30,22,i==0?(state.paused?">":"II"):i==1?size:tools[i],i==0&&state.paused);}
    for(unsigned i=0;i<5;++i){const unsigned m=state.page*5u+i;button(30+int(i)*43,188,41,22,material_name(m),m==state.material);if(!allowed(state.scene,m))pocket::line(33+int(i)*43,208,66+int(i)*43,190,FAINT);}
    button(252,188,30,22,state.page?"1/2":"2/2");
    const int radius=state.brush==2?3:int(state.brush),x0=clamp(int(state.cx)-radius,0,W-1),y0=clamp(int(state.cy)-radius,0,H-1),x1=clamp(int(state.cx)+radius,0,W-1),y1=clamp(int(state.cy)+radius,0,H-1);
    pocket::outline(30+3*x0,40+3*y0,3*(x1-x0+1),3*(y1-y0+1),INK);
    if(state.ui==HELP)help();else if(state.ui==TASKS)draw_tasks();else if(state.ui==CONFIRM)draw_confirm();
}
size_t serialize(uint8_t *data,size_t capacity){return encode(state,data,capacity);}
void load(const uint8_t *data,size_t size){State candidate;if(decode(candidate,data,size)){state=candidate;release();save_clock=reaction_clock=0;effects.set_enabled(!state.muted);pocket::log("Elements: restored exact world rng step ui");pocket::redraw();}else pocket::log("Elements: rejected invalid save");}
}
