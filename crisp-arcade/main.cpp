// SPDX-License-Identifier: MIT
#include "../common/pocket-play/runtime.hpp"
#include "backend.hpp"
#include "core_bridge.h"
#include "vendor/src/lib/cglp.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {
using pocket::Key;
using pocket::Phase;
constexpr uint16_t NAVY = pocket::rgb(12,17,34), CARD = pocket::rgb(25,33,53);
constexpr uint16_t WHITE_INK = pocket::rgb(246,247,255), DIM = pocket::rgb(159,174,200);
constexpr uint32_t MAX_SCORE = 99999999, MAX_PLAYS = 99999999;
struct Entry {
    const char *name, *about1, *about2, *control1, *control2, *footer;
    uint16_t theme;
};
Entry localized_entry(int which) {
    switch(which) {
    case 0: return {pocket::tr("CAST N", "抛线钓鱼"), pocket::tr("Catch blue fish", "钓起蓝色鱼"), pocket::tr("Avoid red fish", "避开红色鱼"), pocket::tr("Hold power / release cast", "按住蓄力，松手抛线"), pocket::tr("Tap to reel in your catch", "轻触收线钓起鱼儿"), pocket::tr("Hold: power  Release: cast  Tap: pull", "按住蓄力 松手抛线 轻触收线"), pocket::rgb(61,210,221)};
    case 1: return {pocket::tr("GRAPPLING H", "飞钩攀登"), pocket::tr("Swing between hooks", "钩住锚点荡起来"), pocket::tr("Climb before you fall", "掉落之前向上爬"), pocket::tr("Tap to release the hook", "轻触松开钩子"), pocket::tr("Release, then hold to grab", "松手，再按住抓住锚点"), pocket::tr("Tap: release hook   Hold: grab anchor", "轻触松钩 按住抓住锚点"), pocket::rgb(250,182,69)};
    case 2: return {pocket::tr("REFLECTOR", "反射飞碟"), pocket::tr("Deflect tank shots", "反弹坦克炮弹"), pocket::tr("Chain huge explosions", "连锁爆炸赢高分"), pocket::tr("Tap to turn your UFO", "轻触让飞碟转向"), pocket::tr("Hold to power the reflector", "按住增强反射器"), pocket::tr("Tap: turn   Hold: power reflector", "轻触转向 按住增强反射"), pocket::rgb(173,137,255)};
    case 3: return {pocket::tr("COLOR ROLL", "彩条连击"), pocket::tr("Match the rolling bars", "命中同色滚动条"), pocket::tr("Chain matching colors", "同色连击赢高分"), pocket::tr("Tap to shoot through bars", "轻触发射穿过彩条"), pocket::tr("Time each shot for a chain", "把握时机达成连击"), pocket::tr("Tap: shoot   Match colors for chains", "轻触发射 同色连击"), pocket::rgb(252,109,147)};
    default: return {};
    }
}
uint32_t best[4]{}, plays[4]{};
int selected = 0, active = -1;
uint32_t accumulator = 0;
bool touch_down = false, armed = true, frame_action = false, last_raw_action = false;
bool edge_queue[16]{};
unsigned edge_begin = 0, edge_count = 0;
int last_state = 1, last_score = 0;

void message(const char *event, int value = -1) {
    char buf[128];
    if (value >= 0) std::snprintf(buf,sizeof(buf),"crisp %s game=%s score=%d best=%u",event,localized_entry(active >= 0 ? active : selected).name,value,best[active >= 0 ? active : selected]);
    else std::snprintf(buf,sizeof(buf),"crisp %s game=%s",event,localized_entry(active >= 0 ? active : selected).name);
    pocket::log(buf);
}
void clear_input() {
    touch_down = frame_action = last_raw_action = false;
    edge_begin = edge_count = 0;
}
void push_edge(bool value) {
    const bool previous = edge_count ? edge_queue[(edge_begin + edge_count - 1) % 16] : frame_action;
    if (previous == value) return;
    if (edge_count == 16) {
        // Bounded backlog: keep the oldest complete taps, then settle to held().
        return;
    }
    edge_queue[(edge_begin + edge_count) % 16] = value;
    ++edge_count;
}
bool raw_action() { return touch_down || pocket::held(Key::A); }
void capture_action() {
    const bool raw = raw_action();
    if (!armed) {
        if (!raw) armed = true;
        return;
    }
    if (raw != last_raw_action) { push_edge(raw); last_raw_action = raw; }
}
void observe_score() {
    if (active < 0) return;
    const int value = std::isfinite(score) ? int(std::max(0.0,std::min(double(MAX_SCORE),double(score)))) : 0;
    if (value != last_score) { last_score = value; message("score",value); }
    if (uint32_t(value) > best[active]) {
        best[active] = uint32_t(value);
        crisp_core_best(value);
        pocket::save();
    }
    const int state = crisp_core_state();
    if (state == 2 && last_state != 2) {
        message("game-over",value);
        pocket::save();
    }
    if (state == 1 && last_state != 1) {
        if (plays[active] < MAX_PLAYS) ++plays[active];
        message("restart",0);
        pocket::save();
    }
    last_state = state;
}
void launch(int which) {
    selected = active = which;
    clear_input();
    // A press that selects a card must be released before becoming game input.
    armed = !pocket::held(Key::A);
    accumulator = 0; last_score = 0; last_state = 1;
    if (plays[which] < MAX_PLAYS) ++plays[which];
    pocket::silence();
    crisp_core_start(which,int(best[which]),pocket::random());
    // Produce the initialized first game view while preserving its tick-0 rules.
    setButtonState(false,false,false,false,false,false);
    updateFrame();
    message("launch",0);
    pocket::save(); pocket::redraw();
}
void home() {
    observe_score();
    if (active >= 0) message("home",last_score);
    active = -1; accumulator = 0; clear_input(); armed = true;
    pocket::silence(); pocket::save(); pocket::redraw();
}
void icon(int x, int y, int which, uint16_t color) {
    using pocket::rect; using pocket::line; using pocket::outline;
    rect(x,y,30,30,pocket::rgb(12,20,37));
    if (which == 0) {
        line(x+4,y+7,x+19,y+3,color); line(x+19,y+3,x+22,y+18,color);
        line(x+22,y+18,x+18,y+20,color);
        rect(x+4,y+21,11,5,color); line(x+15,y+23,x+19,y+19,color); line(x+15,y+23,x+19,y+27,color);
        rect(x+6,y+22,2,1,NAVY); line(x+1,y+28,x+28,y+28,color);
    } else if (which == 1) {
        outline(x+20,y+2,5,5,color); line(x+22,y+7,x+9,y+19,color);
        outline(x+5,y+17,9,9,color); line(x+13,y+25,x+20,y+29,color);
        rect(x+3,y+3,4,4,color);
    } else if (which == 2) {
        rect(x+7,y+8,15,4,color); rect(x+11,y+4,7,4,color);
        line(x+6,y+17,x+24,y+17,color); line(x+2,y+28,x+10,y+20,pocket::WARN);
        line(x+18,y+20,x+27,y+28,pocket::WARN); rect(x+2,y+5,2,2,WHITE_INK);
    } else {
        const uint16_t colors[] = {pocket::BAD,pocket::GOOD,pocket::ACCENT,pocket::WARN};
        for (int i=0;i<4;++i) { rect(x+2,y+2+i*5,11,4,colors[i]); rect(x+14,y+2+i*5,13,4,colors[(i+1)%4]); }
        line(x+15,y+28,x+15,y+15,WHITE_INK);
    }
}
void launcher() {
    pocket::clear(NAVY);
    pocket::text(12,9,pocket::tr("CRISP ARCADE", "清爽街机"),WHITE_INK,2);
    for (int i=0;i<4;++i) {
        const int x = (i%2)*140+8, y = (i/2)*75+36;
        pocket::rect(x,y,132,69,CARD);
        pocket::rect(x,y,3,69,localized_entry(i).theme);
        if (i==selected) pocket::outline(x,y,132,69,localized_entry(i).theme);
        icon(x+7,y+7,i,localized_entry(i).theme);
        pocket::text(x+44,y+8,localized_entry(i).name,WHITE_INK);
        char buf[24]; std::snprintf(buf,sizeof(buf),pocket::tr("HI %u", "最高%u"),best[i]);
        pocket::text(x+44,y+23,buf,localized_entry(i).theme);
        pocket::text(x+7,y+45,localized_entry(i).about1,DIM);
        pocket::text(x+7,y+57,localized_entry(i).about2,DIM);
    }
    pocket::text(12,187,localized_entry(selected).control1,localized_entry(selected).theme);
    pocket::text(12,199,localized_entry(selected).control2,DIM);
    pocket::text(12,211,pocket::tr("Arrows: select  A: play  Touch: card", "方向键选择 A开始 轻触卡片"),DIM);
}
void play_view() {
    pocket::clear(NAVY);
    const Entry &entry=localized_entry(active);
    pocket::rect(4,3,40,22,CARD); pocket::outline(4,3,40,22,entry.theme);
    pocket::text(7,8,pocket::tr("< HOME", "<返回"),entry.theme);
    pocket::text(53,9,entry.name,WHITE_INK);
    char buf[24]; std::snprintf(buf,sizeof(buf),pocket::tr("HI %u", "最高%u"),best[active]);
    pocket::text(258-pocket::text_width(buf),9,buf,entry.theme);
    const int sw=crisp::view_width(), sh=crisp::view_height();
    int dw=264, dh=sh*264/sw;
    if (dh>180) { dh=180; dw=sw*180/sh; }
    const int ox=(pocket::WIDTH-dw)/2, oy=30+(180-dh)/2;
    const uint16_t *source=crisp::view_pixels();
    uint16_t *dest=pocket::framebuffer();
    if (dest) for (int y=0;y<dh;++y) {
        const uint16_t *row=source+(y*sh/dh)*sw;
        for (int x=0;x<dw;++x) dest[(oy+y)*pocket::WIDTH+ox+x]=row[x*sw/dw];
    }
    pocket::outline(ox-1,oy-1,dw+2,dh+2,entry.theme);
    char current_score[48]; std::snprintf(current_score,sizeof(current_score),pocket::tr("SCORE %u", "得分%u"),unsigned(last_score));
    pocket::rect(ox+2,oy+2,pocket::text_width(current_score)+8,14,NAVY);
    pocket::text(ox+6,oy+4,current_score,WHITE_INK);
    if (crisp_core_state()==2) {
        const char *over=pocket::tr("GAME OVER", "游戏结束");
        const char *retry=pocket::tr("Tap / A to retry", "轻触 / A再试一次");
        const int box_w=std::max(pocket::text_width(over,2),pocket::text_width(retry))+16;
        const int box_x=(288-box_w)/2, box_y=oy+dh/2-18;
        pocket::rect(box_x,box_y,box_w,46,NAVY);pocket::outline(box_x,box_y,box_w,46,entry.theme);
        pocket::text((288-pocket::text_width(over,2))/2,box_y+5,over,WHITE_INK,2);
        pocket::text((288-pocket::text_width(retry))/2,box_y+30,retry,entry.theme);
    }

    if (active==1) {
        icon(22,49,1,entry.theme);
        pocket::text(15,101,pocket::tr("TAP", "轻触"),entry.theme); pocket::text(15,113,pocket::tr("RELEASE", "松开"),DIM); pocket::text(15,125,pocket::tr("HOOK", "钩子"),DIM);
        pocket::text(218,101,pocket::tr("HOLD", "按住"),entry.theme); pocket::text(218,113,pocket::tr("GRAB", "抓住"),DIM); pocket::text(218,125,pocket::tr("ANCHOR", "锚点"),DIM);
        pocket::text(15,163,pocket::tr("CLIMB", "攀登"),DIM); pocket::text(218,163,pocket::tr("HIGHER", "更高"),DIM);
    } else if (active==2) {
        pocket::text(13,101,pocket::tr("TAP", "轻触"),entry.theme); pocket::text(13,113,pocket::tr("TURN", "转向"),DIM);
        pocket::text(239,101,pocket::tr("HOLD", "按住"),entry.theme); pocket::text(239,113,pocket::tr("POWER", "增强"),DIM);
    } else if (active==3) {
        pocket::text(13,101,pocket::tr("TAP", "轻触"),entry.theme); pocket::text(13,113,pocket::tr("SHOOT", "发射"),DIM);
        pocket::text(239,101,pocket::tr("MATCH", "匹配"),entry.theme); pocket::text(239,113,pocket::tr("COLOR", "颜色"),DIM);
    }
    pocket::text((288-pocket::text_width(entry.footer))/2,212,entry.footer,entry.theme);
}
uint32_t read32(const uint8_t *p) { return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24); }
void write32(uint8_t *p,uint32_t v) { for (int i=0;i<4;++i) p[i]=uint8_t(v>>(i*8)); }
uint32_t checksum(const uint8_t *p,size_t n) { uint32_t h=2166136261u; for(size_t i=0;i<n;++i) h=(h^p[i])*16777619u; return h; }
}
namespace game {
const char *title() { return pocket::tr("Crisp Arcade", "清爽街机"); }
const char *app_id() { return "pxa-crisp-arcade"; }
void init() {
    std::fill(best,best+4,0); std::fill(plays,plays+4,0);
    selected=0; active=-1; accumulator=0; clear_input(); armed=true;
    crisp_core_init(); pocket::silence();
    pocket::log("crisp init four-games"); pocket::redraw();
}
void reset() { if(active>=0) launch(active); else { selected=0; clear_input(); pocket::redraw(); } }
void key(Key key) {
    if (active>=0) {
        if(key==Key::B) { home(); return; }
        if(key==Key::A) {
            capture_action();
            // Preserve an edge even if the runtime drains DOWN and UP before tick.
            if (armed && !raw_action()) { push_edge(true); push_edge(false); }
        }
        return;
    }
    if (key==Key::LEFT || key==Key::UP) selected=(selected+3)%4;
    else if (key==Key::RIGHT || key==Key::DOWN) selected=(selected+1)%4;
    else if (key==Key::A) { launch(selected); return; }
    else return;
    pocket::save(); pocket::redraw();
}
void pointer(Phase phase,int x,int y) {
    if (phase==Phase::UP || phase==Phase::CANCEL) {
        touch_down=false; if(active>=0) capture_action(); return;
    }
    if (phase==Phase::MOVE) return;
    if (phase!=Phase::DOWN) return;
    if (active<0) {
        if (x<8 || x>=280) return;
        for (int i=0;i<4;++i) {
            const int cx=(i%2)*140+8, cy=(i/2)*75+36;
            if(x>=cx && x<cx+132 && y>=cy && y<cy+69) { launch(i); return; }
        }
    } else if(x>=4 && x<44 && y>=3 && y<25) home();
    else if(x>=0 && x<288 && y>=30 && y<212) { touch_down=true; capture_action(); }
}
void tick(uint32_t elapsed_ms) {
    if (active<0) return;
    capture_action();
    // Units are 1/60 millisecond: exact original 60 FPS, at most six updates.
    accumulator+=std::min(elapsed_ms,100u)*60u;
    unsigned updates=0;
    while(accumulator>=1000u && updates<6) {
        accumulator-=1000u; ++updates;
        if(edge_count) { frame_action=edge_queue[edge_begin]; edge_begin=(edge_begin+1)%16; --edge_count; }
        else frame_action=armed && raw_action();
        setButtonState(false,false,false,false,false,frame_action);
        updateFrame(); observe_score();
    }
    if(accumulator>=1000u) accumulator%=1000u;
    if(updates) pocket::redraw();
}
void draw() { if(active<0) launcher(); else play_view(); }
size_t serialize(uint8_t *data,size_t capacity) {
    constexpr size_t size=48;
    if(!data || capacity<size) return 0;
    std::memset(data,0,size); std::memcpy(data,"CRSP",4);
    data[4]=1; data[5]=uint8_t(selected);
    for(int i=0;i<4;++i) { write32(data+8+4*i,best[i]); write32(data+24+4*i,plays[i]); }
    write32(data+44,checksum(data,44)); return size;
}
void load(const uint8_t *data,size_t size) {
    bool valid=data && size==48;
    if(valid) valid=std::memcmp(data,"CRSP",4)==0 && data[4]==1 && data[5]<4 && !data[6] && !data[7] && read32(data+40)==0 && read32(data+44)==checksum(data,44);
    if(valid) for(int i=0;i<4;++i) if(read32(data+8+i*4)>MAX_SCORE || read32(data+24+i*4)>MAX_PLAYS) valid=false;
    if(!valid) { pocket::log("crisp save-rejected"); return; }
    selected=data[5];
    for(int i=0;i<4;++i) { best[i]=read32(data+8+i*4); plays[i]=read32(data+24+i*4); }
    char buf[128]; std::snprintf(buf,sizeof(buf),"crisp save-restored selected=%d best=%u,%u,%u,%u",selected,best[0],best[1],best[2],best[3]); pocket::log(buf);
    if(active>=0) crisp_core_best(int(best[active]));
    pocket::redraw();
}
}
