// SPDX-License-Identifier: MIT
#include "../common/pocket-play/runtime.hpp"
#include "../common/pocket-play/service-hooks.hpp"
#include "../common/pocket-studio/studio.hpp"
#include "animation.hpp"
#include "core.hpp"
#include "layout.hpp"
#include "dialogue.hpp"
#include "chat.hpp"
#include "pxa_window.h"
#include "icons.hpp"
#include "room_seaside.inc"
#include "room_moonlight.inc"
#include "room_aquarium.inc"
#include <stdio.h>
#include <stdlib.h>

namespace {
using namespace pocket;
using namespace fish;
State state;
enum class View { HOME, CLIPS, SETTINGS, ABOUT };
View view = View::HOME;
art::Clip clip = art::IDLE;
unsigned selection = 0, page = 0;
uint32_t clip_ms, auto_ms, save_ms, walk_ms, cooldown, toast_ms;
int pressed = -1, down_x, down_y, original_position, walk_direction = 1;
bool dragging;
bool dock_visible = true, dock_expanded;
const char *message_en = "A little whale, a little company.", *message_zh = "今天也要和你待在一起。";
struct Theme {
    uint16_t ink, muted, paper, edge, accent, tint, shadow;
};
constexpr Theme SEASIDE = {rgb(56, 67, 85), rgb(117, 127, 139), rgb(255, 248, 232),
    rgb(213, 195, 166), rgb(93, 145, 158), rgb(224, 239, 233), rgb(44, 48, 65)};
constexpr Theme MOONLIGHT = {rgb(241, 234, 249), rgb(181, 177, 204), rgb(41, 47, 76),
    rgb(112, 108, 147), rgb(192, 171, 216), rgb(71, 77, 111), rgb(11, 18, 37)};
constexpr Theme AQUARIUM = {rgb(45, 77, 84), rgb(105, 139, 139), rgb(242, 252, 241),
    rgb(163, 202, 185), rgb(77, 143, 145), rgb(209, 237, 228), rgb(26, 61, 69)};

bool night() { return state.night || state.sleeping; }
const Theme &theme() { return night() ? MOONLIGHT : state.room == 2 ? AQUARIUM : SEASIDE; }
Controls controls() { return fish::controls(WIDTH, HEIGHT, pocket::dp(44)); }
uint16_t ink() { return theme().ink; }
const char *clip_name(unsigned id) {
    const auto &a = art::animations[id];
    return tr(a.en, a.zh);
}
void label(int x, int y, int width, const char *value, uint16_t color, unsigned align = 1) {
    studio::label(x, y, width, value, color, 1, align);
}
void panel(int x, int y, int width, int height, int radius = 10, int opacity = 232) {
    const auto &t = theme();
    studio::round_alpha(x, y + 3, width, height, radius, t.shadow, 45);
    studio::round_alpha(x, y, width, height, radius, t.paper, opacity);
    studio::round_outline(x, y, width, height, radius, studio::mix(t.edge, t.ink, 40), 220);
    studio::round_alpha(x + radius, y + 1, width - radius * 2, 1, 0,
        studio::mix(t.paper, 0xffff, 100), 130);
}
void chevron(int x, int y, uint16_t color, bool right = false) {
    if (right) { line(x, y - 3, x + 3, y, color); line(x + 3, y, x, y + 3, color); }
    else { line(x - 3, y - 1, x, y + 2, color); line(x, y + 2, x + 3, y - 1, color); }
}
void chat_button(int x, int y, int size) {
    panel(x, y, size, size, size / 2, 226);
    const int left = x + (size - 16) / 2, top = y + (size - 13) / 2;
    studio::round(left, top, 16, 11, 4, theme().tint);
    studio::round_outline(left, top, 16, 11, 4, theme().accent, 240);
    line(left + 4, top + 10, left + 4, top + 13, theme().accent);
    line(left + 4, top + 13, left + 8, top + 10, theme().accent);
    for (int i = 0; i < 3; ++i) studio::disc(left + 4 + i * 4, top + 5, 1, theme().accent);
}
void ai_button(int x, int y, int height) {
    const unsigned stage = chat::voice_stage();
    panel(x, y, height, height, height / 2, stage ? 250 : 226);
    if (stage) {
        studio::round_alpha(x + 2, y + 2, height - 4, height - 4, height / 2 - 2, theme().tint, 240);
        studio::round_outline(x, y, height, height, height / 2, theme().accent, 255);
        studio::disc(x + height - 3, y + 3, 3, theme().paper);
        studio::disc(x + height - 3, y + 3, 2, theme().accent);
    }
    const int cx = x + height / 2, cy = y + height / 2;
    studio::round(cx - 3, cy - 7, 6, 10, 3, theme().accent);
    line(cx - 6, cy - 1, cx - 6, cy + 3, theme().accent);
    line(cx + 6, cy - 1, cx + 6, cy + 3, theme().accent);
    line(cx - 6, cy + 3, cx, cy + 6, theme().accent);
    line(cx, cy + 6, cx + 6, cy + 3, theme().accent);
    line(cx, cy + 6, cx, cy + 8, theme().accent);
}
void notice(const char *en, const char *zh) {
    message_en = en; message_zh = zh; toast_ms = 5000; redraw();
}
void record(const char *event, bool persist = true) {
    char line[180];
    snprintf(line, sizeof(line), "FatFish: %s food=%u joy=%u energy=%u bond=%u sleep=%u touches=%u position=%d room=%u",
        event, state.food, state.joy, state.energy, state.affection, unsigned(state.sleeping), state.touches, state.position, state.room);
    log(line);
    if (persist) save();
    redraw();
}
void play(art::Clip next) {
    clip = next; clip_ms = 0; auto_ms = 0;
    animation::play(next); redraw();
}
void home() {
    view = View::HOME; selection = 0; pressed = -1; dragging = false;
    dock_visible = true; dock_expanded = false;
    redraw();
}
void care(Action action, bool automatic = false) {
    if (cooldown && action != Action::REST && !(action == Action::PET && state.sleeping)) return;
    Result result = act(state, action);
    if (automatic && result == Result::SLEPT) state.auto_sleep = true;
    switch (result) {
    case Result::PETTED: {
        const art::Clip reactions[] = {art::HAPPY, art::TICKLE, art::SHY, art::WAVE};
        play(reactions[pocket::random() % 4]);
        notice("Hehe! Stay a little longer?", "嘿嘿，再陪我一会儿嘛。");
        cooldown = 1000; record("pet"); break;
    }
    case Result::FED:
        play(pocket::random() % 3 ? art::FEED : art::HOTPOT);
        notice(automatic ? "Time for a little snack!" : "Yum! Thank you for the snack.",
               automatic ? "肚子咕咕叫，我去吃点东西啦。" : "啊呜！吃饱才有力气陪你。");
        cooldown = 3000; record(automatic ? "auto-feed" : "feed"); break;
    case Result::PLAYED: {
        const art::Clip games[] = {art::CUBE, art::DANCE, art::BUBBLES};
        play(games[pocket::random() % 3]);
        notice(automatic ? "A little game will cheer me up." : "Let's play! Happiness +15",
               automatic ? "找点好玩的，自己也要开开心心。" : "一起玩！开心 +15");
        cooldown = 4000; record(automatic ? "auto-play" : "play"); break;
    }
    case Result::SLEPT:
        play(art::SLEEP_START);
        notice(automatic ? "Feeling sleepy. I'll wake up rested." : "Good night. Tap me to wake up.",
               automatic ? "有点困啦，睡饱了再来找你。" : "晚安，轻轻点我就能叫醒我。");
        record(automatic ? "auto-sleep" : "sleep"); break;
    case Result::WOKE:
        play(art::WAKE); notice("I'm awake! Hello again.", "睡醒啦！又见到你了。");
        record(automatic ? "auto-wake" : "wake"); break;
    case Result::FULL: notice("My tummy is full. Let's play!", "肚子已经圆滚滚了，去玩吧！"); break;
    case Result::SLEEPING: notice("Shhh... tap me or Wake first.", "呼呼大睡中，点我或点唤醒。"); break;
    case Result::TIRED: notice("A little tired. Time for a nap?", "有点困啦，先让我睡一觉吧。"); break;
    }
}
void button(int x, int y, int width, int height, const char *value, bool selected = false) {
    panel(x, y, width, height, 7);
    if (selected) {
        studio::round_alpha(x + 2, y + 2, width - 4, height - 4, 6, theme().tint, 224);
        studio::round(x + 5, y + height / 2 - 3, 2, 6, 1, theme().accent);
    }
    label(x + 8, y + (height - 10) / 2, width - 16, value, ink());
}
const studio::Image &room_image(unsigned room) {
    const studio::Image *rooms[] = {&room_seaside_image, &room_moonlight_image, &room_aquarium_image};
    return *rooms[room];
}
unsigned active_room() { return night() ? 1 : state.room; }
RoomLayout room_layout() {
    const Rug rugs[] = {room_seaside_rug, room_moonlight_rug, room_aquarium_rug};
    const auto &room = room_image(active_room());
    return fish::room_layout(WIDTH, HEIGHT, room.width, room.height, rugs[active_room()]);
}
void backdrop(int darkness = 0) {
    const auto &room = room_image(active_room());
    const auto scene = room_layout();
    studio::picture_region(0, 0, WIDTH, HEIGHT, room,
        scene.sx, scene.sy, scene.sw, scene.sh, darkness);
}
void heading(const char *title) {
    backdrop(20);
    studio::round_alpha(0, 0, WIDTH, HEIGHT, 0, theme().paper, 185);
    const auto top = fish::header(WIDTH);
    panel(top.x, top.y, top.width, top.height, 11, 226);
    icon(0, top.x + 7, top.y + 7, 14);
    label(top.x + 27, top.y + 9, top.width - 34, title, ink(), 0);
}
void scenery() {
    static studio::RasterCache background;
    if (!background.restore_frame(active_room())) {
        backdrop();
        background.capture();
    }
    const auto scene = room_layout();
    const int floor = scene.rug_y, height = scene.pet_height;
    const int center = scene.foot_x(state.position);
    for (int row = -3; row <= 3; ++row) {
        const int half = int(height * .22f * sqrtf(1 - row * row / 16.0f));
        studio::round_alpha(center - half, floor + row, half * 2, 1, 0,
            theme().shadow, 42 - abs(row) * 8);
    }
    // All clips share the idle sprite's foot anchor. Animated jumps retain their motion.
    animation::draw(scene.pet_x(state.position), scene.pet_y(), scene.pet_width, height);
    if (state.sleeping) label(center + 33, floor - height + 28 - int(millis() / 700 % 3), 35, "z Z", theme().accent);
    if (!state.sleeping && clip_ms < 1100 && (clip == art::HAPPY || clip == art::TICKLE || clip == art::SHY)) {
        const int rise = int(clip_ms / 140);
        icon(0, center - height / 3, floor - height / 2 - rise, 10);
        studio::sparkle(center + height / 3, floor - height * 2 / 3 - rise, theme().accent);
    }
}
void dialogue(const char *message, int bottom) {
    const int line_height = std::max(13, pocket::sp(14));
    const unsigned lines = unsigned(std::clamp((bottom - 71) / line_height, 1, 3));
    const auto text = wrap_dialogue(message, std::min(175, WIDTH - 67), pocket::sp(12), lines);
    if (!text.count) return;
    const auto bubble = bubble_layout(WIDTH, room_layout(), state.position, bottom, text, line_height);
    const auto &t = theme();
    const uint16_t edge = studio::mix(t.edge, t.ink, 40);
    const int tail_y = bubble.y + bubble.height - 2;
    for (int row = 0; row < 8; ++row) {
        const int left = bubble.tail_x - 7 + row / 2;
        const int width = 15 - row * 2;
        studio::round_alpha(left, tail_y + row + 3, width, 2, 0, t.shadow, 45);
        studio::round_alpha(left, tail_y + row, width, 2, 0, edge, 220);
        if (width > 2) studio::round(left + 1, tail_y + row, width - 2, 2, 0, t.paper);
    }
    panel(bubble.x, bubble.y, bubble.width, bubble.height, 11, 256);
    studio::round(bubble.tail_x - 5, tail_y, 9, 2, 0, t.paper);
    studio::disc(bubble.x + 18, bubble.y + bubble.height / 2, 9, t.tint);
    icon(state.sleeping ? 3 : 0, bubble.x + 12, bubble.y + bubble.height / 2 - 6, 12);
    for (unsigned i = 0; i < text.count; ++i)
        label(bubble.x + 33, bubble.y + 10 + int(i) * line_height, bubble.width - 43, text.lines[i], ink(), 0);
}
void home_draw() {
    scenery();
    const auto hud = fish::header(WIDTH);
    panel(hud.x, hud.y, hud.width, hud.height, 11, 226);
    const unsigned status_icons[] = {1, 7, 8, 0};
    const unsigned values[] = {state.food, state.joy, state.energy, state.affection};
    const uint16_t colors[] = {rgb(220, 176, 111), rgb(220, 143, 165), rgb(118, 172, 163), rgb(178, 149, 195)};
    const int status_cell = (hud.width - 10) / 4, status_icon = std::clamp(pocket::dp(12), 10, 16);
    for (unsigned i = 0; i < 4; ++i) {
        const int x = 17 + int(i) * status_cell;
        if (i) rect(x - 3, 14, 1, 12, studio::mix(theme().edge, theme().paper, 140));
        icon(status_icons[i], x, 19 - status_icon / 2, status_icon);
        studio::round_alpha(x, 29, status_cell - 6, 2, 1, theme().edge, 115);
        if (values[i]) studio::round(x, 29, std::max(2, int(values[i] * (status_cell - 6) / 100)), 2, 1, colors[i]);
        char value[8]; snprintf(value, sizeof(value), "%u", values[i]);
        label(x + status_icon + 2, 15, status_cell - status_icon - 3, value, ink(), 0);
    }
    const char *status = toast_ms ? tr(message_en, message_zh) : state.sleeping
        ? tr("Resting... tap me to wake up.", "睡得香香，精力正在慢慢恢复。")
        : tr("Stay with me. Pat my head?", "今天也要陪着你呀，摸摸我的头吧。");
    if (animation::failed()) status = tr("Animation unavailable. See About.", "动画读取失败，请检查资源包。");
    if (chat::voice_mode()) {
        if (!toast_ms) status = chat::voice_stage() == 1 ? tr("I'm listening. Just talk to me.", "我在听，直接和我说话吧。") :
                               tr("Let me think...", "让我想一想……");
        if (chat::voice_stage() == 3 && chat::spoken_reply()) status = chat::spoken_reply();
    }
    const auto bar = controls();
    const bool compact = bar.columns == 3 && !dock_expanded;
    const int top = compact ? bar.y(3) : bar.top;
    if (toast_ms || millis() < 5000 || animation::failed() || chat::voice_mode())
        dialogue(status, dock_visible ? top : HEIGHT - 12);
    if (!dock_visible) {
        chat_button(12, HEIGHT - 46, 34);
        ai_button(52, HEIGHT - 46, 34);
        panel(WIDTH - 46, HEIGHT - 46, 34, 34, 17, 218);
        icon(6, WIDTH - 39, HEIGHT - 39, 20);
        return;
    }
    // A single floating dock overlays the room. Narrow screens reveal the
    // remaining actions on demand instead of reserving a permanent second row.
    panel(9, top, WIDTH - 18, HEIGHT - 12 - top, 13, 223);
    for (unsigned i = 0; i < 6; ++i) {
        if (compact && i >= 3) break;
        const unsigned action = compact && i == 2 ? 6 : i;
        const int x = bar.x(i);
        const int y = compact ? bar.y(3) : bar.y(i);
        const bool down = pressed == int(action);
        const bool selected = selection == i && action != 6;
        const int ring = std::min(bar.width - 7, bar.height - 7);
        if (selected || down) {
            studio::round_alpha(x + (bar.width - ring) / 2, y + (bar.height - ring) / 2,
                ring, ring, ring / 2, theme().tint, down ? 245 : 185);
            studio::round_outline(x + (bar.width - ring) / 2, y + (bar.height - ring) / 2,
                ring, ring, ring / 2, theme().accent, down ? 190 : 105);
            studio::round(x + bar.width / 2 - 2, y + bar.height - 5, 4, 2, 1, theme().accent);
        }
        icon(action, x + (bar.width - bar.icon) / 2, y + (bar.height - bar.icon) / 2 + int(down),
            bar.icon, state.sleeping);
    }
    panel(WIDTH - 38, top - 25, 24, 20, 9, 218);
    chevron(WIDTH - 26, top - 16, theme().muted);
    chat_button(14, top - 27, 24);
    ai_button(44, top - 27, 24);
}
void clips_draw() {
    heading(tr("ANIMATION COLLECTION", "大肥鱼的动作收藏"));
    char title[60]; snprintf(title, sizeof(title), tr("Little moments  /  %02u - %02u", "陪伴的小瞬间  /  %02u - %02u"), page * 8 + 1, page * 8 + 8);
    label(10, 35, WIDTH - 20, title, theme().muted);
    int cell = (WIDTH - 28) / 2, row_h = (HEIGHT - 92) / 4;
    const unsigned symbols[] = {9, 0, 7, 0, 0, 1, 1, 2, 2, 7, 8, 4, 3, 3, 9, 2};
    for (unsigned i = 0; i < 8; ++i) {
        const unsigned id = page * 8 + i;
        const int x = 12 + int(i % 2) * (cell + 4), y = 53 + int(i / 2) * row_h;
        panel(x, y, cell, row_h - 4, 7);
        if (selection == i) studio::round_alpha(x + 2, y + 2, cell - 4, row_h - 8, 6, theme().tint, 200);
        const int icon_size = std::min(18, row_h - 12);
        icon(symbols[id], x + 6, y + (row_h - 4 - icon_size) / 2, icon_size);
        if (selection == i) studio::disc(x + cell - 7, y + 6, 1, theme().accent);
        const int line_height = std::max(13, pocket::sp(14));
        const auto name = wrap_dialogue(clip_name(id), cell - 33, pocket::sp(12));
        const unsigned lines = std::min(2u, name.count);
        const int text_y = y + std::max(2, (row_h - 4 - int(lines) * line_height) / 2) + 2;
        for (unsigned line = 0; line < lines; ++line)
            label(x + 28, text_y + int(line) * line_height, cell - 33, name.lines[line], ink(), 0);
    }
    button(12, HEIGHT - 35, 70, 25, tr("Back", "返回"));
    button(WIDTH / 2 - 35, HEIGHT - 35, 70, 25, tr("< Page >", "换一页"));
    button(WIDTH - 82, HEIGHT - 35, 70, 25, tr("Play", "播放"), true);
}
int settings_step() { return std::min(32, (HEIGHT - 93) / 5); }
void settings_draw() {
    heading(tr("A COZY LITTLE ROOM", "房间与陪伴"));
    const char *rooms[] = {tr("Seaside", "晴日海景"), tr("Moonlight", "月光卧室"), tr("Underwater", "海底观景室")};
    const char *names[] = {
        rooms[state.room],
        tr("Wander", "自由漫步"), tr("Surprises", "随机小动作"),
        tr("Self-care", "自动照顾"), tr("Credits", "素材署名")};
    const unsigned symbols[] = {9, 2, 7, 0, 4};
    const int row = settings_step(), height = row - 5;
    for (unsigned i = 0; i < 5; ++i) {
        const int y = 45 + int(i) * row;
        panel(22, y, WIDTH - 44, height, 8);
        if (selection == i) studio::round_alpha(24, y + 2, WIDTH - 48, height - 4, 7, theme().tint, 150);
        if (i == 0) {
            studio::round(28, y + (height - 15) / 2, 20, 15, 3, theme().edge);
            studio::picture(29, y + (height - 12) / 2, 18, 12, room_image(state.room));
        } else icon(symbols[i], 29, y + (height - 15) / 2, 15);
        label(51, y + (height - 10) / 2, WIDTH - 131, names[i], ink(), 0);
        if (i >= 1 && i <= 3) {
            const bool enabled = i == 1 ? state.wander : i == 2 ? state.autoplay : state.autocare;
            studio::round(WIDTH - 64, y + (height - 13) / 2, 30, 13, 7, enabled ? theme().accent : theme().edge);
            studio::disc(WIDTH - (enabled ? 41 : 57), y + height / 2, 4, theme().paper);
        } else chevron(WIDTH - 41, y + height / 2, theme().muted, true);
    }
    char time[72]; snprintf(time, sizeof(time), tr("Together %um / %u pats", "已陪伴 %u 分钟 / 摸摸 %u 次"), state.seconds / 60, state.touches);
    label(10, HEIGHT - 44, WIDTH - 20, time, theme().muted);
    button(22, HEIGHT - 28, WIDTH - 44, 22, tr("Back", "返回房间"));
}
void about_draw() {
    heading(tr("ABOUT FAT FISH", "关于大肥鱼"));
    panel(12, 43, WIDTH - 24, HEIGHT - 83, 10, 235);
    const char *lines[] = {tr("Fat Fish / 0.8.2", "大肥鱼 PXA 版 / 0.8.2"),
        "PC2005-cloud/dsh-pet",
        tr("Original / Shangshanwuxing", "溟月原型 / 上善无形"),
        tr("Whale maid / ZipZipPipe", "鲸鱼娘设计 / ZipZipPipe"),
        "CC BY-NC-SA 4.0",
        tr("PXA code / lucinhu / MIT", "PXA 适配代码 / lucinhu / MIT"),
        tr("Progress autosaves every 15 seconds.", "每 15 秒及照顾后自动保存进度。")};
    const int row_height = (HEIGHT - 95) / 7;
    for (unsigned i = 0; i < 7; ++i) label(18, 49 + int(i) * row_height, WIDTH - 36, lines[i], i == 0 ? theme().accent : ink());
    button(22, HEIGHT - 30, WIDTH - 44, 24, tr("Back", "返回"));
}
void activate(unsigned id) {
    if (view == View::HOME) {
        if (id == 9) {
            if (state.sleeping) notice("Wake me up to chat.", "先叫醒我，再一起聊天吧。");
            else chat::toggle_voice();
            redraw(); return;
        }
        if (id == 8) { chat::open(); return; }
        if (id == 7) { dock_visible = !dock_visible; dock_expanded = false; }
        if (id == 6) dock_expanded = !dock_expanded;
        if (id < 4) { selection = id; care(Action(id)); dock_expanded = false; }
        if (id == 4) { chat::stop_voice(); view = View::CLIPS; selection = page = 0; }
        if (id == 5) { chat::stop_voice(); view = View::SETTINGS; selection = 0; }
    } else if (view == View::CLIPS) {
        if (id == 20) home();
        else if (id == 21) { page ^= 1; selection = 0; }
        else {
            if (id < 8) selection = id;
            if (id < 8 || id == 22) {
                if (state.sleeping) {
                    home(); notice("Wake me before choosing a clip.", "先叫醒我，再一起选小动作吧。");
                } else {
                    const auto chosen = art::Clip(page * 8 + selection);
                    home(); play(chosen); notice(art::animations[chosen].en, art::animations[chosen].zh);
                    log("FatFish: preview");
                }
            }
        }
    } else if (view == View::SETTINGS) {
        if (id == 20) home();
        else if (id == 4) { view = View::ABOUT; selection = 0; }
        else {
            selection = id;
            if (id == 0) { state.room = uint8_t((state.room + 1) % 3); state.night = state.room == 1; }
            if (id == 1) state.wander = !state.wander;
            if (id == 2) state.autoplay = !state.autoplay;
            if (id == 3) state.autocare = !state.autocare;
            record("settings");
        }
    } else { view = View::SETTINGS; selection = 4; }
    redraw();
}
int hit(int x, int y) {
    if (x < 10 || x >= WIDTH - 10) return -1;
    if (view == View::HOME) {
        const auto bar = controls();
        if (!dock_visible) {
            if (x >= 12 && x < 46 && y >= HEIGHT - 46 && y < HEIGHT - 12) return 8;
            if (x >= 52 && x < 86 && y >= HEIGHT - 46 && y < HEIGHT - 12) return 9;
            return x >= WIDTH - 48 && y >= HEIGHT - 48 ? 7 : y >= 36 ? 30 : -1;
        }
        const bool compact = bar.columns == 3 && !dock_expanded;
        const int top = compact ? bar.y(3) : bar.top;
        if (x >= 14 && x < 38 && y >= top - 27 && y < top - 3) return 8;
        if (x >= 44 && x < 68 && y >= top - 27 && y < top - 3) return 9;
        if (x >= WIDTH - 38 && x < WIDTH - 14 && y >= top - 25 && y < top - 5) return 7;
        if (compact && y >= bar.y(3) && y < bar.y(3) + bar.height) {
            for (unsigned i = 0; i < 3; ++i)
                if (x >= bar.x(i) && x < bar.x(i) + bar.width) return i == 2 ? 6 : int(i);
        } else if (!compact) {
            const int button = bar.hit(x, y);
            if (button >= 0) return button;
        }
        return y >= 36 && y < top - 25 ? 30 : -1;
    }
    if (view == View::CLIPS) {
        int cell = (WIDTH - 28) / 2, row_h = (HEIGHT - 92) / 4;
        if (x >= 12 && x < WIDTH - 12 && y >= 53 && y < 53 + 4 * row_h) {
            int col = (x - 12) / (cell + 4), row = (y - 53) / row_h;
            return col < 2 && (x - 12) % (cell + 4) < cell && (y - 53) % row_h < row_h - 4 ? row * 2 + col : -1;
        }
        if (y >= HEIGHT - 35 && y < HEIGHT - 10) {
            if (x < 82) return 20;
            if (x >= WIDTH / 2 - 35 && x < WIDTH / 2 + 35) return 21;
            if (x >= WIDTH - 82) return 22;
        }
    }
    if (view == View::SETTINGS) {
        const int row = settings_step();
        if (x >= 22 && x < WIDTH - 22 && y >= 45 && y < 45 + row * 5 && (y - 45) % row < row - 5) return (y - 45) / row;
        if (y >= HEIGHT - 28 && y < HEIGHT - 6) return 20;
    }
    if (view == View::ABOUT && y >= HEIGHT - 30 && y < HEIGHT - 6) return 20;
    return -1;
}
} // namespace

namespace game {
const char *title() { return pocket::tr("FAT FISH", "大肥鱼"); }
const char *app_id() { return "pxa-fat-fish"; }
void init() { state = State{}; home(); play(art::IDLE); chat::init(); }
void reset() {
    state = State{}; cooldown = save_ms = auto_ms = walk_ms = 0;
    home(); play(art::WAVE); notice("Hello, I'm your little whale!", "你好呀，我是你的大肥鱼！"); record("reset");
}
void key(Key key) {
    if (chat::is_open()) { if (key == Key::B) chat::close(); return; }
    if (key == Key::SELECT && view == View::HOME) { chat::open(); return; }
    if (key == Key::B) {
        if (view == View::HOME) open_menu();
        else home();
        return;
    }
    if (key == Key::X && view == View::HOME) { care(Action::REST); return; }
    if (key == Key::Y && view == View::HOME) { activate(4); return; }
    if (view == View::CLIPS && (key == Key::L || key == Key::R)) { activate(21); return; }
    unsigned count = view == View::HOME ? 6 : view == View::CLIPS ? 8 : view == View::SETTINGS ? 5 : 1;
    unsigned delta = view == View::CLIPS && (key == Key::UP || key == Key::DOWN) ? 2 : 1;
    if (key == Key::LEFT || key == Key::UP) selection = (selection + count - delta) % count;
    if (key == Key::RIGHT || key == Key::DOWN) selection = (selection + delta) % count;
    if (key == Key::A) activate(view == View::ABOUT ? 20 : selection);
    if (view == View::HOME && key != Key::A) {
        dock_visible = true;
        dock_expanded = controls().columns == 3 && selection >= 2;
    }
    redraw();
}
void pointer(Phase phase, int x, int y) {
    if (chat::is_open()) { pressed = -1; dragging = false; return; }
    if (phase == Phase::DOWN) {
        pressed = hit(x, y); down_x = x; down_y = y;
        original_position = state.position; dragging = false;
    } else if (phase == Phase::MOVE && pressed == 30 && !state.sleeping) {
        if (abs(x - down_x) + abs(y - down_y) > 5) dragging = true;
        if (dragging) {
            const auto scene = room_layout();
            const int offset = scene.foot_x(original_position) - scene.rug_x + x - down_x;
            state.position = int16_t(scene.drag_position(offset));
            redraw();
        }
    } else if (phase == Phase::UP) {
        int id = pressed; pressed = -1;
        if (dragging) { dragging = false; play(art::WAVE); record("drag"); }
        else if (id >= 0 && hit(x, y) == id) { if (id == 30) care(Action::PET); else activate(unsigned(id)); }
    } else if (phase == Phase::CANCEL) { pressed = -1; dragging = false; }
}
void tick(uint32_t ms) {
    const uint32_t previous_minute = state.seconds / 60;
    step(state, ms);
    if (state.seconds / 60 != previous_minute) redraw();
    clip_ms += ms; auto_ms += ms; save_ms += ms; walk_ms += ms;
    cooldown = cooldown > ms ? cooldown - ms : 0;
    if (toast_ms && toast_ms <= ms) redraw();
    toast_ms = toast_ms > ms ? toast_ms - ms : 0;
    const unsigned duration = art::animations[clip].count * 1000 / art::FPS;
    if (clip_ms >= duration && clip != art::IDLE && !(clip == art::SLEEP && state.sleeping)) {
        play(state.sleeping ? art::SLEEP : art::IDLE);
    }
    if (state.sleeping && clip != art::SLEEP_START && clip != art::SLEEP) play(art::SLEEP);
    if (view == View::HOME && !chat::is_open() && !chat::voice_stage() && pressed < 0 &&
        !cooldown && !toast_ms && auto_ms >= 5000 && (clip == art::IDLE || clip == art::SLEEP)) {
        Action action;
        if (automatic_care(state, action)) care(action, true);
    }
    if (!state.sleeping && state.autoplay && view == View::HOME && !chat::is_open() && !dragging && auto_ms > 24000) {
        const art::Clip random_clips[] = {art::WAVE, art::CUBE, art::DANCE, art::BUBBLES, art::STRETCH, art::THINK, art::WALK};
        play(random_clips[pocket::random() % 7]);
    }
    if (walk_ms >= 350) {
        walk_ms = 0;
        if (state.wander && !state.sleeping && !dragging && clip == art::WALK && view == View::HOME && !chat::is_open()) {
            if (state.position <= -45) walk_direction = 1;
            if (state.position >= 45) walk_direction = -1;
            state.position = int16_t(std::clamp(int(state.position) + walk_direction, -64, 64));
        }
    }
    if (save_ms >= 15000) { save_ms = 0; save(); }
    // Native text and scenery update at a bounded cadence, including sleeping.
    static uint32_t paint_ms;
    paint_ms += ms;
    if (paint_ms >= 1000) { paint_ms = 0; if (view == View::HOME && !chat::is_open()) redraw(); }
}
void draw() {
    static int previous_width, previous_height, previous_touch_size;
    static unsigned previous_room = 3;
    const int touch_size = pocket::dp(44);
    if (previous_width != WIDTH || previous_height != HEIGHT || previous_touch_size != touch_size || previous_room != active_room()) {
        previous_width = WIDTH; previous_height = HEIGHT; previous_touch_size = touch_size;
        previous_room = active_room();
        const auto bar = controls();
        char line[160];
        snprintf(line, sizeof(line), "FatFish: layout canvas=%dx%d touch=%d columns=%d cell=%d top=%d height=%d icon=%d",
            WIDTH, HEIGHT, touch_size, bar.columns, bar.cell, bar.top, bar.height, bar.icon);
        log(line);
        const auto scene = room_layout();
        snprintf(line, sizeof(line), "FatFish: placement room=%u rug=%d,%d feet=%d,%d crop=%d,%d,%d,%d travel=%d",
            active_room(), scene.rug_x, scene.rug_y, scene.foot_x(state.position), scene.rug_y,
            scene.sx, scene.sy, scene.sw, scene.sh, scene.travel);
        log(line);
    }
    if (view == View::HOME) home_draw();
    else if (view == View::CLIPS) clips_draw();
    else if (view == View::SETTINGS) settings_draw();
    else about_draw();
}
bool menu_hit(int x, int y) { return fish::header(WIDTH).menu_hit(x, y); }
void draw_menu(bool paused, int row, bool exiting) {
    if (paused) {
        backdrop(25);
        studio::round_alpha(0, 0, WIDTH, HEIGHT, 0, theme().shadow, 75);
    }
    const auto top = fish::header(WIDTH);
    const int mx = top.menu_x, my = top.y;
    panel(mx, my, top.menu_width, top.height, 11, 226);
    if (paused) {
        line(mx + 9, my + 9, mx + 19, my + 19, ink());
        line(mx + 19, my + 9, mx + 9, my + 19, ink());
    } else {
        // Two logical pixels survive the fitted surface's fractional scale.
        for (int i = 0; i < 3; ++i) studio::round(mx + 8, my + 7 + i * 6, 12, 2, 1, ink());
    }
    if (!paused) return;
    const int x = (WIDTH - 192) / 2, y = HEIGHT > 224 ? (HEIGHT - 180) / 2 : 32;
    panel(x, y, 192, 180, 12, 246);
    icon(3, x + 13, y + 9, 18);
    label(x + 34, y + 13, 143, tr("Pause", "休息一下"), ink(), 0);
    const char *labels[] = {tr("Continue", "回到小房间"), tr("Restart", "重新开始"),
        tr("Save & exit", "保存并退出"), tr("Language", "语言 / 中文")};
    for (int i = 0; i < 4; ++i) button(x + 8, y + 40 + i * 30, 176, 27, labels[i], row == i);
    label(x + 8, y + 160, 176, exiting ? tr("Saving...", "正在保存...") : tr("A OK / B Back", "A 确认 / B 返回"), theme().muted);
}
size_t serialize(uint8_t *data, size_t capacity) { return encode(state, data, capacity); }
void load(const uint8_t *data, size_t size) {
    if (decode(state, data, size)) { play(state.sleeping ? art::SLEEP : art::IDLE); record("loaded", data[18] == 0); }
    else log("FatFish: invalid save ignored");
}
bool service_event(const pxa_event_t &event, bool foreground, uint64_t now_us) {
    return chat::event(event, foreground, now_us) || animation::event(event);
}
void service_update(bool foreground, uint64_t now_us) {
    static unsigned previous_voice_stage;
    if (view != View::HOME || state.sleeping) chat::stop_voice();
    chat::update(foreground, now_us);
    unsigned stage = chat::voice_stage();
    if (stage != previous_voice_stage) {
        (void)pxa_window_keep_screen_on(stage != 0);
        if (!stage && previous_voice_stage && view == View::HOME && !chat::is_open())
            notice(chat::voice_status(), chat::voice_status());
        previous_voice_stage = stage;
        redraw();
    }
    if (foreground && !chat::is_open()) {
        if (const char *reply = chat::take_reply()) notice(reply, reply);
        int action = chat::take_animation();
        if (action >= 0 && !state.sleeping) play(art::Clip(action));
    }
    animation::update(foreground && view == View::HOME && !chat::is_open());
}
} // namespace game
