// SPDX-License-Identifier: MIT
#include "voice.hpp"
#include "../common/pocket-play/runtime.hpp"
#include "../common/media/decoder.hpp"
#include "../common/media/pcm_packet.hpp"
#include "../translator/ogg.hpp"
#include "pxa_permission.h"
#include <cstdio>
#include <cstring>

namespace fish::voice {
namespace {
constexpr uint64_t BASE = UINT64_C(2) << 48;
uint64_t sequence = BASE, permission_token, capture_token, playback_token, gain_token, query_token;
uint64_t microphone_permission, speaker_permission, capture, playback, now_ms, capture_at, deadline;
uint8_t scratch[4096];
translate::OggOpus ogg;
media::Decoder decoder;
media::PcmPacket pcm;
const uint8_t *mp3;
size_t mp3_size, mp3_offset;
unsigned decoded_frames, decoded_offset, source_rate, source_channels;
bool wanted, ready, output_wanted, output_ready, microphone_request;
bool continuous, chunk_taken;
bool fault;
char notification[160];

void tell(const char *en, const char *zh) {
    std::snprintf(notification, sizeof(notification), "%s", pocket::tr(en, zh));
}
void fail(const char *en, const char *zh) { fault = true; tell(en, zh); }
void close_handle(uint64_t &handle) {
    if (handle) (void)pxa_close_handle(handle);
    handle = 0;
}
void cancel_token(uint64_t &token) {
    if (token) (void)pxa_cancel(token);
    token = 0;
}
void stop_output() {
    cancel_token(playback_token); cancel_token(gain_token); cancel_token(query_token);
    close_handle(playback);
    output_wanted = output_ready = false;
    mp3 = nullptr; mp3_size = mp3_offset = 0;
    decoded_frames = decoded_offset = 0;
    pcm.reset(); decoder.reset();
}
bool acquire(const char *name, const char *scope) {
    microphone_request = !std::strcmp(name, "audio.capture");
    uint32_t written;
    permission_token = ++sequence;
    if (!pxa_permission_build(scratch, sizeof(scratch), PXA_PERMISSION_ACQUIRE, permission_token,
            name, std::strlen(name), (const uint8_t *)scope, std::strlen(scope), &written) ||
        pxa_control_packet(scratch, written)) {
        permission_token = 0;
        fail("Cannot request audio access.", "暂时无法申请录音或播放权限。"); return false;
    }
    return true;
}
void open_capture() {
    uint8_t value[8]; uint32_t bytes;
    capture_token = ++sequence;
    pxa_store_u64(value, microphone_permission);
    if (!pxa_build_message(scratch, sizeof(scratch), PXA_AUDIO_SERVICE, 6, capture_token,
            value, sizeof(value), &bytes) || pxa_control_packet(scratch, bytes)) {
        capture_token = 0; wanted = false;
        fail("Microphone unavailable.", "麦克风暂不可用，请稍后重试。");
    }
    deadline = now_ms + 10000;
}
void output_error() {
    stop_output();
    fail("Speech playback failed. The text is kept.", "语音播放未完成，可以继续查看文字。");
}
void pump_output() {
    if (!output_wanted) return;
    if (now_ms >= deadline) { output_error(); return; }
    for (unsigned work = 0; work < 8; ++work) {
        if (pcm.full()) {
            if (!output_ready) return;
            int n = pcm.write(playback);
            if (n == PXA_STATUS_WOULD_BLOCK) return;
            if (n < 0) { output_error(); return; }
            continue;
        }
        if (decoded_offset < decoded_frames) {
            decoded_offset += pcm.append(decoder.pcm + decoded_offset * source_channels,
                                          decoded_frames - decoded_offset);
            continue;
        }
        if (mp3_offset >= mp3_size) {
            if (!output_ready) return;
            if (pcm.frames) {
                int n = pcm.write(playback);
                if (n == PXA_STATUS_WOULD_BLOCK) return;
                if (n < 0) { output_error(); return; }
                return;
            }
            if (!query_token) {
                query_token = ++sequence;
                if (pxa_audio_query(query_token, playback)) output_error();
            }
            return;
        }
        mp3dec_frame_info_t info{};
        int frames = decoder.decode(mp3 + mp3_offset, int(mp3_size - mp3_offset), info);
        if (info.frame_bytes <= 0 || size_t(info.frame_bytes) > mp3_size - mp3_offset) {
            output_error(); return;
        }
        mp3_offset += unsigned(info.frame_bytes);
        if (!frames) continue;
        if (source_rate && (unsigned(info.hz) != source_rate || unsigned(info.channels) != source_channels)) {
            output_error(); return;
        }
        decoded_frames = unsigned(frames); decoded_offset = 0;
        if (!source_rate) {
            source_rate = unsigned(info.hz); source_channels = unsigned(info.channels);
            pcm.configure(source_rate, source_channels);
            playback_token = ++sequence;
            if (pxa_audio_open_pcm(playback_token, speaker_permission, source_rate, source_channels)) {
                output_error(); return;
            }
        }
    }
}
} // namespace
void press() {
    if (wanted || capture || capture_token || ready || permission_token) return;
    stop_output(); ogg.reset(); chunk_taken = false; continuous = false; wanted = true; fault = false;
    if (!microphone_permission) {
        if (!acquire("audio.capture", "microphone")) { wanted = false; return; }
        tell("Allow microphone access, then hold again.", "允许录音后，再次按住说话。");
    } else open_capture();
}
void listen() { press(); continuous = true; }
void release(bool send) {
    if (!wanted && !capture && !capture_token) return;
    wanted = false;
    cancel_token(capture_token); close_handle(capture);
    ready = send && ogg.granule >= 9600 && ogg.finish();
    if (ready) {
        tell("Recognizing speech...", "正在识别语音……");
        pocket::log("FatFish: voice recording ready");
    } else {
        ogg.size = 0;
        tell(send ? "Hold a little longer to speak." : "Recording cancelled.",
             send ? "请按住稍久一点，再松开发送。" : "录音已取消。");
    }
}
void cancel() {
    release(false); ready = wanted = false;
    cancel_token(permission_token); stop_output();
    continuous = chunk_taken = false; notification[0] = 0;
}
bool recording() { return capture || capture_token || wanted; }
bool playing() { return output_wanted; }
bool busy() { return recording() || ready || output_wanted || permission_token; }
bool take_failure() { bool result = fault; fault = false; return result; }
bool event(const pxa_event_t &event, bool foreground, uint64_t now_us) {
    now_ms = now_us / 1000;
    if (event.token >> 48 == 2 && event.service == PXA_PERMISSION_SERVICE && event.opcode == PXA_PERMISSION_ACQUIRE) {
        pxa_permission_acquire_result_t result{};
        if (!pxa_permission_parse_acquire(&event, event.token, &result)) return true;
        if (event.token != permission_token) { if (result.handle) (void)pxa_close_handle(result.handle); return true; }
        permission_token = 0;
        if (result.status) { wanted = false; stop_output(); fail("Audio access was declined.", "录音或播放权限未获允许。"); return true; }
        if (microphone_request) microphone_permission = result.handle;
        else speaker_permission = result.handle;
        if (wanted && foreground) open_capture();
        else if (!output_wanted) tell("Hold the microphone to speak.", "按住麦克风说话，松开发送。");
        return true;
    }
    pxa_permission_revoked_t revoked{};
    if (pxa_permission_parse_revoked(&event, &revoked) &&
        ((revoked.name_size == 13 && !std::memcmp(revoked.name, "audio.capture", 13)) ||
         (revoked.name_size == 14 && !std::memcmp(revoked.name, "audio.playback", 14)))) {
        cancel(); close_handle(microphone_permission); close_handle(speaker_permission);
        fail("Audio access was revoked.", "录音或播放权限已撤销。"); return true;
    }
    if (event.service != PXA_AUDIO_SERVICE || event.token >> 48 != 2) return false;
    if (event.opcode == 6) {
        if (event.payload_size < 4) return true;
        int result = int32_t(pxa_load_u32(event.payload));
        uint64_t handle = !result && event.payload_size >= 12 ? pxa_load_u64(event.payload + 4) : 0;
        if (event.token != capture_token || !wanted || !foreground) { if (handle) (void)pxa_close_handle(handle); return true; }
        capture_token = 0;
        if (result || event.payload_size != 24 || pxa_load_u32(event.payload + 12) != 16000 ||
            event.payload[16] != 1 || event.payload[17] != 1 || pxa_load_u16(event.payload + 18) != 20) {
            if (handle) (void)pxa_close_handle(handle);
            wanted = false; fail("Microphone format unavailable.", "设备的录音格式暂不可用。"); return true;
        }
        capture = handle; capture_at = now_ms;
        tell("Recording. Release to send (20s max).", "录音中，松开发送，最长20秒。");
        pocket::log("FatFish: voice recording started"); return true;
    }
    pxa_audio_pcm_result_t result{};
    if (pxa_audio_parse_pcm(&event, event.token, &result)) {
        if (event.token != playback_token || !output_wanted || !foreground) {
            if (result.source.handle) (void)pxa_close_handle(result.source.handle);
            if (event.token == playback_token) stop_output();
            return true;
        }
        playback_token = 0;
        if (result.source.status) { output_error(); return true; }
        playback = result.source.handle; gain_token = ++sequence;
        if (pxa_audio_commit_gain(gain_token, playback, -3 * 256)) output_error();
        return true;
    }
    int32_t status;
    if (pxa_audio_parse_status(&event, gain_token, PXA_AUDIO_COMMIT_GRAPH, &status)) {
        gain_token = 0;
        if (status) output_error();
        else { output_ready = true; pocket::log("FatFish: local speech playback started"); }
        return true;
    }
    pxa_audio_state_result_t state{};
    if (pxa_audio_parse_state(&event, query_token, &state)) {
        query_token = 0;
        if (state.status) output_error();
        else if (!state.queued_samples) {
            stop_output(); tell("Hold to speak again.", "按住说话，再聊一句吧。");
            pocket::log("FatFish: local speech playback completed");
        }
        return true;
    }
    return false;
}
void update(bool foreground, uint64_t now_us) {
    now_ms = now_us / 1000;
    if (output_wanted && now_ms >= deadline) output_error();
    if (!foreground) { if (recording()) release(false); if (playback) stop_output(); return; }
    if (capture_token && now_ms >= deadline) { release(false); fail("Microphone timed out.", "麦克风没有及时就绪，请重试。"); }
    if (capture) {
        if (chunk_taken) { ogg.reset(); chunk_taken = false; }
        uint8_t packet[1275];
        for (unsigned work = 0; work < 8; ++work) {
            int n = pxa_io(capture, 1, packet, sizeof(packet));
            if (n == PXA_STATUS_WOULD_BLOCK) break;
            if (n <= 0 || unsigned(n) > sizeof(packet)) { release(false); fail("Recording interrupted.", "录音已中断，请再试一次。"); break; }
            if (!ogg.append(packet, unsigned(n), 20)) {
                if (continuous) {
                    release(false); fail("Voice connection is too slow.", "语音连接较慢，请重新开启 AI 语音。");
                } else release(true);
                break;
            }
        }
        if (capture && !continuous && now_ms - capture_at >= 20000) release(true);
    }
    if (output_wanted && !speaker_permission && !permission_token) {
        if (!acquire("audio.playback", "media")) stop_output();
        return;
    }
    if (speaker_permission) pump_output();
}
const char *notice() {
    if (!notification[0]) return nullptr;
    static char returned[160];
    std::memcpy(returned, notification, sizeof(returned)); notification[0] = 0;
    return returned;
}
bool take_recording(const uint8_t *&bytes, size_t &size) {
    if (!ready) return false;
    ready = false; bytes = ogg.data; size = ogg.size; return true;
}
bool take_chunk(const uint8_t *&bytes, size_t &size) {
    if (!capture || !continuous || ogg.granule < 48000 || !ogg.finish()) return false;
    bytes = ogg.data; size = ogg.size; chunk_taken = true; return true;
}
void play(const uint8_t *data, size_t size) {
    stop_output();
    mp3 = data; mp3_size = size; source_rate = source_channels = 0;
    output_wanted = true; deadline = now_ms + 45000;
    tell("Speaking...", "大肥鱼在说话……");
}
} // namespace fish::voice
