// SPDX-License-Identifier: MIT
#include "../common/pocket-game/runtime.hpp"
#include "../common/pocket-game/music.hpp"
#include "core.h"
#include "advisor.hpp"
#include "../common/pocket-studio/particles.hpp"
#include "../common/pocket-studio/studio.hpp"
#include <stdio.h>
#include <string.h>

namespace {
struct State {
    uint8_t board[4][4], undo_board[4][4];
    uint32_t score, best, moves, undo_score, undo_moves;
    bool undo;
};
State state{};
using pocket_game_sfx::Cue;
using pocket_game_sfx::cue;
constexpr int GRID_X = 4, GRID_Y = 38, GRID_SIZE = 180, TILE = 40;
bool dragging = false;
int pointer_x, pointer_y;
bool new_game_prompt=false;
int dialog_press=-1;
uint32_t flash_ms,slide_ms;
uint16_t merge_mask;
studio::Particles<> effects;
int hint_direction=-1;uint32_t hint_ms=0;
struct Slide {int sx,sy,tx,ty;unsigned exponent;};
Slide slides[16];unsigned slide_count;
void plan_slide(const uint8_t previous[4][4],pocket::Key key){
    slide_count=0;merge_mask=0;uint8_t hits[16]={};
    for(int lane=0;lane<4;++lane){int target=0;unsigned last=0;bool merge=false;
        for(int k=0;k<4;++k){
            int x=key==pocket::Key::LEFT?k:key==pocket::Key::RIGHT?3-k:lane;
            int y=key==pocket::Key::UP?k:key==pocket::Key::DOWN?3-k:lane;
            unsigned e=previous[x][y];if(!e)continue;
            int dest;if(merge&&last==e){dest=target-1;merge=false;}else{dest=target++;last=e;merge=true;}
            int tx=key==pocket::Key::LEFT?dest:key==pocket::Key::RIGHT?3-dest:lane;
            int ty=key==pocket::Key::UP?dest:key==pocket::Key::DOWN?3-dest:lane;
            slides[slide_count++]={x,y,tx,ty,e};
            if(++hits[ty*4+tx]==2)merge_mask|=uint16_t(1u<<(ty*4+tx));
        }
    }
    slide_ms=120;
}
int spawned_x = -1, spawned_y = -1;

void changed() { pocket::redraw(); pocket::save(); }
void prepare_board() {
    initBoard(state.board);
    state.score = state.moves = 0;
    state.undo = false;
    effects.clear();hint_direction=-1;hint_ms=0;dragging = false;new_game_prompt=false;dialog_press=-1;
    spawned_x = spawned_y = -1;
    flash_ms = slide_ms = 0; merge_mask=0;
}
void start_new_game() {prepare_board();cue(Cue::Reset);changed();}
void request_new_game() {
    dragging=false;dialog_press=-1;
    if(!state.moves){start_new_game();return;}
    new_game_prompt=true;pocket::redraw();
}
int dialog_hit(int x,int y) {
    if(y<144||y>=172)return -1;
    return x>=48&&x<136?0:x>=152&&x<240?1:-1;
}
void dialog_action(int action) {
    if(action==1)start_new_game();
    else if(action==0){new_game_prompt=false;dialog_press=-1;dragging=false;pocket::redraw();}
}
void draw_new_game_prompt() {
    using namespace pocket;
    clear(rgb(8,22,43));studio::gradient(0,0,288,224,rgb(8,22,43),rgb(17,40,62));
    studio::label(48,9,192,"2048",WARN,3);
    studio::card(32,48,224,138,rgb(23,43,65),rgb(70,95,119),9);
    studio::label(40,61,208,tr("START A NEW GAME?","开始新一局？"),INK,2);
    studio::label(40,94,208,tr("Keep playing, or start fresh.","继续当前棋局，或重新出发。"),INK);
    studio::label(40,110,208,tr("Your best score is kept.","最高分会保留。"),MUTED);
    studio::button(48,144,88,28,tr("Keep · B","继续 · B"),dialog_press==0?rgb(30,68,78):rgb(38,88,98),INK);
    studio::button(152,144,88,28,tr("New · A","新局 · A"),dialog_press==1?rgb(61,46,31):rgb(83,62,42),WARN);
    studio::label(32,199,224,tr("A new game · B keep playing","A 开始新局 · B 继续当前棋局"),MUTED);
}
void undo() {
    if (!state.undo) return;
    memcpy(state.board, state.undo_board, sizeof(state.board));
    state.score = state.undo_score;
    state.moves = state.undo_moves;
    state.undo = false;
    effects.clear();hint_direction=-1;hint_ms=0;
    flash_ms = slide_ms = 0; merge_mask=0;
    cue(Cue::Undo);
    changed();
}
void move(pocket::Key key) {
    uint8_t previous[4][4];
    memcpy(previous, state.board, sizeof(previous));
    const uint32_t previous_score = state.score;
    bool moved = false;
    switch (key) {
    case pocket::Key::UP: moved = moveUp(state.board, &state.score); break;
    case pocket::Key::RIGHT: moved = moveRight(state.board, &state.score); break;
    case pocket::Key::DOWN: moved = moveDown(state.board, &state.score); break;
    case pocket::Key::LEFT: moved = moveLeft(state.board, &state.score); break;
    default: return;
    }
    if (!moved) { cue(Cue::Blocked); return; }
    hint_direction=-1;hint_ms=0;
    plan_slide(previous,key);
    bool was_won = false, now_won = false;
    for (int x = 0; x < 4; ++x) for (int y = 0; y < 4; ++y) {
        was_won = was_won || previous[x][y] >= 11;
        now_won = now_won || state.board[x][y] >= 11;
    }
    cue(now_won && !was_won ? Cue::Goal : state.score != previous_score ? Cue::Merge : Cue::Move);
    memcpy(state.undo_board, previous, sizeof(previous));
    state.undo_score = previous_score;
    state.undo_moves = state.moves++;
    state.undo = true;
    if (state.score > state.best) state.best = state.score;
    memcpy(previous, state.board, sizeof(previous));
    addRandom(state.board);
    for (int x = 0; x < 4; ++x) for (int y = 0; y < 4; ++y)
        if (state.board[x][y] != previous[x][y]) { spawned_x = x; spawned_y = y; }
    flash_ms = 340;
    changed();
}
bool in_grid(int x, int y) {
    return x >= GRID_X && x < GRID_X + GRID_SIZE && y >= GRID_Y && y < GRID_Y + GRID_SIZE;
}
void request_hint(){hint_direction=advisor2048::suggest(state.board);hint_ms=3000;cue(hint_direction<0?Cue::Blocked:Cue::Select);pocket::redraw();}
bool in_button(int x, int y, int top) { return x >= 192 && x < 284 && y >= top && y < top + 24; }
uint16_t tile_color(unsigned exponent) {
    constexpr uint16_t colors[] = {
        pocket::rgb(56, 65, 79), pocket::rgb(238, 228, 218), pocket::rgb(237, 224, 200),
        pocket::rgb(242, 177, 121), pocket::rgb(245, 149, 99), pocket::rgb(246, 124, 95),
        pocket::rgb(246, 94, 59), pocket::rgb(237, 207, 114), pocket::rgb(237, 204, 97),
        pocket::rgb(237, 200, 80), pocket::rgb(237, 197, 63), pocket::rgb(237, 194, 46)
    };
    return exponent < sizeof(colors) / sizeof(colors[0]) ? colors[exponent] : pocket::rgb(153, 104, 206);
}
void put32(uint8_t *p, uint32_t value) { for (int i = 0; i < 4; ++i) p[i] = uint8_t(value >> (i * 8)); }
uint32_t get32(const uint8_t *p) {
    uint32_t value = 0;
    for (int i = 0; i < 4; ++i) value |= uint32_t(p[i]) << (i * 8);
    return value;
}
uint32_t checksum(const uint8_t *p, size_t size) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < size; ++i) hash = (hash ^ p[i]) * 16777619u;
    return hash;
}
bool valid_board(const uint8_t *p) {
    bool nonempty = false;
    for (int i = 0; i < 16; ++i) { if (p[i] > 31) return false; nonempty |= p[i] != 0; }
    return nonempty;
}
}

namespace game {
const char *title() { return "2048"; }
const char *app_id() { return "pxa-game-2048"; }
void init() {
    state = {};
    game2048_set_random(pocket::random);
    prepare_board();
    pocket::save();
}
void reset() { request_new_game(); }
void key(pocket::Key key) {
    if(new_game_prompt){if(key==pocket::Key::A)dialog_action(1);else if(key==pocket::Key::B)dialog_action(0);return;}
    switch (key) {
    case pocket::Key::A: request_hint(); break;
    case pocket::Key::B: undo(); break;
    case pocket::Key::X: reset(); break;
    default: move(key); break;
    }
}
void pointer(pocket::Phase phase, int x, int y) {
    if(new_game_prompt){
        if(phase==pocket::Phase::DOWN)dialog_press=dialog_hit(x,y);
        else if(phase==pocket::Phase::CANCEL)dialog_press=-1;
        else if(phase==pocket::Phase::UP){int action=dialog_press;dialog_press=-1;if(action>=0&&action==dialog_hit(x,y))dialog_action(action);}
        pocket::redraw();return;
    }
    if (phase == pocket::Phase::DOWN) {
        dragging = true; pointer_x = x; pointer_y = y;
    } else if (phase == pocket::Phase::CANCEL) {
        dragging = false;
    } else if (phase == pocket::Phase::UP && dragging) {
        dragging = false;
        const int dx = x - pointer_x, dy = y - pointer_y;
        const int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
        if (in_grid(pointer_x, pointer_y) && (ax >= 14 || ay >= 14)) {
            move(ax > ay ? (dx > 0 ? pocket::Key::RIGHT : pocket::Key::LEFT)
                         : (dy > 0 ? pocket::Key::DOWN : pocket::Key::UP));
        } else if (in_button(pointer_x, pointer_y, 136) && in_button(x, y, 136)) {
            undo();
        } else if(in_button(pointer_x,pointer_y,164)&&in_button(x,y,164)){request_hint();
        } else if (in_button(pointer_x, pointer_y, 192) && in_button(x, y, 192)) {
            reset();
        }
    }
}
void tick(uint32_t elapsed_ms) {
    if(effects.tick(elapsed_ms))pocket::redraw();
    if(hint_ms){hint_ms=elapsed_ms>=hint_ms?0:hint_ms-elapsed_ms;if(!hint_ms){hint_direction=-1;pocket::redraw();}}
    uint32_t before_slide=slide_ms;
    if(slide_ms)slide_ms=elapsed_ms>=slide_ms?0:slide_ms-elapsed_ms;
    if(before_slide&&!slide_ms)for(int y=0;y<4;++y)for(int x=0;x<4;++x)if(merge_mask&(1u<<(y*4+x)))effects.burst(GRID_X+24+44*x,GRID_Y+24+44*y,pocket::WARN,8);
    if (!flash_ms) return;
    flash_ms = elapsed_ms >= flash_ms ? 0 : flash_ms - elapsed_ms;
    pocket::redraw();
}
void draw() {
    if(new_game_prompt){draw_new_game_prompt();return;}
    using namespace pocket;
    const uint16_t navy=rgb(8,22,43),edge=rgb(46,74,104),gold=rgb(255,210,112),white=rgb(245,243,230);
    clear(navy);
    studio::gradient(0,0,288,224,navy,rgb(17,40,62));
    studio::label(26,5,74,"2048",gold,3);
    text(110,8,tr("ONE MORE MERGE","再合并一次"),white);
    text(110,23,tr("Swipe to move","滑动，向2048前进"),MUTED);
    studio::card(GRID_X,GRID_Y,GRID_SIZE,GRID_SIZE,rgb(13,29,48),edge,8);
    bool won=false;
    for(int x=0;x<4;++x)for(int y=0;y<4;++y){
        const int tx=GRID_X+4+44*x,ty=GRID_Y+4+44*y;
        unsigned e=slide_ms?0:state.board[x][y];uint16_t color=e?tile_color(e):rgb(30,49,71);
        const bool merged=!slide_ms&&flash_ms>100&&(merge_mask&(1u<<(y*4+x)));
        if(merged){int strength=int(flash_ms-100);if(strength>120)strength=120;studio::glow(tx+20,ty+20,26,gold,strength);color=studio::mix(color,white,strength/3);}
        const bool fresh=flash_ms&&x==spawned_x&&y==spawned_y;
        int inset=fresh&&flash_ms>150?2:0;
        studio::round(tx+inset,ty+3+inset,TILE-inset*2,TILE-inset*2,5,studio::mix(color,navy,110));
        studio::round(tx+inset,ty+inset,TILE-inset*2,TILE-2-inset*2,5,color);
        if(e){
            line(tx+5,ty+2,tx+TILE-6,ty+2,studio::mix(color,0xffff,95));
            char v[16];if(e<=16)snprintf(v,sizeof(v),"%lu",static_cast<unsigned long>(uint32_t(1)<<e));else snprintf(v,sizeof(v),"2^%u",e);
            int scale=text_width(v,2)<=TILE-3?2:1;
            studio::label(tx+1,ty+(TILE-10*scale)/2,TILE-2,v,e<=2?rgb(85,64,43):rgb(78,43,28),scale);
        }
        if(fresh)studio::sparkle(tx+34,ty+6,white);
        won|=state.board[x][y]>=11;
    }
    if(slide_ms){
        // Ease-out interpolation is purely visual; undo/save always see final rules.
        float t=1-float(slide_ms)/120.f;t=1-(1-t)*(1-t);
        for(unsigned i=0;i<slide_count;++i){const auto &v=slides[i];
            int tx=GRID_X+4+int(44*(v.sx+(v.tx-v.sx)*t)),ty=GRID_Y+4+int(44*(v.sy+(v.ty-v.sy)*t));
            uint16_t c=tile_color(v.exponent);studio::round(tx,ty+3,TILE,TILE,5,studio::mix(c,navy,110));studio::round(tx,ty,TILE,TILE-2,5,c);
            char vtext[16];if(v.exponent<=16)snprintf(vtext,sizeof(vtext),"%lu",static_cast<unsigned long>(uint32_t(1)<<v.exponent));else snprintf(vtext,sizeof(vtext),"2^%u",v.exponent);
            int scale=text_width(vtext,2)<=TILE-3?2:1;studio::label(tx+1,ty+(TILE-scale*10)/2,TILE-2,vtext,rgb(78,43,28),scale);
        }
    }
    studio::card(192,39,92,66,rgb(23,43,65),edge);
    studio::label(196,44,84,tr("SCORE","得分"),MUTED);
    char b[32];snprintf(b,sizeof(b),"%lu",static_cast<unsigned long>(state.score));studio::label(196,57,84,b,merge_mask&&flash_ms?gold:white,2);
    snprintf(b,sizeof(b),tr("Best %lu","最高 %lu"),static_cast<unsigned long>(state.best));studio::label(196,85,84,b,gold);
    bool over=gameEnded(state.board);
    const char *hints[]={tr("Try UP","建议向上"),tr("Try RIGHT","建议向右"),tr("Try DOWN","建议向下"),tr("Try LEFT","建议向左")};
    studio::label(192,113,92,hint_direction>=0&&hint_ms?hints[hint_direction]:over?tr("NO MORE MOVES","暂时无路可走"):won?tr("2048 REACHED!","达成2048！"):tr("SMALL STEPS","慢慢来，再试一步"),hint_direction>=0&&hint_ms?ACCENT:over?BAD:won?GOOD:MUTED);
    snprintf(b,sizeof(b),tr("%lu moves","已移动 %lu 步"),static_cast<unsigned long>(state.moves));studio::label(192,124,92,over?(state.undo?tr("UNDO OR NEW","撤销或新局"):tr("TRY AGAIN","再试一次")):b,MUTED);
    studio::button(192,136,92,24,tr("Undo · B","撤销 · B"),dragging&&in_button(pointer_x,pointer_y,136)?rgb(24,61,83):state.undo?rgb(34,81,106):rgb(27,45,63),state.undo?white:MUTED,state.undo);
    studio::button(192,164,92,24,tr("Hint · A","提示 · A"),rgb(34,66,65),rgb(166,224,198));
    studio::button(192,192,92,24,tr("New game · X","新一局 · X"),dragging&&in_button(pointer_x,pointer_y,192)?rgb(56,43,32):rgb(83,62,42),gold);
    effects.draw();
}

size_t serialize(uint8_t *data, size_t capacity) {
    if (capacity < 64) return 0;
    memset(data, 0, 64);
    memcpy(data, "P204", 4); data[4] = 1; data[5] = state.undo;
    put32(data + 8, state.score); put32(data + 12, state.best); put32(data + 16, state.moves);
    memcpy(data + 20, state.board, 16);
    put32(data + 36, state.undo_score); put32(data + 40, state.undo_moves);
    memcpy(data + 44, state.undo_board, 16);
    put32(data + 60, checksum(data, 60));
    return 64;
}
void load(const uint8_t *data, size_t size) {
    if (size != 64 || memcmp(data, "P204", 4) || data[4] != 1 || data[5] > 1 || data[6] || data[7] ||
        get32(data + 60) != checksum(data, 60) || !valid_board(data + 20) ||
        (data[5] && !valid_board(data + 44)) || get32(data + 12) < get32(data + 8)) return;
    State restored{};
    restored.score = get32(data + 8); restored.best = get32(data + 12); restored.moves = get32(data + 16);
    memcpy(restored.board, data + 20, 16);
    restored.undo_score = get32(data + 36); restored.undo_moves = get32(data + 40);
    memcpy(restored.undo_board, data + 44, 16); restored.undo = data[5];
    state = restored;
    effects.clear();hint_direction=-1;hint_ms=0;
    flash_ms = slide_ms = 0; merge_mask=0; dragging = false;new_game_prompt=false;dialog_press=-1;
    pocket::redraw();
}
}
