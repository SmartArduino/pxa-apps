// SPDX-License-Identifier: MIT
#include "../common/pocket-play/runtime.hpp"
#include "../common/pocket-studio/studio.hpp"
#include "animation.hpp"
#include "core.hpp"
#include "startup_frames.inc"
#include "pxa_assets.h"
#include <cstdio>
#include <string.h>

namespace fish::animation {
namespace {
constexpr unsigned GROUP_FRAMES = 12, GROUP_BYTES = 98304;
struct Group {
    uint8_t data[GROUP_BYTES];
    unsigned first, end, bytes, loaded;
    bool ready;
} groups[2];
uint8_t pixels[art::WIDTH * art::HEIGHT], decoded[art::WIDTH * art::HEIGHT];
uint8_t history[art::WIDTH * art::HEIGHT];
art::Clip history_clip = art::COUNT;
art::Clip selected = art::IDLE, displayed = art::IDLE;
uint32_t started;
uint32_t retry_at;
unsigned retries;
int filling = -1;
uint64_t sequence = 1000, pending;
int shown = -1;
int history_frame = -1;
bool ready, error;

void fail(const char *stage, int status = 0) {
    error = true; pending = 0;
    char message[128];
    std::snprintf(message, sizeof(message), "FatFish: animation resource failed stage=%s status=%d clip=%u",
                  stage, status, unsigned(selected));
    pocket::log(message);
    pocket::redraw();
}
void read_error(int status) {
    pending = 0;
    const bool transient = status == PXA_STATUS_BUSY || status == PXA_STATUS_WOULD_BLOCK ||
        status == PXA_STATUS_RESOURCE_LIMIT || status == PXA_STATUS_TIMED_OUT ||
        status == PXA_STATUS_UNAVAILABLE || status == PXA_STATUS_IO_ERROR;
    if (!transient || retries >= 3) { fail("read", status); return; }
    retry_at = pocket::millis() + (100u << retries++);
    char message[100];
    std::snprintf(message, sizeof(message), "FatFish: retry animation read status=%d attempt=%u", status, retries);
    pocket::log(message);
}
void read_next() {
    auto &group = groups[filling];
    const uint32_t bytes = std::min(group.bytes - group.loaded, unsigned(PXA_ASSETS_READ_MAX_BYTES));
    pending = ++sequence;
    const int status = pxa_assets_read(pending, "assets/animations.bin", art::frames[group.first].offset + group.loaded, bytes);
    if (status) read_error(status);
}
void read_group(unsigned first, unsigned slot) {
    if (pending) (void)pxa_cancel(pending);
    auto &group = groups[slot];
    const auto &clip = art::animations[selected];
    group.first = first;
    group.end = std::min(first + GROUP_FRAMES, unsigned(clip.first + clip.count));
    const auto &last = art::frames[group.end - 1];
    group.bytes = last.offset + last.bytes - art::frames[first].offset;
    group.loaded = 0; group.ready = false; filling = int(slot); retries = 0;
    if (!group.bytes || group.bytes > GROUP_BYTES) { fail("group"); return; }
    read_next();
}
} // namespace
void play(art::Clip clip) {
    if (clip >= art::COUNT) return;
    if (pending) { (void)pxa_cancel(pending); pending = 0; }
    filling = -1;
    retry_at = 0; retries = 0;
    selected = clip; started = pocket::millis(); shown = -1; error = false;
}
void update(bool foreground) {
    if (!foreground || error) return;
    if (retry_at) {
        if (int32_t(pocket::millis() - retry_at) < 0) return;
        retry_at = 0;
        read_next();
        return;
    }
    const auto &clip = art::animations[selected];
    const unsigned local = ((pocket::millis() - started) * art::FPS / 1000) % clip.count;
    const unsigned frame = clip.first + local;
    const unsigned group_first = clip.first + local / GROUP_FRAMES * GROUP_FRAMES;
    int slot = -1;
    for (unsigned i = 0; i < 2; ++i)
        if (groups[i].ready && groups[i].first == group_first) slot = int(i);
    if (slot < 0) {
        if (filling < 0 || groups[filling].first != group_first)
            read_group(group_first, groups[0].ready && groups[0].first != group_first ? 1 : 0);
        return;
    }
    const auto &group = groups[slot];
    if (shown != int(frame)) {
        unsigned first = art::frames[frame].key;
        if (history_clip == selected && history_frame >= int(first) && history_frame < int(frame))
            first = unsigned(history_frame) + 1;
        for (unsigned index = first; index <= frame; ++index) {
            const auto &descriptor = art::frames[index];
            if (index < group.first || index >= group.end ||
                (descriptor.delta && (history_clip != selected || history_frame + 1 != int(index))) ||
                !unpack(group.data + descriptor.offset - art::frames[group.first].offset,
                        descriptor.bytes, decoded, sizeof(decoded), descriptor.delta ? history : nullptr)) {
                fail("decode"); return;
            }
            memcpy(history, decoded, sizeof(history)); history_frame = int(index); history_clip = selected;
        }
        memcpy(pixels, history, sizeof(pixels)); displayed = selected; shown = int(frame); ready = true;
        pocket::redraw();
    }
    const unsigned next = group.end == unsigned(clip.first + clip.count) ? clip.first : group.end;
    const unsigned other = unsigned(1 - slot);
    if (filling < 0 && !(groups[other].ready && groups[other].first == next)) read_group(next, other);
}
bool event(const pxa_event_t &event) {
    if (event.service != PXA_ASSETS_SERVICE) return false;
    // Results from cancelled/obsolete generations must never replace a new clip.
    if (event.token != pending || !pending) return true;
    pxa_asset_read_result_t result;
    if (filling < 0) return true;
    auto &group = groups[filling];
    const unsigned offset = art::frames[group.first].offset + group.loaded;
    if (!pxa_assets_parse_read(&event, pending, &result)) { fail("response"); return true; }
    if (result.status) { read_error(result.status); return true; }
    if (result.offset != offset || !result.bytes ||
        result.bytes > group.bytes - group.loaded ||
        result.total_bytes < art::frames[group.first].offset + group.bytes) {
        fail("range"); return true;
    }
    pending = 0;
    memcpy(group.data + group.loaded, result.data, result.bytes);
    group.loaded += result.bytes;
    if (group.loaded < group.bytes) read_next();
    else { group.ready = true; filling = -1; }
    return true;
}
void draw(int x, int y, int width, int height) {
    if (!ready) {
        const bool sleeping = selected == art::SLEEP || selected == art::SLEEP_START;
        const auto *poster = sleeping ? art::poster_sleep : art::poster_idle;
        const size_t bytes = sleeping ? sizeof(art::poster_sleep) : sizeof(art::poster_idle);
        if (!unpack(poster, bytes, decoded, sizeof(decoded))) return;
        memcpy(pixels, decoded, sizeof(pixels));
        displayed = sleeping ? art::SLEEP : art::IDLE;
        memcpy(history, pixels, sizeof(history)); history_clip = displayed;
        history_frame = art::animations[displayed].first;
        ready = true;
    }
    studio::Image image{int(art::WIDTH), int(art::HEIGHT), art::animations[displayed].palette, pixels};
    studio::picture_region(x, y, width, height, image, 0, 0, art::WIDTH, art::HEIGHT, 0, true);
}
bool failed() { return error; }
} // namespace fish::animation
