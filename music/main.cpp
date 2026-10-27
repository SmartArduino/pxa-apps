// SPDX-License-Identifier: MIT
#include "../common/media/canvas.hpp"
#include "../common/media/decoder.hpp"
#include "../common/media/pcm_packet.hpp"
#include "../common/media/scroll.hpp"
#include "model.hpp"
#include "pxa_audio.h"
#include "pxa_clock.h"
#include "pxa_http.h"
#include "pxa_image_decode.h"
#include "pxa_permission.h"
#include "pxa_storage.h"
#include "pxa_window.h"
#include "seek.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
namespace {
using media::json::Array;
using media::json::Value;
using music::Track;
uint64_t now() {
  timespec t{};
  clock_gettime(CLOCK_MONOTONIC, &t);
  return uint64_t(t.tv_sec) * 1000 + t.tv_nsec / 1000000;
}
void close(uint64_t &h) {
  if (h) {
    pxa_close_handle(h);
    h = 0;
  }
}
void cancel(uint64_t &t) {
  if (t) {
    pxa_cancel(t);
    t = 0;
  }
}
void log(const char *s) { pxa_log_write(PXA_LOG_LEVEL_INFO, s); }
uint8_t packet[4096], head[4096], json_data[131072], compressed[65536],
    storage_data[1024];
uint64_t serial = 1000, net_permission = 0, audio_permission = 0;
media::Canvas scene;
media::Decoder decoder;
media::PcmPacket pcm_packet;
music::Lyrics lyrics;
Track tracks[12], favs[16], queue[16], current, pending_favorite;
Track discovery[music::MaxTracks], discovery_incoming[music::MaxTracks];
unsigned discovery_count = 0;
bool discovery_pending = false, discovery_cached = false,
     discovery_live = false;
uint64_t discovery_fetched_at = 0, discovery_attempt_at = 0;
uint8_t discovery_cache[PXA_STORAGE_MAX_VALUE]{};
size_t discovery_cache_pending = 0;
char discovery_status[160] = "正在获取推荐新歌…";
void request_discovery(bool force = false);
void pump_discovery();
bool occupied[16]{}, have_current = false, dirty = true, background = false;
unsigned count = 0, queue_count = 0, queue_index = 0, view = 0, focus = 0,
         load_slot = 0;
media::SmoothList scroll;
bool touch_list = false;
int mode = 0, save_slot = -1;
unsigned player_return_view = 0, search_return_view = 0;
uint64_t rendered_second = UINT64_MAX;
int rendered_lyric = -2;
bool saving = false, removing = false;
int width = 320, height = 240;
uint32_t last_buttons = 0;
uint64_t ui_at = 0, render_ms = 0, render_count = 0, decode_ms = 0,
         decode_count = 0;
char catalog_status[160] = "搜索你喜欢的声音。";
char query[97]{}, draft[97]{}, message[160] = "尚未播放",
                               lyric_status[128] = "暂无歌词";
uint64_t stream = 0, play_token = 0, audio = 0, audio_token = 0,
         commit_token = 0, query_token = 0;
unsigned rate = 0, channels = 0, pcm_frames = 0, pcm_offset = 0;
size_t compressed_size = 0, compressed_head = 0;
bool audio_queue_full = false;
bool want_play = false, paused = false, audio_ready = false, buffering = true,
     eof = false, exhausted = false, advance = false;
uint64_t stream_started = 0;
uint64_t sent_frames = 0, played_frames = 0, position = 0, last_data = 0,
         last_pcm = 0, query_at = 0, retry_at = 0, stats_at = 0;
unsigned retries = 0, redirects = 0;
char play_url[513]{};
bool play_headers = false;
uint32_t nonzero = 0;
music::SeekMap seek_map;
uint64_t range_start = 0, decode_byte = 0, position_base = 0, seek_target = 0,
         skip_frames = 0;
bool seek_pending = false, scrub = false, hold_seeking = false;
uint64_t preview_position = 0, hold_at = 0, hold_repeat = 0;
int hold_direction = 0;
uint64_t display_position() {
  return scrub || hold_seeking ? preview_position : position;
}

enum Job { None, Search, Resolve, Detail, Lyric, LrcFallback, Discover };
Job job = None;
uint64_t job_token = 0, job_handle = 0, job_song = 0, job_cooldown = 0,
         job_retry_at = 0;
unsigned job_retries = 0;
size_t json_size = 0;
bool job_headers = false;
char job_url[513]{};
unsigned job_redirects = 0;
uint64_t cover_token = 0, cover_source = 0, image_token = 0, cover = 0,
         cover_song = 0, cover_at = 0, cover_deadline = 0;
bool cover_headers = false, cover_requested = false;
char cover_url[513]{};
unsigned cover_redirects = 0;
const pxa_image_decode_options_t ImageOptions{
    160, 160, 25600, 131072, PXA_IMAGE_FORMAT_JPEG | PXA_IMAGE_FORMAT_PNG};
void status(const char *value) {
  std::snprintf(message, sizeof(message), "%s", value);
  dirty = true;
}
void catalog(const char *value) {
  std::snprintf(catalog_status, sizeof(catalog_status), "%s", value);
  dirty = true;
}
void lyric_message(const char *value) {
  std::snprintf(lyric_status, sizeof(lyric_status), "%s", value);
  dirty = true;
}
bool valid_url(const char *p) {
  if (!p || (std::strncmp(p, "http://", 7) && std::strncmp(p, "https://", 8)))
    return false;
  const char *host = std::strstr(p, "://") + 3;
  if (!*host || *host == '/')
    return false;
  for (const char *c = p; *c; ++c)
    if ((unsigned char)*c < 33 || (unsigned char)*c > 126 || *c == '\\' ||
        (*c == '@' && c < host + std::strcspn(host, "/?#")))
      return false;
  return std::strlen(p) <= 512;
}
bool redirect(char *url, const char *location) {
  char out[513];
  int n = -1;
  if (!std::strncmp(location, "https://", 8) ||
      !std::strncmp(location, "http://", 7))
    n = std::snprintf(out, sizeof(out), "%s", location);
  else {
    const char *host = std::strstr(url, "://");
    if (!host)
      return false;
    if (location[0] == '/' && location[1] == '/')
      n = std::snprintf(out, sizeof(out), "%.*s:%s", int(host - url), url,
                        location);
    else if (location[0] == '/') {
      const char *end = std::strchr(host + 3, '/');
      n = std::snprintf(out, sizeof(out), "%.*s%s",
                        int(end ? end - url : std::strlen(url)), url, location);
    } else {
      const char *end = std::strrchr(host + 3, '/');
      n = std::snprintf(out, sizeof(out), "%.*s/%s",
                        int(end ? end - url : std::strlen(url)), url, location);
    }
  }
  if (n < 0 || size_t(n) >= sizeof(out) || !valid_url(out))
    return false;
  std::memcpy(url, out, n + 1);
  return true;
}
bool header(const pxa_http_response_t &r, const char *key, char *out,
            size_t cap) {
  out[0] = 0;
  size_t offset = 0;
  pxa_net_header_t v{};
  while (pxa_http_header_next(&r, &offset, &v))
    if (pxa_http_header_equal(v.name, v.name_size, key, std::strlen(key))) {
      size_t n = std::min(cap - 1, size_t(v.value_size));
      std::memcpy(out, v.value, n);
      out[n] = 0;
      return true;
    }
  return false;
}
bool http(uint64_t &token, const char *url, int kind) {
  static const uint8_t ua[] =
      "PXA-Music/0.3 (https://github.com/SmartArduino/pxa-apps)";
  static const uint8_t referer[] = "https://music.163.com/";
  char range[64]{};
  if (kind == 1 && range_start)
    std::snprintf(range, sizeof(range), "bytes=%llu-",
                  (unsigned long long)range_start);
  pxa_net_header_t hs[] = {
      {"user-agent", 10, ua, sizeof(ua) - 1},
      {"referer", 7, referer, sizeof(referer) - 1},
      {"range", 5, (const uint8_t *)range, uint16_t(std::strlen(range))}};
  static const char *wanted[] = {"content-type", "location", "retry-after",
                                 "content-range", "content-length"};
  static const uint16_t lens[] = {12, 8, 11, 13, 14};
  pxa_http_request_t r = pxa_http_request_defaults();
  r.url = url;
  r.url_size = std::strlen(url);
  r.permission_handle = net_permission;
  r.max_body_bytes = kind == 1   ? 67108864
                     : kind == 2 ? 131072
                                 : sizeof(json_data);
  r.buffer_bytes = kind == 1 ? 32768 : 8192;
  r.header_bytes = 4096;
  r.timeout_ms = kind == 1 ? PXA_HTTP_MAX_TIMEOUT_MS : 10000;
  r.headers = hs;
  r.header_count = range[0] ? 3 : 2;
  r.wanted_headers = wanted;
  r.wanted_header_sizes = lens;
  r.wanted_header_count = 5;
  token = ++serial;
  if (pxa_http_open(token, &r, packet, sizeof(packet))) {
    token = 0;
    return false;
  }
  return true;
}
void ask(uint64_t token, const char *name, const char *scope) {
  uint32_t n = 0;
  if (!pxa_permission_build(packet, sizeof(packet), PXA_PERMISSION_ACQUIRE,
                            token, name, std::strlen(name),
                            (const uint8_t *)scope, std::strlen(scope), &n) ||
      pxa_control_packet(packet, n))
    status("无法请求权限，请重试。");
}
void reset_audio() {
  cancel(play_token);
  cancel(audio_token);
  cancel(commit_token);
  cancel(query_token);
  close(stream);
  close(audio);
  rate = channels = pcm_frames = pcm_offset = 0;
  compressed_size = compressed_head = 0;
  play_headers = false;
  audio_ready = false;
  audio_queue_full = false;
  buffering = true;
  eof = exhausted = false;
  decoder.reset();
  pcm_packet.reset();
  sent_frames = played_frames = 0;
  position = seek_pending ? seek_target : position_base;
  skip_frames = 0;
  decode_byte = range_start;
  nonzero = 0;
  stats_at = now();
}
void stop() {
  seek_pending = scrub = hold_seeking = false;
  hold_direction = 0;
  range_start = position_base = seek_target = 0;
  want_play = paused = false;
  retry_at = 0;
  reset_audio();
  status("已停止");
}
void open_player();
void start_job(Job, const char *);
void choose(const Track &, bool = false);
const Track *list(unsigned);
void next(int, bool = false);
void fail(const char *text, bool retry = false) {
  if (retry && position && current.duration) {
    seek_target = position;
    seek_pending = true;
    uint64_t pre = position > 1000 ? position - 1000 : 0;
    range_start = seek_map.offset(pre, current.duration);
    position_base = range_start ? pre : 0;
  }
  reset_audio();
  status(text);
  if (retry && want_play && retries < 3)
    retry_at = now() + (2000u << retries++);
  else {
    want_play = false;
    retry_at = 0;
  }
}
void open_player() {
  if (!want_play || background)
    return;
  if (!net_permission) {
    ask(1, "net.client", "web");
    return;
  }
  if (!audio_permission) {
    ask(2, "audio.playback", "media");
    return;
  }
  reset_audio();
  redirects = 0;
  stream_started = last_data = last_pcm = now();
  status(seek_pending ? "正在跳转…" : "正在连接音源…");
  if (!http(play_token, play_url, 1))
    fail("音源连接失败，正在重试…", true);
}
void seek_to(uint64_t target) {
  if (!have_current || !current.duration || !play_url[0] || !want_play)
    return;
  target = std::min<uint64_t>(
      target, current.duration > 500 ? current.duration - 500 : 0);
  uint64_t pre = target > 1000 ? target - 1000 : 0;
  range_start = seek_map.offset(pre, current.duration);
  position_base = range_start ? pre : 0;
  seek_target = target;
  seek_pending = true;
  retry_at = 0;
  retries = 0;
  open_player();
}
void toggle() {
  if (!have_current) {
    if (const Track *t = list(0))
      choose(*t);
    return;
  }
  if (!want_play) {
    choose(current, true);
    return;
  }
  paused = !paused;
  status(paused ? "已暂停" : buffering ? "正在缓冲…" : "正在播放");
}
const Track *list(unsigned n) {
  if (view == 0)
    return n < discovery_count ? &discovery[n] : nullptr;
  if (view == 2) {
    for (unsigned i = 0; i < 16; ++i)
      if (occupied[i] && !n--)
        return &favs[i];
    return nullptr;
  }
  return n < count ? &tracks[n] : nullptr;
}
unsigned list_count() {
  if (view == 0)
    return discovery_count;
  if (view != 2)
    return count;
  unsigned n = 0;
  for (bool x : occupied)
    n += x;
  return n;
}
int favorite(const Track &t) {
  for (unsigned i = 0; i < 16; ++i)
    if (occupied[i] && favs[i].id == t.id)
      return i;
  return -1;
}
void load_favorite() {
  char key[24];
  std::snprintf(key, sizeof(key), "favorite.%u", load_slot);
  pxa_storage_request_get(4, key, std::strlen(key));
}
void save_favorite() {
  if (!have_current || saving)
    return;
  save_slot = favorite(current);
  removing = save_slot >= 0;
  if (save_slot < 0)
    for (unsigned i = 0; i < 16; ++i)
      if (!occupied[i]) {
        save_slot = i;
        break;
      }
  if (save_slot < 0) {
    status("收藏已满，请先取消一个收藏。");
    return;
  }
  char key[24];
  std::snprintf(key, sizeof(key), "favorite.%d", save_slot);
  pending_favorite = current;
  size_t n = music::save_track(current, storage_data, sizeof(storage_data));
  int e = removing ? pxa_storage_request_remove(5, key, std::strlen(key))
          : n      ? pxa_storage_request_set(packet, sizeof(packet), 5, key,
                                             std::strlen(key), storage_data, n)
                   : -1;
  if (e)
    status("收藏保存失败。");
  else
    saving = true;
  dirty = true;
}

void cancel_job() {
  cancel(job_token);
  close(job_handle);
  job = None;
  json_size = 0;
  job_headers = false;
  job_retry_at = 0;
}
void start_job(Job type, const char *url) {
  cancel_job();
  job = type;
  job_song = current.id;
  std::snprintf(job_url, sizeof(job_url), "%s", url);
  job_redirects = 0;
  job_retries = 0;
  if (now() < job_cooldown) {
    if (type == Discover) {
      std::snprintf(discovery_status, sizeof(discovery_status),
                    "服务繁忙，请稍后刷新");
      dirty = true;
    } else if (type == Search || type == Resolve) {
      if (type == Resolve)
        want_play = false;
      status("请求过于频繁，请稍后再试。");
    } else
      lyric_message("请求过于频繁，稍后可重新打开歌词。");
    job = None;
    return;
  }
  if (!net_permission) {
    ask(1, "net.client", "web");
    return;
  }
  if (!http(job_token, job_url, 0)) {
    job = None;
    if (type == Resolve)
      want_play = false;
    if (type == Discover) {
      std::snprintf(discovery_status, sizeof(discovery_status),
                    "无法联网，点击刷新重试");
      dirty = true;
    } else
      status("无法发起网络请求。");
  }
}
void fetch_lyrics() {
  char url[256];
  std::snprintf(url, sizeof(url),
                "http://music.163.com/api/song/lyric?id=%llu&lv=-1&kv=-1&tv=-1",
                (unsigned long long)current.id);
  lyric_message("正在寻找歌词…");
  start_job(Lyric, url);
}
void choose(const Track &t, bool keep_queue) {
  if (view < 3 && !keep_queue)
    player_return_view = view;
  // Reopening the current session must retain the queue, stream, cover,
  // decoder and paused/buffering/seek state. Automatic repeat still restarts.
  if (!keep_queue && have_current && want_play && current.id == t.id) {
    if (view == 3)
      scene.invalidate();
    view = 4;
    dirty = true;
    return;
  }
  if (!keep_queue) {
    queue_count = list_count();
    queue_count = std::min(queue_count, 16u);
    queue_index = 0;
    for (unsigned i = 0; i < queue_count; ++i) {
      queue[i] = *list(i);
      if (queue[i].id == t.id)
        queue_index = i;
    }
  }
  stop();
  cancel_job();
  seek_map = music::SeekMap{};
  play_url[0] = 0; // A new track must resolve its own URL before seeking.
  current = t;
  have_current = true;
  cover_requested = false;
  cancel(cover_token);
  cancel(image_token);
  close(cover_source);
  close(cover);
  lyrics.parse("");
  lyric_message("正在寻找歌词…");
  want_play = true;
  paused = false;
  retries = 0;
  advance = false;
  char url[256];
  std::snprintf(url, sizeof(url),
                "http://music.163.com/api/song/enhance/player/"
                "url?ids=%%5B%llu%%5D&br=128000",
                (unsigned long long)t.id);
  start_job(Resolve, url);
  if (!keep_queue) {
    if (view == 3)
      scene.invalidate();
    view = 4;
  }
  status("正在获取播放地址…");
}
void next(int direction, bool automatic) {
  if (!queue_count)
    return;
  if (automatic && mode == 2) {
    choose(current, true);
    return;
  }
  int i = int(queue_index) + direction;
  if (automatic && mode == 0 && i >= int(queue_count)) {
    want_play = false;
    status("播放完毕");
    return;
  }
  queue_index = (i + int(queue_count)) % int(queue_count);
  choose(queue[queue_index], true);
}
void request_discovery(bool force) {
  if (job == Discover || discovery_pending)
    return;
  if (discovery_attempt_at && now() - discovery_attempt_at < 10000) {
    std::snprintf(discovery_status, sizeof(discovery_status),
                  "刚刚已刷新，请稍后再试");
    dirty = true;
    return;
  }
  if (!force && discovery_live && discovery_fetched_at &&
      uint64_t(std::time(nullptr)) < discovery_fetched_at + 900)
    return;
  discovery_pending = true;
  std::snprintf(discovery_status, sizeof(discovery_status),
                discovery_count ? "正在更新推荐新歌…" : "正在获取推荐新歌…");
  dirty = true;
}
void pump_discovery() {
  // Playback URL / lyrics / explicit search always get the metadata lane first.
  if (!discovery_pending || job != None || background || view != 0)
    return;
  discovery_pending = false;
  if (now() < job_cooldown) {
    std::snprintf(discovery_status, sizeof(discovery_status),
                  "服务繁忙，请稍后刷新");
    dirty = true;
    return;
  }
  discovery_attempt_at = now();
  start_job(Discover, "http://music.163.com/api/personalized/newsong?limit=12");
}
void discovery_failed(const char *reason) {
  discovery_cached = discovery_count != 0;
  std::snprintf(discovery_status, sizeof(discovery_status), "%s%s", reason,
                discovery_count ? " · 显示上次结果" : " · 点击刷新重试");
  dirty = true;
}
void search() {
  char encoded[385], url[513];
  if (!media::json::encode(draft, encoded, sizeof(encoded)) || !draft[0]) {
    status("请输入歌曲或歌手名称。");
    return;
  }
  std::snprintf(query, sizeof(query), "%s", draft);
  std::snprintf(
      url, sizeof(url),
      "http://music.163.com/api/search/get/web?s=%s&type=1&limit=12&offset=0",
      encoded);
  count = 0;
  focus = 0;
  scroll.reset();
  view = 1;
  scene.invalidate();
  start_job(Search, url);
  catalog("正在搜索…");
}
void job_failed(bool transport = false) {
  if (transport && job != None && job_retries < 2 && !background) {
    cancel(job_token);
    close(job_handle);
    json_size = 0;
    job_headers = false;
    job_retry_at = now() + (2000u << job_retries++);
    if (job == Resolve)
      status("网络较慢，正在重试…");
    else if (job == Search)
      catalog("网络较慢，正在重试…");
    else if (job == Discover) {
      std::snprintf(discovery_status, sizeof(discovery_status),
                    "网络较慢，正在重试…");
      dirty = true;
    }
    return;
  }

  Job old = job;
  cancel_job();
  if (old == Discover)
    discovery_failed("推荐暂时不可用");
  else if (old == Resolve)
    fail("音源暂时不可用，可换一首或点击重试。");
  else if (old == Lyric || old == LrcFallback)
    lyric_message("暂未找到歌词，享受音乐本身。");
  else if (old == Detail)
    fetch_lyrics();
  else
    catalog("搜索服务暂不可用，请稍后重试。");
}
void job_done() {
  Value root{json_data, json_size};
  Job finished = job;
  uint64_t song = job_song;
  close(job_handle);
  job = None;
  if (finished != Search && finished != Discover && song != current.id)
    return;
  if (finished == Discover) {
    unsigned n =
        music::parse_discovery(root, discovery_incoming, music::MaxTracks);
    if (!n) {
      discovery_failed("推荐数据暂不可用");
      return;
    }
    std::copy_n(discovery_incoming, n, discovery);
    discovery_count = n;
    discovery_fetched_at = uint64_t(std::max<time_t>(0, std::time(nullptr)));
    discovery_cached = false;
    discovery_live = true;
    if (view == 0) {
      focus = std::min(focus, n - 1);
      scroll.layout(77, height - 47, height < 210 ? 42 : 48, n);
    }
    std::snprintf(discovery_status, sizeof(discovery_status),
                  "网易云 · 推荐新歌 · %u 首", n);
    size_t bytes =
        music::save_discovery(discovery, n, discovery_fetched_at,
                              discovery_cache, sizeof(discovery_cache));
    discovery_cache_pending = bytes;
    // Flash-backed Storage may stall for a commit. Persist only when audio is
    // paused/stopped; a refresh must never interrupt the playback clock.
    char trace[128];
    std::snprintf(trace, sizeof(trace),
                  "MUSIC discovery live count=%u first_id=%llu", n,
                  (unsigned long long)discovery[0].id);
    log(trace);
  } else if (finished == Search) {
    count = music::parse_tracks(root, tracks, 12);
    catalog(count ? "搜索结果 · 点击歌曲播放"
                  : "没有找到歌曲，换个关键词试试。");
  } else if (finished == Resolve) {
    Value item;
    Array a(root.get("data"));
    char type[20]{};
    bool url_cut = false;
    if (!a.next(item) || item.get("code").number() != 200 ||
        !item.get("freeTrialInfo").is_null() ||
        !item.get("url").string(play_url, sizeof(play_url), &url_cut) ||
        url_cut || !valid_url(play_url)) {
      fail("此歌曲需登录、会员或仅提供试听，暂不可播。");
      return;
    }
    item.get("type").string(type, sizeof(type));
    if (std::strcmp(type, "mp3")) {
      fail("此音源暂不支持 MP3 播放。");
      return;
    }
    uint64_t duration = item.get("time").number();
    if (duration && duration <= 14400000)
      current.duration = duration;
    seek_map.bytes = item.get("size").number();
    open_player();
    char url[256];
    std::snprintf(
        url, sizeof(url),
        "http://music.163.com/api/song/detail/?id=%llu&ids=%%5B%llu%%5D",
        (unsigned long long)current.id, (unsigned long long)current.id);
    start_job(Detail, url);
  } else if (finished == Detail) {
    Track t;
    Value item;
    Array a(root.get("songs"));
    if (a.next(item) && music::parse_track(item, t) && t.id == current.id) {
      current = t;
      dirty = true;
    }
    fetch_lyrics();
  } else {
    static char text[32768];
    bool got = (finished == Lyric ? root.get("lrc").get("lyric")
                                  : root.get("syncedLyrics"))
                   .string(text, sizeof(text));
    if (!got && finished == LrcFallback)
      got = root.get("plainLyrics").string(text, sizeof(text));
    if (got && text[0]) {
      lyrics.parse(text);
      lyric_message(lyrics.count
                        ? (lyrics.timed ? "歌词随音乐同步" : "纯文本歌词")
                        : "纯音乐，请静心聆听。");
    } else if (finished == Lyric) {
      char title[385], artist[385], url[513];
      if (media::json::encode(current.name, title, sizeof(title)) &&
          media::json::encode(current.artist, artist, sizeof(artist)) &&
          std::snprintf(url, sizeof(url),
                        "https://lrclib.net/api/"
                        "get?track_name=%s&artist_name=%s&duration=%u",
                        title, artist,
                        current.duration / 1000) < int(sizeof(url)))
        start_job(LrcFallback, url);
      else
        lyric_message("暂未找到歌词。");
    } else
      lyric_message(root.get("instrumental").boolean() ? "纯音乐，请静心聆听。"
                                                       : "暂未找到歌词。");
  }
  dirty = true;
}
void flush_discovery_cache() {
  if (!discovery_cache_pending || (want_play && !paused))
    return;
  size_t bytes = discovery_cache_pending;
  discovery_cache_pending = 0;
  if (pxa_storage_request_set(packet, sizeof(packet), 8, "discovery.latest", 16,
                              discovery_cache, bytes))
    log("MUSIC discovery cache save failed");
}
void pump_job() {
  if (job_retry_at) {
    if (now() >= job_retry_at) {
      job_retry_at = 0;
      if (!http(job_token, job_url, 0))
        job_failed(true);
    }
    return;
  }
  if (!job_handle)
    return;
  if (!job_headers) {
    int n = pxa_http_read_response(job_handle, head, sizeof(head));
    if (n == PXA_STATUS_WOULD_BLOCK)
      return;
    pxa_http_response_t r{};
    if (n < 0 || !pxa_http_parse_response(head, n, &r)) {
      job_failed(true);
      return;
    }
    if (r.status_code >= 300 && r.status_code < 400) {
      char location[513];
      if (++job_redirects > 4 ||
          !header(r, "location", location, sizeof(location)) ||
          !redirect(job_url, location)) {
        job_failed();
        return;
      }
      close(job_handle);
      if (!http(job_token, job_url, 0))
        job_failed();
      return;
    }
    if (r.status_code == 429 || r.status_code == 503) {
      char wait[32];
      header(r, "retry-after", wait, sizeof(wait));
      uint32_t seconds = 60;
      unsigned value = 0;
      if (wait[0]) {
        for (const char *p = wait; *p && value < 86400; ++p) {
          if (*p < '0' || *p > '9') {
            value = 60;
            break;
          }
          value = value * 10 + *p - '0';
        }
        seconds = std::clamp(value, 1u, 86400u);
      }
      job_cooldown = now() + uint64_t(seconds) * 1000;
      job_failed();
      return;
    }
    if (r.status_code != 200) {
      if (job == Lyric || job == LrcFallback) {
        json_size = 0;
        json_data[0] = '{';
        json_data[1] = '}';
        json_size = 2;
        job_done();
      } else
        job_failed();
      return;
    }
    job_headers = true;
  }
  for (unsigned i = 0; i < 3 && job_handle; ++i) {
    if (json_size == sizeof(json_data)) {
      job_failed();
      return;
    }
    int n =
        pxa_http_read(job_handle, json_data + json_size,
                      std::min(size_t(4096), sizeof(json_data) - json_size));
    if (n == PXA_STATUS_WOULD_BLOCK)
      return;
    if (n < 0) {
      job_failed(true);
      return;
    }
    if (!n) {
      job_done();
      return;
    }
    json_size += n;
  }
}
void pump_cover() {
  if (image_token && now() > cover_deadline) {
    cancel(image_token);
    close(cover_source);
    return;
  }
  if (!cover_requested && have_current && current.cover[0] && audio_ready &&
      job == None && !cover_token && !cover_source && !image_token &&
      now() > cover_at + 1000) {
    cover_requested = true;
    cover_song = current.id;
    if (!std::strncmp(current.cover, "https://", 8))
      std::snprintf(cover_url, sizeof(cover_url), "http://%s?param=160y160",
                    current.cover + 8);
    else
      std::snprintf(cover_url, sizeof(cover_url), "%s?param=160y160",
                    current.cover);
    cover_redirects = 0;
    cover_headers = false;
    http(cover_token, cover_url, 2);
  }
  if (!cover_source || image_token)
    return;
  int n = pxa_http_read_response(cover_source, head, sizeof(head));
  if (n == PXA_STATUS_WOULD_BLOCK)
    return;
  pxa_http_response_t r{};
  if (n < 0 || !pxa_http_parse_response(head, n, &r)) {
    close(cover_source);
    return;
  }
  if (r.status_code >= 300 && r.status_code < 400) {
    char location[513];
    if (++cover_redirects > 4 ||
        !header(r, "location", location, sizeof(location)) ||
        !redirect(cover_url, location)) {
      close(cover_source);
      return;
    }
    close(cover_source);
    http(cover_token, cover_url, 2);
    return;
  }
  if (r.status_code != 200) {
    close(cover_source);
    return;
  }
  image_token = ++serial;
  cover_deadline = now() + 12000;
  if (pxa_image_decode(image_token, cover_source, PXA_IMAGE_SOURCE_STREAM,
                       ImageOptions, nullptr)) {
    image_token = 0;
    close(cover_source);
  }
}
void pump_player() {
  if (!want_play || background)
    return;
  if (retry_at) {
    if (now() >= retry_at) {
      retry_at = 0;
      open_player();
    }
    return;
  }
  if (!stream)
    return;
  if (eof && exhausted && !sent_frames && !pcm_packet.frames) {
    fail("未能识别 MP3 音频，请换一首。");
    return;
  }
  if (!paused && !eof && now() - last_data > 20000) {
    fail("网络暂时中断，正在重连…", true);
    return;
  }
  if (!play_headers) {
    int n = pxa_http_read_response(stream, head, sizeof(head));
    if (n == PXA_STATUS_WOULD_BLOCK)
      return;
    pxa_http_response_t r{};
    if (n < 0 || !pxa_http_parse_response(head, n, &r)) {
      fail("音源响应异常。", true);
      return;
    }
    if (r.status_code >= 300 && r.status_code < 400) {
      char location[513];
      if (++redirects > 5 ||
          !header(r, "location", location, sizeof(location)) ||
          !redirect(play_url, location)) {
        fail("音源跳转异常。");
        return;
      }
      close(stream);
      if (!http(play_token, play_url, 1))
        fail("音源连接失败。", true);
      return;
    }
    char value[96];
    if (range_start && r.status_code == 206) {
      uint64_t total = 0;
      if (!header(r, "content-range", value, sizeof(value)) ||
          !music::content_range(value, range_start, total)) {
        fail("音源返回的跳转位置不正确，请重新播放。");
        return;
      }
      if (!seek_map.bytes)
        seek_map.bytes = total;
    } else if (r.status_code == 200) {
      if (range_start) {
        range_start = decode_byte = position_base = 0;
        status("音源不支持分段，正在定位…");
      }
      if (!seek_map.bytes &&
          header(r, "content-length", value, sizeof(value))) {
        uint64_t bytes = 0;
        for (const char *p = value; *p >= '0' && *p <= '9' && bytes < 67108864;
             ++p)
          bytes = bytes * 10 + *p - '0';
        if (bytes < 67108864)
          seek_map.bytes = bytes;
      }
    } else {
      fail("音源暂不支持此位置，请重新播放。");
      return;
    }
    if (seek_pending) {
      char trace[192];
      std::snprintf(
          trace, sizeof(trace),
          "MUSIC seek target_ms=%llu range_start=%llu http=%u base_ms=%llu",
          (unsigned long long)seek_target, (unsigned long long)range_start,
          r.status_code, (unsigned long long)position_base);
      log(trace);
    }
    play_headers = true;
    status(seek_pending ? "正在定位播放位置…" : "正在缓冲…");
  }
  constexpr size_t high_water = sizeof(compressed) / 2;
  for (unsigned i = 0; i < 2 && !eof && compressed_size < high_water; ++i) {
    size_t tail = sizeof(compressed) - compressed_head - compressed_size;
    if (!tail && compressed_head) {
      std::memmove(compressed, compressed + compressed_head, compressed_size);
      compressed_head = 0;
      tail = sizeof(compressed) - compressed_size;
    }
    int n = pxa_http_read(
        stream, compressed + compressed_head + compressed_size,
        std::min({size_t(4096), tail, high_water - compressed_size}));
    if (n == PXA_STATUS_WOULD_BLOCK)
      break;
    if (n < 0) {
      fail("网络连接中断，正在重试…", true);
      return;
    }
    if (!n) {
      eof = true;
      break;
    }
    compressed_size += n;
    last_data = now();
  }
  if (paused && !seek_pending)
    return;
  if (buffering) {
    if (compressed_size < 8192 && !eof)
      return;
    buffering = false;
  }
  uint64_t began = now();
  for (unsigned work = 0; work < 32; ++work) {
    // Submit completed PCM even if the preceding decode used the time slice.
    if (pcm_packet.full() || (eof && exhausted && pcm_packet.frames)) {
      if (paused || !audio_ready)
        return;
      int n = pcm_packet.write(audio);
      if (n == PXA_STATUS_WOULD_BLOCK) {
        audio_queue_full = true;
        return;
      }
      audio_queue_full = false;
      if (n < 0) {
        fail("音频输出异常。");
        return;
      }
      sent_frames += n;
      last_pcm = now();
      if (sent_frames == unsigned(n) || std::strstr(message, "缓冲")) {
        status("正在播放");
        log("MUSIC audio started");
      }
      continue;
    }
    if (pcm_offset < pcm_frames) {
      if (paused || !audio_ready)
        return;
      pcm_offset += pcm_packet.append(decoder.pcm + pcm_offset * channels,
                                      pcm_frames - pcm_offset);
      continue;
    }
    if (compressed_size < (eof ? 1u : 1024u)) {
      if (eof) {
        exhausted = true;
        if (pcm_packet.frames)
          continue;
      } else if (sent_frames && now() - last_pcm > 500) {
        buffering = true;
        status("网络抖动，正在缓冲…");
      }
      return;
    }
    // One 48 kHz stereo frame can take > 12 ms on GameKit. Allow
    // several frames to replenish the bounded Host queue before yielding;
    // WOULD_BLOCK above still ends the pump as soon as that queue is full.
    if (now() - began >= 64 && work)
      return;
    mp3dec_frame_info_t info{};
    const auto decode_begin = now();
    int n = decoder.decode(compressed + compressed_head, compressed_size, info);
    decode_ms += now() - decode_begin;
    ++decode_count;
    if (info.frame_bytes < 0 || size_t(info.frame_bytes) > compressed_size) {
      fail("音频数据异常。");
      return;
    }
    if (info.frame_bytes) {
      if (!range_start && info.hz && info.frame_offset >= 0 &&
          info.frame_offset + 4 <= info.frame_bytes)
        seek_map.frame(compressed + compressed_head + info.frame_offset,
                       info.frame_bytes - info.frame_offset,
                       decode_byte + info.frame_offset, info.bitrate_kbps);
      decode_byte += info.frame_bytes;
      compressed_size -= info.frame_bytes;
      compressed_head += info.frame_bytes;
      if (!compressed_size)
        compressed_head = 0;
    } else {
      if (eof) {
        compressed_size = compressed_head = 0;
        exhausted = true;
      }
      return;
    }
    if (!n)
      continue;
    if (info.layer != 3 || info.hz < 8000 || info.hz > 48000 ||
        (info.channels != 1 && info.channels != 2)) {
      fail("暂不支持此音频格式。");
      return;
    }
    pcm_frames = n;
    pcm_offset = 0;
    if (seek_pending) {
      if (!rate)
        skip_frames = (seek_target - position_base) * info.hz / 1000;
      unsigned drop = std::min<uint64_t>(skip_frames, n);
      pcm_offset = drop;
      skip_frames -= drop;
      if (!skip_frames) {
        seek_pending = false;
        position_base = position = seek_target;
        status(paused ? "已暂停" : "正在缓冲…");
      }
    }
    for (int i = 0; i < n * info.channels; ++i) {
      nonzero += decoder.pcm[i] != 0;
    }
    if (pcm_offset == pcm_frames) {
      rate = info.hz;
      channels = info.channels;
      continue;
    }
    if (rate != unsigned(info.hz) || channels != unsigned(info.channels) ||
        (!audio && !audio_token)) {
      close(audio);
      cancel(audio_token);
      cancel(commit_token);
      cancel(query_token);
      audio_ready = false;
      rate = info.hz;
      channels = info.channels;
      pcm_packet.configure(rate, channels);
      audio_token = ++serial;
      if (pxa_audio_open_pcm(audio_token, audio_permission, rate, channels)) {
        audio_token = 0;
        fail("固件不支持该采样率。");
      }
      return;
    }
  }
}
constexpr unsigned Home = 1000, SearchOpen = 1001, FavoritesOpen = 1002,
                   Submit = 1003, Back = 1004, ShowPlayer = 1005,
                   ShowLyrics = 1006, Play = 1007, Previous = 1008, Next = 1009,
                   Star = 1010, Order = 1011, ScrollUp = 1014,
                   ScrollDown = 1015, FollowLyrics = 1016, Seek = 1017,
                   RefreshDiscovery = 1018, TrackBase = 1100;
constexpr uint32_t Bg = 0x101620, Panel = 0x1b2533, Ink = 0xf5f1ea,
                   Muted = 0x91a2b6, Dim = 0x556a80, Accent = 0xd5b7fa,
                   Peach = 0xefad80;
int touch_x = 0, touch_y = 0;
unsigned touch_action = 0;
int lyric_shift = 0;
uint64_t lyric_manual_at = 0;
void artwork(int x, int y, int size, uint64_t, bool actual = false) {
  if (actual && cover) {
    scene.image(x, y, size, size, cover);
    return;
  }
  // One cached image instead of dozens of gradient/ellipse commands per row.
  unsigned physical = uint64_t(size) * scene.density / 65536;
  unsigned source = physical <= 24 ? 24 : physical <= 48 ? 48 : 160;
  char path[80];
  std::snprintf(path, sizeof(path), "assets/ui/cover-placeholder-%u.png",
                source);
  pxa_canvas_image(&scene.frame, x, y, size, size, 255, 2, path,
                   std::strlen(path));
}
void time_text(uint64_t ms, char *out, size_t cap) {
  unsigned sec = ms / 1000;
  std::snprintf(out, cap, "%u:%02u", sec / 60, sec % 60);
}
void progress(int y) {
  int m = 16, w = width;
  scene.rect(m, y - 2, w - 2 * m, 4, 0x354252, 2);
  int fill =
      current.duration
          ? std::min<uint64_t>(w - 2 * m, display_position() * (w - 2 * m) /
                                              current.duration)
          : 0;
  if (fill > 0)
    scene.rect(m, y - 2, fill, 4, Accent, 2);
  char left[24], right[24];
  const int radius = scrub || hold_seeking ? 6 : 4;
  // Center the visible native-pixel bounds. Independent DP rounding of the
  // track top and handle diameter can otherwise shift their centers by a pixel.
  const auto pixels = [&](int dp) {
    return int(int64_t(dp) * scene.density / 65536);
  };
  const int track_center2 = 2 * pixels(y - 2) + std::max(1, pixels(4)) - 1;
  int handle_top = y - radius, diameter = radius * 2, best = INT32_MAX;
  for (int size = radius * 2 - 1; size <= radius * 2 + 2; ++size)
    for (int top = y - radius - 2; top <= y - radius + 2; ++top) {
      int error = std::abs(2 * pixels(top) + std::max(1, pixels(size)) - 1 -
                           track_center2);
      int score = error * 100 + std::abs(size - radius * 2) * 4 +
                  std::abs(top - (y - radius));
      if (score < best) {
        best = score;
        handle_top = top;
        diameter = size;
      }
    }
  scene.rect(m + fill - diameter / 2, handle_top, diameter, diameter, Accent,
             diameter / 2);
  scene.hit(Seek, m - 6, y - 10, w - 2 * m + 12, 33);
  time_text(display_position(), left, sizeof(left));
  time_text(current.duration, right, sizeof(right));
  scene.text(m, y + 8, 60, left, Muted);
  scene.text(w - m - 60, y + 8, 60, right, Muted, 0, 2);
}
void controls(int y) {
  int cx = width / 2;
  scene.skip_icon(cx - 60, y, -1, Ink);
  scene.hit(Previous, cx - 82, y - 20, 44, 40);
  scene.circle(cx, y, 21, Accent);
  scene.play_icon(cx, y, want_play && !paused, 0x222139);
  scene.hit(Play, cx - 27, y - 26, 54, 52);
  scene.skip_icon(cx + 60, y, 1, Ink);
  scene.hit(Next, cx + 38, y - 20, 44, 40);
  scene.icon(mode == 0   ? "order"
             : mode == 1 ? "repeat"
                         : "repeat-one",
             24, y, Muted);
  scene.hit(Order, 0, y - 20, 48, 40);
}
void text_box(int x, int y, int w, int h, uint32_t c, uint8_t role,
              uint8_t align, uint8_t vertical, const char *text,
              size_t length) {
  if (w <= 0 || h <= 0)
    return;
  pxa_canvas_clip_push(&scene.frame, x, y, w, h);
  pxa_canvas_text_box_role(&scene.frame, x, y, w, h, c, role, align, vertical,
                           text, length);
  pxa_canvas_clip_pop(&scene.frame);
}
void render() {
  const auto render_begin = now();
  dirty = false;
  int w = width, h = height, m = w < 280 ? 12 : 16;
  bool compact = h < 210;
  scene.begin(w, h, Bg);
  if (view == 4) {
    scene.back(20, 21, Ink);
    scene.hit(Back, 0, 0, 40, 40);
    scene.text(44, 14, w - 126, "N O W  P L A Y I N G", Muted);
    constexpr int action_height = 44, action_center = action_height / 2;
    scene.text(w - 88, action_center - 10, 44, "词", Accent, 1, 1);
    scene.hit(ShowLyrics, w - 88, 0, 44, action_height);
    // Match the CJK glyph's visible center, including its font leading.
    scene.heart(w - 22, action_center + 3,
                have_current && favorite(current) >= 0, Accent);
    scene.hit(Star, w - 44, 0, 44, action_height);
    int size = std::min(w / 3, h - 123), x = m, y = compact ? 47 : 52;
    size = std::max(48, size);
    artwork(x, y, size, current.id, true);
    if (!cover && want_play && !paused && audio_ready)
      scene.orbit(x + size / 2, y + size / 2, size / 2 - 3, now(), Accent);
    int tx = x + size + 16, tw = w - tx - m;
    text_box(tx, y + 1, tw, compact ? 32 : 46, pxa_canvas_rgba(Ink),
             compact ? PXA_CANVAS_FONT_BODY : PXA_CANVAS_FONT_TITLE, 0, 0,
             current.name, std::strlen(current.name));
    scene.text(tx, y + (compact ? 35 : 48), tw, current.artist, Muted);
    char line[512];
    int at = lyrics.current(display_position());
    if (at >= 0 && lyrics.count)
      lyrics.get(at, line, sizeof(line));
    else
      std::snprintf(line, sizeof(line), "%s", lyrics.count ? "" : lyric_status);
    const bool show_status =
        !audio_ready || buffering || seek_pending || retry_at || !want_play;
    if (compact) {
      if (show_status)
        scene.text(tx, y + 52, tw, message, Muted);
    } else {
      int ly = y + 73;
      text_box(tx, ly, tw, std::max(17, h - 91 - ly), pxa_canvas_rgba(Accent),
               PXA_CANVAS_FONT_CAPTION, 0, 0, line, std::strlen(line));
      if (show_status)
        scene.text(m, h - 92, w - 2 * m, message, Muted, 0, 1);
    }
    scene.hit(ShowLyrics, tx, y, tw, h - 94 - y);
    progress(h - 72);
    controls(h - 27);
  } else if (view == 5) {
    scene.back(20, 20, Ink);
    scene.hit(Back, 0, 0, 40, 39);
    scene.text(43, 9, w - 90, current.name, Ink, 1);
    scene.text(43, 29, w - 90, current.artist, Muted);
    scene.heart(w - 22, 25, favorite(current) >= 0, Accent);
    scene.hit(Star, w - 44, 0, 44, 40);
    int at = lyrics.current(display_position());
    if (at < 0)
      at = 0;
    if (lyrics.timed && now() - lyric_manual_at > 4000)
      lyric_shift = 0;
    at = std::clamp(at + lyric_shift, 0, std::max(0, int(lyrics.count) - 1));
    char line[512];
    int area = h - 128, cy = 54 + area / 2;
    if (lyrics.count) {
      for (int delta = -2; delta <= 2; ++delta) {
        int i = at + delta;
        if (i < 0 || i >= int(lyrics.count))
          continue;
        int y = compact ? (delta < 0    ? 52
                           : delta == 0 ? 69
                                        : 101)
                        : (delta < 0    ? 101 + delta * 24
                           : delta == 0 ? 101
                                        : 141 + (delta - 1) * 24);
        int hh = delta == 0 ? (compact ? 30 : 36) : (compact ? 14 : 17);
        if (compact && (delta < -1 || delta > 1))
          continue;
        if (y < 51 || y + hh > h - 76)
          continue;
        lyrics.get(i, line, sizeof(line));
        text_box(m, y, w - 2 * m, hh,
                 pxa_canvas_rgba(delta == 0                  ? Ink
                                 : delta == 1 || delta == -1 ? Muted
                                                             : Dim),
                 delta == 0 && !compact ? PXA_CANVAS_FONT_BODY
                                        : PXA_CANVAS_FONT_CAPTION,
                 1, 0, line, std::strlen(line));
      }
    } else {
      scene.text(m, cy - 8, w - 2 * m, "暂无歌词", Ink, 1, 1);
      scene.text(m, cy + 22, w - 2 * m, lyric_status, Muted, 0, 1);
    }
    scene.hit(FollowLyrics, m, 49, w - 2 * m, h - 125);
    progress(h - 72);
    controls(h - 27);
  } else if (view == 3) {
    scene.back(20, 22, Ink);
    scene.hit(Back, 0, 0, 40, 42);
    scene.text(44, 13, w - 60, "寻找下一首喜欢", Ink, 1);
    scene.text(m, 48, w - 2 * m, "歌曲 / 歌手", Muted);
    scene.pill(Submit, m, 111, w - 2 * m, "搜索", true, 0x8c6db3, Ink, Panel);
    scene.text(m, 151, w - 2 * m, "公开曲库，部分歌曲可能需要平台会员。",
               Muted);
  } else {
    scene.text(m, 12, w - 2 * m, "声屿", Ink, 2);
    scene.text(m + 50, 17, 130, "S O N O R A", Muted);
    if (view == 0) {
      if (job == Discover) {
        int angle = (now() / 5) % 360;
        scene.arc(w - 59, 22, 7, angle, angle + 285, Accent, 2);
      } else
        scene.icon("refresh", w - 59, 22, Muted);
      scene.hit(RefreshDiscovery, w - 78, 1, 36, 40);
    }
    scene.search(w - 22, 23, Ink);
    scene.hit(SearchOpen, w - 42, 0, 42, 42);
    scene.pill(Home, m, 43, 69, "发现", view == 0, 0x8c6db3, Ink, Panel);
    scene.pill(FavoritesOpen, m + 76, 43, 69, "收藏", view == 2, 0x8c6db3, Ink,
               Panel);
    char source_label[48];
    std::snprintf(source_label, sizeof(source_label), "%s · %u",
                  view == 0   ? (job == Discover || discovery_pending ? "更新中"
                                 : discovery_cached                   ? "缓存"
                                                                      : "新歌")
                  : view == 1 ? "搜索"
                              : "收藏",
                  list_count());
    int label_width = std::max(0, w - 2 * m - 153);
    scene.text(w - m - label_width, 50, label_width, source_label, Muted, 0, 2);
    int top = 77, bottom = h - 47, row = compact ? 42 : 48;
    unsigned n = list_count();
    scroll.layout(top, bottom, row, n);
    pxa_canvas_clip_push(&scene.frame, m, top, w - 2 * m, bottom - top);
    for (unsigned i = scroll.first(); i < n && scroll.y(i) < bottom; ++i) {
      const Track &t = *list(i);
      int y = scroll.y(i);
      bool selected = have_current && current.id == t.id;
      scene.rect(m, y, w - 2 * m, row,
                 selected     ? 0x292b40
                 : i == focus ? 0x1b2533
                              : Bg,
                 8);
      if (selected)
        scene.rect(m, y + 10, 3, row - 20, Accent, 1);
      artwork(m + 9, y + 7, row - 15, t.id, selected);
      int tx = m + row + 3, tw = w - tx - m - 24;
      scene.text(tx, y + 5, tw, t.name, Ink, compact ? 0 : 1);
      char sub[180];
      std::snprintf(sub, sizeof(sub), "%s%s", t.artist,
                    t.fee == 1 || t.fee == 4 ? " · 会员" : "");
      scene.text(tx, y + 26, tw, sub, Muted);
      if (selected && want_play && !paused && audio_ready)
        scene.activity(w - m - 20, y + 17, now(), Accent);
      scene.line(tx, y + row - 1, w - m - 8, y + row - 1, 0x263142);
      scene.hit(TrackBase + i, m, std::max(top, y), w - 2 * m,
                std::min(bottom, y + row) - std::max(top, y));
    }
    pxa_canvas_clip_pop(&scene.frame);
    if (!n) {
      scene.text(m, top + 12, w - 2 * m,
                 view == 2   ? "暂无收藏"
                 : view == 0 ? discovery_status
                             : catalog_status,
                 Ink, 1, 1);
      scene.text(m, top + 39, w - 2 * m,
                 view == 2   ? ""
                 : view == 0 ? "网易云推荐新歌"
                             : "",
                 Muted, 0, 1);
    }
    if (scroll.maximum > 0)
      scene.rect(w - 7, scroll.thumb_y(), 3, scroll.thumb_size(), Dim, 1);
    scene.line(m, h - 47, w - m, h - 47, 0x2e3948);
    if (have_current)
      artwork(m, h - 39, 30, current.id, true);
    else {
      scene.circle(m + 15, h - 24, 15, 0x29384a);
      scene.circle(m + 15, h - 24, 5, Accent);
    }
    scene.text(m + 42, h - 39, w - 2 * m - 77,
               have_current ? current.name : "尚未播放", Ink);
    scene.text(m + 42, h - 22, w - 2 * m - 77,
               have_current
                   ? (audio_ready && !buffering && !retry_at && !seek_pending
                          ? current.artist
                          : message)
               : view == 0 ? (discovery_cached ? "上次推荐 · 联网后可刷新"
                                               : "网易云 · 推荐新歌")
                           : "发现今日好音乐",
               Muted);
    scene.hit(ShowPlayer, m, h - 45, w - 2 * m - 40, 44);
    scene.circle(w - m - 14, h - 24, 16, Accent);
    scene.play_icon(w - m - 14, h - 24, want_play && !paused, 0x222139);
    scene.hit(Play, w - m - 35, h - 45, 42, 44);
  }
  bool fresh = !scene.initialized;
  if (!scene.present())
    log("MUSIC UI frame failed");
  if (fresh && view == 3 && !scene.input(20, m, 68, w - 2 * m, draft, 96))
    log("MUSIC search input failed");
  ui_at = now();
  render_ms += ui_at - render_begin;
  ++render_count;
  rendered_second = display_position() / 1000;
  rendered_lyric = lyrics.current(display_position());
}
bool navigate_back() {
  const unsigned previous_view = view;
  if (view == 5)
    view = 4;
  else if (view == 4)
    view = player_return_view;
  else if (view == 3)
    view = search_return_view;
  else if (view == 1)
    view = 3;
  else if (view == 2)
    view = 0;
  else
    return false;
  hold_direction = 0;
  hold_seeking = scrub = false;
  touch_list = false;
  scroll.dragging = false;
  // Only the search form owns native input nodes. Canvas-only pages reuse
  // their surface, preserving pointer ownership and avoiding a full rebuild.
  if (previous_view == 3 || view == 3)
    scene.invalidate();
  dirty = true;
  return true;
}
void click(unsigned id) {
  if (id >= TrackBase && id < TrackBase + 16) {
    unsigned i = id - TrackBase;
    if (const Track *t = list(i)) {
      focus = i;
      choose(*t);
    }
    return;
  }
  if (id == RefreshDiscovery)
    request_discovery(true);
  else if (id == Play)
    toggle();
  else if (id == Star)
    save_favorite();
  else if (id == Previous || id == Next)
    next(id == Previous ? -1 : 1);
  else if (id == Order) {
    mode = (mode + 1) % 3;
    const char *label = mode == 0   ? "顺序播放"
                        : mode == 1 ? "列表循环"
                                    : "单曲循环";
    pxa_window_show_toast(1000, label, std::strlen(label));
    dirty = true;
  } else if (id == Back) {
    if (!navigate_back())
      pxa_window_request_back();
  } else if (id == Home || id == FavoritesOpen) {
    view = id == Home ? 0 : 2;
    focus = 0;
    scroll.reset();
    if (view == 0)
      request_discovery();
    scene.invalidate();
    dirty = true;
  } else if (id == SearchOpen) {
    if (view < 3) {
      if (view != 1)
        search_return_view = view;
    }
    view = 3;
    scene.invalidate();
    dirty = true;
  } else if (id == Submit)
    search();
  else if (id == ShowPlayer) {
    if (have_current) {
      if (view < 3) {
        player_return_view = view;
      }
      if (view == 3)
        scene.invalidate();
      view = 4;
      dirty = true;
    } else if (const Track *t = list(0))
      choose(*t);
  } else if (id == ShowLyrics) {
    if (view == 3)
      scene.invalidate();
    view = 5;
    lyric_shift = 0;
    dirty = true;
    if (!lyrics.count && job == None && have_current)
      fetch_lyrics();
  } else if (id == FollowLyrics) {
    lyric_shift = 0;
    dirty = true;
  } else if (id == ScrollUp || id == ScrollDown) {
    unsigned n = list_count();
    if (n)
      focus =
          id == ScrollUp ? (focus ? focus - 1 : 0) : std::min(focus + 1, n - 1);
    scroll.ensure(focus);
    dirty = true;
  }
}
void environment(const pxa_ui_environment_t &e) {
  uint32_t d = e.density_q16 ? e.density_q16 : 65536;
  scene.density = d;
  width = (uint64_t(e.width) * 65536 + d - 1) / d;
  height = (uint64_t(e.height) * 65536 + d - 1) / d;
  dirty = true;
}
} // namespace
extern "C" int32_t pxa_app_start(const uint8_t *data, uint32_t size) {
  pxa_ui_environment_t e{};
  if (pxa_ui_parse_start_environment(data, size, &e))
    environment(e);
  pxa_window_config_t w{};
  w.edge_to_edge = 1;
  w.status_bar_mode = 2;
  w.navigation_bar_mode = 2;
  pxa_window_configure(&w);
  decoder.reset();
  request_discovery();
  pxa_storage_request_get(7, "discovery.latest", 16);
  pxa_clock_set_period(PXA_CLOCK_MIN_PERIOD_MS);
  load_favorite();
  render();
  return PXA_STATUS_OK;
}
extern "C" int32_t pxa_app_on_event(const uint8_t *data, uint32_t size) {
  pxa_event_t e{};
  if (!pxa_parse_event(data, size, &e))
    return PXA_EVENT_UNHANDLED;
  if (pxa_window_is_back_requested(&e) && !navigate_back())
    return PXA_EVENT_UNHANDLED;
  if (e.service == PXA_PERMISSION_SERVICE &&
      e.opcode == PXA_PERMISSION_ACQUIRE) {
    pxa_permission_acquire_result_t r{};
    if (pxa_permission_parse_acquire(&e, e.token, &r)) {
      if (r.status) {
        if (e.token == 1) {
          bool discovering = job == Discover;
          cancel_job();
          if (discovering)
            discovery_failed("请允许联网权限");
          if (!discovering || want_play)
            fail("请允许联网权限后重试。");
        } else
          fail("请允许音频权限后重试。");
      } else if (e.token == 1) {
        net_permission = r.handle;
        if (job != None && !job_token)
          http(job_token, job_url, 0);
        else if (want_play)
          open_player();
      } else if (e.token == 2) {
        audio_permission = r.handle;
        if (want_play)
          open_player();
      }
    }
  } else if (e.service == PXA_NET_SERVICE && e.opcode == PXA_HTTP_OPEN) {
    pxa_http_open_result_t r{};
    if (pxa_http_parse_open(&e, e.token, &r)) {
      if (e.token == play_token) {
        play_token = 0;
        if (r.status)
          fail("音源连接失败，正在重试…", true);
        else
          stream = r.handle;
      } else if (e.token == job_token) {
        job_token = 0;
        if (r.status)
          job_failed(true);
        else
          job_handle = r.handle;
      } else if (e.token == cover_token) {
        cover_token = 0;
        if (!r.status)
          cover_source = r.handle;
      } else if (r.handle)
        pxa_close_handle(r.handle);
    }
  } else if (e.service == PXA_AUDIO_SERVICE && e.opcode == PXA_AUDIO_OPEN_PCM) {
    pxa_audio_pcm_result_t r{};
    if (pxa_audio_parse_pcm(&e, e.token, &r)) {
      if (e.token == audio_token) {
        audio_token = 0;
        if (r.source.status)
          fail("音频设备暂不可用。");
        else {
          audio = r.source.handle;
          commit_token = ++serial;
          if (pxa_audio_commit_gain(commit_token, audio, 0)) {
            commit_token = 0;
            fail("音频配置失败。");
          }
        }
      } else if (r.source.handle)
        pxa_close_handle(r.source.handle);
    }
  } else if (e.service == PXA_AUDIO_SERVICE &&
             e.opcode == PXA_AUDIO_COMMIT_GRAPH && e.token == commit_token) {
    int32_t status_code = 0;
    if (pxa_audio_parse_status(&e, e.token, PXA_AUDIO_COMMIT_GRAPH,
                               &status_code)) {
      commit_token = 0;
      if (status_code)
        fail("音频配置失败。");
      else {
        audio_ready = true;
        cover_at = now();
      }
    }
  } else if (e.service == PXA_AUDIO_SERVICE &&
             e.opcode == PXA_AUDIO_QUERY_STATE && e.token == query_token) {
    pxa_audio_state_result_t r{};
    if (pxa_audio_parse_state(&e, e.token, &r)) {
      query_token = 0;
      if (!r.status && rate) {
        played_frames = r.accepted_samples;
        if (!seek_pending)
          position = position_base + played_frames * 1000 / rate;
        if (!paused && eof && exhausted && pcm_offset == pcm_frames &&
            !pcm_packet.frames && played_frames >= sent_frames)
          advance = true;
      }
    }
  } else if (e.service == PXA_ASSETS_SERVICE &&
             e.opcode == PXA_ASSETS_DECODE_IMAGE) {
    pxa_image_decode_result_t r{};
    if (pxa_image_decode_parse(&e, e.token, PXA_ASSETS_DECODE_IMAGE, 0,
                               ImageOptions, &r)) {
      if (e.token == image_token) {
        image_token = 0;
        close(cover_source);
        if (!r.status && cover_song == current.id) {
          close(cover);
          cover = r.handle;
          dirty = true;
        } else if (r.handle)
          pxa_close_handle(r.handle);
      } else if (r.handle)
        pxa_close_handle(r.handle);
    }
  } else if (e.service == PXA_STORAGE_SERVICE) {
    if (e.opcode == PXA_STORAGE_GET) {
      pxa_storage_get_result_t r{};
      if (pxa_storage_parse_get(&e, e.token, &r)) {
        if (e.token == 7 && r.status == 0 && !discovery_live) {
          uint64_t fetched = 0;
          unsigned n = music::load_discovery(r.value.data, r.value.size,
                                             discovery_incoming,
                                             music::MaxTracks, fetched);
          if (n) {
            std::copy_n(discovery_incoming, n, discovery);
            discovery_count = n;
            discovery_fetched_at = fetched;
            discovery_cached = true;
            dirty = true;
            log("MUSIC discovery cache loaded");
          }
        }
        if (e.token == 4) {
          if (r.status == 0)
            occupied[load_slot] =
                music::load_track(r.value.data, r.value.size, favs[load_slot]);
          if (++load_slot < 16)
            load_favorite();
          else
            dirty = true;
        }
      }
    } else if (e.token == 8 && e.opcode == PXA_STORAGE_SET) {
      int32_t code = 0;
      if (pxa_storage_parse_status(&e, 8, PXA_STORAGE_SET, &code) && code)
        log("MUSIC discovery cache save failed");
    } else if (e.token == 5) {
      int32_t status_code = 0;
      if (pxa_storage_parse_status(
              &e, e.token, removing ? PXA_STORAGE_REMOVE : PXA_STORAGE_SET,
              &status_code)) {
        saving = false;
        if (!status_code) {
          occupied[save_slot] = !removing;
          if (!removing)
            favs[save_slot] = pending_favorite;
        } else
          status("收藏保存失败。");
        dirty = true;
      }
    }
  } else if (e.service == PXA_SERVICE_SYSTEM &&
             e.opcode == PXA_SYSTEM_LIFECYCLE_EVENT && e.payload_size == 1) {
    background = e.payload[0] == PXA_SYSTEM_LIFECYCLE_BACKGROUND;
    if (background) {
      scroll.up(touch_y, now(), true);
      touch_list = false;
      touch_action = 0;
      stop();
      cancel_job();
      cancel(cover_token);
      cancel(image_token);
      close(cover_source);
      pxa_clock_set_period(0);
      last_buttons = 0;
    } else {
      pxa_clock_set_period(PXA_CLOCK_MIN_PERIOD_MS);
      if (view == 0)
        request_discovery();
      dirty = true;
    }
  }
  pxa_ui_environment_t env{};
  if (pxa_ui_parse_environment_event(&e, &env))
    environment(env);
  pxa_ui_event_data_t ui{};
  if (pxa_ui_parse_event(&e, &ui) && ui.node == 20 &&
      ui.kind == PXA_UI_EVENT_TEXT_KIND &&
      ui.generation == scene.ui_generation) {
    pxa_ui_event_text(&ui, draft, sizeof(draft));
    return PXA_EVENT_HANDLED;
  }
  pxa_ui_pointer_data_t p{};
  if (pxa_ui_parse_pointer(&e, &p) && p.node == 2 &&
      p.generation == scene.ui_generation) {
    int x = p.x, y = p.y;
    auto scrub_at = [&]() {
      preview_position = uint64_t(std::clamp(x - 16, 0, width - 32)) *
                         current.duration / std::max(1, width - 32);
      dirty = true;
    };
    if (p.phase == PXA_POINTER_DOWN) {
      touch_x = x;
      touch_y = y;
      touch_action = scene.find(x, y);
      scrub = touch_action == Seek && want_play && current.duration;
      if (scrub)
        scrub_at();
      touch_list = view < 3 && scroll.down(y, now());
    } else if (p.phase == PXA_POINTER_MOVE) {
      if (scrub)
        scrub_at();
      else if (touch_list) {
        scroll.move(y, now());
        dirty = true;
      }
    } else if (p.phase == PXA_POINTER_UP || p.phase == PXA_POINTER_CANCEL) {
      bool canceled = p.phase == PXA_POINTER_CANCEL;
      bool moved = touch_list && scroll.up(y, now(), canceled);
      if (scrub) {
        if (!canceled) {
          scrub_at();
          seek_to(preview_position);
          lyric_shift = 0;
        }
        scrub = false;
      } else if (!canceled && !moved && std::abs(y - touch_y) > 18 &&
                 view == 5) {
        lyric_shift += y < touch_y ? 1 : -1;
        lyric_manual_at = now();
      } else if (!canceled && !moved && std::abs(x - touch_x) < 16 &&
                 std::abs(y - touch_y) < 16 && touch_action &&
                 touch_action == scene.find(x, y))
        click(touch_action);
      touch_list = false;
      touch_action = 0;
      dirty = true;
    }
  }
  pxa_ui_controller_data_t pad{};
  if (pxa_ui_parse_controller(&e, &pad) && pad.controller == 0) {
    uint32_t pressed = pad.buttons & ~last_buttons;
    last_buttons = pad.connected ? pad.buttons : 0;
    if (pressed & PXA_CONTROLLER_A) {
      if (view == 3)
        search();
      else if (view >= 4)
        toggle();
      else if (const Track *t = list(focus))
        choose(*t);
    }
    if (pressed & PXA_CONTROLLER_START) {
      if (view == 3)
        search();
      else
        toggle();
    }
    if (pressed & PXA_CONTROLLER_B) {
      click(Back);
    }
    if (pressed & PXA_CONTROLLER_X)
      save_favorite();
    if (pressed & PXA_CONTROLLER_Y && have_current) {
      if (view < 3)
        player_return_view = view;
      if (view == 3)
        scene.invalidate();
      view = view == 5 ? 4 : 5;
      dirty = true;
      if (view == 5 && !lyrics.count && job == None)
        fetch_lyrics();
    }
    if (pressed & PXA_CONTROLLER_SELECT)
      click(FavoritesOpen);
    if (pressed & PXA_CONTROLLER_LEFT && view == 0)
      click(RefreshDiscovery);
    if (view >= 4) {
      if (pressed & (PXA_CONTROLLER_LEFT | PXA_CONTROLLER_RIGHT)) {
        hold_direction = pressed & PXA_CONTROLLER_LEFT ? -1 : 1;
        hold_at = hold_repeat = now();
        preview_position = position;
        hold_seeking = false;
      }
      uint32_t held =
          hold_direction < 0 ? PXA_CONTROLLER_LEFT : PXA_CONTROLLER_RIGHT;
      if (hold_direction && !(last_buttons & held)) {
        int direction = hold_direction;
        hold_direction = 0;
        if (pad.connected && hold_seeking) {
          hold_seeking = false;
          seek_to(preview_position);
          lyric_shift = 0;
        } else if (pad.connected)
          next(direction);
        hold_seeking = false;
      }
    } else {
      hold_direction = 0;
      hold_seeking = false;
    }
    if (pressed & (PXA_CONTROLLER_UP | PXA_CONTROLLER_DOWN)) {
      if (view == 5) {
        lyric_shift += pressed & PXA_CONTROLLER_DOWN ? 1 : -1;
        lyric_manual_at = now();
        dirty = true;
      } else if (view < 3)
        click(pressed & PXA_CONTROLLER_DOWN ? ScrollDown : ScrollUp);
    }
  }
  uint64_t tick = 0;
  if (pxa_clock_parse_tick(&e, &tick) && !background) {
    if (hold_direction && now() - hold_at >= 400 &&
        now() - hold_repeat >= 200) {
      hold_seeking = true;
      hold_repeat = now();
      preview_position =
          std::clamp<int64_t>(int64_t(preview_position) + hold_direction * 5000,
                              0, current.duration);
      dirty = true;
    }
    pump_player();
    pump_job();
    pump_discovery();
    if (job == Discover && now() - ui_at >= 80)
      dirty = true;
    if (view < 3 && scroll.tick(now()))
      dirty = true;
    pump_cover();
    if (audio_ready && !query_token && now() - query_at >= 250) {
      query_at = now();
      query_token = ++serial;
      if (pxa_audio_query(query_token, audio))
        query_token = 0;
    }
    if (advance) {
      advance = false;
      next(1, true);
    }
    if (want_play && !paused) {
      if (view < 3 && now() - ui_at >= 200)
        dirty = true;
      else if (view >= 4 &&
               (rendered_second != display_position() / 1000 ||
                rendered_lyric != lyrics.current(display_position()) ||
                (view == 4 && !cover && audio_ready && now() - ui_at >= 200)))
        dirty = true;
    }
    if (audio_ready && now() - stats_at >= 5000) {
      char line[256];
      std::snprintf(
          line, sizeof(line),
          "MUSIC frames=%llu played=%llu position_ms=%llu rate=%u nonzero=%u "
          "paused=%d buffer=%u lyrics=%u lyric_index=%d retries=%u",
          (unsigned long long)sent_frames, (unsigned long long)played_frames,
          (unsigned long long)position, rate, nonzero, paused,
          unsigned(compressed_size), lyrics.count, lyrics.current(position),
          retries);
      log(line);
      std::snprintf(
          line, sizeof(line),
          "MUSIC timing render_ms=%llu renders=%llu decode_ms=%llu "
          "decoded=%llu",
          (unsigned long long)render_ms, (unsigned long long)render_count,
          (unsigned long long)decode_ms, (unsigned long long)decode_count);
      log(line);
      stats_at = now();
    }
  }
  if (dirty && !background &&
      (!audio_ready ||
       now() - ui_at >= ((scrub || hold_seeking ||
                          (view < 3 && (scroll.dragging || scroll.easing ||
                                        std::abs(scroll.velocity) > .015f)))
                             ? (paused ? 33u : 100u)
                             : 100u) ||
       !scene.initialized)) {
    pump_player();
    // Repaint only after refilling the bounded PCM queue. Under decode load,
    // retain dirty and let the next event replenish audio before drawing.
    if (!audio_ready || !want_play || paused || buffering || retry_at ||
        !scene.initialized || audio_queue_full) {
      render();
      pump_player();
    }
  }
  flush_discovery_cache();
  return PXA_EVENT_HANDLED;
}
extern "C" void pxa_app_stop(uint32_t) {
  stream = audio = job_handle = cover_source = cover = 0;
  want_play = false;
}
