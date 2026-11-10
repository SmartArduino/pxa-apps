// SPDX-License-Identifier: MIT
#include "../core.hpp"
#include "../animation_data.hpp"
#include "../layout.hpp"
#include "../dialogue.hpp"
#include "../startup_frames.inc"
#include <assert.h>
#include <fstream>
#include <vector>

int main(int argc, char **argv) {
    using namespace fish;
    for (int width : {288, 299, 336, 420}) {
        const auto top = header(width);
        assert(top.x == width - top.menu_x - top.menu_width);
        assert(top.height == top.menu_width && top.menu_x - top.x - top.width == 8);
        assert(top.menu_hit(top.menu_x + top.menu_width / 2, top.y + top.height / 2));
        assert(top.menu_hit(width - 20, 29));
        assert(!top.menu_hit(top.menu_x - 1, top.y));
        assert(!top.menu_hit(top.menu_x + top.menu_width, top.y));
        assert(!top.menu_hit(top.menu_x, top.y + top.height));
        for (int height : {224, 288, 384}) {
            for (int touch : {22, 44, 56, 88}) {
                auto bar = controls(width, height, touch);
                assert(bar.top > 100 && bar.icon >= 14);
                for (unsigned i = 0; i < 6; ++i) {
                    assert(bar.x(i) >= 12 && bar.x(i) + bar.width <= width - 12);
                    assert(bar.y(i) + bar.height <= height - 12);
                    assert(bar.hit(bar.x(i) + bar.width / 2, bar.y(i) + bar.height / 2) == int(i));
                    assert(bar.hit(bar.x(i) + bar.width, bar.y(i)) == -1);
                }
            }
        }
    }
    assert(controls(288, 288, 44).columns == 6);
    assert(controls(288, 384, 80).columns == 3);
    for (const char *message : {"Stay with me? A little pat makes my day.",
            "今天也要陪着你呀，摸摸我的头吧。", "晚安，轻轻点我就能叫醒我。", "", "Hello\nworld"}) {
        const auto text = wrap_dialogue(message, 149);
        assert(text.count <= 3 && text.width <= 149);
        for (unsigned i = 0; i < text.count; ++i) {
            assert(dialogue_width(text.lines[i]) <= 149);
            const char *cursor = text.lines[i];
            while (*cursor) assert(pocket_ui::next_utf8(cursor) != 0xfffd);
        }
    }
    const auto words = wrap_dialogue("Hello world again", 80);
    assert(words.count == 2 && !strcmp(words.lines[0], "Hello world") && !strcmp(words.lines[1], "again"));
    const auto long_text = wrap_dialogue("abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz", 80);
    assert(long_text.count == 3 && strstr(long_text.lines[2], "...") && long_text.width <= 80);
    const auto compact_speech = wrap_dialogue("今天也要陪着你呀，摸摸我的头吧。", 175, 24, 1);
    assert(compact_speech.count == 1 && compact_speech.width <= 175);
    const auto compact_room = room_layout(288, 224, 384, 256, {191, 194, 90});
    const auto compact_bubble = bubble_layout(288, compact_room, 0, 104, compact_speech, 34);
    assert(compact_bubble.y >= 44 && compact_bubble.y + compact_bubble.height <= 94);
    for (int width : {240, 288, 299, 480, 720}) {
        for (int height : {224, 288, 320, 384, 720}) {
            for (Rug rug : {Rug{191, 194, 90}, Rug{191, 193, 97}, Rug{190, 194, 92}}) {
                const auto scene = room_layout(width, height, 384, 256, rug);
                assert(scene.sx >= 0 && scene.sy >= 0);
                assert(scene.sx + scene.sw <= 384 && scene.sy + scene.sh <= 256);
                assert(scene.pet_x(0) + scene.pet_width * 94 / 192 == scene.rug_x);
                assert(scene.pet_y() + scene.pet_height * 132 / 144 == scene.rug_y);
                assert(scene.rug_y > height * 2 / 3 && scene.rug_y < height * 4 / 5);
                assert(scene.foot_x(-64) >= scene.rug_x - scene.travel);
                assert(scene.foot_x(64) <= scene.rug_x + scene.travel);
                assert(scene.drag_position(-10000) == (scene.travel ? -64 : 0));
                assert(scene.drag_position(10000) == (scene.travel ? 64 : 0));
                assert(scene.drag_position(0) == 0);
                const auto text = wrap_dialogue("今天也要陪着你呀，摸摸我的头吧。", 149);
                const int dock_top = controls(width, height, 44).top;
                for (int position : {-64, 0, 64}) {
                    const auto bubble = bubble_layout(width, scene, position, dock_top, text);
                    assert(bubble.x >= 12 && bubble.x + bubble.width <= width - 12);
                    assert(bubble.y >= 44 && bubble.y + bubble.height + 8 <= dock_top);
                    assert(bubble.tail_x >= bubble.x + 18 && bubble.tail_x <= bubble.x + bubble.width - 18);
                }
            }
        }
    }
    State s;
    assert(act(s, Action::PET) == Result::PETTED && s.affection == 1 && s.touches == 1);
    assert(act(s, Action::FEED) == Result::FED && s.food == 92);
    assert(act(s, Action::FEED) == Result::FED && s.food == 100);
    assert(act(s, Action::FEED) == Result::FULL);
    s.energy = 9;
    assert(act(s, Action::PLAY) == Result::TIRED);
    assert(act(s, Action::REST) == Result::SLEPT);
    assert(act(s, Action::FEED) == Result::SLEEPING);
    for (unsigned i = 0; i < 50; ++i) step(s, 100);
    assert(s.energy == 12 && s.seconds == 5);
    assert(act(s, Action::PET) == Result::WOKE);
    s.position = -64;
    s.room = 2;
    uint8_t data[64];
    assert(encode(s, data, 63) == 0);
    assert(encode(s, data, sizeof(data)) == 64);
    State loaded;
    assert(decode(loaded, data, sizeof(data)) && loaded.position == -64 && loaded.touches == 1 && loaded.room == 2);
    for (unsigned i = 0; i < 64; ++i) {
        data[i] ^= 1;
        assert(!decode(loaded, data, sizeof(data)));
        data[i] ^= 1;
    }
    data[8] = 101; put32(data + 60, checksum(data, 60));
    assert(!decode(loaded, data, sizeof(data)) && loaded.food == 100);
    data[8] = 100; data[17] = 3; put32(data + 60, checksum(data, 60));
    assert(!decode(loaded, data, sizeof(data)));
    data[17] = 0; put32(data + 60, checksum(data, 60));
    assert(decode(loaded, data, sizeof(data)) && loaded.room == 0 && loaded.position == -64);
    data[18] = 0; put32(data + 60, checksum(data, 60));
    assert(decode(loaded, data, sizeof(data)) && loaded.position == 0 && loaded.food == 100 && loaded.touches == 1);
    data[18] = 2; put32(data + 60, checksum(data, 60));
    assert(!decode(loaded, data, sizeof(data)) && loaded.position == 0);
    State independent;
    Action next;
    assert(!automatic_care(independent, next));
    independent.food = 30;
    assert(automatic_care(independent, next) && next == Action::FEED);
    assert(act(independent, next) == Result::FED && independent.food == 50);
    independent.energy = 20;
    assert(automatic_care(independent, next) && next == Action::REST);
    assert(act(independent, next) == Result::SLEPT);
    independent.auto_sleep = true;
    independent.energy = 89;
    assert(!automatic_care(independent, next));
    independent.energy = 90;
    assert(automatic_care(independent, next) && next == Action::REST);
    assert(act(independent, next) == Result::WOKE && !independent.auto_sleep);
    independent.joy = 35;
    assert(automatic_care(independent, next) && next == Action::PLAY);
    assert(act(independent, next) == Result::PLAYED && independent.joy == 50);
    independent.energy = 34; independent.joy = 0;
    assert(!automatic_care(independent, next));
    independent.autocare = false; independent.food = 0; independent.energy = 0;
    assert(!automatic_care(independent, next));
    independent.autocare = true;
    assert(automatic_care(independent, next) && next == Action::FEED);
    act(independent, Action::REST); independent.energy = 100;
    assert(!automatic_care(independent, next)); // A manually requested sleep is retained.
    independent.auto_sleep = true;
    assert(encode(independent, data, sizeof(data)) == 64);
    assert(decode(loaded, data, sizeof(data)) && loaded.autocare && loaded.auto_sleep);
    assert(automatic_care(loaded, next) && next == Action::REST);
    independent.autocare = false;
    encode(independent, data, sizeof(data));
    assert(decode(loaded, data, sizeof(data)) && !loaded.autocare && loaded.auto_sleep);
    data[19] = data[32] = 0; put32(data + 60, checksum(data, 60));
    assert(decode(loaded, data, sizeof(data)) && loaded.autocare && !loaded.auto_sleep);
    data[32] = 2; put32(data + 60, checksum(data, 60));
    assert(!decode(loaded, data, sizeof(data)));
    data[32] = 1; data[12] = 0; put32(data + 60, checksum(data, 60));
    assert(!decode(loaded, data, sizeof(data)));
    uint8_t out[5];
    const uint8_t valid[] = {120, 218, 99, 103, 103, 231, 224, 4, 0, 0, 114, 0, 39};
    assert(unpack(valid, sizeof(valid), out, 5) && out[0] == 7 && out[4] == 9);
    assert(!unpack(valid, sizeof(valid) - 1, out, 5));
    assert(!unpack(valid, sizeof(valid), out, 4));
    assert(!unpack(valid, sizeof(valid), out, 6));
    assert(argc == 2);
    std::ifstream stream(argv[1], std::ios::binary);
    std::vector<uint8_t> binary((std::istreambuf_iterator<char>(stream)), {});
    assert(!binary.empty());
    std::vector<uint8_t> frame(art::WIDTH * art::HEIGHT);
    std::vector<uint8_t> previous(frame.size());
    std::vector<uint32_t> hashes;
    for (const auto &clip : art::animations) {
        assert(!art::frames[clip.first].delta);
        for (unsigned first = clip.first; first < clip.first + clip.count; first += 12) {
            unsigned last = std::min(first + 12, unsigned(clip.first + clip.count)) - 1;
            assert(!art::frames[first].delta);
            assert(art::frames[last].offset + art::frames[last].bytes - art::frames[first].offset <= 98304);
        }
    }
    uint32_t end = 0;
    for (unsigned index = 0; index < sizeof(art::frames) / sizeof(art::frames[0]); ++index) {
        auto descriptor = art::frames[index];
        assert(descriptor.offset == end && descriptor.bytes <= 32768);
        assert(descriptor.key <= index && index - descriptor.key < 12);
        assert(descriptor.delta == (descriptor.key != index));
        end += descriptor.bytes;
        assert(end <= binary.size());
        assert(unpack(binary.data() + descriptor.offset, descriptor.bytes, frame.data(), frame.size(), descriptor.delta ? previous.data() : nullptr));
        hashes.push_back(checksum(frame.data(), frame.size()));
        previous = frame;
        unsigned transparent = 0, opaque = 0;
        for (uint8_t p : frame) { if (p) ++opaque; else ++transparent; }
        assert(transparent > 1000 && opaque > 500);
    }
    assert(end == binary.size());
    for (unsigned target = 0; target < hashes.size(); target += 17) {
        for (unsigned index = art::frames[target].key; index <= target; ++index) {
            auto descriptor = art::frames[index];
            assert(unpack(binary.data() + descriptor.offset, descriptor.bytes, frame.data(), frame.size(), descriptor.delta ? previous.data() : nullptr));
            previous = frame;
        }
        assert(checksum(frame.data(), frame.size()) == hashes[target]);
    }
    std::vector<uint8_t> poster(frame.size());
    assert(unpack(art::poster_idle, sizeof(art::poster_idle), poster.data(), poster.size()));
    const auto idle = art::frames[art::animations[art::IDLE].first];
    assert(unpack(binary.data() + idle.offset, idle.bytes, frame.data(), frame.size()) && poster == frame);
    assert(unpack(art::poster_sleep, sizeof(art::poster_sleep), poster.data(), poster.size()));
    const auto sleep = art::frames[art::animations[art::SLEEP].first];
    assert(unpack(binary.data() + sleep.offset, sleep.bytes, frame.data(), frame.size()) && poster == frame);
}
