// SPDX-License-Identifier: CC0-1.0
#include "../common/pocket-game/runtime.hpp"
#include "../common/pocket-game/music.hpp"
#include "core.hpp"
#include "navigation.hpp"
#include "../common/pocket-studio/particles.hpp"
#include "../common/pocket-studio/studio.hpp"
#include <stdio.h>
#include <string.h>

namespace {
sokoban::State state{};
using pocket_game_sfx::Cue;
using pocket_game_sfx::cue;
constexpr int GRID_X = 4, GRID_Y = 40, TILE = 18, SIDE = 190, SIDE_W = 94;
bool dragging = false;
int press_x = 0, press_y = 0;
uint32_t blocked_ms = 0, move_ms = 0;
int player_from=-1,crate_from=-1,crate_to=-1;
studio::Particles<> effects;
uint8_t walk_route[sokoban::CELLS];unsigned walk_size=0,walk_at=0;uint32_t walk_due=0;
void stop_walk(){walk_size=walk_at=walk_due=0;}
bool choosing=false;int level_cursor=0,level_press=-1;uint32_t level_notice_ms=0;
void changed() { pocket::redraw(); pocket::save(); }
void restart() {
    stop_walk();effects.clear();choosing=false;level_press=-1;level_notice_ms=0;
    cue(Cue::Reset);
    sokoban::reset_level(state, state.level);
    blocked_ms = move_ms = 0; dragging = false;
    changed();
}
void step(sokoban::Direction direction) {
    const uint32_t pushes = state.pushes;
    uint8_t boxes[sokoban::CELLS];memcpy(boxes,state.upper,sizeof(boxes));int old_player=state.player;
    if (sokoban::move(state, direction)) {
        player_from=old_player;crate_from=crate_to=-1;move_ms=90;
        for(unsigned i=0;i<sokoban::CELLS;++i){if(boxes[i]&&!state.upper[i])crate_from=int(i);if(!boxes[i]&&state.upper[i])crate_to=int(i);}
        if(crate_to>=0&&state.lower[crate_to]==sokoban::GOAL)effects.burst(GRID_X+(crate_to%10)*TILE+8,GRID_Y+(crate_to/10)*TILE+8,pocket::GOOD,18);
        cue(sokoban::won(state) ? Cue::Goal : state.pushes != pushes ? Cue::Push : Cue::Move);
        blocked_ms = 0; changed();
    } else if (!sokoban::won(state)) { cue(Cue::Blocked); blocked_ms = 320; pocket::redraw(); }
}
void undo() {
    stop_walk();effects.clear();
    if (sokoban::undo(state)) { cue(Cue::Undo); blocked_ms = move_ms = 0; changed(); }
}
void choose(int delta) {
    stop_walk();effects.clear();
    if (sokoban::select(state, delta)) { cue(Cue::Select); blocked_ms = move_ms = 0; changed(); }
}
void next() {
    stop_walk();effects.clear();
    if (!sokoban::won(state)) return;
    cue(Cue::Select);
    sokoban::reset_level(state, (unsigned(state.level) + 1) % sokoban::LEVEL_COUNT);
    blocked_ms = move_ms = 0;
    changed();
}
void open_levels(){stop_walk();choosing=true;dragging=false;level_cursor=state.level;level_press=-1;level_notice_ms=0;move_ms=0;pocket::redraw();}
int level_hit(int x,int y){
    if(x>=16&&x<272&&y>=43&&y<205){int col=(x-16)/43,row=(y-43)/27;if((x-16)%43<39&&(y-43)%27<24){int n=row*6+col;if(n<int(sokoban::LEVEL_COUNT))return n;}}
    if(x>=96&&x<192&&y>=210&&y<224)return -2;
    return -1;
}
void pick_level(int n){
    if(n<0||n>=int(sokoban::LEVEL_COUNT))return;
    level_cursor=n;
    if(n>state.unlocked){level_notice_ms=1800;cue(Cue::Blocked);pocket::redraw();return;}
    choosing=false;level_press=-1;
    if(n!=state.level){sokoban::reset_level(state,unsigned(n));blocked_ms=move_ms=0;cue(Cue::Select);changed();}
    else pocket::redraw();
}
void draw_levels(){
    using namespace pocket;
    clear(rgb(9,23,43));studio::gradient(0,0,288,224,rgb(9,23,43),rgb(21,40,59));
    studio::label(24,7,208,tr("CHOOSE A LEVEL","选择关卡"),INK,2);
    char b[80];snprintf(b,sizeof(b),tr("%u cleared · %u unlocked","已完成 %u 关 · 已解锁 %u 关"),sokoban::completed_count(state),unsigned(state.unlocked)+1);
    studio::label(16,29,256,level_notice_ms?tr("Finish earlier levels first.","先完成前面的关卡即可解锁。 "):b,level_notice_ms?WARN:MUTED);
    for(unsigned i=0;i<sokoban::LEVEL_COUNT;++i){
        int x=16+int(i%6)*43,y=43+int(i/6)*27;bool unlocked=i<=state.unlocked,done=bool(state.completed&(uint64_t(1)<<i)),focus=int(i)==level_cursor;
        uint16_t fill=focus?rgb(38,81,106):done?rgb(29,70,63):rgb(24,43,63);
        if(level_press==int(i))fill=studio::mix(fill,0,35);
        studio::card(x,y,39,24,fill,focus?ACCENT:done?rgb(78,151,119):rgb(45,65,85),4);
        snprintf(b,sizeof(b),"%02u",i+1);studio::label(x+2,y+3,35,b,unlocked?INK:rgb(100,119,137),2);
        if(done){line(x+29,y+18,x+31,y+20,GOOD);line(x+31,y+20,x+35,y+15,GOOD);}
        else if(!unlocked){outline(x+29,y+17,5,4,MUTED);outline(x+30,y+14,3,4,MUTED);}
    }
    studio::button(96,210,96,12,tr("Back · B","返回 · B"),rgb(30,52,76),INK);
}
bool in_grid(int x, int y) {
    return x >= GRID_X && x < GRID_X + TILE * 10 && y >= GRID_Y && y < GRID_Y + TILE * 10;
}
bool in_button(int x, int y, int top, int left = SIDE, int width = SIDE_W, int height = 26) {
    return x >= left && x < left + width && y >= top && y < top + height;
}
void button(int x, int y, int width, int height, const char *label, uint16_t color) {
    studio::button(x,y,width,height,label,dragging&&in_button(press_x,press_y,y,x,width,height)?pocket::rgb(21,40,62):pocket::rgb(30,52,76),color);
}
void stat(int top, const char *label, uint32_t value) {
    char text[16];
    snprintf(text, sizeof(text), "%lu", static_cast<unsigned long>(value));
    pocket::text(SIDE + 2, top, label, pocket::MUTED);
    pocket::text(SIDE_W + SIDE - pocket::text_width(text) - 2, top, text, pocket::INK);
}
}
namespace game {
const char *title() { return pocket::tr("Pocket Sokoban", "推箱子"); }
const char *app_id() { return "pxa-pocket-sokoban"; }
void init() { stop_walk();effects.clear();sokoban::init(state); dragging = false; blocked_ms = move_ms = 0;choosing=false;level_notice_ms=0;level_press=-1; }
void reset() { restart(); }
void key(pocket::Key key) {
    if(choosing){
        if(key==pocket::Key::B||key==pocket::Key::SELECT){choosing=false;level_press=-1;}
        else if(key==pocket::Key::A)pick_level(level_cursor);
        else {int delta=key==pocket::Key::LEFT?-1:key==pocket::Key::RIGHT?1:key==pocket::Key::UP?-6:key==pocket::Key::DOWN?6:0;level_cursor+=delta;if(level_cursor<0)level_cursor=0;if(level_cursor>=int(sokoban::LEVEL_COUNT))level_cursor=int(sokoban::LEVEL_COUNT)-1;level_notice_ms=0;}
        pocket::redraw();return;
    }
    if(key==pocket::Key::SELECT){open_levels();return;}
    stop_walk();
    switch (key) {
    case pocket::Key::UP: step(sokoban::UP); break;
    case pocket::Key::RIGHT: step(sokoban::RIGHT); break;
    case pocket::Key::DOWN: step(sokoban::DOWN); break;
    case pocket::Key::LEFT: step(sokoban::LEFT); break;
    case pocket::Key::B: undo(); break;
    case pocket::Key::X: restart(); break;
    case pocket::Key::L: choose(-1); break;
    case pocket::Key::R: case pocket::Key::Y: choose(1); break;
    case pocket::Key::A: next(); break;
    default: break;
    }
}
void pointer(pocket::Phase phase, int x, int y) {
    if(choosing){
        if(phase==pocket::Phase::DOWN){level_press=level_hit(x,y);if(level_press>=0)level_cursor=level_press;}
        else if(phase==pocket::Phase::CANCEL)level_press=-1;
        else if(phase==pocket::Phase::UP){int n=level_press;level_press=-1;if(n==level_hit(x,y)){if(n==-2)choosing=false;else if(n>=0)pick_level(n);}}
        pocket::redraw();return;
    }
    if (phase == pocket::Phase::DOWN) {
        stop_walk();dragging = true; press_x = x; press_y = y;
    } else if (phase == pocket::Phase::CANCEL) {
        stop_walk();dragging = false;
    } else if (phase == pocket::Phase::UP && dragging) {
        dragging = false;
        if (x < 0 || x >= pocket::WIDTH || y < 0 || y >= pocket::HEIGHT) return;
        const int dx = x - press_x, dy = y - press_y;
        const int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
        if(press_x>=144&&press_x<236&&press_y<26&&x>=144&&x<236&&y<26){open_levels();return;}
        if (in_grid(press_x, press_y)) {
            if (ax >= 12 || ay >= 12) {
                step(ax > ay ? (dx > 0 ? sokoban::RIGHT : sokoban::LEFT)
                             : (dy > 0 ? sokoban::DOWN : sokoban::UP));
            } else if (in_grid(x, y)) {
                const int cx = (x - GRID_X) / TILE, cy = (y - GRID_Y) / TILE;
                const int px = state.player % 10, py = state.player / 10;
                if (cx == px && cy == py - 1) step(sokoban::UP);
                if (cx == px + 1 && cy == py) step(sokoban::RIGHT);
                if (cx == px && cy == py + 1) step(sokoban::DOWN);
                if (cx == px - 1 && cy == py) step(sokoban::LEFT);
                if(!sokoban::won(state)&&(cx-px)*(cx-px)+(cy-py)*(cy-py)>1&&cx>=0&&cx<10&&cy>=0&&cy<10){
                    int count=sokoban::walk_path(state,unsigned(cy*10+cx),walk_route,sizeof(walk_route));
                    if(count>0){walk_size=unsigned(count);walk_at=walk_due=0;pocket::redraw();}
                    else if(count<0){blocked_ms=500;pocket::redraw();}
                }
            }
        } else if (in_button(press_x, press_y, 94) && in_button(x, y, 94)) undo();
        else if (in_button(press_x, press_y, 124) && in_button(x, y, 124)) restart();
        else if (in_button(press_x, press_y, 154, SIDE, 44) && in_button(x, y, 154, SIDE, 44)) choose(-1);
        else if (in_button(press_x, press_y, 154, SIDE + 48, 46) && in_button(x, y, 154, SIDE + 48, 46)) choose(1);
        else if (in_button(press_x, press_y, 184, SIDE, SIDE_W, 36) && in_button(x, y, 184, SIDE, SIDE_W, 36)) next();
    }
}
void tick(uint32_t elapsed_ms) {
    if(effects.tick(elapsed_ms))pocket::redraw();
    if(level_notice_ms){level_notice_ms=elapsed_ms>=level_notice_ms?0:level_notice_ms-elapsed_ms;if(!level_notice_ms)pocket::redraw();}
    if(move_ms){move_ms=elapsed_ms>=move_ms?0:move_ms-elapsed_ms;pocket::redraw();}
    if(blocked_ms){blocked_ms = elapsed_ms >= blocked_ms ? 0 : blocked_ms - elapsed_ms;if (!blocked_ms) pocket::redraw();}
    if(walk_at<walk_size){walk_due+=elapsed_ms;if(walk_due>=100){walk_due-=100;uint32_t before=state.steps;step(sokoban::Direction(walk_route[walk_at++]));if(state.steps==before||walk_at==walk_size)stop_walk();}}
}
void draw() {
    if(choosing){draw_levels();return;}
    using namespace pocket;
    const uint16_t navy=rgb(9,23,43),gold=rgb(255,197,90),mint=rgb(112,239,166);
    const uint16_t stones[]={rgb(62,80,111),rgb(53,96,88),rgb(119,91,65),rgb(89,68,114)};
    const uint16_t floors[]={rgb(29,44,62),rgb(27,52,47),rgb(58,43,33),rgb(43,33,62)};
    unsigned theme=(state.level/8)%4;uint16_t stone=stones[theme];
    clear(navy);
    studio::gradient(0,0,288,224,navy,rgb(21,40,59));
    text(28,5,tr("SOKOBAN","口袋推箱子"),rgb(157,220,255),2);
    char b[48];snprintf(b,sizeof(b),tr("LEVEL %02u / %02u","第 %02u / %02u 关"),unsigned(state.level)+1,sokoban::LEVEL_COUNT);
    studio::button(144,3,92,22,b,rgb(27,47,69),INK);
    text(8,27,tr("Tap an empty tile to walk there","点空地自动走路，滑动或点邻格推箱"),MUTED);
    studio::round(2,38,184,184,6,rgb(6,14,28));
    for(unsigned i=0;i<sokoban::CELLS;++i){
        int x=GRID_X+int(i%10)*TILE,y=GRID_Y+int(i/10)*TILE;
        if(state.lower[i]==sokoban::WALL){
            uint16_t c=studio::mix(stone,rgb(111,136,168),(int(i)*17)%48);
            rect(x,y+3,17,14,rgb(32,45,68));studio::round(x,y,17,14,2,c);
            line(x+2,y+1,x+14,y+1,rgb(124,143,169));line(x+1,y+2,x+1,y+11,rgb(90,111,143));
            rect(x+3,y+6,5,2,studio::mix(c,navy,35));rect(x+11,y+10,4,2,studio::mix(c,navy,45));
        }else{
            uint16_t floor=(i+i/10)%2?floors[theme]:studio::mix(floors[theme],0xffff,12);rect(x,y,17,17,floor);
            line(x+1,y+16,x+16,y+16,rgb(13,28,46));
            if(state.lower[i]==sokoban::GOAL){studio::round(x+2,y+2,13,13,3,rgb(29,89,75));studio::round(x+4,y+4,9,9,2,mint);studio::round(x+6,y+6,5,5,1,rgb(28,75,65));}
        }
    }
    unsigned goals=0,filled=0;
    for(unsigned i=0;i<sokoban::CELLS;++i)if(state.lower[i]==sokoban::GOAL){
        ++goals;if(state.upper[i])++filled;
        studio::glow(GRID_X+int(i%10)*TILE+8,GRID_Y+int(i/10)*TILE+8,10,mint,state.upper[i]?40:65);
    }
    if(walk_at<walk_size){int cell=state.player;for(unsigned i=walk_at;i<walk_size;++i){const int dx[]={0,1,0,-1},dy[]={-1,0,1,0};cell+=dx[walk_route[i]]+10*dy[walk_route[i]];if(cell>=0&&cell<100)studio::disc(GRID_X+(cell%10)*TILE+8,GRID_Y+(cell/10)*TILE+8,i+1==walk_size?2:1,ACCENT);}}
    float t=1-float(move_ms)/90.f;t=t*t*(3-2*t);
    for(unsigned i=0;i<sokoban::CELLS;++i)if(state.upper[i]){
        int x=GRID_X+int(i%10)*TILE,y=GRID_Y+int(i/10)*TILE;
        if(move_ms&&int(i)==crate_to&&crate_from>=0){
            x=GRID_X+int(TILE*((crate_from%10)*(1-t)+(crate_to%10)*t));
            y=GRID_Y+int(TILE*((crate_from/10)*(1-t)+(crate_to/10)*t));
        }
            bool on=state.lower[i]==sokoban::GOAL;uint16_t c=on?mint:gold;
            studio::round(x+2,y+4,14,13,2,rgb(66,41,27));studio::round(x+1,y+1,14,13,2,on?rgb(56,158,106):rgb(184,111,42));
            rect(x+3,y+3,10,8,on?rgb(84,184,125):rgb(218,146,61));
            for(int j=0;j<3;++j)line(x+3,y+5+j*2,x+12,y+5+j*2,on?rgb(54,140,95):rgb(177,103,37));
            line(x+3,y+3,x+12,y+11,c);line(x+12,y+3,x+3,y+11,c);rect(x+1,y+1,14,2,c);rect(x+1,y+2,2,11,c);rect(x+13,y+2,2,11,c);rect(x+1,y+12,14,2,c);
            rect(x+2,y+2,1,1,rgb(255,235,183));rect(x+13,y+11,1,1,rgb(87,61,36));
    }
    {
        int i=state.player,x=GRID_X+(i%10)*TILE,y=GRID_Y+(i/10)*TILE;
        if(move_ms&&player_from>=0){x=GRID_X+int(TILE*((player_from%10)*(1-t)+(i%10)*t));y=GRID_Y+int(TILE*((player_from/10)*(1-t)+(i/10)*t));}
            studio::disc(x+9,y+14,6,rgb(13,27,41));rect(x+4,y+10,4,6,rgb(32,144,213));rect(x+10,y+10,4,6,rgb(28,111,182));
            studio::round(x+4,y+6,10,7,2,rgb(67,189,245));studio::round(x+3,y+1,11,9,2,rgb(247,244,218));
            rect(x+4,y+2,8,2,0xffff);rect(x+6,y+5,2,3,navy);rect(x+11,y+5,2,3,navy);
    }
    studio::card(SIDE,40,SIDE_W,32,rgb(24,44,65),rgb(48,73,97));
    stat(44,tr("STEPS","步数"),state.steps);stat(58,tr("PUSH","推箱"),state.pushes);
    bool solved=sokoban::won(state),stuck=sokoban::cornered_crate(state);
    studio::label(SIDE,78,SIDE_W,solved?tr("LEVEL COMPLETE","全部归位！"):blocked_ms?tr("TRY ANOTHER WAY","换个方向试试"):stuck?tr("CORNER! UNDO","死角了，试试撤销"):tr("FIND YOUR WAY","让木箱回家"),solved?mint:blocked_ms?BAD:stuck?WARN:MUTED);
    button(SIDE,94,SIDE_W,26,tr("Undo · B","撤销 · B"),state.history_count?ACCENT:MUTED);
    button(SIDE,124,SIDE_W,26,tr("Retry · X","重试 · X"),INK);
    button(SIDE,154,44,26,tr("< L","上关 L"),INK);button(SIDE+48,154,46,26,tr("R >","下关 R"),INK);
    studio::button(SIDE,184,SIDE_W,36,solved?(unsigned(state.level)+1==sokoban::LEVEL_COUNT?tr("Play again · A","重新挑战 · A"):tr("Next level · A","下一关 · A")):tr("Fill every goal","点亮所有目标"),solved?rgb(27,104,77):rgb(28,46,65),solved?mint:MUTED,solved);
    studio::round(SIDE+8,213,SIDE_W-16,3,1,rgb(18,35,51));
    if(goals&&filled)studio::round(SIDE+8,213,int((SIDE_W-16)*filled/goals),3,1,mint);
    effects.draw();
}

size_t serialize(uint8_t *data, size_t capacity) { return sokoban::serialize(state, data, capacity); }
void load(const uint8_t *data, size_t size) {
    if (sokoban::deserialize(state, data, size)) {
        stop_walk();effects.clear();dragging = false; blocked_ms = move_ms = 0;choosing=false;level_press=-1;level_notice_ms=0; pocket::redraw();
    }
}
}
