// SPDX-License-Identifier: CC0-1.0
#include "../common/pocket-play/runtime.hpp"
#include "core.hpp"
#include "renderer.hpp"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
namespace {
using pocket::Key;using pocket::Phase;
marble::State state;
enum Screen { INTRO,RACE,ISLANDS };
Screen screen=INTRO;
int selected;
bool dragging,touch_brake;
int anchor_x,anchor_y,drag_x,drag_y;
uint32_t checkpoint_ms,toast_until;
const char *toast;
static uint16_t view[marble::VIEW_W*marble::VIEW_H];
constexpr uint16_t DEEP=pocket::rgb(24,58,83),CREAM=pocket::rgb(244,246,218),GOLD=pocket::rgb(255,208,100),MINT=pocket::rgb(145,235,190),CORAL=pocket::rgb(247,146,158);
const char *level_name(unsigned level) {
    switch(level) {
    case 0: return pocket::tr("First Ripples", "初泛涟漪");
    case 1: return pocket::tr("Ribbon Bridge", "丝带窄桥");
    case 2: return pocket::tr("Blue Windows", "碧水天窗");
    case 3: return pocket::tr("Tidal Gate", "潮汐之门");
    case 4: return pocket::tr("Pebble Garden", "卵石花园");
    case 5: return pocket::tr("Windmill Walk", "风车小径");
    case 6: return pocket::tr("Three Tides", "三重潮汐");
    case 7: return pocket::tr("Sunset Crown", "落日之冠");
    default: return "";
    }
}
const char *level_hint(unsigned level) {
    switch(level) {
    case 0: return pocket::tr("Collect gold gems, then roll into the mint goal.", "收齐金色宝石，再滚入薄荷绿终点。");
    case 1: return pocket::tr("Small moves. Brake before each turn.", "小幅移动，每次转弯前先刹车。");
    case 2: return pocket::tr("Water holes have no invisible rails.", "水洞没有护栏，小心绕行。");
    case 3: return pocket::tr("Coral gate: wait for the mint opening.", "珊瑚红的门关闭，变薄荷绿再通过。");
    case 4: return pocket::tr("Pink bumpers bounce. Take the clear lanes.", "粉色弹柱会反弹，走空旷路线。");
    case 5: return pocket::tr("Dark blocks stop you; the outer loop is safe.", "深色墙挡路，沿外环行走更安全。");
    case 6: return pocket::tr("Three timed gates. Slow down and read the tide.", "三道定时潮门，放慢速度看准时机。");
    case 7: return pocket::tr("One last loop. Bring every gem home.", "最后一圈，把每颗宝石带回家。");
    default: return "";
    }
}
int clamp(int v,int a,int b) { return v<a?a:v>b?b:v; }
void release() { dragging=touch_brake=false;drag_x=drag_y=0; }
void log_run(const char *action) { char b[96];snprintf(b,sizeof(b),"Marble: %s level=%u gems=%u time=%u",action,state.run.level+1,marble::collected(state.run),state.run.elapsed_ms);pocket::log(b); }
void changed(bool durable=true) { pocket::redraw();if(durable)pocket::save(); }
void begin(unsigned level) { if(!marble::start(state,level))return;screen=RACE;release();checkpoint_ms=0;log_run("start");changed(); }
void show_islands() { release();selected=state.run.level;screen=ISLANDS;pocket::redraw(); }
void choose() {
    if(selected<state.progress.unlocked)begin(unsigned(selected));
    else { toast="locked";toast_until=pocket::millis()+2300;pocket::tone(180,.06f);pocket::redraw(); }
}
void advance() { if(state.run.level+1<state.progress.unlocked)begin(state.run.level+1);else show_islands(); }
void circle(int cx,int cy,int r,uint16_t color,bool filled) {
    int x=r,y=0,err=1-r;
    while(x>=y) {
        if(filled) { pocket::line(cx-x,cy+y,cx+x,cy+y,color);pocket::line(cx-x,cy-y,cx+x,cy-y,color);pocket::line(cx-y,cy+x,cx+y,cy+x,color);pocket::line(cx-y,cy-x,cx+y,cy-x,color); }
        else { const int xs[8]={x,y,-y,-x,-x,-y,y,x},ys[8]={y,x,x,y,-y,-x,-x,-y};for(int i=0;i<8;++i)pocket::pixel(cx+xs[i],cy+ys[i],color); }
        ++y;if(err<0)err+=2*y+1;else { --x;err+=2*(y-x)+1; }
    }
}
void diamond(int x,int y,int r,uint16_t color) { for(int i=-r;i<=r;++i)pocket::line(x-(r-abs(i)),y+i,x+(r-abs(i)),y+i,color); }
void center(int y,const char *s,uint16_t color=CREAM,int scale=1) { pocket::text((288-pocket::text_width(s,scale))/2,y,s,color,scale); }
void wrap(int x,int y,int width,const char *value,uint16_t color) {
    // Advance whole UTF-8 glyphs and measure pixels; never split a Chinese byte sequence.
    while(*value) {
        char b[192];unsigned n=0,last_space=0;
        while(value[n]) {
            unsigned char lead=static_cast<unsigned char>(value[n]);
            unsigned bytes=lead<0x80?1:lead<0xe0?2:lead<0xf0?3:4;
            if(n+bytes>=sizeof(b))break;
            memcpy(b+n,value+n,bytes);b[n+bytes]=0;
            if(pocket::text_width(b)>width)break;
            if(value[n]==' ')last_space=n;
            n+=bytes;
        }
        if(!n)return;
        if(value[n]&&last_space)n=last_space;
        memcpy(b,value,n);b[n]=0;pocket::text(x,y,b,color);y+=12;value+=n;while(*value==' ')++value;
    }
}
void button(int x,int y,int w,const char *text,uint16_t color=DEEP) { pocket::rect(x,y,w,18,color);pocket::outline(x,y,w,18,CREAM);pocket::text(x+(w-pocket::text_width(text))/2,y+4,text,CREAM); }
void tutorial() {
    pocket::rect(14,36,260,161,DEEP);pocket::outline(14,36,260,161,MINT);
    center(48,pocket::tr("MARBLE", "滚珠"),CREAM,2);center(71,pocket::tr("ISLANDS", "群岛"),MINT,2);
    diamond(41,96,4,GOLD);pocket::text(55,93,pocket::tr("EVERY GEM OPENS THE GOAL", "收齐宝石才能打开终点"),CREAM);
    pocket::text(29,112,pocket::tr("DRAG ANYWHERE TO TILT", "在游戏区拖动即可倾斜"),CREAM);
    pocket::text(29,124,pocket::tr("ARROWS ROLL / HOLD A TO BRAKE", "方向键滚动 / 按住A刹车"),CREAM);
    pocket::text(29,136,pocket::tr("TOUCH BRAKE TO STOP ON CORNERS", "转弯时按住刹车圆钮减速"),CREAM);
    pocket::text(29,150,pocket::tr("X RETRY   B ISLANDS   START MENU", "X重试 B选岛 START菜单"),MINT);
    button(49,172,190,pocket::tr("TOUCH TO ROLL / PRESS A", "轻触开始 / 按A"),pocket::rgb(45,102,116));
}
void islands() {
    pocket::clear(DEEP);center(8,pocket::tr("CHOOSE AN ISLAND", "选择岛屿"),CREAM,2);
    center(30,pocket::tr("GOLD: ALL GEMS WITHIN THE PAR TIME", "金章：目标时间内收齐宝石通关"),MINT);
    for(int i=0;i<8;++i) {
        int x=8+(i%2)*140,y=43+(i/2)*35;bool open=i<state.progress.unlocked,focus=i==selected;
        pocket::rect(x,y,132,31,focus?pocket::rgb(53,106,119):pocket::rgb(33,73,96));
        pocket::outline(x,y,132,31,focus?GOLD:open?MINT:pocket::MUTED);
        char b[96];snprintf(b,sizeof(b),"%d %s",i+1,open?level_name(unsigned(i)):pocket::tr("LOCKED", "未解锁"));pocket::text(x+5,y+5,b,open?CREAM:pocket::MUTED);
        if(state.progress.best_ms[i]) {snprintf(b,sizeof(b),pocket::tr("BEST %u.%us", "最佳%u.%u秒"),state.progress.best_ms[i]/1000,(state.progress.best_ms[i]/100)%10);pocket::text(x+5,y+18,b,MINT);}
        else pocket::text(x+5,y+18,open?pocket::tr("READY TO EXPLORE", "准备出发"):pocket::tr("FINISH EARLIER ISLAND", "先完成前一岛"),open?MINT:pocket::MUTED);
        for(unsigned j=0;j<state.progress.medals[i];++j)diamond(x+104+j*9,y+22,3,state.progress.medals[i]==3?GOLD:state.progress.medals[i]==2?CREAM:CORAL);
    }
    if(toast&&int32_t(toast_until-pocket::millis())>0)wrap(12,181,264,pocket::tr("Finish earlier islands to unlock this one.", "先完成前面的岛屿即可解锁。"),GOLD);
    else center(181,pocket::tr("ARROWS + A / TAP A CARD TO PLAY", "方向键+A / 轻触卡片开始"),MINT);
    button(8,194,78,pocket::tr("BACK", "返回"));button(98,194,182,pocket::tr("HOW TO PLAY", "玩法说明"));
}
void won() {
    pocket::rect(27,49,234,137,DEEP);pocket::outline(27,49,234,137,MINT);
    center(62,state.run.level==7?pocket::tr("ISLANDS COMPLETE!", "群岛全部完成！"):pocket::tr("ISLAND CLEAR!", "小岛通关！"),MINT,2);
    uint32_t par=marble::levels[state.run.level].gold_ms;
    unsigned medal=state.run.elapsed_ms<=par?3:state.run.elapsed_ms<=par*3/2?2:1;
    for(unsigned i=0;i<3;++i)diamond(112+i*32,94,10,i<medal?GOLD:pocket::rgb(73,103,123));
    char b[64];snprintf(b,sizeof(b),pocket::tr("TIME %u.%us   GOLD %us", "用时%u.%u秒  金章%u秒"),state.run.elapsed_ms/1000,(state.run.elapsed_ms/100)%10,marble::levels[state.run.level].gold_ms/1000);center(115,b);
    center(130,medal==3?pocket::tr("A PERFECT LITTLE VOYAGE.", "完美的小小旅程。"):pocket::tr("TRY AGAIN FOR A FASTER MEDAL.", "再试一次，争取更快的奖章。"),MINT);
    button(45,153,103,state.run.level==7?pocket::tr("ISLANDS", "群岛"):pocket::tr("NEXT / A", "下一岛 / A"));button(155,153,88,pocket::tr("RETRY / X", "重试 / X"));
}
void draw_race() {
    marble::render(state.run,view,pocket::millis());
    uint16_t *frame=pocket::framebuffer();
    if(frame)for(int y=0;y<100;++y)for(int x=0;x<144;++x) { unsigned out=unsigned((20+y*2)*288+x*2);uint16_t c=view[y*144+x];frame[out]=frame[out+1]=frame[out+288]=frame[out+289]=c; }
    pocket::rect(0,0,264,20,DEEP);
    char b[72];snprintf(b,sizeof(b),"%u %s",state.run.level+1,level_name(state.run.level));pocket::text(7,6,b,CREAM);
    snprintf(b,sizeof(b),"%u/%u",marble::collected(state.run),marble::coin_count(state.run.level));diamond(190,9,3,GOLD);pocket::text(199,6,b,GOLD);
    snprintf(b,sizeof(b),pocket::tr("%us", "%u秒"),state.run.elapsed_ms/1000);pocket::text(262-pocket::text_width(b),6,b,CREAM);
    if(screen==INTRO) {tutorial();return;}
    if(state.run.phase==marble::Phase::WON) { won();return; }
    pocket::rect(8,26,272,27,CREAM);wrap(15,31,258,level_hint(state.run.level),DEEP);
    if(state.run.phase==marble::Phase::FALLING) {
        pocket::rect(58,97,172,35,DEEP);center(101,pocket::tr("SPLASH!", "扑通！"),CORAL,2);center(123,pocket::tr("A FRESH START IS COMING.", "马上重新出发。"),CREAM);
    }
    int ax=dragging?anchor_x:42,ay=dragging?anchor_y:170;
    circle(ax,ay,21,DEEP,true);circle(ax,ay,21,MINT,false);circle(ax,ay,16,pocket::rgb(70,139,154),false);
    circle(ax+drag_x/2,ay+drag_y/2,6,CREAM,true);
    circle(253,170,22,touch_brake||pocket::held(Key::A)?CORAL:DEEP,true);circle(253,170,22,CREAM,false);pocket::text(253-pocket::text_width(pocket::tr("BRAKE", "刹车"))/2,165,pocket::tr("BRAKE", "刹车"),CREAM);
    if(dragging)pocket::text(103,184,pocket::tr("EASE INTO TURNS", "转弯时放慢"),CREAM);
    else pocket::text(96,184,pocket::tr("DRAG TO TILT", "拖动来倾斜"),CREAM);
    pocket::rect(0,192,288,32,DEEP);button(4,194,78,pocket::tr("ISLANDS / B", "选岛 / B"));button(91,194,69,pocket::tr("RETRY / X", "重试 / X"));pocket::text(174,200,pocket::tr("HOLD A TO BRAKE", "按住A刹车"),MINT);
}
}
namespace game {
const char *title() { return pocket::tr("Marble Islands", "滚珠群岛"); }
const char *app_id() { return "pxa-marble-islands"; }
void init() { marble::init(state);screen=INTRO;selected=0;release();checkpoint_ms=0;toast=nullptr;log_run("start");changed(); }
void reset() { begin(state.run.level); }
void key(Key key) {
    if(key==Key::X) { reset();return; }
    if(screen==INTRO) { if(key==Key::A||key==Key::B) { screen=RACE;pocket::redraw(); }return; }
    if(screen==ISLANDS) {
        if(key==Key::B) {screen=RACE;release();pocket::redraw();}
        else if(key==Key::A)choose();
        else { selected=clamp(selected+(key==Key::UP?-2:key==Key::DOWN?2:key==Key::LEFT?-1:key==Key::RIGHT?1:0),0,7);pocket::redraw(); }return;
    }
    if(key==Key::B)show_islands();
    else if(state.run.phase==marble::Phase::WON&&key==Key::A)advance();
}
void pointer(Phase phase,int x,int y) {
    if(phase==Phase::CANCEL||phase==Phase::UP) { release();pocket::redraw();return; }
    if(screen==INTRO) { if(phase==Phase::DOWN) {screen=RACE;release();pocket::redraw();}return; }
    if(screen==ISLANDS) {
        if(phase==Phase::DOWN) {
            if(y>=192) { if(x<90)screen=RACE;else screen=INTRO;release();pocket::redraw(); }
            else if(y>=43&&y<179) { int col=x<148?0:1,row=(y-43)/35;selected=row*2+col;choose(); }
        }return;
    }
    if(state.run.phase==marble::Phase::WON) {
        if(phase==Phase::DOWN&&y>=149&&y<181) { if(x<151)advance();else reset(); }return;
    }
    if(state.run.phase!=marble::Phase::ROLLING)return;
    if(phase==Phase::DOWN) {
        release();
        if(y>=192) {if(x<87)show_islands();else if(x<166)reset();return;}
        int dx=x-253,dy=y-170;
        if(dx*dx+dy*dy<=25*25)touch_brake=true;
        else { dragging=true;anchor_x=x;anchor_y=y; }
    } else if(dragging) {drag_x=clamp(x-anchor_x,-30,30);drag_y=clamp(y-anchor_y,-30,30);}
    pocket::redraw();
}
void tick(uint32_t ms) {
    if(screen!=RACE) { if(screen==INTRO)pocket::redraw();return; }
    if(state.run.phase==marble::Phase::FALLING&&state.run.fall_ms>=650) {begin(state.run.level);return;}
    marble::Input in{float(drag_x)/30,float(drag_y)/30,touch_brake||pocket::held(Key::A)};
    if(pocket::held(Key::LEFT))in.x-=1;
    if(pocket::held(Key::RIGHT))in.x+=1;
    if(pocket::held(Key::UP))in.y-=1;
    if(pocket::held(Key::DOWN))in.y+=1;
    unsigned e=marble::step(state,in,ms);
    if(e&marble::COLLECT) {log_run("collect");pocket::tone(760,.055f);pocket::tone(1120,.07f,pocket::millis()/1000.f+.065f);pocket::save();}
    if(e&marble::BOUNCE)pocket::tone(220,.025f);
    if(e&marble::FALL) {release();log_run("fall");pocket::tone(150,.16f);pocket::save();}
    if(e&marble::WIN) {release();log_run("win");float now=pocket::millis()/1000.f;pocket::tone(660,.09f);pocket::tone(880,.09f,now+.11f);pocket::tone(1320,.2f,now+.23f);pocket::save();}
    checkpoint_ms+=ms;if(checkpoint_ms>=2000&&state.run.phase!=marble::Phase::WON) {checkpoint_ms=0;pocket::save();}
    pocket::redraw();
}
void draw() { pocket::clear(DEEP);if(screen==ISLANDS)islands();else draw_race(); }
size_t serialize(uint8_t *data,size_t capacity) { size_t n=marble::serialize(state,data,capacity);if(n)log_run("serialize");return n; }
void load(const uint8_t *data,size_t size) { if(marble::load(state,data,size)) {screen=RACE;release();log_run("load");pocket::redraw();}else pocket::log("Marble: rejected invalid save"); }
}
