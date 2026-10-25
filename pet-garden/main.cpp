// SPDX-License-Identifier: MIT
#include "../common/pocket-play/runtime.hpp"
#include "core.hpp"
#include <stdio.h>

namespace {
using namespace pocket;
using garden::Mode;
using garden::Event;
garden::State s;
constexpr uint16_t INK2 = rgb(54, 48, 73), CREAM = rgb(255, 247, 221);
constexpr uint16_t PINK = rgb(244, 143, 165), PURPLE = rgb(133, 107, 162);
constexpr uint16_t GREEN = rgb(100, 163, 116), DARKGREEN = rgb(55, 103, 86);
constexpr uint16_t SKY = rgb(173, 220, 230), GOLD = rgb(243, 196, 98);
uint32_t animation_ms = 0, save_ms = 0, toast_ms = 0, effect_ms = 0;
int touch_x = 0, touch_y = 0;
bool touching = false;
uint8_t help_page = 0;
Event effect = Event::NONE;
struct Message { const char *en, *zh; const char *get() const { return pocket::tr(en, zh); } };
Message toast = {"HELLO! PET PIP OR CHOOSE A CARE BUTTON.", "你好！摸摸皮皮，或选择照顾按钮。"};
const char *pet_name() {
    return s.stage < 2 ? pocket::tr("PIP", "皮皮") : s.branch == 1 ? pocket::tr("MOSSBUN", "苔苔兔") : s.branch == 2 ? pocket::tr("SUNFOX", "暖阳狐") : pocket::tr("MOONMOTH", "月光蛾");
}
const char *stage_name() { return s.stage == 0 ? pocket::tr("BUD", "花苞") : s.stage == 1 ? pocket::tr("SPROUT", "嫩芽") : pocket::tr("BLOOM", "盛放"); }

void center(int x, int y, int w, const char *label, uint16_t color = INK2, int scale = 1) {
    text(x + (w - text_width(label, scale)) / 2, y, label, color, scale);
}
void number(int x, int y, unsigned value, uint16_t color = INK2) {
    char buf[16]; snprintf(buf, sizeof(buf), "%u", value); text(x, y, buf, color);
}
void panel(int x, int y, int w, int h, uint16_t color, uint16_t edge = INK2) {
    rect(x + 3, y, w - 6, h, edge); rect(x, y + 3, w, h - 6, edge);
    rect(x + 3, y + 2, w - 6, h - 4, color); rect(x + 2, y + 3, w - 4, h - 6, color);
}
void oval(int x, int y, int w, int h, uint16_t color) {
    // Integer, bounded scan lines. Stepped edges make deliberately pixel-shaped art.
    int rx = w / 2, ry = h / 2;
    for (int yy = -ry; yy <= ry; yy += 2) {
        int xx = rx;
        while (xx > 0 && xx * xx * ry * ry + yy * yy * rx * rx > rx * rx * ry * ry) --xx;
        rect(x + rx - xx, y + ry + yy, xx * 2 + 1, 2, color);
    }
}
void heart(int x, int y, uint16_t color, int z = 1) {
    rect(x, y, 3*z, 2*z, color); rect(x+4*z, y, 3*z, 2*z, color);
    rect(x, y+2*z, 7*z, 2*z, color); rect(x+z, y+4*z, 5*z, z, color); rect(x+2*z, y+5*z, 3*z, z, color); rect(x+3*z, y+6*z, z, z, color);
}
void flower(int x, int y, uint16_t color) {
    rect(x, y+5, 2, 12, DARKGREEN); rect(x+2, y+10, 4, 2, GREEN);
    rect(x-4, y, 10, 6, color); rect(x-2, y-2, 6, 10, color); rect(x, y+2, 2, 2, GOLD);
}
void berry(int x, int y, bool golden = false) {
    rect(x-3, y-2, 8, 8, golden ? GOLD : PINK); rect(x-1,y-4,4,2,DARKGREEN); rect(x-2,y-1,2,2,CREAM);
}
void star(int x, int y, uint16_t color) { rect(x-1, y-5, 3, 11, color); rect(x-5,y-1,11,3,color); }
void announce(Message message, uint32_t duration = 2600) {
    toast = message; toast_ms = duration; redraw();
}
void record(const char *tag) {
    char line[192];
    snprintf(line, sizeof(line), "pet-garden %s mode=%u growth=%u stage=%u branch=%u coins=%u snacks=%u stats=%u/%u/%u/%u rounds=%u score=%u", tag, unsigned(s.mode), s.growth, s.stage, s.branch, s.coins, s.snacks, s.food, s.clean, s.joy, s.energy, s.rounds, s.score);
    log(line); save(); redraw();
}
void event(Event e) {
    if (e == Event::NONE) return;
    effect = e; effect_ms = 1300;
    Message label = {"", ""};
    switch (e) {
    case Event::FED: label = Message{"YUM! A GARDEN BERRY. +7 GROWTH", "好吃的莓果！成长 +7"}; tone(660, .07f); break;
    case Event::WASHED: label = Message{"BUBBLES! ALL FRESH. +7 GROWTH", "泡泡洗香香！成长 +7"}; tone(880,.07f); break;
    case Event::PETTED: label = Message{"PIP LOVES YOUR PATS! +3 GROWTH", "皮皮喜欢摸摸！成长 +3"}; tone(520,.06f); break;
    case Event::RESTING: label = Message{"COZY NAP... ENERGY +2 EACH SECOND", "舒服小睡中，每秒精力 +2"}; break;
    case Event::AWAKE: label = Message{"GOOD MORNING, LITTLE FRIEND!", "早安，小伙伴！"}; break;
    case Event::NO_FOOD: label = Message{"BERRIES GROW EVERY 30 PLAY SECONDS.", "每游玩30秒长出一颗莓果。"}; break;
    case Event::WAIT: label = s.sleeping ? Message{"A NAP! REST AGAIN TO WAKE PIP.", "正在小睡，再按休息唤醒皮皮。"} : Message{"ALL GOOD! TRY ANOTHER CARE ACTION.", "状态很好！试试其他照顾。"}; break;
    case Event::EVOLVED: label = Message{"A NEW BLOOM! YOUR FRIEND HAS GROWN!", "开花啦！小伙伴长大了！"}; tone(523,.12f); tone(659,.12f,float(millis())/1000.0f+.13f); tone(784,.2f,float(millis())/1000.0f+.26f); break;
    case Event::HARVEST: label = Message{"A RIPE GARDEN BERRY! +1 SNACK", "莓果成熟啦！点心 +1"}; break;
    case Event::BOUGHT: label = Message{"A LITTLE HOME, A LITTLE MORE COZY.", "添件小装饰，家里更温馨。"}; tone(784,.09f); break;
    case Event::TOGGLED: label = Message{"DECORATION CHANGED.", "装饰已切换。"}; break;
    case Event::POOR: label = Message{"PLAY A MINIGAME TO EARN COINS.", "玩小游戏可以赚金币。"}; break;
    case Event::CAUGHT: label = {"", ""}; tone(720,.035f); break;
    case Event::BEAT: label = Message{"NICE BLOOM!", "花开得真好！"}; tone(880,.06f); break;
    case Event::MISSED: label = Message{"TRY THE NEXT ONE!", "试试下一朵！"}; break;
    case Event::FINISHED: label = Message{"GREAT PLAY! COINS, BERRIES + GROWTH.", "玩得开心！获得金币、莓果和成长。"}; tone(660,.1f); tone(880,.15f,float(millis())/1000.0f+.12f); break;
    default: break;
    }
    if (*label.en) announce(label);
    static const char *tags[] = {"none","feed","wash","pet","rest","wake","no-food","wait","evolve","harvest","buy","decorate","poor","start","catch","miss","beat","finish"};
    if (e != Event::WAIT && e != Event::NO_FOOD && e != Event::POOR) record(tags[unsigned(e)]);
}
void go_home() { garden::home(s); touching = false; silence(); announce(Message{"HOME SWEET HOME. GROWTH IS SAFE.", "回到温暖的家，成长已保存。"}); record("home"); }
void do_action(unsigned action) {
    if (action > 5) return;
    s.selection = uint8_t(action);
    Mode before = s.mode;
    event(garden::action(s, garden::Action(action)));
    if (s.mode != before) record(s.mode == Mode::GAMES ? "games-menu" : "decor-menu");
}
void start_game(unsigned which) {
    garden::start(s, uint8_t(which)); touching = false;
    announce(which ? Message{"TAP / A AS THE RING MEETS THE FLOWER!", "圆环碰到花朵时，轻触或按A！"} : Message{"DRAG BASKET / HOLD LEFT OR RIGHT!", "拖动篮子，或按住左右方向！"});
    record(which ? "bloom-start" : "catch-start");
}
void heading(const char *title) {
    clear(CREAM); rect(0,0,264,22,PURPLE); text(7,7,title,CREAM);
    // Top-right 24x20 belongs to the runtime menu.
}
void meter(int x, const char *label, unsigned value, uint16_t color) {
    text(x,36,label,INK2); rect(x+25,35,35,9,INK2); rect(x+26,36,33,7,CREAM); rect(x+26,36,int(value*33/100),7,color);
}
void critter(int x, int baseline, bool small = false) {
    int bounce = !s.sleeping && (animation_ms / 240 % 4 == 1) ? 2 : 0;
    int y = baseline - (small ? 38 : 53) - bounce;
    int w = small ? 42 : 62, h = small ? 34 : 48, left = x-w/2;
    uint16_t body = s.branch == 2 ? GOLD : s.branch == 3 ? rgb(181,173,226) : rgb(181,217,153);
    uint16_t shade = s.branch == 2 ? rgb(219,150,87) : s.branch == 3 ? PURPLE : GREEN;
    oval(x-w/2-3,baseline-3,w+6,8,rgb(132,145,114));
    if (s.stage == 2 && s.branch == 3) {
        oval(left-17,y+6,26,33,PURPLE); oval(left-14,y+9,20,23,PINK);
        oval(left+w-9,y+6,26,33,PURPLE); oval(left+w-6,y+9,20,23,PINK);
        star(left-6,y+19,CREAM); star(left+w+7,y+19,CREAM);
    }
    // Ears each have separate silhouette, inner color and motion.
    int ear_h = s.stage == 0 ? 12 : s.branch == 2 ? 23 : 30;
    int ear_w = s.branch == 2 ? 19 : 13;
    int ear_tilt = !s.sleeping && animation_ms/450%2 ? 2 : 0;
    panel(left+5-ear_tilt,y-ear_h+7,ear_w,ear_h,body);
    rect(left+9-ear_tilt,y-ear_h+12,ear_w-8,ear_h-9,PINK);
    panel(left+w-ear_w-5+ear_tilt,y-ear_h+4,ear_w,ear_h,body);
    rect(left+w-ear_w-1+ear_tilt,y-ear_h+9,ear_w-8,ear_h-9,PINK);
    panel(left,y,w,h,body);
    rect(left+5,y+h-6,w-10,4,shade);
    oval(x-15,y+h-20,30,17,CREAM);
    // Paw feet and little hands.
    panel(left+3,baseline-10,18,10,body); panel(left+w-21,baseline-10,18,10,body);
    rect(left-3,y+26,6,11,shade); rect(left+w-3,y+25,6,11,shade);
    bool blink = s.sleeping || animation_ms%4100 > 3880;
    bool happy = effect_ms && (effect == Event::PETTED || effect == Event::FED || effect == Event::EVOLVED || effect == Event::BEAT);
    int eye_y = y+18;
    if (blink) { rect(x-16,eye_y+3,9,2,INK2); rect(x+7,eye_y+3,9,2,INK2); }
    else if (happy) {
        line(x-17,eye_y+4,x-13,eye_y,INK2); line(x-13,eye_y,x-9,eye_y+4,INK2);
        line(x+9,eye_y+4,x+13,eye_y,INK2); line(x+13,eye_y,x+17,eye_y+4,INK2);
    } else {
        rect(x-16,eye_y,7,s.energy < 25 ? 5 : 10,INK2); rect(x+9,eye_y,7,s.energy < 25 ? 5 : 10,INK2);
        rect(x-15,eye_y+1,2,3,CREAM); rect(x+10,eye_y+1,2,3,CREAM);
    }
    rect(x-24,eye_y+10,8,4,PINK); rect(x+17,eye_y+10,8,4,PINK);
    rect(x-2,eye_y+11,4,3,INK2);
    if (s.food < 25) { rect(x-4,eye_y+19,8,2,INK2); rect(x-3,eye_y+17,6,2,INK2); }
    else { rect(x-1,eye_y+14,2,3,INK2); rect(x-5,eye_y+17,10,2,INK2); }
    if (s.stage == 0) { rect(x-1,y-7,3,8,DARKGREEN); rect(x-8,y-10,8,5,GREEN); rect(x+1,y-13,8,5,GREEN); }
    if (s.stage == 2 && s.branch == 1) flower(x+2,y-9,PINK);
    if (s.stage == 2 && s.branch == 2) { rect(x-8,y-4,16,5,CREAM); rect(x-5,y-7,10,3,GOLD); }
    if (s.stage == 2 && s.branch == 3) { line(x-7,y,x-13,y-9,INK2); line(x+7,y,x+13,y-9,INK2); rect(x-15,y-12,4,4,GOLD); rect(x+11,y-12,4,4,GOLD); }
    if (s.sleeping) { text(x+39,y+1,"Z",PURPLE,2); text(x+54,y-10,"z",PURPLE); }
    if (s.clean < 35) { rect(left+7,y+33,7,4,shade); rect(x+13,y+34,5,5,shade); }
    if (effect_ms) {
        int rise = int((1300-effect_ms)/90);
        if (effect == Event::PETTED || effect == Event::EVOLVED) { heart(x-42,y+15-rise,PINK); heart(x+36,y+5-rise,PINK); }
        if (effect == Event::FED) berry(x+42,y+14-rise);
        if (effect == Event::WASHED) { outline(x-43,y+16-rise,9,9,CREAM); outline(x+39,y+6-rise,12,12,SKY); outline(x+30,y-4-rise,6,6,CREAM); }
    }
}
void decor_scene() {
    if (s.decor_on & 1) { rect(23,118,18,19,rgb(188,115,97)); rect(20,117,24,5,CREAM); flower(31,105,PINK); }
    if (s.decor_on & 2) { rect(242,58,3,26,INK2); panel(232,79,24,23,GOLD); rect(238,84,12,11,CREAM); rect(234,102,20,3,INK2); }
    if (s.decor_on & 4) { oval(86,137,116,16,PURPLE); star(144,145,GOLD); }
    if (s.decor_on & 8) {
        line(52,52,233,52,INK2);
        for (int i=0;i<9;++i) { int x=55+i*20; rect(x,53,12,5,i%2?PINK:GOLD); rect(x+2,58,8,4,i%2?PINK:GOLD); rect(x+4,62,4,3,i%2?PINK:GOLD); }
    }
}
void scene() {
    if (!s.room) {
        rect(0,48,288,82,rgb(232,212,206));
        for(int x=8;x<288;x+=24) rect(x,48,2,82,rgb(219,196,198));
        rect(0,129,288,29,rgb(185,147,125));
        for(int y=132;y<158;y+=12) line(0,y,287,y,rgb(158,124,112));
        for(int x=18;x<288;x+=52) line(x,132,x,142,rgb(158,124,112));
        panel(34,59,53,49,CREAM); rect(39,64,43,39,SKY);
        rect(43,90,16,9,GREEN); rect(60,85,18,14,GREEN); rect(58,64,3,39,CREAM); rect(39,81,43,3,CREAM);
        rect(32,107,58,4,INK2);
        panel(207,112,55,25,rgb(145,118,126)); rect(212,108,45,10,PINK); rect(218,113,16,6,CREAM);
        rect(208,137,5,7,INK2); rect(256,137,5,7,INK2);
        panel(201,58,25,25,CREAM); heart(207,65,PINK);
        rect(6,145,21,8,SKY); rect(8,143,17,3,CREAM);
    } else {
        rect(0,48,288,88,SKY); rect(0,112,288,46,rgb(150,193,119));
        oval(18,54,31,24,GOLD); rect(65,63,26,7,CREAM); rect(74,59,23,8,CREAM); rect(170,72,37,7,CREAM); rect(181,67,19,9,CREAM);
        for(int x=0;x<288;x+=19) { rect(x,106,8,28,CREAM); rect(x+2,103,4,3,CREAM); }
        rect(0,114,288,4,CREAM); rect(0,126,288,4,CREAM);
        rect(7,130,64,20,rgb(133,103,87)); rect(10,132,58,4,rgb(164,124,92));
        for(int i=0;i<4;++i) { rect(18+i*13,126,2,13,DARKGREEN); berry(19+i*13,127); }
        flower(247,128,PINK); flower(269,140,GOLD); flower(221,143,rgb(189,158,216));
        for(int x=8;x<288;x+=33) { rect(x,153,2,3,GREEN); rect(x+3,150,2,6,GREEN); }
        rect(90,147,103,11,rgb(192,194,153));
        rect(93,149,18,3,CREAM); rect(127,151,20,3,CREAM); rect(163,148,24,3,CREAM);
        text(5,89,pocket::tr("BERRY PATCH", "莓果园"),DARKGREEN);
    }
    decor_scene(); critter(s.pet_x,149);
}
void home_draw() {
    heading(pocket::tr("PET GARDEN", "宠物花园")); text(164,7,pocket::tr("COINS", "金币"),CREAM); number(202,7,s.coins,CREAM);
    char buf[96]; snprintf(buf,sizeof(buf),"%s / %s",pet_name(),stage_name()); text(6,25,buf,INK2);
    int target = s.stage == 0 ? 45 : s.stage == 1 ? 120 : 999;
    rect(171,24,78,8,INK2); rect(172,25,76,6,CREAM); rect(172,25,int(s.growth*76/target),6,GREEN);
    panel(254,23,30,18,CREAM,PURPLE); center(254,27,30,pocket::tr("HELP", "帮助"),PURPLE);
    meter(5,pocket::tr("FED", "饱"),s.food,PINK); meter(67,pocket::tr("TID", "洁"),s.clean,SKY); meter(129,pocket::tr("JOY", "乐"),s.joy,GOLD); meter(191,pocket::tr("NAP", "眠"),s.energy,GREEN);
    scene();
    rect(0,158,288,17,CREAM);
    if (toast_ms || animation_ms<4000) center(2,161,284,toast.get(),INK2);
    else { snprintf(buf,sizeof(buf),pocket::tr("%u BERRIES   GROWTH %u   %s", "莓果%u  成长%u  %s"),s.snacks,s.growth,s.room ? pocket::tr("GARDEN", "花园") : pocket::tr("COZY ROOM", "小屋")); center(2,161,284,buf,INK2); }
    const char *actions[] = {pocket::tr("FEED", "喂食"),pocket::tr("WASH", "洗澡"),pocket::tr("PET", "摸摸"),pocket::tr("PLAY", "玩耍"),pocket::tr("REST", "休息"),pocket::tr("DECOR", "装饰")};
    for(int i=0;i<6;++i) {
        int x=3+i*47;
        panel(x,178,44,27,s.selection==i ? GOLD : rgb(236,220,200),s.selection==i ? PURPLE : INK2);
        center(x,186,44,actions[i]);
        if(s.selection==i) rect(x+17,176,10,2,PURPLE);
    }
    center(0,211,288,pocket::tr("<> CHOOSE  A DO  ^ ROOM  Y HELP", "<>选 A确认 ^换房 Y帮助"),PURPLE);
}
void games_draw() {
    heading(pocket::tr("PLAY TOGETHER", "一起玩耍"));
    center(0,31,288,pocket::tr("PRACTICE, EARN COINS, HELP PIP GROW", "练练手，赚金币，陪皮皮长大"));
    for(int i=0;i<2;++i) {
        int y=51+i*64; panel(10,y,268,57,s.selection==i?GOLD:rgb(236,220,200),s.selection==i?PURPLE:INK2);
        text(64,y+6,i?pocket::tr("BLOOM BEAT", "花开节拍"):pocket::tr("BERRY CATCH", "接莓果"),INK2,2);
        text(64,y+29,i?pocket::tr("8 BLOOMS / TAP WITH THE RING", "8朵花 / 跟着圆环轻触"):pocket::tr("15 SECONDS / CATCH THE BERRIES", "15秒 / 接住下落莓果"),INK2);
        if(i) { flower(34,y+22,PINK); outline(22,y+10,26,26,PURPLE); }
        else { berry(34,y+16); rect(21,y+32,29,9,GREEN); }
        char b[32]; snprintf(b,sizeof(b),pocket::tr("BEST %u", "最佳 %u"),i?s.best_rhythm:s.best_catch); text(64,y+43,b,PURPLE);
    }
    center(0,187,288,pocket::tr("EVERY FINISH: COINS + 2 BERRIES", "每次完成：金币 + 2颗莓果"));
    center(0,211,288,pocket::tr("^v CHOOSE  A START  B HOME", "^v选择 A开始 B回家"),PURPLE);
}
void game_heading(const char *label) {
    heading(label); char b[32]; snprintf(b,sizeof(b),pocket::tr("SCORE %u", "得分 %u"),s.score); text(8,29,b,INK2);
    snprintf(b,sizeof(b),pocket::tr("%us", "%u秒"),unsigned((s.last_game?11200:15000)-s.game_ms+999)/1000); text(223,29,b,PURPLE);
}
void catch_draw() {
    game_heading(pocket::tr("BERRY CATCH", "接莓果")); rect(0,43,288,142,SKY); rect(0,166,288,23,GREEN);
    for(int x=8;x<288;x+=32) { rect(x,49,19,8,DARKGREEN); rect(x+7,56,2,7,GREEN); }
    for(const garden::Drop &d:s.drops) if(d.active) berry(d.x,44+d.y/4,d.kind!=0);
    rect(s.basket_x-20,156,40,18,INK2); rect(s.basket_x-18,158,36,13,rgb(202,150,97));
    rect(s.basket_x-22,154,44,5,CREAM); rect(s.basket_x-14,161,28,2,GOLD);
    line(s.basket_x-16,152,s.basket_x-10,144,INK2); line(s.basket_x-10,144,s.basket_x+10,144,INK2); line(s.basket_x+10,144,s.basket_x+16,152,INK2);
    center(0,193,288,pocket::tr("PINK +1 / GOLD +2 / NO PENALTIES", "粉色+1 / 金色+2 / 漏接不扣分"));
    center(0,211,288,pocket::tr("HOLD <> OR DRAG BASKET  B HOME", "按住<>或拖篮子 B回家"),PURPLE);
}
void rhythm_draw() {
    game_heading(pocket::tr("BLOOM BEAT", "花开节拍"));
    rect(0,44,288,144,rgb(226,231,208));
    for(int i=0;i<8;++i) { int x=43+i*27; panel(x,52,15,13,i<int(s.beat_index)?GREEN:CREAM); }
    int delta=int(s.game_ms)-int(1500+s.beat_index*1200);
    int size=delta<0 ? 24+(-delta>1000?1000:-delta)*70/1000 : 24+delta/15;
    if(size>94) size=94;
    uint16_t ring=delta>=-230 && delta<=230 ? GREEN : PURPLE;
    outline(144-size/2,119-size/2,size,size,ring); outline(145-size/2,120-size/2,size-2,size-2,ring);
    flower(144,110,PINK); rect(143,128,3,23,DARKGREEN); rect(146,136,11,4,GREEN);
    if(s.beat_index>=8) center(0,168,288,pocket::tr("ALL EIGHT BLOOMS! WELL PLAYED!", "八朵都开啦！玩得真好！"),PURPLE);
    else if(s.beat_done) center(0,168,288,effect==Event::BEAT?pocket::tr("NICE BLOOM!", "花开得真好！"):pocket::tr("NEXT BLOOM SOON", "下一朵就要开了"),PURPLE);
    else center(0,168,288,delta>=-230 && delta<=230?pocket::tr("NOW! TAP / A", "就是现在！轻触 / A"):pocket::tr("WAIT FOR THE GREEN RING", "等待圆环变绿"),INK2);
    center(0,194,288,pocket::tr("ONE TAP PER BLOOM. TIMING +/-230 MS", "每朵轻触一次，时机范围±230毫秒"));
    center(0,211,288,pocket::tr("A OR TAP ANYWHERE  B HOME", "A或轻触任意处 B回家"),PURPLE);
}
void result_draw() {
    heading(pocket::tr("A LOVELY DAY", "美好的一天"));
    critter(144,119); char b[96];
    snprintf(b,sizeof(b),pocket::tr("%s  SCORE %u", "%s  得分%u"),s.last_game?pocket::tr("BLOOM BEAT", "花开节拍"):pocket::tr("BERRY CATCH", "接莓果"),s.score); center(0,130,288,b,INK2);
    snprintf(b,sizeof(b),pocket::tr("+%u COINS  +2 BERRIES  +%u GROWTH", "+%u金币 +2莓果 +%u成长"),s.reward,10+s.score); center(0,147,288,b,PURPLE);
    snprintf(b,sizeof(b),pocket::tr("%s / %s / GROWTH %u", "%s / %s / 成长%u"),pet_name(),stage_name(),s.growth); center(0,163,288,b,INK2);
    panel(22,180,115,26,GOLD); center(22,190,115,pocket::tr("A PLAY AGAIN", "A再玩一次"));
    panel(151,180,115,26,SKY); center(151,190,115,pocket::tr("B HOME", "B回家"));
    center(0,211,288,pocket::tr("TOUCH A BUTTON / REWARDS ARE SAVED", "轻触按钮 / 奖励已保存"),PURPLE);
}
void decor_draw() {
    heading(pocket::tr("LITTLE HOME SHOP", "温馨小屋商店")); char b[48]; snprintf(b,sizeof(b),pocket::tr("COINS %u / OWNED ITEMS CAN TOGGLE", "金币%u / 已购装饰可开关"),s.coins); center(0,31,288,b);
    const char *names[]={pocket::tr("BLOOM POT", "花朵盆栽"),pocket::tr("HONEY LANTERN", "蜂蜜灯笼"),pocket::tr("STARRY RUG", "星空地毯"),pocket::tr("FESTIVAL FLAGS", "节日彩旗")};
    for(int i=0;i<4;++i) {
        int y=48+i*35; bool own=s.decor_owned&(1u<<i), on=s.decor_on&(1u<<i);
        panel(12,y,264,30,s.selection==i?GOLD:rgb(236,220,200),s.selection==i?PURPLE:INK2);
        text(23,y+9,names[i],INK2);
        if(own) text(229,y+9,on?pocket::tr("ON", "开"):pocket::tr("OFF", "关"),DARKGREEN);
        else { snprintf(b,sizeof(b),pocket::tr("%u C", "%u金"),garden::DECOR_COST[i]); text(228,y+9,b,PURPLE); }
    }
    if(toast_ms) center(0,194,288,toast.get(),PURPLE);
    center(0,211,288,pocket::tr("^v CHOOSE  A BUY / TOGGLE  B HOME", "^v选择 A购买/开关 B回家"),PURPLE);
}
void help_draw() {
    heading(help_page?pocket::tr("GROW YOUR OWN FRIEND", "陪伴小伙伴成长"):pocket::tr("WELCOME TO PET GARDEN", "欢迎来到宠物花园"));
    const char *p0[]={pocket::tr("PIP IS YOUR LITTLE GARDEN FRIEND.", "皮皮是你的花园小伙伴。"),pocket::tr("FEED BERRIES, WASH, PET AND REST.", "喂莓果、洗澡、摸摸和休息。"),pocket::tr("TOUCH PIP FOR A PAT. TOUCH BUTTONS.", "摸摸皮皮，或轻触照顾按钮。"),pocket::tr("LEFT/RIGHT: CHOOSE CARE. A: DO IT.", "左右选择照顾方式，A确认。"),pocket::tr("UP/DOWN OR SELECT: ROOM / GARDEN.", "上下或SELECT：切换小屋和花园。"),pocket::tr("X: TWO MINIGAMES. Y: THIS GUIDE.", "X：两个小游戏。Y：本指南。"),pocket::tr("PLAY MENUS: UP/DOWN + A. B: HOME.", "游戏菜单：上下选、A开始、B回家。"),pocket::tr("CATCH: HOLD LEFT/RIGHT OR DRAG.", "接莓果：按住左右或拖动篮子。"),pocket::tr("BLOOM: TAP / A WITH THE GREEN RING.", "花开：圆环变绿时轻触或按A。"),pocket::tr("START / TOP RIGHT: PAUSE & EXIT.", "START或右上角：暂停与退出。")};
    const char *p1[]={pocket::tr("BUD -> SPROUT: 45 GROWTH.", "花苞到嫩芽：45成长。"),pocket::tr("SPROUT -> BLOOM: 120 GROWTH.", "嫩芽到盛放：120成长。"),pocket::tr("CARE: MOSSBUN. PLAY: SUNFOX.", "照顾：苔苔兔。玩耍：暖阳狐。"),pocket::tr("PATS + REST: MOONMOTH. MOST WINS.", "摸摸和休息：月光蛾。最多者胜。"),pocket::tr("PATS: +3 / 3 SEC. FEED/WASH: +7.", "摸摸：每3秒+3。喂食/洗澡：+7。"),pocket::tr("GAMES: +10 + SCORE. ABOUT 2-5 MIN.", "游戏：+10和得分。约2至5分钟。"),pocket::tr("BERRIES REGROW EVERY 30 PLAY SEC.", "每游玩30秒长出一颗莓果。"),pocket::tr("NO OFFLINE DECAY. NOBODY CAN DIE.", "离线不衰退，小伙伴不会死亡。"),pocket::tr("DECOR: BUY WITH COINS, THEN TOGGLE.", "装饰：金币购买，之后自由开关。"),pocket::tr("RESET RETURNS HOME; GROWTH IS SAFE.", "重开会回家，成长保留。")};
    const char **lines=help_page?p1:p0;
    for(int i=0;i<10;++i) text(9,35+i*16,lines[i],INK2);
    center(0,211,288,help_page?pocket::tr("< PAGE 1    A / Y / B HOME", "<前页 A/Y/B回家"):pocket::tr("A / > NEXT PAGE    B HOME", "A/>下一页 B回家"),PURPLE);
}
int clamp_basket(int x) { return x<20?20:x>268?268:x; }
}

namespace game {
const char *title() { return pocket::tr("Pet Garden", "宠物花园"); }
const char *app_id() { return "pxa-pet-garden"; }
void init() {
    garden::fresh(s); animation_ms=save_ms=effect_ms=0; toast_ms=4500; touching=false;
    toast = {"HELLO! PET PIP OR CHOOSE A CARE BUTTON.", "你好！摸摸皮皮，或选择照顾按钮。"};
    log("pet-garden init original pixel pet; Y guide"); redraw();
}
void reset() { go_home(); }
void key(Key k) {
    if(k==Key::B) { go_home(); return; }
    if(k==Key::Y) { if(s.mode==Mode::HELP) go_home(); else { s.mode=Mode::HELP; help_page=0; record("help"); } return; }
    switch(s.mode) {
    case Mode::HOME:
        if(k==Key::LEFT) s.selection=uint8_t((s.selection+5)%6);
        else if(k==Key::RIGHT) s.selection=uint8_t((s.selection+1)%6);
        else if(k==Key::UP || k==Key::DOWN || k==Key::SELECT) { s.room^=1; record("room"); }
        else if(k==Key::A) do_action(s.selection);
        else if(k==Key::X) do_action(3);
        else if(k==Key::L || k==Key::R) event(garden::action(s,garden::Action::PET));
        break;
    case Mode::GAMES:
        if(k==Key::UP || k==Key::DOWN || k==Key::LEFT || k==Key::RIGHT) s.selection^=1;
        else if(k==Key::A) start_game(s.selection);
        break;
    case Mode::CATCH:
        if(k==Key::LEFT) s.basket_x=int16_t(clamp_basket(s.basket_x-8));
        if(k==Key::RIGHT) s.basket_x=int16_t(clamp_basket(s.basket_x+8));
        break;
    case Mode::RHYTHM: if(k==Key::A) event(garden::beat(s)); break;
    case Mode::DECOR:
        if(k==Key::UP || k==Key::LEFT) s.selection=uint8_t((s.selection+3)%4);
        else if(k==Key::DOWN || k==Key::RIGHT) s.selection=uint8_t((s.selection+1)%4);
        else if(k==Key::A) event(garden::decorate(s,s.selection));
        break;
    case Mode::RESULT: if(k==Key::A) start_game(s.last_game); break;
    case Mode::HELP:
        if(k==Key::A || k==Key::RIGHT) { if(help_page) go_home(); else help_page=1; }
        if(k==Key::LEFT) help_page=0;
        break;
    }
    redraw();
}
void pointer(Phase phase,int x,int y) {
    if(phase==Phase::CANCEL) { touching=false; return; }
    if(phase==Phase::DOWN) {
        touching=true; touch_x=x; touch_y=y;
        if(s.mode==Mode::CATCH && y>=44 && y<190) s.basket_x=int16_t(clamp_basket(x));
        if(s.mode==Mode::RHYTHM && y>=22) event(garden::beat(s));
        redraw(); return;
    }
    if(phase==Phase::MOVE) {
        if(touching && s.mode==Mode::CATCH) { s.basket_x=int16_t(clamp_basket(x)); redraw(); }
        return;
    }
    if(phase!=Phase::UP || !touching) return;
    touching=false;
    if(x<0 || x>=288 || y<22 || y>=224 || x-touch_x>18 || touch_x-x>18 || y-touch_y>18 || touch_y-y>18) return;
    switch(s.mode) {
    case Mode::HOME:
        if(x>=252 && y>=23 && y<44) { s.mode=Mode::HELP; help_page=0; record("help"); }
        else if(y>=177 && y<207) { unsigned i=unsigned(x)/47; if(i<6) do_action(i); }
        else if(y>=48 && y<158) {
            if(x<72) { s.room^=1; record("room"); }
            else if(x>=s.pet_x-52 && x<=s.pet_x+52) event(garden::action(s,garden::Action::PET));
            else { s.pet_x=int16_t(x<84?84:x>204?204:x); record("move-pet"); }
        } else if(y>=209) { s.mode=Mode::HELP; help_page=0; record("help"); }
        break;
    case Mode::GAMES: if(y>=51 && y<172) start_game(y>=115?1:0); break;
    case Mode::DECOR: if(y>=48 && y<188) { s.selection=uint8_t((y-48)/35); event(garden::decorate(s,s.selection)); } break;
    case Mode::RESULT: if(y>=180 && y<207) { if(x<144) start_game(s.last_game); else go_home(); } break;
    case Mode::HELP: if(help_page) go_home(); else help_page=1; break;
    default: break;
    }
    redraw();
}
void tick(uint32_t ms) {
    if(ms>100) ms=100;
    animation_ms+=ms; save_ms+=ms;
    toast_ms=toast_ms>ms?toast_ms-ms:0;
    effect_ms=effect_ms>ms?effect_ms-ms:0;
    if(s.mode==Mode::CATCH) {
        int direction=int(held(Key::RIGHT))-int(held(Key::LEFT));
        if(direction) s.basket_x=int16_t(clamp_basket(s.basket_x+direction*int(ms*240/1000)));
    }
    Event e=garden::step(s,ms); event(e);
    if(save_ms>=5000) { save_ms=0; save(); }
    // 10 fps cozy animation; minigames use the runtime's bounded foreground tick.
    if(s.mode==Mode::CATCH || s.mode==Mode::RHYTHM || animation_ms/100!=(animation_ms-ms)/100) redraw();
}
void draw() {
    switch(s.mode) {
    case Mode::HOME: home_draw(); break;
    case Mode::GAMES: games_draw(); break;
    case Mode::CATCH: catch_draw(); break;
    case Mode::RHYTHM: rhythm_draw(); break;
    case Mode::DECOR: decor_draw(); break;
    case Mode::RESULT: result_draw(); break;
    case Mode::HELP: help_draw(); break;
    }
}
size_t serialize(uint8_t *data,size_t capacity) { return garden::encode(s,data,capacity); }
void load(const uint8_t *data,size_t size) {
    if(garden::decode(s,data,size)) { touching=false; announce(Message{"WELCOME BACK! YOUR GARDEN IS SAFE.", "欢迎回来！花园已保存。"}); record("loaded"); }
    else log("pet-garden rejected corrupt or unsupported save");
}
}
