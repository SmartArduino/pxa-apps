// SPDX-License-Identifier: MIT
#include "../../common/pocket-game/runtime.hpp"
#include "../advisor.hpp"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

namespace {
struct Label { int x, y, w, h; std::string value; };
std::vector<Label> labels;
bool visible(const char *s) { for (const auto &label : labels) if (label.value == s) return true; return false; }
uint32_t random_state = 1;
unsigned saves, redraws, tiles, lines;
void put32(uint8_t *p, uint32_t n) { for(int i=0;i<4;++i) p[i]=uint8_t(n>>(8*i)); }
uint32_t get32(const uint8_t *p) { return uint32_t(p[0]) | uint32_t(p[1])<<8 | uint32_t(p[2])<<16 | uint32_t(p[3])<<24; }
void seal(uint8_t *p) {
    uint32_t h=2166136261u;
    for(int i=0;i<60;++i) h=(h^p[i])*16777619u;
    put32(p+60,h);
}
void fixture(uint8_t *p) {
    memset(p,0,64); memcpy(p,"P204",4); p[4]=1;
    for(int x=0;x<4;++x) p[20+4*x]=1;
    seal(p);
}
void snapshot(uint8_t *p) { assert(game::serialize(p,64)==64); }
}
namespace pocket {
unsigned tone_calls = 0, silence_calls = 0;
void tone(float hz, float seconds, float when) {
    assert(hz >= 30 && hz <= 6000 && seconds > 0 && seconds <= 4 && when >= 0);
    ++tone_calls;
}
void silence() { ++silence_calls; }
void clear(uint16_t) { labels.clear(); }
void rect(int,int,int w,int h,uint16_t) { assert(w>=0 && h>=0); ++tiles; }
void outline(int,int,int,int,uint16_t) {}
void line(int,int,int,int,uint16_t) { ++lines; }
void text(int x,int y,const char *s,uint16_t,int scale) {
    int h = 7;
    for (const char *p = s; *p;) if (pocket_ui::next_utf8(p) >= 128) h = 10;
    h *= scale;
    const int w = text_width(s, scale);
    assert(x >= 0 && y >= 0 && x + w <= WIDTH && y + h <= HEIGHT);
    for (const auto &other : labels)
        assert(x + w <= other.x || other.x + other.w <= x || y + h <= other.y || other.y + other.h <= y);
    labels.push_back({x, y, w, h, s});
}
int text_width(const char *s,int scale) { return pocket_ui::text_width(s,scale); }
void redraw() { ++redraws; }
void save() { ++saves; }
uint32_t random() { random_state=random_state*1664525u+1013904223u; return random_state; }
uint32_t millis() { return 0; }
void log(const char*) {}
}
int main() {
    using pocket::Key; using pocket::Phase;
    game::init();
    uint8_t start[64], moved[64], restored[64], other[64];
    snapshot(start);
    int nonzero=0; for(int i=20;i<36;++i) nonzero+=start[i]!=0;
    assert(nonzero==2 && game::serialize(start,63)==0);

    fixture(start); game::load(start,64); random_state=7;
    game::key(Key::LEFT); snapshot(moved);
    assert(get32(moved+8)==8 && get32(moved+12)==8 && get32(moved+16)==1 && moved[5]);
    assert(moved[20]==2 && moved[24]==2 && pocket::tone_calls == 2);
    nonzero=0; for(int i=20;i<36;++i) nonzero+=moved[i]!=0;
    assert(nonzero==3 && saves==2);
    game::key(Key::B); snapshot(restored);
    assert(!memcmp(start+20,restored+20,16) && get32(restored+8)==0 && get32(restored+12)==8 && !restored[5]);
    game::key(Key::B); snapshot(other); assert(!memcmp(restored,other,64));

    game::init(); game::load(moved,64); game::key(Key::B); snapshot(other);
    assert(!memcmp(start+20,other+20,16) && get32(other+8)==0);
    game::load(moved,64); snapshot(restored);
    moved[21]^=1; game::load(moved,64); snapshot(other); assert(!memcmp(restored,other,64));
    moved[21]^=1;
    moved[20]=255; seal(moved); game::load(moved,64); snapshot(other); assert(!memcmp(restored,other,64));
    game::load(start,63); snapshot(other); assert(!memcmp(restored,other,64));

    game::load(start,64); random_state=123;
    game::key(Key::LEFT); snapshot(moved);
    game::load(start,64); random_state=123;
    game::pointer(Phase::DOWN,140,80); game::pointer(Phase::MOVE,100,82); game::pointer(Phase::UP,20,81);
    snapshot(other); assert(!memcmp(moved,other,64));
    game::pointer(Phase::DOWN,130,80); game::pointer(Phase::CANCEL,0,0); game::pointer(Phase::UP,20,80);
    snapshot(other); assert(!memcmp(moved,other,64));
    game::pointer(Phase::DOWN,200,150); game::pointer(Phase::UP,200,150);
    snapshot(other); assert(!memcmp(start+20,other+20,16));

    // 无效方向不新增方块，也不消耗撤销。
    fixture(start); start[20]=1; start[24]=2; start[28]=3; start[32]=4; seal(start);
    game::load(start,64); snapshot(restored); const unsigned old_saves=saves;
    game::key(Key::LEFT); snapshot(other); assert(!memcmp(restored,other,64) && saves==old_saves);
    game::draw(); assert(tiles>=19);
    game::key(Key::X); snapshot(other); assert(get32(other+8)==0 && !other[5]);
    assert(redraws>0);
    fixture(start); start[20]=17; seal(start); game::load(start,64); game::draw();
    assert(visible("2^17"));
    // A new-game request must preserve a live board until explicitly confirmed.
    fixture(start);game::load(start,64);game::key(Key::LEFT);snapshot(moved);
    game::key(Key::X);snapshot(other);assert(!memcmp(moved,other,64));
    for(bool en:{false,true}){pocket::set_english(en);game::draw();assert(visible(en?"START A NEW GAME?":"开始新一局？"));}
    pocket::set_english(false);
    game::key(Key::UP);snapshot(other);assert(!memcmp(moved,other,64));
    game::pointer(Phase::DOWN,190,155);game::pointer(Phase::CANCEL,190,155);game::pointer(Phase::UP,190,155);
    snapshot(other);assert(!memcmp(moved,other,64));
    game::pointer(Phase::DOWN,190,155);game::pointer(Phase::UP,10,10);snapshot(other);assert(!memcmp(moved,other,64));
    game::key(Key::B);snapshot(other);assert(!memcmp(moved,other,64));
    game::key(Key::X);game::pointer(Phase::DOWN,190,155);game::pointer(Phase::UP,190,155);snapshot(other);
    assert(get32(other+16)==0&&get32(other+8)==0&&!other[5]&&get32(other+12)==get32(moved+12));
    fixture(start);game::load(start,64);game::key(Key::LEFT);game::reset();game::key(Key::A);snapshot(other);assert(get32(other+16)==0);
    // Both locales must fit every HUD branch without changing saved state.
    for (bool en : {false, true}) {
        pocket::set_english(en);
        fixture(start); game::load(start,64); snapshot(restored); game::draw();
        assert(visible(en ? "SCORE" : "得分") && visible(en ? "SMALL STEPS" : "慢慢来，再试一步"));
        assert(visible(en ? "Undo · B" : "撤销 · B") && visible(en ? "New game · X" : "新一局 · X"));
        snapshot(other); assert(!memcmp(restored,other,64));
        start[20]=11; seal(start); game::load(start,64); game::draw(); assert(visible(en ? "2048 REACHED!" : "达成2048！"));
        for (int x=0;x<4;++x) for (int y=0;y<4;++y) start[20+4*x+y]=uint8_t(1+(x+y)%2);
        for (int i=0;i<2;++i) {
            start[5]=uint8_t(i); memcpy(start+44,start+20,16); seal(start); game::load(start,64); game::draw();
            assert(visible(en ? "NO MORE MOVES" : "暂时无路可走"));
            assert(visible(i ? (en ? "UNDO OR NEW" : "撤销或新局") : (en ? "TRY AGAIN" : "再试一次")));
        }
        put32(start+8,UINT32_MAX); put32(start+12,UINT32_MAX); seal(start); game::load(start,64); game::draw();
    }
    pocket::set_english(false);
    snapshot(restored); unsigned before_tones = pocket::tone_calls;
    for (int i=0;i<100;++i) game::tick(20);
    snapshot(other); assert(!memcmp(restored,other,64));
    assert(pocket::tone_calls == before_tones);
    // Advice is legal and read-only, including the RNG and undo state.
    fixture(start);game::load(start,64);snapshot(restored);
    const uint32_t rng_before=random_state;const unsigned saves_before=saves;
    game::key(Key::A);game::draw();snapshot(other);
    assert(!memcmp(restored,other,64)&&rng_before==random_state&&saves_before==saves);
    assert(visible("建议向上")||visible("建议向右")||visible("建议向下")||visible("建议向左"));
    game::tick(3100);game::draw();assert(visible("慢慢来，再试一步"));
    uint32_t seed=17;
    for(unsigned k=0;k<1000;++k){
        uint8_t board[4][4],before[4][4];for(auto &column:board)for(auto &v:column){seed=seed*1664525u+1013904223u;v=uint8_t(seed>>28);}
        memcpy(before,board,16);int direction=advisor2048::suggest(board);assert(!memcmp(board,before,16));
        if(direction>=0)assert(direction<4&&advisor2048::move(board,unsigned(direction)));
        else assert(gameEnded(board));
    }
    uint8_t stuck[4][4];for(int x=0;x<4;++x)for(int y=0;y<4;++y)stuck[x][y]=uint8_t(1+(x+y)%2);
    assert(advisor2048::suggest(stuck)==-1);
    puts("2048 advice: legal suggestions on 1000 boards, no legal move, no save/RNG/undo mutation passed");
    puts("2048 app: save/load, corruption, undo, score, swipe, cancel, no-op move, reset and bilingual HUD bounds/overlap/save invariance passed");
}
