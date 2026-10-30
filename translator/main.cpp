// SPDX-License-Identifier: MIT
#include "../common/media/canvas.hpp"
#include "../common/media/decoder.hpp"
#include "../common/media/pcm_packet.hpp"
#include "controls.hpp"
#include "http.hpp"
#include "model.hpp"
#include "ogg.hpp"
#include "pxa_clock.h"
#include "pxa_permission.h"
#include "pxa_storage.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
namespace {
using namespace translate;
constexpr uint32_t Paper = 0xf4f7f6, White = 0xffffff, Ink = 0x203b38,
                   Muted = 0x718580, Accent = 0x178577, Soft = 0xe4efeb,
                   Line = 0xd9e6e0, Error = 0xba5047;
constexpr uint64_t NetPermission = 1, PlayPermission = 2, MicPermission = 3,
                   LoadSettings = 10, SaveSettings = 11,
                   WindowSnapshot = 0x5452414e000001;
constexpr unsigned Back = 10, Edit = 11, DoTranslate = 12, Mic = 13,
                   Listen = 14, Swap = 15, Source = 16, Target = 17,
                   History = 18, Options = 19, DoneEdit = 20, ClearInput = 21,
                   ShowOutput = 22, PagePrevious = 23, PageNext = 24,
                   SaveForm = 25, NetworkOptions = 26, VoiceOptions = 27,
                   KeepHistory = 28, ClearHistory = 29, ProviderMemory = 30,
                   ProviderLibre = 31, PresetGroq = 32, PresetOpenAI = 33,
                   PresetCustom = 34, StopRecord = 35, Quit = 36,
                   InputNode = 1000, UrlNode = 1001, KeyNode = 1002,
                   ModelNode = 1003, HistoryBase = 100, LanguageBase = 200;
enum class View {
  Home,
  Editor,
  Languages,
  History,
  Settings,
  Network,
  VoiceSettings,
  Voice,
  Output
};
enum class Job { None, Translation, Recognition, Tts };
enum class Pending { None, Translation, Recording, Tts };
NativeCanvas scene;
media::Decoder decoder;
media::PcmPacket pcm_packet;
Settings settings, editing;
Record record, history[HistoryCount];
unsigned history_count = 0, language_page = 0, asr_frame_ms = 20;
unsigned history_page = 0, settings_page = 0, form_page = 0;
int output_total = 0, output_page_height = 1;
bool choosing_source = false, dirty = true, background = false,
     loading_settings = true, settings_busy = false, settings_dirty = false,
     recording = false, voice_wanted = false, net_pending = false,
     play_pending = false, mic_pending = false, audio_ready = false,
     tts_active = false;
View view = View::Home;
Job job = Job::None;
Pending pending = Pending::None;
Http http;
OggOpus ogg;
uint64_t permission_net = 0, permission_play = 0, permission_mic = 0,
         capture = 0, capture_token = 0, capture_deadline = 0,
         capture_started = 0, audio = 0, audio_open_token = 0,
         audio_commit_token = 0, serial = 100, ui_at = 0;
char message[192]{}, draft[InputBytes]{}, pending_output[OutputBytes]{},
    part_text[OutputBytes]{}, post_body[InputBytes * 6 + 2048]{},
    multipart_head[2048]{}, multipart_tail[96]{};
uint8_t control[4096]{}, storage_value[1800]{};
size_t translate_at = 0, translate_next = 0, tts_at = 0, tts_next = 0,
       mp3_at = 0;
unsigned pcm_frames = 0, pcm_offset = 0, audio_rate = 0, audio_channels = 0;
uint64_t playback_frames = 0, audio_deadline = 0, notice_until = 0;
int output_scroll = 0;
struct Rect {
  int x, y, w, h;
  bool contains(int px, int py) const {
    return px >= x && py >= y && px < x + w && py < y + h;
  }
};
Rect input_box{}, output_box{};
uint64_t now_ms() {
  timespec t{};
  clock_gettime(CLOCK_MONOTONIC, &t);
  return uint64_t(t.tv_sec) * 1000 + t.tv_nsec / 1000000;
}
void status(const char *s) {
  std::snprintf(message, sizeof(message), "%s", s);
  dirty = true;
  if (view == View::Editor || view == View::Network ||
      view == View::VoiceSettings || view == View::Settings ||
      view == View::Output ||
      (view == View::Home && record.output[0] && std::strcmp(s, "翻译完成")))
    notice_until = now_ms() + 4500;
}
void close(uint64_t &handle) {
  if (handle)
    pxa_close_handle(handle);
  handle = 0;
}
void cancel(uint64_t &token) {
  if (token)
    pxa_cancel(token);
  token = 0;
}
void render();
void click(unsigned);
void start_translation();
void start_tts();
void start_recording();
void stop_recording(bool);
void resume_pending();
void change_view(View next) {
  view = next;
  notice_until = 0;
  output_scroll = 0;
  form_page = 0;
  settings_page = 0;
  scene.invalidate();
  dirty = true;
}
void stop_audio() {
  cancel(audio_open_token);
  cancel(audio_commit_token);
  close(audio);
  audio_ready = tts_active = false;
  pcm_frames = pcm_offset = 0;
  pcm_packet.reset();
  decoder.reset();
}
void abort_job() {
  http.cancel();
  job = Job::None;
  pending = Pending::None;
  voice_wanted = false;
  cancel(capture_token);
  close(capture);
  recording = false;
  ogg.size = 0;
  stop_audio();
}
void save_settings() { settings_dirty = true; }
void pump_settings() {
  if (loading_settings || settings_busy || !settings_dirty)
    return;
  size_t n =
      translate::save_settings(settings, storage_value, sizeof(storage_value));
  uint32_t size = 0;
  if (!n ||
      !pxa_storage_build(control, sizeof(control), PXA_STORAGE_SET,
                         SaveSettings, "translator.settings.v1", 22,
                         storage_value, n, &size) ||
      pxa_control_packet(control, size)) {
    settings_dirty = false;
    status("设置暂未保存，请稍后重试");
    return;
  }
  settings_busy = true;
  settings_dirty = false;
}
void request_permission(uint64_t token, const char *name, const char *scope) {
  uint32_t size = 0;
  if (!pxa_permission_build(control, sizeof(control), PXA_PERMISSION_ACQUIRE,
                            token, name, std::strlen(name),
                            (const uint8_t *)scope, std::strlen(scope),
                            &size) ||
      pxa_control_packet(control, size)) {
    pending = Pending::None;
    status("无法请求权限，请重试");
    return;
  }
  if (token == NetPermission)
    net_pending = true;
  else if (token == PlayPermission)
    play_pending = true;
  else
    mic_pending = true;
}
bool ensure_net(Pending action) {
  if (permission_net)
    return true;
  pending = action;
  if (!net_pending)
    request_permission(NetPermission, "net.client", "web");
  status("请允许联网后继续");
  return false;
}
void remember() {
  if (!settings.history)
    return;
  if (history_count && !std::strcmp(history[0].input, record.input) &&
      !std::strcmp(history[0].output, record.output) &&
      history[0].source == record.source && history[0].target == record.target)
    return;
  unsigned n = std::min<unsigned>(history_count, HistoryCount - 1);
  for (unsigned i = n; i > 0; --i)
    history[i] = history[i - 1];
  history[0] = record;
  history_count = std::min<unsigned>(history_count + 1, HistoryCount);
}
void complete_translation() {
  std::snprintf(record.output, sizeof(record.output), "%s", pending_output);
  record.source = settings.source;
  record.target = settings.target;
  job = Job::None;
  http.cancel();
  remember();
  output_scroll = 0;
  status("翻译完成");
  pxa_log_write(2, "Translator: translation completed");
}
bool next_translation() {
  char address[513];
  translate_next = translate_at;
  if (!settings.provider) {
    if (!translation_url(record.input, translate_at, settings.source,
                         settings.target, address, sizeof(address),
                         translate_next))
      return false;
    return http.begin(address, permission_net, now_ms());
  }
  if (!endpoint(settings.libre_url, settings.libre_key[0]) ||
      !libre_body(record.input, settings, post_body, sizeof(post_body)))
    return false;
  translate_next = std::strlen(record.input);
  Upload upload{(const uint8_t *)post_body, std::strlen(post_body)};
  return http.begin(settings.libre_url, permission_net, now_ms(),
                    "application/json", &upload, 1);
}
void start_translation() {
  if (loading_settings) {
    status("正在读取设置…");
    return;
  }
  if (!valid_input(record.input)) {
    status(record.input[0] ? "文字过长或格式无效，请分段输入"
                           : "先输入要翻译的文字");
    return;
  }
  if (!ensure_net(Pending::Translation))
    return;
  abort_job();
  translate_at = 0;
  pending_output[0] = 0;
  record.output[0] = 0;
  if (settings.source == settings.target) {
    std::snprintf(pending_output, sizeof(pending_output), "%s", record.input);
    complete_translation();
    return;
  }
  job = Job::Translation;
  if (!next_translation()) {
    job = Job::None;
    status(settings.provider ? "请先配置文本接口地址" : "无法准备翻译请求");
    return;
  }
  status("正在翻译…");
}
const char *network_error() {
  if (http.status == 401 || http.status == 403)
    return "接口未授权，请检查密钥和账号权限";
  if (http.status == 429)
    return "请求频繁或额度不足，请稍后重试";
  if (http.status == 404)
    return "接口地址或模型不存在，请检查设置";
  if (http.status >= 300 && http.status < 400)
    return "接口发生跳转，请填写最终服务地址";
  if (http.status == 400 || http.status == 415 || http.status == 422)
    return "服务不接受当前模型或音频格式";
  if (http.status >= 500)
    return "服务暂时不可用，请稍后重试";
  return "网络连接或响应失败，请重试";
}
void fail_job(const char *why) {
  http.cancel();
  job = Job::None;
  close(capture);
  recording = false;
  voice_wanted = false;
  stop_audio();
  status(why);
  pxa_log_write(3, "Translator: operation failed");
}
bool next_tts() {
  char address[513];
  if (!tts_url(record.output, tts_at, record.target, address, sizeof(address),
               tts_next))
    return false;
  job = Job::Tts;
  return http.begin(address, permission_net, now_ms(), nullptr, nullptr, 0,
                    nullptr, sizeof(http.data));
}
void start_tts() {
  if (!record.output[0]) {
    status("先翻译，再听译文");
    return;
  }
  if (!ensure_net(Pending::Tts))
    return;
  if (!permission_play) {
    pending = Pending::Tts;
    if (!play_pending)
      request_permission(PlayPermission, "audio.playback", "media");
    return;
  }
  abort_job();
  tts_at = 0;
  playback_frames = 0;
  if (!next_tts()) {
    fail_job("无法准备朗读请求");
    return;
  }
  status("正在准备朗读…");
}
void start_recording() {
  if (loading_settings)
    return;
  if (!endpoint(settings.asr_url, settings.asr_key[0]) ||
      !model_name(settings.asr_model) ||
      ((settings.asr_preset < 2) && !settings.asr_key[0])) {
    editing = settings;
    change_view(View::VoiceSettings);
    status("先填写语音服务的密钥");
    return;
  }
  if (!ensure_net(Pending::Recording))
    return;
  if (!permission_mic) {
    pending = Pending::Recording;
    if (!mic_pending)
      request_permission(MicPermission, "audio.capture", "microphone");
    return;
  }
  abort_job();
  ogg.reset();
  voice_wanted = true;
  change_view(View::Voice);
  status("正在打开麦克风…");
  capture_token = ++serial;
  capture_deadline = now_ms() + 10000;
  uint8_t payload[8];
  pxa_store_u64(payload, permission_mic);
  uint32_t bytes = 0;
  if (!pxa_build_message(control, sizeof(control), PXA_AUDIO_SERVICE, 6,
                         capture_token, payload, sizeof(payload), &bytes) ||
      pxa_control_packet(control, bytes)) {
    capture_token = 0;
    voice_wanted = false;
    status("此设备不支持录音，可继续使用文字翻译");
  }
}
void stop_recording(bool send) {
  close(capture);
  cancel(capture_token);
  recording = voice_wanted = false;
  if (!send) {
    ogg.size = 0;
    status("已取消录音");
    return;
  }
  if (ogg.granule < 4800 || !ogg.finish()) {
    status("没有录到语音，请再试一次");
    return;
  }
  int head = std::snprintf(
      multipart_head, sizeof(multipart_head),
      "--pxa-translator-multipart-v1\r\nContent-Disposition: form-data; "
      "name=\"model\"\r\n\r\n%s\r\n--pxa-translator-multipart-v1\r\nContent-"
      "Disposition: form-data; "
      "name=\"language\"\r\n\r\n%s\r\n--pxa-translator-multipart-v1\r\nContent-"
      "Disposition: form-data; "
      "name=\"response_format\"\r\n\r\njson\r\n--pxa-translator-multipart-"
      "v1\r\nContent-Disposition: form-data; name=\"file\"; "
      "filename=\"speech.ogg\"\r\nContent-Type: audio/ogg\r\n\r\n",
      settings.asr_model, Languages[settings.source].speech);
  std::strcpy(multipart_tail, "\r\n--pxa-translator-multipart-v1--\r\n");
  if (head <= 0 || size_t(head) >= sizeof(multipart_head)) {
    fail_job("录音请求过长");
    return;
  }
  Upload chunks[] = {
      {(const uint8_t *)multipart_head, size_t(head)},
      {ogg.data, ogg.size},
      {(const uint8_t *)multipart_tail, std::strlen(multipart_tail)}};
  job = Job::Recognition;
  if (!http.begin(settings.asr_url, permission_net, now_ms(),
                  "multipart/form-data; boundary=pxa-translator-multipart-v1",
                  chunks, 3, settings.asr_key)) {
    fail_job("无法发送录音，请检查服务设置");
    return;
  }
  status("正在识别语音…");
}
void resume_pending() {
  auto action = pending;
  pending = Pending::None;
  if (background)
    return;
  if (action == Pending::Translation)
    start_translation();
  else if (action == Pending::Recording)
    start_recording();
  else if (action == Pending::Tts)
    start_tts();
}
void pump_recording() {
  if (capture_token && now_ms() > capture_deadline) {
    cancel(capture_token);
    voice_wanted = false;
    status("麦克风未就绪，可稍后重试");
    return;
  }
  if (!recording || !capture)
    return;
  uint8_t opus[1275];
  for (unsigned i = 0; i < 8; ++i) {
    int n = pxa_io(capture, 1, opus, sizeof(opus));
    if (n == PXA_STATUS_WOULD_BLOCK)
      break;
    if (n < 0) {
      stop_recording(false);
      status("录音中断，请重新录制");
      return;
    }
    if (!n)
      break;
    if (n > int(sizeof(opus)) || !ogg.append(opus, n, asr_frame_ms)) {
      stop_recording(true);
      return;
    }
  }
  if (now_ms() - capture_started >= 20000)
    stop_recording(true);
  if (now_ms() - ui_at >= 200)
    dirty = true;
}
void pump_audio() {
  if (!tts_active)
    return;
  if (now_ms() > audio_deadline) {
    fail_job("音频播放超时，请重新播放");
    return;
  }
  for (unsigned work = 0; work < 12; ++work) {
    if (pcm_packet.full()) {
      if (!audio_ready)
        return;
      int n = pcm_packet.write(audio);
      if (n == PXA_STATUS_WOULD_BLOCK)
        return;
      if (n < 0) {
        fail_job("音频输出失败，请重新播放");
        return;
      }
      playback_frames += n;
      continue;
    }
    if (pcm_offset < pcm_frames) {
      if (!audio_ready)
        return;
      pcm_offset += pcm_packet.append(decoder.pcm + pcm_offset * audio_channels,
                                      pcm_frames - pcm_offset);
      continue;
    }
    if (mp3_at >= http.used) {
      if (pcm_packet.frames) {
        if (!audio_ready)
          return;
        int n = pcm_packet.write(audio);
        if (n == PXA_STATUS_WOULD_BLOCK)
          return;
        if (n < 0) {
          fail_job("音频输出失败");
          return;
        }
        playback_frames += n;
        return;
      }
      if (audio && audio_ready) {
        // Wait for the final Host packet to drain before closing the source.
        if (!audio_commit_token) {
          audio_commit_token = ++serial;
          if (pxa_audio_query(audio_commit_token, audio))
            fail_job("无法查询音频播放状态");
        }
        return;
      }
      fail_job("朗读接口没有返回可播放的 MP3");
      return;
    }
    mp3dec_frame_info_t info{};
    int n = decoder.decode(http.data + mp3_at, int(http.used - mp3_at), info);
    if (info.frame_bytes <= 0) {
      mp3_at = http.used;
      continue;
    }
    if (size_t(info.frame_bytes) > http.used - mp3_at) {
      fail_job("朗读音频格式异常");
      return;
    }
    mp3_at += info.frame_bytes;
    if (!n)
      continue;
    if (info.layer != 3 || info.hz < 8000 || info.hz > 48000 ||
        (info.channels != 1 && info.channels != 2)) {
      fail_job("朗读服务没有返回 MP3 音频");
      return;
    }
    pcm_frames = n;
    pcm_offset = 0;
    if (audio_rate != unsigned(info.hz) ||
        audio_channels != unsigned(info.channels) ||
        (!audio && !audio_open_token)) {
      close(audio);
      cancel(audio_open_token);
      audio_ready = false;
      audio_rate = info.hz;
      audio_channels = info.channels;
      pcm_packet.configure(audio_rate, audio_channels);
      audio_open_token = ++serial;
      if (pxa_audio_open_pcm(audio_open_token, permission_play, audio_rate,
                             audio_channels)) {
        audio_open_token = 0;
        fail_job("此设备暂不支持音频播放");
      }
      return;
    }
  }
}
void pump_http() {
  http.pump(now_ms());
  if (!http.done || job == Job::None)
    return;
  if (http.failed) {
    const char *why = network_error();
    fail_job(why);
    return;
  }
  auto completed = job;
  job = Job::None;
  if (completed == Job::Translation) {
    auto result =
        parse_translation(http.data, http.used, settings.provider != 0,
                          part_text, sizeof(part_text));
    if (result != Parse::Ok) {
      fail_job(result == Parse::Quota     ? "翻译额度已用完，请稍后再试"
               : result == Parse::TooLong ? "译文太长，请分段翻译"
                                          : "翻译接口返回的数据无效");
      return;
    }
    size_t at = std::strlen(pending_output), n = std::strlen(part_text);
    if (at + n + 2 >= sizeof(pending_output)) {
      fail_job("译文太长，请分段翻译");
      return;
    }
    std::memcpy(pending_output + at, part_text, n + 1);
    if (translate_next < std::strlen(record.input)) {
      char last = record.input[translate_next - 1];
      if (last == '\n' || last == ' ') {
        pending_output[at + n] = last;
        pending_output[at + n + 1] = 0;
      }
    }
    translate_at = translate_next;
    if (translate_at < std::strlen(record.input)) {
      job = Job::Translation;
      if (!next_translation()) {
        fail_job("无法继续翻译");
        return;
      }
      char text[100];
      std::snprintf(text, sizeof(text), "正在翻译… %u%%",
                    unsigned(translate_at * 100 / std::strlen(record.input)));
      status(text);
    } else
      complete_translation();
  } else if (completed == Job::Recognition) {
    auto parsed =
        parse_transcription(http.data, http.used, draft, sizeof(draft));
    ogg.size = 0;
    if (parsed != Parse::Ok || !valid_input(draft)) {
      fail_job(parsed == Parse::Empty ? "没有识别到文字，请再试一次"
                                      : "识别结果无效或过长，请缩短录音");
      return;
    }
    std::snprintf(record.input, sizeof(record.input), "%s", draft);
    record.output[0] = 0;
    change_view(View::Home);
    status("语音识别完成，正在翻译…");
    start_translation();
  } else {
    tts_active = true;
    audio_deadline = now_ms() + 90000;
    mp3_at = 0;
    pcm_frames = pcm_offset = 0;
    decoder.reset();
    status("正在朗读译文…");
    pxa_log_write(2, "Translator: speech ready");
  }
}

void text(Rect r, const char *value, uint32_t color = Ink,
          uint8_t role = PXA_CANVAS_FONT_CAPTION,
          uint8_t align = PXA_CANVAS_ALIGN_LEFT) {
  if (r.w <= 0 || r.h <= 0)
    return;
  scene.clip(r.x, r.y, r.w, r.h);
  pxa_canvas_text_box_role(
      &scene.frame, r.x + scene.viewport.content.x,
      r.y + scene.viewport.content.y, r.w, r.h, pxa_canvas_rgba(color), role,
      align, PXA_CANVAS_TEXT_ALIGN_MIDDLE, value, std::strlen(value));
  scene.unclip();
}
void icon(Rect r, const char *glyph, uint32_t color = Ink) {
  const char *name = !std::strcmp(glyph, "\xef\x84\xb0")   ? "mic"
                     : !std::strcmp(glyph, "\xef\x87\x9a") ? "history"
                     : !std::strcmp(glyph, "\xef\x83\xac") ? "swap"
                     : !std::strcmp(glyph, "\xef\x80\x94") ? "trash"
                     : !std::strcmp(glyph, "\xef\x81\x8d") ? "stop"
                                                           : nullptr;
  if (name) {
    int side = std::clamp(std::min(r.w, r.h) - 8, 16, 32);
    unsigned physical = uint64_t(side) * scene.density / 65536;
    unsigned source = physical <= 18   ? 18
                      : physical <= 24 ? 24
                      : physical <= 30 ? 30
                                       : 48;
    char path[100];
    std::snprintf(path, sizeof(path), "assets/ui/%s-%06x-%u.png", name,
                  unsigned(color), source);
    pxa_canvas_image(&scene.frame,
                     r.x + (r.w - side) / 2 + scene.viewport.content.x,
                     r.y + (r.h - side) / 2 + scene.viewport.content.y, side,
                     side, 255, 1, path, std::strlen(path));
    return;
  }
  text(r, glyph, color, PXA_CANVAS_FONT_ICON, PXA_CANVAS_ALIGN_CENTER);
}
void button(unsigned id, Rect r, const char *value, bool primary = false,
            bool selected = false) {
  scene.rect(r.x, r.y + 1, r.w, r.h, primary ? 0x116c61 : Line, 10);
  scene.rect(r.x, r.y, r.w, std::max(1, r.h - 1),
             primary    ? Accent
             : selected ? Soft
                        : White,
             10);
  text(r, value, primary ? White : Ink,
       r.w < 125 ? PXA_CANVAS_FONT_CAPTION : PXA_CANVAS_FONT_BODY,
       PXA_CANVAS_ALIGN_CENTER);
  scene.hit(id, r.x, r.y, r.w, r.h);
}
int wrap(const char *s, Rect box, uint32_t color, int offset = 0,
         int height = 20, bool paint = true) {
  if (paint)
    scene.clip(box.x, box.y, box.w, box.h);
  size_t at = 0, n = std::strlen(s);
  int row = 0;
  while (at < n) {
    char line[520];
    size_t end = at, last_space = at;
    int pixels = 0;
    while (end < n && s[end] != '\n') {
      unsigned char c = s[end];
      size_t bytes = c < 128 ? 1 : c < 224 ? 2 : c < 240 ? 3 : 4;
      if (end + bytes > n)
        break;
      int advance = c < 128 ? 7 : 14;
      if (pixels + advance > box.w || end - at + bytes >= sizeof(line))
        break;
      pixels += advance;
      end += bytes;
      if (c == ' ')
        last_space = end;
    }
    if (end == at && s[end] != '\n')
      end = std::min(n, at + 1);
    if (end < n && s[end] != '\n' && last_space > at + (end - at) / 3)
      end = last_space;
    size_t count = std::min(end - at, sizeof(line) - 1);
    std::memcpy(line, s + at, count);
    line[count] = 0;
    int y = box.y + row * height - offset;
    if (paint && y + height > box.y && y < box.y + box.h)
      text({box.x, y, box.w, height}, line, color);
    ++row;
    at = end;
    if (at < n && s[at] == '\n')
      ++at;
  }
  if (paint)
    scene.unclip();
  return row * height;
}
void header(const char *name) {
  int w = scene.w;
  int hh = scene.h < 180 ? 29 : 38;
  icon({4, 3, 32, 32}, "\xef\x81\x93");
  scene.hit(Back, 0, 0, 40, hh);
  if (notice_until > now_ms())
    wrap(message, {42, 2, w - 48, hh - 4}, Accent, 0, 16);
  else
    text({42, 2, w - 48, hh - 3}, name, Ink,
         w < 220 ? PXA_CANVAS_FONT_BODY : PXA_CANVAS_FONT_TITLE);
}
bool field(unsigned id, Rect r, const char *value, unsigned maximum,
           bool secret = false, bool multiline = false) {
  pxa_ui_transaction_t tx{};
  uint32_t next = ++scene.generation;
  bool ok =
      pxa_ui_transaction_begin(&tx, next, PXA_UI_TRANSACTION_PATCH,
                               scene.scratch, sizeof(scene.scratch)) &&
      pxa_ui_create_typed(&tx, id, 1, 0, PXA_UI_NODE_CONTROL,
                          PXA_UI_CONTROL_TEXT_INPUT) &&
      pxa_ui_set_u8(&tx, id, PXA_UI_PROPERTY_POSITION, 1) &&
      pxa_ui_set_length(&tx, id, PXA_UI_PROPERTY_X, PXA_UI_LENGTH_PX,
                        r.x + scene.viewport.content.x) &&
      pxa_ui_set_length(&tx, id, PXA_UI_PROPERTY_Y, PXA_UI_LENGTH_PX,
                        r.y + scene.viewport.content.y) &&
      pxa_ui_set_length(&tx, id, PXA_UI_PROPERTY_WIDTH, PXA_UI_LENGTH_PX,
                        r.w) &&
      pxa_ui_set_length(&tx, id, PXA_UI_PROPERTY_HEIGHT, PXA_UI_LENGTH_PX,
                        r.h) &&
      pxa_ui_set_rgba(&tx, id, PXA_UI_PROPERTY_BACKGROUND,
                      pxa_canvas_rgba(White)) &&
      pxa_ui_set_rgba(&tx, id, PXA_UI_PROPERTY_FOREGROUND,
                      pxa_canvas_rgba(Ink)) &&
      pxa_ui_set_padding(&tx, id, 5, 2, 5, 2) &&
      pxa_ui_set_font_role(&tx, id,
                           multiline ? PXA_UI_FONT_ROLE_BODY
                                     : PXA_UI_FONT_ROLE_CAPTION) &&
      pxa_ui_set_dp(&tx, id, PXA_UI_PROPERTY_RADIUS, 6) &&
      pxa_ui_set_u8(&tx, id, PXA_UI_PROPERTY_TEXT_SINGLE_LINE, !multiline) &&
      pxa_ui_set_u8(&tx, id, PXA_UI_PROPERTY_TEXT_PASSWORD, secret) &&
      pxa_ui_set_u32(&tx, id, PXA_UI_PROPERTY_TEXT_MAX_BYTES, maximum) &&
      pxa_ui_set_text(&tx, id, value, std::strlen(value)) &&
      pxa_ui_set_event_mask(&tx, id,
                            PXA_UI_EVENT_MASK_TEXT | PXA_UI_EVENT_MASK_CLICK) &&
      pxa_ui_transaction_commit(&tx);
  if (ok)
    scene.ui_generation = next;
  else if (tx.active)
    pxa_ui_transaction_cancel(&tx);
  return ok;
}
bool back() {
  if (view == View::Home)
    return false;
  if (view == View::Voice || job == Job::Recognition)
    abort_job();
  if (view == View::Editor && std::strcmp(record.input, draft)) {
    std::snprintf(record.input, sizeof(record.input), "%s", draft);
    record.output[0] = 0;
  }
  if (view == View::Network || view == View::VoiceSettings)
    change_view(View::Settings);
  else
    change_view(View::Home);
  return true;
}
// Layout uses the Host's logical safe viewport; all draw and native control
// coordinates share it, including density, round displays and IME resize.
struct Layout {
  int w, h, m, gap, header, bh, footer;
  explicit Layout(int width, int height)
      : w(width), h(height),
        m(std::max(scene.margin(width < 220 ? 6 : 12), (width - 920) / 2)),
        gap(height < 210 ? 4 : 8), header(height < 180 ? 29 : 38),
        bh(height < 180   ? 25
           : height < 260 ? 31
                          : 36),
        footer(height - bh - 5) {}
  int width() const { return w - 2 * m; }
  unsigned language_columns() const { return std::clamp(width() / 94, 2, 3); }
  unsigned language_rows() const {
    return std::clamp((footer - header - 9) / 35, 1, 4);
  }
  unsigned history_capacity() const {
    return std::max(1, (footer - header - 8) / 69);
  }
  unsigned settings_capacity() const {
    return std::clamp((footer - header - 7) / 39, 1, 5);
  }
};
const char *action_label(unsigned id) {
  if (id >= LanguageBase && id < LanguageBase + LanguageCount)
    return Languages[id - LanguageBase].name;
  if (id >= HistoryBase && id < HistoryBase + history_count) {
    static char name[32];
    std::snprintf(name, sizeof(name), "翻译记录 %u", id - HistoryBase + 1);
    return name;
  }
  switch (id) {
  case Back:
    return "返回";
  case Edit:
    return "输入原文";
  case DoTranslate:
    return "翻译";
  case Mic:
    return "语音翻译";
  case Listen:
    return "朗读译文";
  case Swap:
    return "交换语言";
  case Source:
    return "原文语言";
  case Target:
    return "译文语言";
  case History:
    return "翻译记录";
  case Options:
    return "设置";
  case DoneEdit:
    return "完成并翻译";
  case ClearInput:
    return "清空原文";
  case ShowOutput:
    return "查看完整译文";
  case PagePrevious:
    return "上一页";
  case PageNext:
    return "下一页";
  case SaveForm:
    return "保存设置";
  case NetworkOptions:
    return "文本翻译接口";
  case VoiceOptions:
    return "语音服务与密钥";
  case KeepHistory:
    return "本次记录开关";
  case ClearHistory:
    return "清空本次记录";
  case ProviderMemory:
    return "MyMemory";
  case ProviderLibre:
    return "LibreTranslate";
  case PresetGroq:
    return "Groq";
  case PresetOpenAI:
    return "OpenAI";
  case PresetCustom:
    return "自定义";
  case StopRecord:
    return recording                 ? "完成并翻译"
           : job == Job::Recognition ? "取消识别"
                                     : "开始说话";
  case Quit:
    return "退出应用";
  default:
    return "操作";
  }
}
const char *language_name(unsigned i, int width) {
  static constexpr const char *short_names[] = {
      "简中", "英语", "日语", "韩语", "法语", "德语", "西语", "俄语", "繁中"};
  return width < 64 ? short_names[i] : Languages[i].name;
}
void page_buttons(const Layout &l, bool enabled = true) {
  if (!enabled)
    return;
  int side = std::min(56, (l.width() - 8) / 3);
  button(PagePrevious, {l.m, l.footer, side, l.bh}, "上页");
  button(PageNext, {l.w - l.m - side, l.footer, side, l.bh}, "下页");
}
void render() {
  dirty = false;
  Layout l(scene.viewport.content.w, scene.viewport.content.h);
  const int w = l.w, h = l.h, m = l.m;
  scene.begin(w, h, Paper);
  if (w < 120 || h < 118) {
    scene.invalidate();
    wrap("显示区域较小，请关闭键盘后继续", {4, 4, w - 8, h - 8}, Ink);
    scene.hit(Quit, 4, std::max(0, h - 24), w - 8, 24);
    scene.present();
    scene.control(scene.hits[0], "退出应用", 0);
    return;
  }
  Rect fields[3]{};
  unsigned field_ids[3]{};
  const char *field_values[3]{};
  unsigned field_count = 0;
  bool native_secret[3]{};
  auto input = [&](unsigned id, Rect r, const char *value,
                   bool secret = false) {
    fields[field_count] = r;
    field_ids[field_count] = id;
    field_values[field_count] = value;
    native_secret[field_count++] = secret;
  };
  auto title = [&](const char *name) { header(name); };
  if (view == View::Home) {
    scene.rect(m, 4, 26, 26, Accent, 8);
    text({m, 4, 26, 26}, "译", White, PXA_CANVAS_FONT_BODY,
         PXA_CANVAS_ALIGN_CENTER);
    text({m + 33, 2, l.width() - 100, 30}, "随译", Ink,
         w < 220 ? PXA_CANVAS_FONT_CAPTION : PXA_CANVAS_FONT_TITLE);
    icon({w - m - 62, 1, 30, 30}, "\xef\x87\x9a");
    scene.hit(History, w - m - 62, 0, 30, 31);
    icon({w - m - 30, 1, 30, 30}, "\xef\x80\x93");
    scene.hit(Options, w - m - 30, 0, 30, 31);
    int bar_y = l.header + 3, bar_h = h < 210 ? 25 : 32;
    int half = (l.width() - 30) / 2;
    button(Source, {m, bar_y, half, bar_h},
           language_name(settings.source, half));
    icon({m + half, bar_y, 30, bar_h}, "\xef\x83\xac", Accent);
    scene.hit(Swap, m + half, bar_y, 30, bar_h);
    button(Target, {m + half + 30, bar_y, half, bar_h},
           language_name(settings.target, half));
    int top = bar_y + bar_h + l.gap, cards = l.footer - top - l.gap;
    bool wide = w >= 360 && w > h;
    bool single = !wide && cards < 78;
    if (wide) {
      int cw = (l.width() - l.gap) / 2;
      input_box = {m, top, cw, cards};
      output_box = {m + cw + l.gap, top, cw, cards};
    } else if (single) {
      input_box = output_box = {m, top, l.width(), cards};
    } else {
      int first = std::max(34, cards * 40 / 100);
      input_box = {m, top, l.width(), first};
      output_box = {m, top + first + l.gap, l.width(), cards - first - l.gap};
    }
    if (!single || !record.output[0]) {
      scene.rect(input_box.x, input_box.y, input_box.w, input_box.h, White, 10);
      int cap = input_box.h > 65 ? 22 : 0;
      if (cap)
        text({input_box.x + 10, input_box.y + 2, input_box.w - 20, 20}, "原文",
             Muted);
      wrap(record.input[0] ? record.input : "输入文字",
           {input_box.x + 9, input_box.y + cap + 3, input_box.w - 18,
            input_box.h - cap - 6},
           record.input[0] ? Ink : Muted);
      scene.hit(Edit, input_box.x, input_box.y, input_box.w, input_box.h);
    }
    if (!single || record.output[0]) {
      scene.rect(output_box.x, output_box.y, output_box.w, output_box.h, White,
                 10);
      int cap = output_box.h > 65 ? 28 : 0;
      if (cap) {
        text({output_box.x + 10, output_box.y + 2, output_box.w - 48, 20},
             "译文", Accent);
        icon({output_box.x + output_box.w - 34, output_box.y, 30, 26},
             tts_active ? "\xef\x81\x8d" : "\xef\x80\xa8", Accent);
        scene.hit(Listen, output_box.x + output_box.w - 34, output_box.y, 30,
                  26);
      }
      wrap(record.output[0] ? record.output
           : message[0]     ? message
                            : "让不同语言，轻松相遇",
           {output_box.x + 9, output_box.y + cap + 3, output_box.w - 18,
            output_box.h - cap - 6},
           record.output[0] ? Ink : Muted);
      scene.hit(ShowOutput, output_box.x, output_box.y + cap, output_box.w,
                output_box.h - cap);
    }
    button(single && record.output[0] ? Edit : DoTranslate,
           {m, l.footer, l.width() - 42, l.bh},
           single && record.output[0] ? "修改原文"
           : job == Job::Translation  ? "取消翻译"
           : record.input[0]          ? "翻译"
                                      : "输入并翻译",
           true);
    scene.rect(w - m - 36, l.footer, 36, l.bh, Soft, 9);
    icon({w - m - 36, l.footer, 36, l.bh}, "\xef\x84\xb0", Accent);
    scene.hit(Mic, w - m - 36, l.footer, 36, l.bh);
  } else if (view == View::Editor) {
    title("输入原文");
    if (h >= 210)
      text({m, l.header, l.width(), 20}, "短句和短段落 · 最多 500 字", Muted);
    int y = l.header + (h >= 210 ? 24 : 3);
    input(InputNode, {m, y, l.width(), 28}, draft);
    if (l.footer - y > 75)
      wrap(draft, {m + 4, y + 36, l.width() - 8, l.footer - y - 42}, Ink);
    button(DoneEdit, {m, l.footer, l.width() - 40, l.bh}, "完成并翻译", true);
    icon({w - m - 34, l.footer, 34, l.bh}, "\xef\x80\x94", Muted);
    scene.hit(ClearInput, w - m - 34, l.footer, 34, l.bh);
  } else if (view == View::Output) {
    title("翻译结果");
    output_page_height = std::max(20, l.footer - l.header - 10);
    // Measure without drawing, then clamp to a full page before presentation.
    output_total =
        wrap(record.output, {m, l.header + 4, l.width(), output_page_height},
             Ink, 0, 20, false);
    output_scroll = std::clamp(output_scroll, 0,
                               std::max(0, output_total - output_page_height));
    wrap(record.output, {m, l.header + 4, l.width(), output_page_height}, Ink,
         output_scroll);
    int side = std::min(56, (l.width() - 8) / 3);
    button(Listen, {m + side + 4, l.footer, l.width() - 2 * side - 8, l.bh},
           tts_active        ? "停止"
           : job == Job::Tts ? "取消"
                             : "朗读",
           true);
    page_buttons(l);
  } else if (view == View::Languages) {
    title(choosing_source ? "原文语言" : "译文语言");
    unsigned cols = l.language_columns(), capacity = cols * l.language_rows();
    unsigned pages = (LanguageCount + capacity - 1) / capacity;
    language_page %= pages;
    int cw = (l.width() - l.gap * (cols - 1)) / cols;
    for (unsigned i = 0;
         i < capacity && language_page * capacity + i < LanguageCount; ++i) {
      unsigned lang = language_page * capacity + i;
      button(LanguageBase + lang,
             {m + int(i % cols) * (cw + l.gap),
              l.header + 4 + int(i / cols) * 35, cw, 30},
             Languages[lang].name, false,
             lang == (choosing_source ? settings.source : settings.target));
    }
    page_buttons(l, pages > 1);
  } else if (view == View::History) {
    title("本次记录");
    unsigned capacity = l.history_capacity(),
             pages = std::max(1u, (history_count + capacity - 1) / capacity);
    history_page %= pages;
    if (!history_count)
      wrap("还没有翻译记录", {m, l.header + 10, l.width(), h - l.header - 25},
           Muted);
    for (unsigned n = 0;
         n < capacity && history_page * capacity + n < history_count; ++n) {
      unsigned i = history_page * capacity + n;
      Rect r{m, l.header + 4 + int(n) * 69, l.width(), 63};
      scene.rect(r.x, r.y, r.w, r.h, White, 9);
      text({r.x + 8, r.y + 4, r.w - 16, 23}, history[i].input, Muted);
      text({r.x + 8, r.y + 30, r.w - 16, 25}, history[i].output);
      scene.hit(HistoryBase + i, r.x, r.y, r.w, r.h);
    }
    page_buttons(l, pages > 1);
    text({m + 55, l.footer, l.width() - 110, l.bh}, "退出后清空", Muted,
         PXA_CANVAS_FONT_CAPTION, PXA_CANVAS_ALIGN_CENTER);
  } else if (view == View::Settings) {
    title("设置");
    unsigned capacity = l.settings_capacity(),
             pages = (5 + capacity - 1) / capacity;
    settings_page %= pages;
    const char *names[] = {"文本翻译接口", "语音服务与密钥",
                           settings.history ? "本次记录 · 已开启"
                                            : "本次记录 · 已关闭",
                           "清空本次记录", "退出应用"};
    unsigned ids[] = {NetworkOptions, VoiceOptions, KeepHistory, ClearHistory,
                      Quit};
    for (unsigned n = 0; n < capacity && settings_page * capacity + n < 5;
         ++n) {
      unsigned i = settings_page * capacity + n;
      button(ids[i], {m, l.header + 4 + int(n) * 39, l.width(), 34}, names[i]);
    }
    page_buttons(l, pages > 1);
  } else if (view == View::Network || view == View::VoiceSettings) {
    bool voice = view == View::VoiceSettings;
    title(voice ? "在线语音服务" : "文本翻译接口");
    int count = voice ? 3 : 2, y = l.header + 2;
    if (voice) {
      int tw = (l.width() - 8) / 3;
      const char *names[] = {"Groq", "OpenAI", "自定义"};
      for (int i = 0; i < 3; ++i)
        button(PresetGroq + i, {m + i * (tw + 4), y, tw, 25}, names[i], false,
               editing.asr_preset == i);
    } else {
      int tw = (l.width() - 4) / 2;
      button(ProviderMemory, {m, y, tw, 25}, "MyMemory", false,
             !editing.provider);
      button(ProviderLibre, {m + tw + 4, y, tw, 25}, "LibreTranslate", false,
             editing.provider);
    }
    y += 29;
    bool paged = l.footer - y < count * 47;
    form_page %= count;
    if (!voice && !editing.provider) {
      wrap("默认免密钥，服务有每日额度。",
           {m, y + 4, l.width(), l.footer - y - 5}, Muted);
    } else {
      const char *labels[] = {"完整接口地址", "API Key · 保存在本机",
                              "语音识别模型"};
      const char *values[] = {voice ? editing.asr_url : editing.libre_url,
                              voice ? editing.asr_key : editing.libre_key,
                              editing.asr_model};
      unsigned ids[] = {UrlNode, KeyNode, ModelNode};
      int step = std::min(62, (l.footer - y - 4) / (paged ? 1 : count));
      for (int n = 0; n < (paged ? 1 : count); ++n) {
        unsigned i = paged ? form_page : n;
        char label[100];
        std::snprintf(label, sizeof(label), paged ? "%s  %u/%u" : "%s",
                      labels[i], i + 1, count);
        text({m, y + n * step, l.width(), 16}, label, Muted);
        input(ids[i],
              {m, y + n * step + 17, l.width(), std::max(20, step - 21)},
              values[i], i == 1);
      }
    }
    int side = paged && (voice || editing.provider)
                   ? std::min(56, (l.width() - 8) / 3)
                   : 0;
    button(SaveForm,
           {m + (side ? side + 4 : 0), l.footer,
            l.width() - (side ? 2 * side + 8 : 0), l.bh},
           "保存", true);
    page_buttons(l, side);
  } else if (view == View::Voice) {
    title("语音翻译");
    int cy = l.header + std::max(22, (l.footer - l.header - 28) / 2);
    int radius = std::clamp((l.footer - l.header - 30) / 2, 14, 30);
    scene.circle(w / 2, cy, radius, recording ? 0xfbe1da : Soft);
    icon({w / 2 - radius, cy - radius, 2 * radius, 2 * radius},
         recording ? "\xef\x81\x8d" : "\xef\x84\xb0",
         recording ? Error : Accent);
    char time[80];
    std::snprintf(time, sizeof(time), "录音 · %us / 20s",
                  unsigned((now_ms() - capture_started) / 1000));
    text({m, std::min(cy + radius + 3, l.footer - 24), l.width(), 21},
         recording ? time : message, Muted, PXA_CANVAS_FONT_CAPTION,
         PXA_CANVAS_ALIGN_CENTER);
    button(StopRecord, {m, l.footer, l.width(), l.bh}, action_label(StopRecord),
           true);
  }
  if (view == View::Home && notice_until > now_ms()) {
    scene.rect(m, l.footer - 38, l.width(), 34, Soft, 8);
    wrap(message, {m + 6, l.footer - 37, l.width() - 12, 32}, Accent, 0, 16);
  }
  // Back comes last in the native tree, so Up from the first action returns.
  // Input fields precede actions, allowing Confirm to open the system IME.
  std::sort(scene.hits, scene.hits + scene.count,
            [](const media::Hit &a, const media::Hit &b) {
              auto rank = [](unsigned id) {
                return id == Back          ? 10000u
                       : id == Edit        ? 0u
                       : id == DoTranslate ? 1u
                       : id == Mic         ? 2u
                                           : id;
              };
              return rank(a.id) < rank(b.id);
            });
  bool changed = scene.previous_count != scene.count;
  for (unsigned i = 0; !changed && i < scene.count; ++i)
    changed = scene.previous[i].hit.id != scene.hits[i].id;
  if (changed)
    scene.invalidate();
  bool fresh = !scene.initialized;
  if (!scene.present())
    pxa_log_write(3, "Translator: canvas presentation failed");
  if (fresh) {
    for (unsigned i = 0; i < field_count; ++i)
      field(field_ids[i], fields[i], field_values[i],
            field_ids[i] == InputNode   ? InputBytes - 1
            : field_ids[i] == ModelNode ? 96
                                        : 256,
            native_secret[i], false);
  }
  for (unsigned i = 0; i < scene.count; ++i)
    if (!scene.control(scene.hits[i], action_label(scene.hits[i].id), i))
      pxa_log_write(3, "Translator: native control failed");
  ui_at = now_ms();
}
void click(unsigned id) {
  if (id == Back) {
    if (!back())
      pxa_window_request_back();
    return;
  }
  if (id == Edit) {
    abort_job();
    std::snprintf(draft, sizeof(draft), "%s", record.input);
    change_view(View::Editor);
  } else if (id == DoneEdit) {
    if (!valid_input(draft)) {
      status("请输入有效文字，最多 500 字");
      return;
    }
    std::snprintf(record.input, sizeof(record.input), "%s", draft);
    record.output[0] = 0;
    change_view(View::Home);
    start_translation();
  } else if (id == ClearInput) {
    draft[0] = 0;
    scene.invalidate();
    dirty = true;
  } else if (id == DoTranslate) {
    if (!record.input[0]) {
      click(Edit);
      return;
    }
    if (job == Job::Translation) {
      abort_job();
      status("已取消翻译");
    } else
      start_translation();
  } else if (id == Mic) {
    if (recording)
      stop_recording(true);
    else
      start_recording();
  } else if (id == StopRecord) {
    if (recording)
      stop_recording(true);
    else if (job == Job::Recognition || capture_token) {
      abort_job();
      status("已取消语音操作");
    } else
      start_recording();
  } else if (id == Listen) {
    if (tts_active || job == Job::Tts) {
      abort_job();
      status("已停止朗读");
    } else
      start_tts();
  } else if (id == Swap) {
    abort_job();
    std::swap(settings.source, settings.target);
    if (record.output[0] && std::strlen(record.output) < sizeof(record.input) &&
        valid_input(record.output))
      std::strcpy(record.input, record.output);
    record.output[0] = 0;
    message[0] = 0;
    save_settings();
    dirty = true;
  } else if (id == Source || id == Target) {
    abort_job();
    choosing_source = id == Source;
    language_page = 0;
    change_view(View::Languages);
  } else if (id >= LanguageBase && id < LanguageBase + LanguageCount) {
    if (choosing_source)
      settings.source = id - LanguageBase;
    else
      settings.target = id - LanguageBase;
    record.output[0] = message[0] = 0;
    save_settings();
    change_view(View::Home);
  } else if (id == PagePrevious || id == PageNext) {
    Layout l(scene.w, scene.h);
    int delta = id == PageNext ? 1 : -1;
    unsigned *page = &language_page, pages = 1;
    if (view == View::Languages) {
      unsigned capacity = l.language_columns() * l.language_rows();
      pages = (LanguageCount + capacity - 1) / capacity;
    } else if (view == View::History) {
      page = &history_page;
      pages = std::max(1u, (history_count + l.history_capacity() - 1) /
                               l.history_capacity());
    } else if (view == View::Settings) {
      page = &settings_page;
      pages = (5 + l.settings_capacity() - 1) / l.settings_capacity();
    } else if (view == View::Network || view == View::VoiceSettings) {
      page = &form_page;
      pages = view == View::VoiceSettings ? 3 : 2;
    } else if (view == View::Output) {
      output_scroll =
          std::clamp(output_scroll + delta * output_page_height, 0,
                     std::max(0, output_total - output_page_height));
      dirty = true;
      return;
    }
    *page = (*page + pages + delta) % pages;
    scene.invalidate();
    dirty = true;
  } else if (id == History) {
    abort_job();
    history_page = 0;
    change_view(View::History);
  } else if (id >= HistoryBase && id < HistoryBase + history_count) {
    record = history[id - HistoryBase];
    settings.source = record.source;
    settings.target = record.target;
    save_settings();
    change_view(View::Home);
  } else if (id == Options) {
    abort_job();
    change_view(View::Settings);
  } else if (id == NetworkOptions || id == VoiceOptions) {
    editing = settings;
    change_view(id == NetworkOptions ? View::Network : View::VoiceSettings);
  } else if (id == ProviderMemory || id == ProviderLibre) {
    editing.provider = id == ProviderLibre;
    scene.invalidate();
    dirty = true;
  } else if (id >= PresetGroq && id <= PresetCustom) {
    asr_preset(editing, id - PresetGroq);
    scene.invalidate();
    dirty = true;
  } else if (id == KeepHistory) {
    settings.history = !settings.history;
    if (!settings.history)
      history_count = 0;
    save_settings();
    dirty = true;
  } else if (id == ClearHistory) {
    history_count = 0;
    for (auto &item : history)
      item = Record{};
    status("本次记录已清空");
    dirty = true;
  } else if (id == Quit) {
    abort_job();
    change_view(View::Home);
    pxa_window_request_back();
  } else if (id == SaveForm) {
    if (view == View::Network) {
      if (editing.provider &&
          (!endpoint(editing.libre_url, editing.libre_key[0]) ||
           !header_value(editing.libre_key))) {
        status("请填写有效地址；使用密钥时需要 HTTPS");
        return;
      }
      settings.provider = editing.provider;
      std::strcpy(settings.libre_url, editing.libre_url);
      std::strcpy(settings.libre_key, editing.libre_key);
    } else {
      if (!endpoint(editing.asr_url, editing.asr_key[0]) ||
          !header_value(editing.asr_key) || !model_name(editing.asr_model)) {
        status("请检查服务地址、密钥和模型名称");
        return;
      }
      settings.asr_preset = editing.asr_preset;
      std::strcpy(settings.asr_url, editing.asr_url);
      std::strcpy(settings.asr_key, editing.asr_key);
      std::strcpy(settings.asr_model, editing.asr_model);
    }
    save_settings();
    change_view(View::Settings);
    status("设置已保存");
  } else if (id == ShowOutput && record.output[0])
    change_view(View::Output);
}
} // namespace

extern "C" int32_t pxa_app_start(const uint8_t *config, uint32_t size) {
  http.tokens = &serial;
  pxa_ui_environment_t environment{};
  if (pxa_ui_parse_start_environment(config, size, &environment))
    scene.environment(environment);
  pxa_window_config_t window{};
  window.edge_to_edge = 1;
  window.status_bar_mode = 1;
  window.navigation_bar_mode = 1;
  pxa_window_configure(&window);
  pxa_window_request_snapshot(WindowSnapshot);
  pxa_clock_set_period(20);
  uint32_t written = 0;
  if (pxa_storage_build(control, sizeof(control), PXA_STORAGE_GET, LoadSettings,
                        "translator.settings.v1", 22, nullptr, 0, &written) &&
      !pxa_control_packet(control, written))
    loading_settings = true;
  else
    loading_settings = false;
  render();
  pxa_log_write(2, "Translator: ready");
  return PXA_STATUS_OK;
}
extern "C" int32_t pxa_app_on_event(const uint8_t *data, uint32_t size) {
  pxa_event_t event{};
  if (!pxa_parse_event(data, size, &event))
    return PXA_EVENT_UNHANDLED;
  if (pxa_window_is_back_requested(&event)) {
    if (!back())
      return PXA_EVENT_UNHANDLED;
  }
  pxa_window_snapshot_t snapshot{};
  if ((pxa_window_parse_metrics_changed(&event, &snapshot) ||
       pxa_window_parse_snapshot(&event, WindowSnapshot, &snapshot)) &&
      scene.window(snapshot)) {
    dirty = true;
  }
  pxa_ui_wire_event_t wire = event;
  if (event.service == PXA_UI_SERVICE) {
    pxa_ui_environment_t environment{};
    if (pxa_ui_parse_environment_event(&wire, &environment) &&
        scene.environment(environment)) {
      dirty = true;
    }
    pxa_ui_event_data_t ui{};
    if (pxa_ui_parse_event(&wire, &ui) &&
        ui.generation == scene.ui_generation) {
      if (ui.kind == PXA_UI_EVENT_ACTION_KIND && ui.node != InputNode &&
          ui.node != UrlNode && ui.node != KeyNode && ui.node != ModelNode)
        click(ui.node);
      if (ui.kind == PXA_UI_EVENT_ACTION_KIND && ui.node == InputNode)
        click(DoneEdit);
      if (ui.kind == PXA_UI_EVENT_TEXT_KIND) {
        dirty = true;
        if (ui.node == InputNode)
          pxa_ui_event_text(&ui, draft, sizeof(draft));
        else if (ui.node == UrlNode)
          pxa_ui_event_text(
              &ui, view == View::Network ? editing.libre_url : editing.asr_url,
              257);
        else if (ui.node == KeyNode)
          pxa_ui_event_text(
              &ui, view == View::Network ? editing.libre_key : editing.asr_key,
              257);
        else if (ui.node == ModelNode)
          pxa_ui_event_text(&ui, editing.asr_model, sizeof(editing.asr_model));
      }
    }
  }
  if (event.service == PXA_PERMISSION_SERVICE &&
      event.opcode == PXA_PERMISSION_ACQUIRE) {
    pxa_permission_acquire_result_t result{};
    if (pxa_permission_parse_acquire(&event, event.token, &result)) {
      if (event.token == NetPermission)
        net_pending = false;
      else if (event.token == PlayPermission)
        play_pending = false;
      else if (event.token == MicPermission)
        mic_pending = false;
      if (result.status) {
        pending = Pending::None;
        status("权限未获允许，可在系统设置中重新开启");
      } else {
        if (event.token == NetPermission)
          permission_net = result.handle;
        else if (event.token == PlayPermission)
          permission_play = result.handle;
        else if (event.token == MicPermission)
          permission_mic = result.handle;
        else if (result.handle)
          pxa_close_handle(result.handle);
        resume_pending();
      }
    }
  }
  http.event(event);
  if (event.service == PXA_AUDIO_SERVICE && event.opcode == 6) {
    if (event.token != capture_token || !voice_wanted) {
      if (event.payload_size >= 12 &&
          int32_t(pxa_load_u32(event.payload)) == 0) {
        uint64_t handle = pxa_load_u64(event.payload + 4);
        if (handle)
          pxa_close_handle(handle);
      }
    } else {
      capture_token = 0;
      if (event.payload_size != 24 || int32_t(pxa_load_u32(event.payload)) ||
          pxa_load_u32(event.payload + 12) != 16000 || event.payload[16] != 1 ||
          event.payload[17] != 1 || pxa_load_u32(event.payload + 20) != 1275) {
        if (event.payload_size >= 12 &&
            int32_t(pxa_load_u32(event.payload)) == 0) {
          uint64_t handle = pxa_load_u64(event.payload + 4);
          if (handle)
            pxa_close_handle(handle);
        }
        voice_wanted = false;
        status("此设备的麦克风暂不可用，可继续使用文字");
      } else {
        asr_frame_ms = pxa_load_u16(event.payload + 18);
        if (asr_frame_ms != 20 && asr_frame_ms != 40 && asr_frame_ms != 60) {
          uint64_t handle = pxa_load_u64(event.payload + 4);
          if (handle)
            pxa_close_handle(handle);
          voice_wanted = false;
          status("不支持的录音格式");
        } else {
          capture = pxa_load_u64(event.payload + 4);
          recording = true;
          capture_started = now_ms();
          status("正在录音，点完成开始翻译");
        }
      }
    }
  }
  if (event.service == PXA_AUDIO_SERVICE &&
      event.opcode == PXA_AUDIO_OPEN_PCM) {
    pxa_audio_pcm_result_t result{};
    if (pxa_audio_parse_pcm(&event, event.token, &result)) {
      if (event.token == audio_open_token && tts_active) {
        audio_open_token = 0;
        if (result.source.status)
          fail_job("音频设备暂不可用");
        else {
          audio = result.source.handle;
          audio_commit_token = ++serial;
          if (pxa_audio_commit_gain(audio_commit_token, audio, 0))
            fail_job("无法配置音频输出");
        }
      } else if (result.source.handle)
        pxa_close_handle(result.source.handle);
    }
  }
  if (event.service == PXA_AUDIO_SERVICE &&
      event.opcode == PXA_AUDIO_COMMIT_GRAPH &&
      event.token == audio_commit_token) {
    int32_t result = 0;
    if (pxa_audio_parse_status(&event, event.token, PXA_AUDIO_COMMIT_GRAPH,
                               &result)) {
      audio_commit_token = 0;
      if (result)
        fail_job("无法配置音频输出");
      else
        audio_ready = true;
    }
  }
  if (event.service == PXA_AUDIO_SERVICE &&
      event.opcode == PXA_AUDIO_QUERY_STATE &&
      event.token == audio_commit_token) {
    pxa_audio_state_result_t state{};
    audio_commit_token = 0;
    if (!pxa_audio_parse_state(&event, event.token, &state) || state.status) {
      fail_job("无法查询音频播放状态");
    } else if (!state.queued_samples) {
      tts_active = false;
      close(audio);
      audio_ready = false;
      tts_at = tts_next;
      if (tts_at < std::strlen(record.output)) {
        if (!next_tts())
          fail_job("无法继续朗读");
      } else {
        status("朗读完成");
        pxa_log_write(2, "Translator: speech completed");
      }
    }
  }
  if (event.service == PXA_STORAGE_SERVICE) {
    if (event.opcode == PXA_STORAGE_GET && event.token == LoadSettings) {
      pxa_storage_get_result_t result{};
      if (pxa_storage_parse_get(&event, LoadSettings, &result)) {
        loading_settings = false;
        if (!result.status)
          load_settings(result.value.data, result.value.size, settings);
        dirty = true;
      }
    } else if (event.token == SaveSettings) {
      int32_t result = 0;
      if (pxa_storage_parse_status(&event, SaveSettings, PXA_STORAGE_SET,
                                   &result)) {
        settings_busy = false;
        if (result)
          status("设置暂未保存，请稍后重试");
      }
    }
  }
  if (event.service == PXA_SERVICE_SYSTEM &&
      event.opcode == PXA_SYSTEM_LIFECYCLE_EVENT && event.payload_size == 1) {
    background = event.payload[0] == PXA_SYSTEM_LIFECYCLE_BACKGROUND;
    if (background) {
      abort_job();
      pxa_clock_set_period(0);
    } else {
      pxa_clock_set_period(20);
      dirty = true;
    }
  }
  if (!background) {
    if (notice_until && now_ms() >= notice_until) {
      notice_until = 0;
      dirty = true;
    }
    pump_http();
    pump_recording();
    pump_audio();
    pump_settings();
    if (dirty && now_ms() - ui_at >= 35)
      render();
  }
  return PXA_EVENT_HANDLED;
}
extern "C" void pxa_app_stop(uint32_t) {
  capture = audio = 0;
  http.handle = http.token =
      0; /* Host has already revoked resources. Do not import here. */
}
