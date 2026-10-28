// SPDX-License-Identifier: MIT
#include "../common/media/canvas.hpp"
#include "../common/media/decoder.hpp"
#include "../common/media/pcm_packet.hpp"
#include "../common/media/scroll.hpp"
#include "presets.hpp"
#include "pxa_audio.h"
#include "pxa_clock.h"
#include "pxa_http.h"
#include "pxa_permission.h"
#include "pxa_storage.h"
#include "pxa_ui.h"
#include "pxa_window.h"
#include "radio_model.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
namespace {
using radio::Station;
constexpr uint64_t NetPermission = 1, AudioPermission = 2, LoadFavorite = 4,
                   SaveFavorite = 5;
constexpr uint32_t Chinese = 10, Popular = 11, Favorites = 12, Custom = 13,
                   SearchToggle = 14, SearchInput = 20, Search = 21,
                   Reload = 22, PrevPage = 23, NextPage = 24;
constexpr uint32_t Play = 50, Star = 51, Previous = 54, Next = 55,
                   CustomName = 60, CustomUrl = 61, CustomPlay = 62;
constexpr uint32_t StationBase = 100;
constexpr const char *Servers[] = {"de1.api.radio-browser.info"};
uint8_t packet[3072], head[4096], directory_data[65536], raw[4096],
    compressed[65536], favorite_data[1024];
uint32_t generation = 0;
int width = 320, height = 240;
constexpr uint64_t WindowSnapshot = UINT64_C(0x4d45444941000001);
Station stations[radio::CatalogCapacity], favorites[radio::FavoriteCount],
    current, pending_favorite;
bool occupied[radio::FavoriteCount]{},
    have_current = false, dirty = true, background = false, want_play = false,
    play_headers = false, dir_headers = false;
bool search_open = false, audio_ready = false, buffering = true,
     directory_paged = false;
bool directory_loading = false, favorite_busy = false, favorite_remove = false;
unsigned count = 0, view = 0, page = 0, server = 0, storage_slot = 0;
int save_slot = -1;
unsigned custom_return_view = 0;
char draft[97]{}, query[97]{}, custom_name[128]{}, custom_url[513]{},
    directory_url[513]{}, stream_url[513]{};
char directory_status[160] = "正在连接电台目录…",
     play_status[160] = "选一个电台，开始收听";
uint64_t next_token = 1000, net_permission = 0, audio_permission = 0,
         dir_open_token = 0, play_open_token = 0, audio_open_token = 0,
         audio_commit_token = 0;
uint64_t directory_handle = 0, stream = 0, audio = 0, retry_at = 0,
         stream_started = 0, last_received = 0, last_pcm = 0, ui_at = 0;
size_t directory_size = 0, compressed_size = 0, compressed_head = 0;
unsigned pcm_frames = 0, pcm_offset = 0, audio_rate = 0, audio_channels = 0,
         retry_count = 0, redirects = 0, dir_redirects = 0;
uint64_t audio_frames = 0, stats_at = 0;
uint32_t nonzero_samples = 0, blocked_writes = 0;
bool controller_mode = false;
media::Decoder decoder;
media::PcmPacket pcm_packet;
radio::IcyFilter icy;
uint64_t now_ms() {
  timespec t{};
  clock_gettime(CLOCK_MONOTONIC, &t);
  return uint64_t(t.tv_sec) * 1000 + t.tv_nsec / 1000000;
}
void status(char *out, size_t n, const char *s) {
  std::snprintf(out, n, "%s", s);
  dirty = true;
}
void log(const char *s) { pxa_log_write(PXA_LOG_LEVEL_INFO, s); }
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
void stop_stream() {
  cancel(play_open_token);
  cancel(audio_open_token);
  cancel(audio_commit_token);
  audio_ready = false;
  buffering = true;
  close(stream);
  close(audio);
  pcm_frames = pcm_offset = 0;
  compressed_size = compressed_head = 0;
  audio_rate = audio_channels = 0;
  audio_frames = 0;
  nonzero_samples = blocked_writes = 0;
  stats_at = now_ms();
  play_headers = false;
  decoder.reset();
  pcm_packet.reset();
  icy.reset(0);
}
void stop_play() {
  want_play = false;
  retry_at = 0;
  stop_stream();
  status(play_status, sizeof(play_status), "已停止");
}
void failure(const char *text, bool retry) {
  stop_stream();
  status(play_status, sizeof(play_status), text);
  if (retry && want_play && retry_count < 3) {
    retry_at = now_ms() + (2000u << retry_count++);
  } else {
    want_play = false;
    retry_at = 0;
  }
}
void permission(uint64_t token, const char *name, const char *scope) {
  uint32_t n = 0;
  if (!pxa_permission_build(packet, sizeof(packet), PXA_PERMISSION_ACQUIRE,
                            token, name, std::strlen(name),
                            (const uint8_t *)scope, std::strlen(scope), &n) ||
      pxa_control_packet(packet, n)) {
    if (token == NetPermission)
      status(directory_status, sizeof(directory_status),
             "无法请求联网权限，请重试");
    else
      failure("无法请求播放权限", false);
  }
}
int favorite_index(const Station &s) {
  for (unsigned i = 0; i < radio::FavoriteCount; ++i)
    if (occupied[i] && radio::same_station(favorites[i], s))
      return int(i);
  return -1;
}
const Station *list_station(unsigned n) {
  if (view == 2) {
    for (unsigned i = 0; i < radio::FavoriteCount; ++i)
      if (occupied[i] && !n--)
        return &favorites[i];
    return nullptr;
  }
  return n < count ? &stations[n] : nullptr;
}
unsigned list_count() {
  if (view != 2)
    return count;
  unsigned n = 0;
  for (bool b : occupied)
    n += b;
  return n;
}
void load_favorite() {
  char key[24];
  std::snprintf(key, sizeof(key), "favorite.%u", storage_slot);
  pxa_storage_request_get(LoadFavorite, key, std::strlen(key));
}

void favorite_toggle() {
  if (!have_current || favorite_busy)
    return;
  save_slot = favorite_index(current);
  favorite_remove = save_slot >= 0;
  if (save_slot < 0) {
    for (unsigned i = 0; i < radio::FavoriteCount; ++i)
      if (!occupied[i]) {
        save_slot = int(i);
        break;
      }
  }
  if (save_slot < 0) {
    status(play_status, sizeof(play_status),
           "收藏已满（12 个），请先取消一个收藏");
    return;
  }
  char key[24];
  std::snprintf(key, sizeof(key), "favorite.%d", save_slot);
  pending_favorite = current;
  size_t n = radio::save_station(current, favorite_data, sizeof(favorite_data));
  int r = favorite_remove
              ? pxa_storage_request_remove(SaveFavorite, key, std::strlen(key))
          : n ? pxa_storage_request_set(packet, sizeof(packet), SaveFavorite,
                                        key, std::strlen(key), favorite_data, n)
              : PXA_STATUS_INVALID_ARGUMENT;
  if (r)
    status(play_status, sizeof(play_status), "收藏保存失败，请重试");
  else
    favorite_busy = true;
  dirty = true;
}
bool http_open(uint64_t &token, const char *url, bool live) {
  static const uint8_t ua[] = "PXA-InternetRadio/0.1";
  static const uint8_t accept_json[] = "application/json",
                       accept_audio[] =
                           "audio/mpeg, application/octet-stream;q=0.5";
  static const uint8_t no_meta[] = "0";
  pxa_net_header_t headers[] = {
      {"user-agent", 10, ua, sizeof(ua) - 1},
      {"accept", 6, live ? accept_audio : accept_json,
       uint16_t(live ? sizeof(accept_audio) - 1 : sizeof(accept_json) - 1)},
      {"icy-metadata", 12, no_meta, 1}};
  static const char *wanted[] = {"content-type", "icy-metaint", "location"};
  static const uint16_t lengths[] = {12, 11, 8};
  pxa_http_request_t r = pxa_http_request_defaults();
  r.url = url;
  r.url_size = std::strlen(url);
  r.permission_handle = net_permission;
  r.max_body_bytes = live ? UINT64_C(4294967296) : sizeof(directory_data);
  r.buffer_bytes = live ? 65536 : 4096;
  r.header_bytes = 4096;
  r.timeout_ms = live ? PXA_HTTP_MAX_TIMEOUT_MS : 12000;
  r.headers = headers;
  r.header_count = live ? 3 : 2;
  r.wanted_headers = wanted;
  r.wanted_header_sizes = lengths;
  r.wanted_header_count = 3;
  token = ++next_token;
  int e = pxa_http_open(token, &r, packet, sizeof(packet));
  if (e) {
    token = 0;
    return false;
  }
  return true;
}
void load_presets() {
  directory_paged = false;
  count = 0;
  for (const auto &p : radio::Presets) {
    Station &st = stations[count++];
    st = Station{};
    std::snprintf(st.uuid, sizeof(st.uuid), "%s", p.id);
    std::snprintf(st.name, sizeof(st.name), "%s", p.name);
    std::snprintf(st.url, sizeof(st.url), "%s", p.url);
    std::strcpy(st.country, "中国");
    st.bitrate = 64;
  }
}
void directory_failed() {
  close(directory_handle);
  cancel(dir_open_token);
  dir_headers = false;
  directory_size = 0;
  if (++server < std::size(Servers)) {
    radio::search_url(directory_url, sizeof(directory_url), Servers[server],
                      query, view == 0, page * radio::PageSize);
    if (http_open(dir_open_token, directory_url, false))
      return;
  }
  directory_loading = false;
  status(directory_status, sizeof(directory_status),
         count ? "目录暂不可用，可收听内置电台或收藏"
               : "电台目录暂时不可用，可重试或收听收藏");
}
void directory_fetch(bool reset_server = true) {
  if (view == 0 && page == 0 && !query[0] && !count)
    load_presets();
  close(directory_handle);
  cancel(dir_open_token);
  directory_size = 0;
  dir_headers = false;
  dir_redirects = 0;
  if (reset_server)
    server = 0;
  directory_loading = true;
  status(directory_status, sizeof(directory_status), "正在获取可播放电台…");
  if (!net_permission) {
    permission(NetPermission, "net.client", "web");
    return;
  }
  if (!radio::search_url(directory_url, sizeof(directory_url), Servers[server],
                         query, view == 0, page * radio::PageSize) ||
      !http_open(dir_open_token, directory_url, false))
    directory_failed();
}
void open_live() {
  if (!want_play || background)
    return;
  stop_stream();
  std::snprintf(stream_url, sizeof(stream_url), "%s", current.url);
  redirects = 0;
  if (!net_permission) {
    permission(NetPermission, "net.client", "web");
    return;
  }
  if (!audio_permission) {
    permission(AudioPermission, "audio.playback", "media");
    return;
  }
  stream_started = last_received = last_pcm = now_ms();
  status(play_status, sizeof(play_status), "正在连接直播…");
  if (!http_open(play_open_token, stream_url, true))
    failure("无法连接直播，正在重试", true);
}
void choose(const Station &s) {
  current = s;
  if (directory_loading) {
    close(directory_handle);
    cancel(dir_open_token);
    directory_loading = false;
    status(directory_status, sizeof(directory_status),
           "目录更新已暂停，可点击刷新");
  }
  have_current = true;
  want_play = true;
  retry_count = 0;
  retry_at = 0;
  open_live();
  dirty = true;
}
void next_station(int step) {
  unsigned n = list_count();
  if (!n)
    return;
  unsigned at = 0;
  for (unsigned i = 0; i < n; ++i)
    if (radio::same_station(*list_station(i), current)) {
      at = i;
      break;
    }
  at = (at + n + step) % n;
  choose(*list_station(at));
}
bool header_value(const pxa_http_response_t &response, const char *name,
                  char *out, size_t cap) {
  size_t offset = 0;
  pxa_net_header_t h{};
  out[0] = 0;
  while (pxa_http_header_next(&response, &offset, &h))
    if (pxa_http_header_equal(h.name, h.name_size, name, std::strlen(name))) {
      size_t n = std::min(cap - 1, size_t(h.value_size));
      std::memcpy(out, h.value, n);
      out[n] = 0;
      return true;
    }
  return false;
}
bool redirect_url(char *url, const char *location) {
  char next[513];
  int n = -1;
  if (!std::strncmp(location, "https://", 8) ||
      !std::strncmp(location, "http://", 7))
    n = std::snprintf(next, sizeof(next), "%s", location);
  else if (location[0] == '/' && location[1] == '/') {
    const char *colon = std::strchr(url, ':');
    if (colon)
      n = std::snprintf(next, sizeof(next), "%.*s:%s", int(colon - url), url,
                        location);
  } else if (location[0] == '/') {
    const char *host = std::strstr(url, "://");
    if (host) {
      const char *end = std::strchr(host + 3, '/');
      n = std::snprintf(next, sizeof(next), "%.*s%s",
                        int(end ? end - url : std::strlen(url)), url, location);
    }
  }
  if (n < 0 || size_t(n) >= sizeof(next) || !radio::valid_url(next))
    return false;
  std::memcpy(url, next, n + 1);
  return true;
}
bool response_headers(uint64_t handle, bool live) {
  int n = pxa_http_read_response(handle, head, sizeof(head));
  if (n == PXA_STATUS_WOULD_BLOCK)
    return false;
  pxa_http_response_t response{};
  if (n < 0 || !pxa_http_parse_response(head, n, &response)) {
    if (live)
      failure("直播响应异常，正在重试", true);
    else
      directory_failed();
    return false;
  }
  char location[513];
  if (response.status_code >= 300 && response.status_code < 400) {
    bool valid = header_value(response, "location", location, sizeof(location));
    unsigned &hops = live ? redirects : dir_redirects;
    char *url = live ? stream_url : directory_url;
    if (!valid || ++hops > 5 || !redirect_url(url, location)) {
      if (live)
        failure("直播地址跳转异常，请换台", false);
      else
        directory_failed();
      return false;
    }
    if (live) {
      close(stream);
      if (!http_open(play_open_token, url, true))
        failure("直播地址暂时不可用", true);
    } else {
      close(directory_handle);
      if (!http_open(dir_open_token, url, false))
        directory_failed();
    }
    return false;
  }
  if (response.status_code != 200) {
    if (live) {
      char s[100];
      std::snprintf(s, sizeof(s), "电台暂时不可用（HTTP %u），请换台",
                    response.status_code);
      failure(s, false);
    } else
      directory_failed();
    return false;
  }
  if (live) {
    char type[128], meta[24];
    header_value(response, "content-type", type, sizeof(type));
    if (std::strstr(type, "aac") || std::strstr(type, "mpegurl") ||
        std::strstr(type, "text/")) {
      failure("该直播格式暂不支持，请选 MP3 电台", false);
      return false;
    }
    header_value(response, "icy-metaint", meta, sizeof(meta));
    unsigned n = 0;
    for (const char *p = meta; *p; ++p) {
      if (*p < '0' || *p > '9' || n > 1000000) {
        failure("电台元数据异常", false);
        return false;
      }
      n = n * 10 + *p - '0';
    }
    icy.reset(n);
    play_headers = true;
    status(play_status, sizeof(play_status), "正在缓冲…");
  } else
    dir_headers = true;
  return true;
}
void directory_pump() {
  if (!directory_handle)
    return;
  if (!dir_headers && !response_headers(directory_handle, false))
    return;
  for (unsigned i = 0; i < 3 && directory_handle; ++i) {
    size_t room = sizeof(directory_data) - directory_size;
    if (!room) {
      directory_failed();
      return;
    }
    int n = pxa_http_read(directory_handle, directory_data + directory_size,
                          std::min(room, size_t(4096)));
    if (n == PXA_STATUS_WOULD_BLOCK)
      return;
    if (n < 0) {
      directory_failed();
      return;
    }
    if (!n) {
      close(directory_handle);
      directory_loading = false;
      unsigned parsed = 0;
      if (!radio::parse_stations(directory_data, directory_size, stations,
                                 radio::PageSize, parsed)) {
        status(directory_status, sizeof(directory_status),
               "电台目录数据异常，请重试");
        return;
      }
      count = parsed;
      directory_paged = true;
      char s[160];
      if (count)
        std::snprintf(s, sizeof(s), "%u 个 MP3 电台 · 来源 Radio Browser",
                      count);
      else
        std::snprintf(s, sizeof(s), "没有找到可播放电台，试试其他关键词");
      status(directory_status, sizeof(directory_status), s);
      return;
    }
    directory_size += n;
  }
}
void playback_pump() {
  if (!want_play || background)
    return;
  if (retry_at) {
    if (now_ms() >= retry_at) {
      retry_at = 0;
      open_live();
    }
    return;
  }
  if (stream && now_ms() - last_received > 20000) {
    failure("网络暂时没有声音，正在重连", true);
    return;
  }
  if (stream && !audio_frames && now_ms() - stream_started > 20000) {
    failure("无法识别 MP3 音频，请换台", false);
    return;
  }
  if (!stream)
    return;
  if (!play_headers && !response_headers(stream, true))
    return;
  constexpr size_t high_water = sizeof(compressed) / 2;
  for (unsigned i = 0; i < 2 && compressed_size < high_water; ++i) {
    size_t tail = sizeof(compressed) - compressed_head - compressed_size;
    if (!tail && compressed_head) {
      std::memmove(compressed, compressed + compressed_head, compressed_size);
      compressed_head = 0;
      tail = sizeof(compressed) - compressed_size;
    }
    int n = pxa_http_read(
        stream, raw,
        std::min({sizeof(raw), tail, high_water - compressed_size}));
    if (n == PXA_STATUS_WOULD_BLOCK)
      break;
    if (n < 0) {
      failure("直播连接中断，正在重连", true);
      return;
    }
    if (!n) {
      failure("直播已断开，正在重连", true);
      return;
    }
    last_received = now_ms();
    compressed_size +=
        icy.consume(raw, n, compressed + compressed_head + compressed_size);
  }
  if (buffering) {
    if (compressed_size < 24576)
      return;
    buffering = false;
  }
  const uint64_t began = now_ms();
  for (unsigned work = 0; work < 32; ++work) {
    if (pcm_packet.full()) {
      if (!audio || !audio_ready)
        return;
      int n = pcm_packet.write(audio);
      if (n == PXA_STATUS_WOULD_BLOCK) {
        ++blocked_writes;
        return;
      }
      if (n < 0) {
        failure("音频输出异常，请重新播放", false);
        return;
      }
      audio_frames += n;
      last_pcm = now_ms();
      if (audio_frames == unsigned(n) || std::strstr(play_status, "网络抖动")) {
        status(play_status, sizeof(play_status), "正在直播 · MP3");
        log("RADIO audio started");
      }
      continue;
    }
    if (pcm_offset < pcm_frames) {
      if (!audio || !audio_ready)
        return;
      pcm_offset += pcm_packet.append(decoder.pcm + pcm_offset * audio_channels,
                                      pcm_frames - pcm_offset);
      continue;
    }
    if (compressed_size < 1024) {
      if (audio_frames && now_ms() - last_pcm > 500) {
        buffering = true;
        status(play_status, sizeof(play_status), "网络抖动，正在缓冲…");
      }
      return;
    }
    // One 48 kHz stereo frame can take > 12 ms on GameKit. Allow
    // several frames to replenish the bounded Host queue before yielding;
    // WOULD_BLOCK above still ends the pump as soon as that queue is full.
    if (now_ms() - began >= 64 && work)
      return;
    mp3dec_frame_info_t info{};
    int n = decoder.decode(compressed + compressed_head, int(compressed_size),
                           info);
    if (info.frame_bytes < 0 || size_t(info.frame_bytes) > compressed_size) {
      failure("音频数据异常", false);
      return;
    }
    if (info.frame_bytes) {
      compressed_size -= info.frame_bytes;
      compressed_head += info.frame_bytes;
      if (!compressed_size)
        compressed_head = 0;
    } else
      return;
    if (!n)
      continue;
    if (info.layer != 3 || info.hz < 8000 || info.hz > 48000 ||
        (info.channels != 1 && info.channels != 2)) {
      failure("电台音频格式不受支持", false);
      return;
    }
    pcm_frames = n;
    pcm_offset = 0;
    for (int i = 0; i < n * info.channels; ++i) {
      nonzero_samples += decoder.pcm[i] != 0;
    }
    if (audio_rate != unsigned(info.hz) ||
        audio_channels != unsigned(info.channels) ||
        (!audio && !audio_open_token)) {
      close(audio);
      cancel(audio_open_token);
      cancel(audio_commit_token);
      audio_ready = false;
      audio_rate = info.hz;
      audio_channels = info.channels;
      pcm_packet.configure(audio_rate, audio_channels);
      audio_open_token = ++next_token;
      if (pxa_audio_open_pcm(audio_open_token, audio_permission, audio_rate,
                             audio_channels)) {
        audio_open_token = 0;
        failure("固件不支持该音频格式", false);
      }
      return;
    }
  }
  if (now_ms() - last_received > 20000)
    failure("网络暂时没有声音，正在重连", true);
}
unsigned focus_index = 0;
uint32_t last_buttons = 0;
media::Canvas scene;
bool player_view = false;
media::SmoothList scroll;
bool touch_list = false;
int touch_x = 0, touch_y = 0;
unsigned touch_action = 0;
uint64_t animation_at = 0;
constexpr unsigned PlayerView = 70, BackView = 71;
constexpr uint32_t Paper = 0xf5f1e8, Ink = 0x242a2a, Muted = 0x858980,
                   Accent = 0xbd572f, Card = 0xfffdf8, Line = 0xe5dfd2;
void click(uint32_t node);
void render() {
  dirty = false;
  bool compact = height < 210;
  int w = width, h = height, m = scene.margin(compact ? 12 : 16);
  scene.begin(w, h, Paper);
  if (w <= 0 || h <= 0) { scene.present(); return; }
  scene.rect(0, 0, w, 3, Accent);
  if (player_view) {
    scene.back(m + 6, 19, Ink);
    scene.hit(BackView, 0, 3, 40, 33);
    scene.text(40, 12, w - 80, "电波", Muted, 0, 1);
    scene.heart(w - m - 7, 19, have_current && favorite_index(current) >= 0,
                Accent);
    scene.hit(Star, w - 42, 3, 40, 32);
    // Leave separate lines for the station title and buffering/error status
    // above the playback controls, including compact high-density screens.
    int r = std::max(10, std::min((h - 164) / 2, w / 5)), cx = w / 2,
        cy = 40 + r;
    scene.disc(cx, cy, r, Accent, Paper);
    if (want_play && audio_ready)
      scene.orbit(cx, cy, r - 4, now_ms(), 0xa5aaa4);
    scene.rect(cx - 29, cy - 7, 58, 14, Accent, 7);
    scene.text(cx - 29, cy - 5, 58, want_play ? "ON AIR" : "RADIO", 0xffffff, 0,
               1);
    scene.text(m, cy + r + 11, w - 2 * m,
               have_current ? current.name : "网络收音机", Ink, 1, 1);
    if (!audio_ready || buffering || retry_at || !want_play)
      scene.text(m, cy + r + 34, w - 2 * m, play_status, Muted, 0, 1);
    int y = h - 37;
    scene.skip_icon(cx - 67, y, -1, Ink);
    scene.hit(Previous, cx - 89, y - 20, 44, 40);
    scene.circle(cx, y, 22, Accent);
    scene.play_icon(cx, y, want_play, 0xffffff);
    scene.hit(Play, cx - 28, y - 27, 56, 54);
    scene.skip_icon(cx + 67, y, 1, Ink);
    scene.hit(Next, cx + 45, y - 20, 44, 40);

  } else if (search_open || view == 3) {
    scene.back(m + 5, 20, Ink);
    scene.hit(BackView, 0, 3, 40, 34);
    scene.text(42, 12, w - 70, view == 3 ? "添加电台" : "寻找一个声音", Ink, 1);
    const bool short_form = view == 3 && h < 200;
    if (short_form) {
      scene.text(m, 50, 36, "名称", Muted);
      scene.text(m, 90, 36, "地址", Muted);
    } else {
      scene.text(m, 42, w - 2 * m, view == 3 ? "电台名称" : "电台名称 / 关键词",
                 Muted);
      if (view == 3)
        scene.text(m, 103, w - 2 * m, "HTTP / HTTPS MP3 直播地址", Muted);
    }
    int y = view == 3 ? (short_form ? std::max(120, h - 31) : 158) : 105;
    scene.pill(view == 3 ? CustomPlay : Search, m, y, w - 2 * m,
               view == 3 ? "播放" : "搜索", true, Accent, Ink, Line);
    scene.text(m, y + 40, w - 2 * m,
               view == 3 ? "" : "中文与全球电台，由 Radio Browser 提供。",
               Muted);
  } else {
    scene.text(m, 11, w - 2 * m, "电波", Ink, 2);

    scene.search(w - 54, 21, Ink);
    scene.hit(SearchToggle, w - 72, 3, 36, 34);
    scene.text(w - 35, 9, 26, "＋", Ink, 1, 1);
    scene.hit(Custom, w - 37, 3, 36, 34);
    int tabs_y = compact ? 39 : 43, tw = (w - 2 * m - 12) / 3;
    scene.pill(Chinese, m, tabs_y, tw, "推荐", view == 0, Accent, Ink, Line);
    scene.pill(Popular, m + tw + 6, tabs_y, tw, "全球", view == 1, Accent, Ink,
               Line);
    scene.pill(Favorites, m + (tw + 6) * 2, tabs_y, tw, "收藏", view == 2,
               Accent, Ink, Line);
    unsigned n = list_count();
    int top = tabs_y + 33, bottom = h - 48, row = compact ? 43 : 48;
    scroll.layout(top, bottom, row, n);
    if (controller_mode)
      scroll.ensure(focus_index);
    scene.clip(m, top, w - 2 * m, std::max(0, bottom - top));
    for (unsigned i = scroll.first(); i < n && scroll.y(i) < bottom; ++i) {
      const Station &st = *list_station(i);
      int y = scroll.y(i);
      bool playing = have_current && radio::same_station(current, st);
      bool focused = controller_mode && i == focus_index;
      scene.rect(m, y, w - 2 * m, row, focused || playing ? 0xebe3d5 : Paper,
                 8);
      if (playing)
        scene.rect(m, y + 10, 3, row - 20, Accent, 1);
      scene.rect(m + 9, y + 7, row - 15, row - 15, playing ? Accent : 0xe8dfce,
                 8);
      char number[8];
      std::snprintf(number, sizeof(number), "%02u", i + 1);
      scene.text(m + 9, y + 16, row - 15, number, playing ? 0xffffff : Accent,
                 0, 1);
      int tx = m + row + 3, tw = w - tx - m - 24;
      scene.text(tx, y + 5, tw, st.name, Ink, compact ? 0 : 1);
      scene.text(tx, y + 26, tw, st.country[0] ? st.country : "网络电台",
                 Muted);
      if (playing && want_play && audio_ready)
        scene.activity(w - m - 20, y + 17, now_ms(), Accent);
      else
        scene.line(w - m - 13, y + 19, w - m - 9, y + 23, Muted, 2);
      scene.line(tx, y + row - 1, w - m - 8, y + row - 1, Line);
      scene.hit(StationBase + i * 4, m, std::max(top, y), w - 2 * m,
                std::min(bottom, y + row) - std::max(top, y));
    }
    scene.unclip();
    char total[32];
    std::snprintf(total, sizeof(total), "R A D I O · %u", n);
    scene.text(m + 48, 16, w - 2 * m - 115,
               directory_loading ? "正在更新…" : total, Muted);
    if (!n) {
      scene.text(m, top + 10, w - 2 * m,
                 view == 2 ? "把喜欢的声音收进这里" : "暂时没有找到电台", Ink,
                 1, 1);
      scene.text(m, top + 37, w - 2 * m, view == 2 ? "" : directory_status,
                 Muted, 0, 1);
    }
    if (scroll.maximum > 0)
      scene.rect(w - 7, scroll.thumb_y(), 3, scroll.thumb_size(), 0xc3b7a0, 1);
    scene.line(m, h - 47, w - m, h - 47, Line);
    scene.circle(m + 13, h - 23, 12, 0x272f2d);
    scene.circle(m + 13, h - 23, 5, Accent);
    scene.text(m + 34, h - 38, w - 2 * m - 78,
               have_current ? current.name : "网络收音机", Ink, 0);
    scene.text(m + 34, h - 21, w - 2 * m - 78,
               have_current ? play_status : "尚未播放", Muted, 0);
    scene.hit(PlayerView, m, h - 46, w - 2 * m - 40, 45);
    scene.circle(w - m - 14, h - 24, 16, Accent);
    scene.play_icon(w - m - 14, h - 24, want_play, 0xffffff);
    scene.hit(Play, w - m - 34, h - 46, 40, 45);
  }
  bool fresh = !scene.initialized;
  if (!scene.present())
    log("RADIO canvas frame failed");
  generation = scene.ui_generation;
  if (fresh && !player_view && (search_open || view == 3)) {
    if (view == 3) {
      const bool short_form = h < 200;
      const int x = m + (short_form ? 38 : 0), field_width = w - m - x;
      scene.input(CustomName, x, short_form ? 42 : 61, field_width, custom_name,
                   127, Card, Ink);
      scene.input(CustomUrl, x, short_form ? 82 : 120, field_width, custom_url,
                   512, Card, Ink);
    } else
      scene.input(SearchInput, m, 61, w - 2 * m, draft, 96, Card, Ink);
    generation = scene.ui_generation;
  }
  ui_at = now_ms();
}
void layout_changed() {
  width = scene.viewport.content.w;
  height = scene.viewport.content.h;
  touch_list = scroll.dragging = false;
  touch_action = 0;
  dirty = true;
}
void environment(const pxa_ui_environment_t &e) {
  if (scene.environment(e)) layout_changed();
}
bool navigate_back() {
  if (player_view)
    player_view = false;
  else if (search_open)
    search_open = false;
  else if (view == 3)
    view = custom_return_view;
  else if (query[0]) {
    query[0] = draft[0] = 0;
    view = 0;
    load_presets();
    scroll.reset();
    focus_index = 0;
  } else if (view != 0)
    click(Chinese);
  else
    return false;
  touch_list = false;
  scroll.dragging = false;
  scene.invalidate();
  dirty = true;
  return true;
}
void click(uint32_t node) {
  if (node == BackView) {
    if (!navigate_back())
      pxa_window_request_back();
    return;
  }
  if (node == PlayerView) {
    player_view = true;
    scene.invalidate();
    dirty = true;
    return;
  }
  if (node >= Chinese && node <= Custom) {
    scene.invalidate();
    player_view = false;
    search_open = false;
    scroll.reset();
    if (node == Custom)
      custom_return_view = view;
    view = node - Chinese;
    focus_index = 0;
    page = 0;
    if (view < 2) {
      count = 0;
      query[0] = draft[0] = 0;
      if (view == 0) {
        close(directory_handle);
        cancel(dir_open_token);
        directory_loading = false;
        load_presets();
      } else
        directory_fetch();
    }
    dirty = true;
  } else if (node == SearchToggle) {
    scene.invalidate();
    player_view = false;
    search_open = !search_open;
    if (view > 1)
      view = 0;
    dirty = true;
  } else if (node == Search) {
    std::snprintf(query, sizeof(query), "%s", draft);
    page = 0;
    count = 0;
    scroll.reset();
    search_open = false;
    scene.invalidate();
    directory_fetch();
  } else if (node == Reload) {
    if (view < 2) {
      if (!net_permission)
        permission(NetPermission, "net.client", "web");
      else
        directory_fetch();
    } else
      dirty = true;
  } else if (node == PrevPage || node == NextPage) {
    if (view > 1 || directory_loading)
      return;
    if (node == PrevPage) {
      if (page)
        --page;
    } else
      ++page;
    directory_fetch();
  } else if (node >= StationBase &&
             node < StationBase + radio::CatalogCapacity * 4 && !(node % 4)) {
    unsigned at = (node - StationBase) / 4;
    const Station *s = list_station(at);
    if (s) {
      focus_index = at;
      controller_mode = false;
      choose(*s);
    }
  } else if (node == Play) {
    if (want_play)
      stop_play();
    else if (have_current) {
      want_play = true;
      retry_count = 0;
      open_live();
    } else if (const Station *s = list_station(0))
      choose(*s);
  } else if (node == Previous || node == Next)
    next_station(node == Previous ? -1 : 1);
  else if (node == Star)
    favorite_toggle();
  else if (node == CustomPlay) {
    if (!radio::valid_url(custom_url) || !custom_name[0])
      status(play_status, sizeof(play_status),
             "请填写名称和有效的 http(s) 直播地址");
    else {
      Station s;
      std::snprintf(s.name, sizeof(s.name), "%s", custom_name);
      std::snprintf(s.url, sizeof(s.url), "%s", custom_url);
      choose(s);
    }
  }
}
} // namespace
extern "C" int32_t pxa_app_start(const uint8_t *config, uint32_t length) {
  pxa_ui_environment_t e{};
  if (pxa_ui_parse_start_environment(config, length, &e))
    environment(e);
  pxa_window_config_t window{};
  window.edge_to_edge = 1;
  window.status_bar_mode = 1;
  window.navigation_bar_mode = 1;
  pxa_window_configure(&window);
  pxa_window_request_snapshot(WindowSnapshot);
  decoder.reset();
  pxa_clock_set_period(PXA_CLOCK_MIN_PERIOD_MS);
  load_favorite();
  load_presets();
  status(directory_status, sizeof(directory_status),
         "内置 32 个电台 · 全球页可发现更多");
  render();
  return PXA_STATUS_OK;
}
extern "C" int32_t pxa_app_on_event(const uint8_t *data, uint32_t size) {
  pxa_event_t e{};
  if (!pxa_parse_event(data, size, &e))
    return PXA_EVENT_UNHANDLED;
  if (pxa_window_is_back_requested(&e) && !navigate_back())
    return PXA_EVENT_UNHANDLED;
  pxa_window_snapshot_t window{};
  if ((pxa_window_parse_metrics_changed(&e, &window) ||
       pxa_window_parse_snapshot(&e, WindowSnapshot, &window)) &&
      scene.window(window))
    layout_changed();
  if (e.service == PXA_PERMISSION_SERVICE &&
      e.opcode == PXA_PERMISSION_ACQUIRE) {
    pxa_permission_acquire_result_t r{};
    if (!pxa_permission_parse_acquire(&e, e.token, &r))
      return PXA_EVENT_UNHANDLED;
    if (e.token == NetPermission) {
      if (r.status) {
        directory_loading = false;
        status(directory_status, sizeof(directory_status),
               "需要联网权限，点击刷新重试");
        if (want_play)
          failure("联网权限未允许", false);
      } else {
        net_permission = r.handle;
        if (directory_loading)
          directory_fetch();
        if (want_play)
          open_live();
      }
    } else if (e.token == AudioPermission) {
      if (r.status)
        failure("播放权限未允许，点击播放重试", false);
      else {
        audio_permission = r.handle;
        if (want_play)
          open_live();
      }
    }
  } else if (e.service == PXA_NET_SERVICE && e.opcode == PXA_HTTP_OPEN) {
    pxa_http_open_result_t r{};
    if (!pxa_http_parse_open(&e, e.token, &r))
      return PXA_EVENT_UNHANDLED;
    if (e.token == dir_open_token) {
      dir_open_token = 0;
      if (r.status)
        directory_failed();
      else
        directory_handle = r.handle;
    } else if (e.token == play_open_token) {
      play_open_token = 0;
      if (r.status)
        failure("直播连接失败，正在重试", true);
      else
        stream = r.handle;
    } else if (r.handle)
      pxa_close_handle(r.handle);
  } else if (e.service == PXA_AUDIO_SERVICE && e.opcode == PXA_AUDIO_OPEN_PCM) {
    pxa_audio_pcm_result_t r{};
    if (!pxa_audio_parse_pcm(&e, e.token, &r))
      return PXA_EVENT_UNHANDLED;
    if (e.token == audio_open_token) {
      audio_open_token = 0;
      if (r.source.status) {
        char info[100];
        std::snprintf(info, sizeof(info),
                      "RADIO PCM open status=%d rate=%u channels=%u",
                      r.source.status, audio_rate, audio_channels);
        log(info);
        failure("音频设备暂不可用，请重试", false);
      } else {
        audio = r.source.handle;
        audio_commit_token = ++next_token;
        if (pxa_audio_commit_gain(audio_commit_token, audio, 0)) {
          audio_commit_token = 0;
          failure("无法配置音频输出", false);
        }
      }
    } else if (r.source.handle)
      pxa_close_handle(r.source.handle);
  } else if (e.service == PXA_AUDIO_SERVICE &&
             e.opcode == PXA_AUDIO_COMMIT_GRAPH &&
             e.token == audio_commit_token) {
    int32_t result = 0;
    if (pxa_audio_parse_status(&e, e.token, PXA_AUDIO_COMMIT_GRAPH, &result)) {
      audio_commit_token = 0;
      if (result)
        failure("无法配置音频输出", false);
      else
        audio_ready = true;
    }
  } else if (e.service == PXA_STORAGE_SERVICE) {
    if (e.opcode == PXA_STORAGE_GET) {
      pxa_storage_get_result_t r{};
      if (!pxa_storage_parse_get(&e, e.token, &r))
        return PXA_EVENT_UNHANDLED;
      if (e.token == LoadFavorite) {
        if (r.status == 0)
          occupied[storage_slot] = radio::load_station(
              r.value.data, r.value.size, favorites[storage_slot]);
        if (++storage_slot < radio::FavoriteCount)
          load_favorite();
        else
          dirty = true;
      }
    } else if (e.token == SaveFavorite) {
      int32_t result = 0;
      if (pxa_storage_parse_status(&e, e.token,
                                   favorite_remove ? PXA_STORAGE_REMOVE
                                                   : PXA_STORAGE_SET,
                                   &result)) {
        favorite_busy = false;
        if (!result) {
          occupied[save_slot] = !favorite_remove;
          if (!favorite_remove)
            favorites[save_slot] = pending_favorite;
        } else
          status(play_status, sizeof(play_status), "收藏保存失败，请重试");
        dirty = true;
      }
    }
  } else if (e.service == PXA_SERVICE_SYSTEM &&
             e.opcode == PXA_SYSTEM_LIFECYCLE_EVENT && e.payload_size == 1) {
    background = e.payload[0] == PXA_SYSTEM_LIFECYCLE_BACKGROUND;
    if (background) {
      stop_play();
      close(directory_handle);
      cancel(dir_open_token);
      directory_loading = false;
      pxa_clock_set_period(0);
      last_buttons = 0;
    } else {
      pxa_clock_set_period(PXA_CLOCK_MIN_PERIOD_MS);
      dirty = true;
    }
  }
  pxa_ui_pointer_data_t pointer{};
  if (pxa_ui_parse_pointer(&e, &pointer) && pointer.node == 2 &&
      pointer.generation == scene.ui_generation) {
    int x = pointer.x - scene.viewport.content.x,
        y = pointer.y - scene.viewport.content.y;
    if (pointer.phase == PXA_POINTER_DOWN) {
      touch_x = x;
      touch_y = y;
      touch_action = scene.find(x, y);
      touch_list =
          !player_view && !search_open && view < 3 &&
          x >= scene.viewport.touch_left && x < width &&
          scroll.down(y, now_ms());
      if (touch_list)
        controller_mode = false;
    } else if (pointer.phase == PXA_POINTER_MOVE && touch_list) {
      scroll.move(y, now_ms());
      dirty = true;
    } else if (pointer.phase == PXA_POINTER_UP ||
               pointer.phase == PXA_POINTER_CANCEL) {
      bool canceled = pointer.phase == PXA_POINTER_CANCEL;
      bool moved = touch_list && scroll.up(y, now_ms(), canceled);
      if (!canceled && !moved && std::abs(x - touch_x) < 14 &&
          std::abs(y - touch_y) < 14 && touch_action &&
          touch_action == scene.find(x, y))
        click(touch_action);
      touch_action = 0;
      touch_list = false;
      dirty = true;
    }
  }
  pxa_ui_environment_t env{};
  if (pxa_ui_parse_environment_event(&e, &env))
    environment(env);
  pxa_ui_event_data_t ui{};
  if (pxa_ui_parse_event(&e, &ui) &&
      pxa_ui_event_is_current(&ui, PXA_UI_PRIMARY_SURFACE, generation)) {
    if (ui.kind == PXA_UI_EVENT_TEXT_KIND) {
      if (ui.node == SearchInput)
        pxa_ui_event_text(&ui, draft, sizeof(draft));
      else if (ui.node == CustomName)
        pxa_ui_event_text(&ui, custom_name, sizeof(custom_name));
      else if (ui.node == CustomUrl)
        pxa_ui_event_text(&ui, custom_url, sizeof(custom_url));
      return PXA_EVENT_HANDLED;
    }
    if (ui.kind == PXA_UI_EVENT_CLICK_KIND)
      click(ui.node);
  }
  pxa_ui_controller_data_t pad{};
  if (pxa_ui_parse_controller(&e, &pad) && pad.controller == 0) {
    uint32_t pressed = pad.buttons & ~last_buttons;
    last_buttons = pad.connected ? pad.buttons : 0;
    if (pressed & PXA_CONTROLLER_A) {
      if (search_open || view == 3)
        click(view == 3 ? CustomPlay : Search);
      else if (player_view)
        click(Play);
      else if (const Station *s = list_station(focus_index))
        choose(*s);
    }
    if (pressed & PXA_CONTROLLER_B) {
      click(BackView);
    }
    if (pressed & PXA_CONTROLLER_Y)
      click(player_view ? BackView : PlayerView);
    if (pressed & PXA_CONTROLLER_START)
      click(search_open ? Search : view == 3 ? CustomPlay : Play);
    if (pressed & PXA_CONTROLLER_X)
      favorite_toggle();
    if (pressed & PXA_CONTROLLER_SELECT)
      click(Favorites);
    unsigned n = list_count();
    if (n) {
      if (!player_view && !search_open && directory_paged && view < 2 &&
          !directory_loading &&
          ((pressed & PXA_CONTROLLER_DOWN && focus_index == n - 1) ||
           (pressed & PXA_CONTROLLER_UP && focus_index == 0 && page))) {
        click(pressed & PXA_CONTROLLER_DOWN ? NextPage : PrevPage);
        focus_index = 0;
        scroll.reset();
      }
      if (pressed & PXA_CONTROLLER_DOWN)
        focus_index = std::min(focus_index + 1, n - 1);
      if (pressed & PXA_CONTROLLER_UP)
        focus_index = focus_index ? focus_index - 1 : 0;
      if (pressed & (PXA_CONTROLLER_DOWN | PXA_CONTROLLER_UP)) {
        controller_mode = true;
        dirty = true;
      }
    }
  }
  uint64_t tick = 0;
  if (pxa_clock_parse_tick(&e, &tick) && !background) {
    playback_pump();
    directory_pump();
    if (!player_view && !search_open && view < 3 && scroll.tick(now_ms()))
      dirty = true;
    if (want_play && audio_ready && now_ms() - animation_at >= 200) {
      animation_at = now_ms();
      dirty = true;
    }

    if (audio_frames && now_ms() - stats_at >= 5000) {
      char line[220];
      std::snprintf(line, sizeof(line),
                    "RADIO playback frames=%llu rate=%u channels=%u nonzero=%u "
                    "blocked=%u buffer=%u reconnects=%u",
                    (unsigned long long)audio_frames, audio_rate,
                    audio_channels, nonzero_samples, blocked_writes,
                    unsigned(compressed_size), retry_count);
      log(line);
      stats_at = now_ms();
    }
  }
  if (dirty && !background &&
      (!audio_ready ||
       now_ms() - ui_at >= ((!player_view && !search_open && view < 3 &&
                             (scroll.dragging || scroll.easing ||
                              std::abs(scroll.velocity) > .015f))
                                ? 50u
                                : 100u) ||
       !scene.initialized)) {
    playback_pump();
    render();
    playback_pump();
  }
  return PXA_EVENT_HANDLED;
}
extern "C" void pxa_app_stop(uint32_t) {
  want_play = false;
  stream = audio = directory_handle =
      0; /* Host revokes handles before stop; no imports here. */
}
