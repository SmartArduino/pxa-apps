// SPDX-License-Identifier: CC0-1.0
#include "../core.hpp"
#include "../navigation.hpp"
#include "../../common/pocket-game/runtime.hpp"
#include "solutions.hpp"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

namespace {
struct Label { int x, y, w, h; std::string value; };
std::vector<Label> labels;
bool visible(const char *s) { for (const auto &label : labels) if (label.value == s) return true; return false; }
}
namespace pocket {
unsigned tone_calls = 0, silence_calls = 0;
void tone(float hz, float seconds, float when) {
    assert(hz >= 30 && hz <= 6000 && seconds > 0 && seconds <= 4 && when >= 0);
    ++tone_calls;
}
void silence() { ++silence_calls; }
unsigned saves = 0, redraws = 0;
void clear(uint16_t) { labels.clear(); }
void rect(int, int, int, int, uint16_t) {}
void outline(int, int, int, int, uint16_t) {}
void line(int, int, int, int, uint16_t) {}
void text(int x, int y, const char *s, uint16_t, int scale) {
    int h = 7;
    for (const char *p = s; *p;) if (pocket_ui::next_utf8(p) >= 128) h = 10;
    h *= scale;
    const int w = text_width(s, scale);
    assert(x >= 0 && y >= 0 && x + w <= WIDTH && y + h <= HEIGHT);
    for (const auto &other : labels)
        assert(x + w <= other.x || other.x + other.w <= x || y + h <= other.y || other.y + other.h <= y);
    labels.push_back({x, y, w, h, s});
}
int text_width(const char *value, int scale) { return pocket_ui::text_width(value, scale); }
void redraw() { ++redraws; }
void save() { ++saves; }
uint32_t random() { return 42; }
uint32_t millis() { return 0; }
void log(const char *) {}
}
namespace {
using namespace sokoban;
Direction direction(char c) {
    switch (c) { case 'U': return UP; case 'R': return RIGHT; case 'D': return DOWN; default: assert(c == 'L'); return LEFT; }
}
bool same_board(const State &a, const State &b) {
    return a.player == b.player && !memcmp(a.lower, b.lower, CELLS) && !memcmp(a.upper, b.upper, CELLS);
}
State corridor() {
    State s{};
    memset(s.lower, WALL, CELLS);
    for (unsigned i = 31; i <= 38; ++i) s.lower[i] = FLOOR;
    s.lower[37] = GOAL; s.player = 34; s.upper[35] = 1;
    return s;
}
uint32_t crc32(const uint8_t *p, size_t n) {
    uint32_t crc = 0xffffffffu;
    for (size_t i = 0; i < n; ++i) {
        crc ^= p[i];
        for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320u : 0u);
    }
    return ~crc;
}
void recheck(uint8_t *p, size_t n) {
    const uint32_t crc = crc32(p, n - 4);
    for (int i = 0; i < 4; ++i) p[n - 4 + i] = uint8_t(crc >> (8 * i));
}
void movement() {
    State s = corridor(), initial = s;
    assert(move(s, RIGHT) && s.player == 35 && s.upper[36] && !s.upper[35]);
    assert(s.steps == 1 && s.pushes == 1 && s.history[0] == (RIGHT | 0x10));
    assert(undo(s) && same_board(s, initial) && !s.steps && !s.pushes);
    assert(move(s, LEFT) && s.player == 33 && !s.pushes);
    assert(undo(s) && same_board(s, initial));
    s.upper[36] = 1; const State two = s;
    assert(!move(s, RIGHT) && !memcmp(&s, &two, sizeof(s)));
    s = initial; s.lower[36] = WALL;
    assert(!move(s, RIGHT)); // Box into wall.
    assert(!move(s, UP)); // Player into wall.
    s = initial;
    assert(move(s, RIGHT) && move(s, RIGHT) && won(s));
    assert(s.lower[37] == GOAL && s.upper[37]);
    assert(undo(s) && s.lower[37] == GOAL && !s.upper[37]);
    assert(undo(s) && same_board(s, initial));
    // Missing enclosing walls must never permit wraparound or OOB access.
    s = {}; s.player = 0;
    assert(!move(s, UP) && !move(s, LEFT));
    s.player = 9; assert(!move(s, RIGHT));
    s.player = 99; assert(!move(s, DOWN) && !move(s, RIGHT));
    s = {}; s.player = 8; s.upper[9] = 1;
    assert(!move(s, RIGHT)); // Second destination lies outside row.
    s = {}; s.player = 10; s.upper[0] = 1;
    assert(!move(s, UP));
    assert(!move(s, static_cast<Direction>(255)));
    puts("PASS single crate / double crate / wall / all boundaries / goal / walking+push rewind");
}
void navigation() {
    State s=corridor(),original=s;uint8_t path[CELLS];
    assert(walk_path(s,31,path,CELLS)==3);
    for(unsigned i=0;i<3;++i)assert(move(s,Direction(path[i])));
    assert(s.player==31&&s.pushes==0&&!memcmp(s.upper,original.upper,CELLS));
    assert(walk_path(original,31,path,2)==-1&&walk_path(original,31,nullptr,CELLS)==-1);
    assert(walk_path(original,35,path,CELLS)==-1&&walk_path(original,37,path,CELLS)==-1);
    assert(walk_path(original,100,path,CELLS)==-1&&walk_path(original,0,path,CELLS)==-1);
    assert(walk_path(original,original.player,nullptr,0)==0);
    s={};s.player=0;assert(walk_path(s,99,path,CELLS)==18);
    for(int i=0;i<18;++i)assert(move(s,Direction(path[i])));
    assert(s.player==99);
    s={};s.upper[0]=1;assert(cornered_crate(s));s.lower[0]=GOAL;assert(!cornered_crate(s));
    s={};s.upper[55]=1;s.lower[54]=s.lower[45]=WALL;assert(cornered_crate(s));s.lower[45]=FLOOR;assert(!cornered_crate(s));
    puts("PASS walking BFS: shortest path, no pushes, blocked targets, limits, open boundaries and static corner warning");
}
void solutions() {
    static_assert(sizeof(SOLUTIONS) / sizeof(SOLUTIONS[0]) == LEVEL_COUNT);
    State s{}; init(s);
    for (unsigned level = 0; level < LEVEL_COUNT; ++level) {
        reset_level(s, level);
        const State initial = s;
        assert(valid(s));
        for (const char *path = SOLUTIONS[level]; *path; ++path) {
            assert(move(s, direction(*path)));
            assert(valid(s));
        }
        assert(won(s) && (s.completed & (uint64_t(1) << level)));
        assert(s.unlocked == (level + 1 < LEVEL_COUNT ? level + 1 : level));
        uint8_t bytes[4096];
        const size_t size = serialize(s, bytes, sizeof(bytes));
        State restored{};
        assert(size <= 3900 && deserialize(restored, bytes, size) && same_board(restored, s));
        assert(restored.completed == s.completed && restored.unlocked == s.unlocked);
        while (s.history_count) assert(undo(s));
        assert(same_board(s, initial) && s.steps == 0 && s.pushes == 0 && valid(s));
        assert(s.completed & (uint64_t(1) << level)); // Completion survives retry/undo.
    }
    assert(completed_count(s) == LEVEL_COUNT);
    printf("PASS all %u packaged levels: offline BFS path, progress, complete rewind, solved save\n", LEVEL_COUNT);
}
void persistence() {
    State source{}; init(source);
    assert(move(source, UP)); assert(move(source, RIGHT));
    assert(move(source, DOWN)); assert(move(source, LEFT));
    uint8_t bytes[4096], broken[4096];
    const size_t size = serialize(source, bytes, sizeof(bytes));
    assert(size && serialize(source, broken, size - 1) == 0);
    State restored{}; init(restored);
    assert(deserialize(restored, bytes, size) && same_board(source, restored));
    assert(restored.steps == source.steps && restored.history_count == source.history_count);
    while (source.history_count) {
        assert(undo(source) && undo(restored));
        assert(same_board(source, restored));
    }
    State sentinel{}; init(sentinel); assert(move(sentinel, DOWN));
    const State before = sentinel;
    for (size_t i = 0; i < size; ++i) {
        memcpy(broken, bytes, size); broken[i] ^= 1;
        assert(!deserialize(sentinel, broken, size));
        assert(!memcmp(&sentinel, &before, sizeof(before)));
    }
    for (size_t truncated = 0; truncated < size; ++truncated) {
        assert(!deserialize(sentinel, bytes, truncated));
        assert(!memcmp(&sentinel, &before, sizeof(before)));
    }
    assert(!deserialize(sentinel, nullptr, size));
    memcpy(broken, bytes, size); broken[size] = 0;
    assert(!deserialize(sentinel, broken, size + 1));
    const unsigned offsets[] = {4, 5, 6, 7, 8, 12, 16, 23, 24, 25, 26, 28, 128, 228};
    const uint8_t values[] = {2, 255, 255, 1, 0, 255, 255, 255, 0, 1, 255, 0, 1, 0x10 | UP};
    for (unsigned i = 0; i < sizeof(offsets) / sizeof(offsets[0]); ++i) {
        memcpy(broken, bytes, size); broken[offsets[i]] = values[i]; recheck(broken, size);
        assert(!deserialize(sentinel, broken, size));
        assert(!memcmp(&sentinel, &before, sizeof(before)));
    }
    // Safe-looking box relocated onto a wall must fail even with a fresh CRC.
    memcpy(broken, bytes, size); broken[128] = 1; broken[128 + 44] = 0; recheck(broken, size);
    assert(!deserialize(sentinel, broken, size));
    printf("PASS save roundtrip / every-byte corruption (%zu) / every truncation / semantic rejection / transactional load\n", size);
}
void circular_history() {
    State s{}; init(s);
    for (unsigned i = 0; i < HISTORY + 30; ++i) assert(move(s, i % 2 ? DOWN : UP));
    assert(s.history_count == HISTORY && valid(s));
    uint8_t bytes[4096];
    size_t size = serialize(s, bytes, sizeof(bytes));
    assert(size == 232 + HISTORY && size <= 3900);
    State restored{};
    assert(deserialize(restored, bytes, size));
    const State before = restored;
    uint8_t corrupt[4096];
    for (size_t byte = 0; byte < size; ++byte) {
        memcpy(corrupt, bytes, size); corrupt[byte] ^= 0x80;
        assert(!deserialize(restored, corrupt, size));
        assert(!memcmp(&restored, &before, sizeof(before)));
        assert(!deserialize(restored, bytes, byte));
        assert(!memcmp(&restored, &before, sizeof(before)));
    }
    for (unsigned i = 0; i < HISTORY; ++i) {
        assert(undo(s) && undo(restored) && same_board(s, restored));
    }
    assert(!undo(s) && s.steps == 30 && valid(s));
    size = serialize(s, bytes, sizeof(bytes));
    assert(deserialize(restored, bytes, size) && restored.steps == 30);
    assert(move(s, UP) && undo(s));
    puts("PASS 2048-entry circular history wrap / full rewind limit / persistence after history exhaustion");
}
State game_state() {
    uint8_t bytes[4096]; const size_t size = game::serialize(bytes, sizeof(bytes));
    State s{}; assert(deserialize(s, bytes, size)); return s;
}
void auto_walk() {
    game::init();State initial=game_state();uint8_t route[CELLS];int target=-1,count=0;
    for(unsigned i=0;i<CELLS;++i){int n=walk_path(initial,i,route,CELLS);if(n>=3){target=int(i);count=n;break;}}
    assert(target>=0);const int x=4+(target%10)*18+8,y=40+(target/10)*18+8;
    auto plan=[&](){game::pointer(pocket::Phase::DOWN,x,y);game::pointer(pocket::Phase::UP,x,y);};
    plan();assert(game_state().steps==0);for(int i=0;i<count;++i)game::tick(100);
    assert(game_state().player==target&&game_state().steps==unsigned(count)&&game_state().pushes==0);
    game::key(pocket::Key::B);assert(game_state().steps==unsigned(count-1));
    game::reset();plan();game::tick(100);game::key(pocket::Key::B);game::tick(2000);assert(same_board(game_state(),initial));
    plan();game::pointer(pocket::Phase::DOWN,x,y);game::pointer(pocket::Phase::CANCEL,x,y);game::tick(2000);assert(game_state().steps==0);
    plan();game::key(pocket::Key::SELECT);game::tick(1000);assert(game_state().steps==0);game::key(pocket::Key::B);
    plan();uint8_t raw[4096];size_t n=game::serialize(raw,sizeof(raw));game::load(raw,n);game::tick(1000);assert(game_state().steps==0);
    puts("PASS tap walking, incremental history, undo, new-touch/CANCEL, selector and load cancel queued movement");
}
void inputs() {
    game::init(); State initial = game_state();
    const unsigned old_tones = pocket::tone_calls;
    // Tap the adjacent crate; then B restores it.
    game::pointer(pocket::Phase::DOWN, 85, 121);
    game::pointer(pocket::Phase::UP, 85, 121);
    assert(game_state().steps == 1 && pocket::tone_calls == old_tones + 2);
    game::key(pocket::Key::B); assert(same_board(game_state(), initial));
    assert(pocket::tone_calls == old_tones + 4);
    const unsigned saved = pocket::saves;
    game::pointer(pocket::Phase::DOWN, 85, 121);
    game::pointer(pocket::Phase::CANCEL, 85, 121);
    game::pointer(pocket::Phase::UP, 110, 121);
    assert(pocket::saves == saved);
    game::pointer(pocket::Phase::DOWN, 85, 121);
    game::pointer(pocket::Phase::UP, 110, 121);
    game::key(pocket::Key::RIGHT); assert(won(game_state()));
    game::key(pocket::Key::A); assert(game_state().level == 1);
    game::key(pocket::Key::L); assert(game_state().level == 0);
    game::key(pocket::Key::Y); assert(game_state().level == 1);
    game::pointer(pocket::Phase::DOWN, 200, 163);
    game::pointer(pocket::Phase::UP, 200, 163);
    assert(game_state().level == 0);
    game::pointer(pocket::Phase::DOWN, 260, 163);
    game::pointer(pocket::Phase::UP, 260, 163);
    assert(game_state().level == 1);
    game::key(pocket::Key::DOWN); assert(game_state().steps == 1);
    game::pointer(pocket::Phase::DOWN, 220, 106);
    game::pointer(pocket::Phase::UP, 220, 106);
    assert(game_state().steps == 0);
    game::key(pocket::Key::DOWN);
    game::pointer(pocket::Phase::DOWN, 220, 137);
    game::pointer(pocket::Phase::UP, 220, 137);
    assert(game_state().steps == 0 && game_state().completed == 1);
    game::draw(); game::tick(100);
    puts("PASS game input hooks: tap / swipe / cancellation / undo / retry / next / unlocked selection");
}
}
void level_picker(){
    using pocket::Key;using pocket::Phase;
    game::init();game::key(Key::RIGHT);uint8_t before[4096],after[4096];size_t n=game::serialize(before,sizeof(before));
    game::key(Key::SELECT);
    for(bool en:{false,true}){pocket::set_english(en);game::draw();assert(visible(en?"CHOOSE A LEVEL":"选择关卡"));}
    pocket::set_english(false);assert(game::serialize(after,sizeof(after))==n&&!memcmp(before,after,n));
    game::key(Key::A);assert(game_state().steps==1); // choosing the current level resumes it.
    game::key(Key::SELECT);game::key(Key::RIGHT);game::key(Key::A);assert(game_state().level==0&&game_state().steps==1);
    game::pointer(Phase::DOWN,32,55);game::pointer(Phase::CANCEL,32,55);game::pointer(Phase::UP,32,55);
    game::key(Key::B);assert(game_state().steps==1);
    game::key(Key::RIGHT);assert(sokoban::won(game_state()));game::key(Key::A);assert(game_state().level==1);
    game::pointer(Phase::DOWN,180,12);game::pointer(Phase::UP,180,12);
    game::pointer(Phase::DOWN,32,55);game::pointer(Phase::UP,32,55);assert(game_state().level==0&&game_state().completed==1);
    game::key(Key::SELECT);game::pointer(Phase::DOWN,78,55);game::pointer(Phase::UP,78,55);assert(game_state().level==1);
    game::key(Key::SELECT);game::pointer(Phase::DOWN,32,55);game::pointer(Phase::UP,500,400);assert(game_state().level==1);game::key(Key::B);
    puts("PASS level selector: bilingual grid, locked refusal, current-level resume, completed progress, touch/controller and cancellation");
}

void bilingual() {
    using namespace sokoban;
    auto draw = [](const char *en, const char *zh) {
        uint8_t before[4096], after[4096]; const size_t n = game::serialize(before,sizeof(before)); assert(n);
        for (bool english : {false, true}) {
            pocket::set_english(english); game::draw();
            if (!visible(english ? en : zh)) fprintf(stderr,"missing HUD label: %s\n",english ? en : zh);
            assert(visible(english ? en : zh));
            assert(game::serialize(after,sizeof(after)) == n && !memcmp(before,after,n));
        }
        pocket::set_english(false);
    };
    game::init(); draw("FIND YOUR WAY", "让木箱回家");
    for (int i=0;i<4;++i) game::key(pocket::Key::UP);
    draw("TRY ANOTHER WAY", "换个方向试试");
    game::tick(400); draw("FIND YOUR WAY", "让木箱回家");
    game::key(pocket::Key::X);
    game::key(pocket::Key::RIGHT); game::key(pocket::Key::RIGHT); draw("Next level · A", "下一关 · A");
    State last{}; init(last); last.unlocked=LEVEL_COUNT-1; last.completed=(uint64_t(1) << (LEVEL_COUNT-1))-1;
    reset_level(last, LEVEL_COUNT-1);
    for (const char *p=SOLUTIONS[LEVEL_COUNT-1]; *p; ++p) assert(move(last,direction(*p)));
    assert(won(last));
    uint8_t bytes[4096]; size_t n=serialize(last,bytes,sizeof(bytes)); assert(n); game::load(bytes,n);
    draw("Play again · A", "重新挑战 · A");
    last.steps=last.pushes=MAX_MOVES; n=serialize(last,bytes,sizeof(bytes)); assert(n); game::load(bytes,n);
    draw("PUSH", "推箱");
    puts("PASS bilingual active/blocked/solved/final-level/max-counter HUD bounds, no text overlap and save invariance");
}
int main() {
    movement(); navigation(); solutions(); persistence(); circular_history(); inputs(); auto_walk(); level_picker(); bilingual();
    uint8_t before[4096],after[4096]; const size_t n=game::serialize(before,sizeof(before));
    unsigned before_tones=pocket::tone_calls;
    for (int i=0;i<100;++i) game::tick(20);
    assert(game::serialize(after,sizeof(after))==n && !memcmp(before,after,n));
    assert(pocket::tone_calls == before_tones);
    puts("ALL SOKOBAN HOST TESTS PASSED; idle is silent and cues preserve save bytes");
}
