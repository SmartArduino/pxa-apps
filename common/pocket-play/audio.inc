// SPDX-License-Identifier: CC0-1.0
// Included inside runtime.cpp's anonymous namespace, after the clock helpers.
// Optional audio is deliberately independent of startup and saving.
enum : uint64_t { SOUND_PERMISSION = 600, SOUND_OPEN, SOUND_LEGACY, SOUND_GRAPH, SOUND_FLUSH };
constexpr uint32_t SOUND_RATE = 16000, SOUND_BLOCK = 320;
struct Tone { uint64_t start, end; uint32_t step; };
Tone tones[16];
int16_t sound_pcm[SOUND_BLOCK];
uint64_t sound_handle, sound_pending_handle, sound_permission, sound_cursor;
bool sound_pending;
uint64_t sound_now() { return foreground_us() * SOUND_RATE / 1000000; }
void clear_sound() {
    memset(tones, 0, sizeof(tones)); sound_pending = false;
    sound_cursor = sound_now();
    if (sound_handle) pxa_audio_flush(SOUND_FLUSH, sound_handle);
}
void add_tone(float frequency, float duration, float when) {
    // Comparisons also reject NaN, keeping float-to-integer conversions defined.
    if (!(frequency >= 30 && frequency <= 6000) || !(duration > 0 && duration <= 4) ||
        !(when >= 0 && when < 10000000) || background || paused || exit_requested) return;
    uint64_t now = sound_now(), start = when == 0 ? now : uint64_t(when * SOUND_RATE);
    if (start + uint64_t(duration * SOUND_RATE) < now || start > now + SOUND_RATE * 60) return;
    // Pending host blocks cannot be rewritten; begin at the next available sample.
    if (start < sound_cursor) start = sound_cursor;
    for (Tone &t : tones) if (t.end <= now || t.end <= sound_cursor) {
        t = {start, start + uint64_t(duration * SOUND_RATE),
             uint32_t(double(frequency) * 4294967296.0 / SOUND_RATE)};
        return;
    }
    // Bounded queue: excess notes are dropped rather than blocking input.
}
void render_sound() {
    const Tone *active[16]; unsigned count = 0;
    for (const Tone &t : tones)
        if (t.end > sound_cursor && t.start < sound_cursor + SOUND_BLOCK) active[count++] = &t;
    if (!count) { memset(sound_pcm, 0, sizeof(sound_pcm)); return; }
    for (unsigned i = 0; i < SOUND_BLOCK; ++i) {
        uint64_t at = sound_cursor + i;
        int mixed = 0;
        for (unsigned j = 0; j < count; ++j) {
            const Tone &t = *active[j];
            if (at < t.start || at >= t.end) continue;
            uint32_t phase = uint32_t((at - t.start) * t.step);
            int p = int(phase >> 16);
            int triangle = p < 32768 ? p * 2 - 32768 : 98303 - p * 2;
            uint64_t edge = at - t.start;
            if (t.end - at < edge) edge = t.end - at;
            int envelope = edge < 96 ? int(edge) : 96;
            mixed += triangle * envelope / (96 * 24);
        }
        if (mixed > 20000) mixed = 20000;
        if (mixed < -20000) mixed = -20000;
        sound_pcm[i] = int16_t(mixed);
    }
}
void service_sound() {
    if (!sound_handle || background || paused || exit_requested) return;
    uint64_t now = sound_now();
    if (sound_cursor + SOUND_RATE / 10 < now) {
        sound_cursor = now; sound_pending = false;
    }
    // At most 60 ms of work per call, and no more than 40 ms ahead.
    for (unsigned block = 0; block < 3 && sound_cursor < now + SOUND_BLOCK * 2; ++block) {
        if (!sound_pending) { render_sound(); sound_pending = true; }
        int32_t n = pxa_audio_write_pcm(sound_handle,
            reinterpret_cast<uint8_t *>(sound_pcm), sizeof(sound_pcm));
        if (n == PXA_STATUS_WOULD_BLOCK) break;
        if (n != int(sizeof(sound_pcm))) {
            pxa_log_write(3, "Pocket: audio unavailable; continuing silently");
            sound_handle = 0; sound_pending = false; break;
        }
        sound_pending = false; sound_cursor += SOUND_BLOCK;
    }
}
int32_t handle_sound(const pxa_event_t &e) {
    if (e.service == PXA_PERMISSION_SERVICE && e.token == SOUND_PERMISSION) {
        pxa_permission_acquire_result_t r;
        if (!pxa_permission_parse_acquire(&e, SOUND_PERMISSION, &r)) return PXA_STATUS_INTERNAL;
        if (!r.status) {
            sound_permission = r.handle;
            pxa_audio_open_pcm(SOUND_OPEN, r.handle, SOUND_RATE, 1);
        } else pxa_log_write(2, "Pocket: optional audio denied; continuing silently");
        return PXA_EVENT_HANDLED;
    }
    pxa_permission_revoked_t revoked;
    if (pxa_permission_parse_revoked(&e, &revoked)) {
        if (revoked.name_size == 14 && !memcmp(revoked.name, "audio.playback", 14)) {
            sound_handle = sound_pending_handle = sound_permission = 0;
            sound_pending = false; memset(tones, 0, sizeof(tones));
        }
        return PXA_EVENT_HANDLED;
    }
    if (e.service == PXA_AUDIO_SERVICE && e.token == SOUND_OPEN) {
        pxa_audio_pcm_result_t r;
        if (!pxa_audio_parse_pcm(&e, SOUND_OPEN, &r)) return PXA_STATUS_INTERNAL;
        if (!r.source.status && r.source.sample_rate == SOUND_RATE && r.source.channels == 1) {
            sound_pending_handle = r.source.handle;
            pxa_audio_commit_gain(SOUND_GRAPH, sound_pending_handle, 0);
        } else pxa_audio_open_media(SOUND_LEGACY, sound_permission);
        return PXA_EVENT_HANDLED;
    }
    if (e.service == PXA_AUDIO_SERVICE && e.token == SOUND_LEGACY) {
        pxa_audio_open_result_t r;
        if (!pxa_audio_parse_open(&e, SOUND_LEGACY, &r)) return PXA_STATUS_INTERNAL;
        if (!r.status && r.sample_rate == SOUND_RATE && r.channels == 1 && r.frame_ms == 20) {
            sound_pending_handle = r.handle;
            pxa_audio_commit_gain(SOUND_GRAPH, sound_pending_handle, 0);
        } else pxa_log_write(2, "Pocket: optional audio unsupported");
        return PXA_EVENT_HANDLED;
    }
    if (e.service == PXA_AUDIO_SERVICE && e.token == SOUND_GRAPH) {
        int32_t status;
        if (!pxa_audio_parse_status(&e, SOUND_GRAPH, PXA_AUDIO_COMMIT_GRAPH, &status)) return PXA_STATUS_INTERNAL;
        if (!status && sound_permission) {
            sound_handle = sound_pending_handle; sound_cursor = sound_now();
            pxa_log_write(2, "Pocket: audio ready 16000 Hz mono");
        }
        return PXA_EVENT_HANDLED;
    }
    if (e.service == PXA_AUDIO_SERVICE && e.token == SOUND_FLUSH) return PXA_EVENT_HANDLED;
    return PXA_EVENT_UNHANDLED;
}
void request_sound() {
    uint32_t n;
    if (pxa_permission_build(packet, sizeof(packet), PXA_PERMISSION_ACQUIRE, SOUND_PERMISSION,
        "audio.playback", 14, reinterpret_cast<const uint8_t *>("media"), 5, &n))
        pxa_control_packet(packet, n);
}
