// SPDX-License-Identifier: Apache-2.0
#include "companion.hpp"
#include "protocol.hpp"
#include "response_wait.hpp"
#include "scene_clock.hpp"
#include "settings_ui.hpp"
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <optional>
#include <opus.h>
#include <pxa/app.hpp>
#include <pxa/ui_environment.hpp>
#include <pxa/ui_gesture.hpp>
#include <pxa/ui_layout.hpp>
#include <unistd.h>
using namespace pxa;
using namespace pxa::ui;
using namespace pxa::ui::literals;
namespace {
std::uint64_t now_ms() {
  timespec t{};
  clock_gettime(CLOCK_MONOTONIC, &t);
  return std::uint64_t(t.tv_sec) * 1000 + t.tv_nsec / 1000000;
}
struct App {
  Environment<> environment;
  Context *context = nullptr;
  State<std::string> status{"正在唤醒小智…"}, subtitle{""}, heard{""};
  State<std::string> url{""}, token{""}, device{""}, client{""},
      endpoint{"https://api.tenclass.net/xiaozhi/ota/"}, draft{""};
  State<std::string> record_label{"开始录音"};
  State<bool> busy{true};
  State<bool> settings_open{false}, official_server{true}, advanced{false},
      auto_listen{true}, voice_enabled{true}, keep_awake{true};
  std::string official_url, official_token, last_saved_config;
  bool settings_saving = false;
  std::optional<bool> awake_request;
  State<std::string> server_label{"官方小智 · 已选择"},
      voice_label{"语音回复：开启"}, listen_label{"自动聆听：开启"};
  xiaozhi::Companion companion;
  xiaozhi::SettingsUi settings_ui;
  unsigned settings_page = 0;
  enum class EditField { none, url, token, endpoint, device, client, text };
  EditField editing = EditField::none;
  State<std::string> edit_value{""};
  State<bool> edit_secret{false};
  State<TextInputBox> editor_box{TextInputBox{}};
  TextInputRef editor;
  std::string edit_error, settings_notice;
  bool focus_pending = false;
  struct SettingsSnapshot {
    std::string url, token, endpoint, device, client, activation_code,
        activation_message;
    bool official, voice, listen, awake, met;
    unsigned scene;
  };
  std::optional<SettingsSnapshot> settings_snapshot;
  pxa::ui::PressGesture press;
  pxa::input::Controller controller;
  unsigned emotion = 0, scene_mode = 0, automatic_scene = 1;
  unsigned capture_frame_ms = 20;
  bool resume_after_format = false;
  unsigned capture_retries = 0;
  bool conversation_preparing = false;
  bool user_paused = false;
  static constexpr std::string_view idle_message = "我在这里，轻触和我聊聊\n长按打开设置";
  xiaozhi::ResponseWait response_wait;
  std::uint64_t capture_retry_after = 0;
  bool met_before = false;
  std::string activation_code, activation_message;
  State<std::string> scene_label{"场景：跟随系统时间"};
  bool time_pending = false;
  std::uint64_t next_time_check = 0;
  Connectivity connectivity = Connectivity::unknown;
  bool profile_loaded = false, network_pending = false, waiting = false,
       want_listen = false, prepare_audio = false, capture_supported = true,
       manual_retry = false, button_long_fired = false, send_draft = false,
       network_lost = false, tts_finished = false, drain_pending = false;
  std::uint64_t next_drain_check = 0, drain_deadline = 0;
  std::uint64_t next_network_check = 0, retry_after = 0, button_began = 0;
  std::uint64_t tap_debounce_until = 0;

  Permission network_permission, socket_permission, play_permission,
      mic_permission;
  WebSocket socket;
  AudioCapture microphone;
  std::optional<AudioSession> playback;
  OpusDecoder *decoder = nullptr;
  std::string session, pending_text;
  std::array<std::byte, 4097> incoming{};
  std::array<std::byte, 1276> mic_packet{};
  std::array<opus_int16, 1920> pcm{};
  std::size_t pcm_count = 0, pcm_offset = 0, mic_size = 0;
  unsigned reply_packets = 0, reply_samples = 0, reply_submitted = 0;
  int reply_peak = 0;
  std::uint64_t opened_at = 0, last_received = 0;
  std::uint64_t connect_deadline = 0;
  bool hello_sent = false, ready = false, listening = false, speaking = false,
       accept_audio = false;
  void launch(Task<void> task) {
    auto result = context->foreground_tasks().start(std::move(task));
    if (!result)
      on_error(*context, result.error());
  }
  void stop_recording() {
    capture_retries = 0;
    capture_retry_after = 0;
    (void)microphone.close();
    listening = false;
    mic_size = 0;
    record_label.set("开始录音");
  }
  void disconnect() {
    connect_deadline = 0;
    tts_finished = false;
    drain_pending = false;
    drain_deadline = 0;
    want_listen = false;
    waiting = false;
    response_wait.reset();
    prepare_audio = false;
    conversation_preparing = false;
    stop_recording();
    (void)socket.close();
    playback.reset();
    if (decoder) {
      opus_decoder_destroy(decoder);
      decoder = nullptr;
    }
    session.clear();
    pending_text.clear();
    ready = false;
    hello_sent = false;
    speaking = false;
    accept_audio = false;
    pcm_count = pcm_offset = 0;
  }
  void fail(std::string message) {
    disconnect();
    busy.set(false);
    retry_after = now_ms() + 10000;
    if (met_before)
      message += "\n我在这里等你，轻触重连";
    status.set(std::move(message));
  }
  void recover_response(std::string_view message) {
    context->foreground_tasks().cancel();
    network_pending = false;
    disconnect();
    busy.set(false);
    manual_retry = !auto_listen.get();
    retry_after = now_ms() + 1800;
    status.set(std::string(message) + (auto_listen.get()
        ? "\n稍后继续聆听，轻触可重试" : "\n轻触重新开始"));
  }
  void pause_conversation() {
    context->foreground_tasks().cancel();
    network_pending = false;
    disconnect();
    busy.set(false);
    user_paused = true;
    manual_retry = true;
    resume_after_format = false;
    heard.set("");
    subtitle.set("");
    emotion = 0;
    status.set(std::string(idle_message));
    companion.previous_hash = 0;
  }
  void on_error(Context &ctx, Error error) {
    char diagnostic[64];
    std::snprintf(diagnostic, sizeof(diagnostic), "Xiaozhi service error: %d",
                  int(error));
    (void)ctx.log().write(LogLevel::warning, diagnostic);
    if (error == Error::denied)
      manual_retry = true;
    if (connectivity == Connectivity::offline)
      fail("未连接网络\n请先在系统设置中连接 Wi-Fi");
    else
      fail(error == Error::denied ? "请允许联网和音频权限\n长按可打开设置"
                                  : "暂时无法连接小智\n轻触重试，长按进入设置");
  }
  Task<void> save() {
    auto root = xiaozhi::Json(cJSON_CreateObject(), cJSON_Delete);
    if (!root)
      co_return std::unexpected(Error::resource_limit);
    cJSON_AddNumberToObject(root.get(), "scene_mode", scene_mode);
    cJSON_AddNumberToObject(root.get(), "config_version", 2);
    cJSON_AddBoolToObject(root.get(), "keep_awake", keep_awake.get());
    cJSON_AddBoolToObject(root.get(), "official", official_server.get());
    cJSON_AddBoolToObject(root.get(), "auto_listen", auto_listen.get());
    cJSON_AddBoolToObject(root.get(), "voice_enabled", voice_enabled.get());
    cJSON_AddStringToObject(root.get(), "url", url.get().c_str());
    cJSON_AddStringToObject(root.get(), "token", token.get().c_str());
    cJSON_AddStringToObject(root.get(), "device", device.get().c_str());
    cJSON_AddStringToObject(root.get(), "client", client.get().c_str());
    cJSON_AddStringToObject(root.get(), "endpoint", endpoint.get().c_str());
    auto encoded = xiaozhi::encode(root.get());
    if (encoded.empty() || encoded.size() > 2048)
      co_return std::unexpected(Error::limit_exceeded);
    if (encoded == last_saved_config)
      co_return Result<void>{};
    auto packet = std::make_unique<std::array<std::byte, 4096>>();
    auto result = co_await context->storage().set(
        "connection", std::as_bytes(std::span{encoded.data(), encoded.size()}),
        *packet);
    if (!result)
      co_return std::unexpected(result.error());
    last_saved_config = std::move(encoded);
    co_return Result<void>{};
  }
  Task<void> load() {
    auto buffer = std::make_unique<std::array<std::byte, 4096>>();
    auto loaded = co_await context->storage().get("connection", *buffer);
    if (loaded) {
      auto root = xiaozhi::parse(
          {reinterpret_cast<const char *>(buffer->data()), *loaded});
      if (root) {
        last_saved_config.assign(reinterpret_cast<const char *>(buffer->data()), *loaded);
        auto scene = cJSON_GetObjectItemCaseSensitive(root.get(), "scene_mode");
        if (cJSON_IsNumber(scene) && scene->valueint >= 0 &&
            scene->valueint <= 4)
          scene_mode = unsigned(scene->valueint);
        url.set(std::string(xiaozhi::text(root.get(), "url")));
        token.set(std::string(xiaozhi::text(root.get(), "token")));
        auto official =
            cJSON_GetObjectItemCaseSensitive(root.get(), "official");
        official_server.set(cJSON_IsBool(official) ? cJSON_IsTrue(official)
                                                   : url.get().empty());
        auto listening =
            cJSON_GetObjectItemCaseSensitive(root.get(), "auto_listen");
        if (cJSON_IsBool(listening))
          auto_listen.set(cJSON_IsTrue(listening));
        auto voice =
            cJSON_GetObjectItemCaseSensitive(root.get(), "voice_enabled");
        if (cJSON_IsBool(voice))
          voice_enabled.set(cJSON_IsTrue(voice));
        auto awake = cJSON_GetObjectItemCaseSensitive(root.get(), "keep_awake");
        if (cJSON_IsBool(awake))
          keep_awake.set(cJSON_IsTrue(awake));
        // Older versions persisted official credentials in the custom fields.
        // Binding depends on the device identity, which remains unchanged.
        auto version = cJSON_GetObjectItemCaseSensitive(root.get(), "config_version");
        if (official_server.get() && (!cJSON_IsNumber(version) || version->valueint < 2)) {
          url.set("");
          token.set("");
        }

        device.set(std::string(xiaozhi::text(root.get(), "device")));
        client.set(std::string(xiaozhi::text(root.get(), "client")));
        auto ota = xiaozhi::text(root.get(), "endpoint");
        if (!ota.empty())
          endpoint.set(std::string(ota));
      }
    }
    if (client.get().empty() || device.get().empty()) {
      unsigned char bytes[16];
      if (getentropy(bytes, sizeof(bytes)) != 0)
        co_return std::unexpected(Error::unavailable);
      bytes[6] = (bytes[6] & 15) | 64;
      bytes[8] = (bytes[8] & 63) | 128;
      char id[64];
      std::snprintf(id, sizeof(id),
                    "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%"
                    "02x%02x%02x",
                    bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5],
                    bytes[6], bytes[7], bytes[8], bytes[9], bytes[10],
                    bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);
      client.set(id);
      std::snprintf(id, sizeof(id), "02:%02x:%02x:%02x:%02x:%02x", bytes[0],
                    bytes[1], bytes[2], bytes[3], bytes[4]);
      device.set(id);
      auto saved = co_await save();
      if (!saved)
        co_return std::unexpected(saved.error());
    }
    busy.set(false);
    profile_loaded = true;
    sync_labels();
    co_return Result<void>{};
  }
  bool valid_config(bool ws = true) {
    const auto &selected_url = official_server.get() ? official_url : url.get();
    const auto &selected_token = official_server.get() ? official_token : token.get();
    const bool identity =
        xiaozhi::safe_header(device.get(), 64) && !device.get().empty() &&
        xiaozhi::safe_header(client.get(), 64) && !client.get().empty();
    const bool target = ws ? xiaozhi::safe_header(selected_token, 1024) &&
                                 xiaozhi::safe_header(selected_url, 512) &&
                                 (selected_url.starts_with("ws://") ||
                                  selected_url.starts_with("wss://"))
                           : xiaozhi::safe_header(endpoint.get(), 512) &&
                                 (endpoint.get().starts_with("http://") ||
                                  endpoint.get().starts_with("https://"));
    if (!identity || !target) {
      status.set("请检查地址、Token 和设备标识");
      return false;
    }
    return true;
  }
  Task<void> acquire_network() {
    if (!network_permission) {
      auto grant = co_await context->permissions().acquire("net.client", "web");
      if (!grant)
        co_return std::unexpected(grant.error());
      network_permission = std::move(*grant);
    }
    co_return Result<void>{};
  }
  Task<bool> discover() {
    if (busy.get() || !valid_config(false)) {
      manual_retry = true;
      co_return false;
    }
    busy.set(true);
    disconnect();
    activation_code.clear();
    activation_message.clear();
    status.set("正在获取小智连接信息…");
    auto grant = co_await acquire_network();
    if (!grant)
      co_return std::unexpected(grant.error());
    connect_deadline = now_ms() + 20000;
    auto information = xiaozhi::Json(cJSON_CreateObject(), cJSON_Delete);
    if (!information)
      co_return std::unexpected(Error::resource_limit);
    cJSON_AddNumberToObject(information.get(), "version", 2);
    cJSON_AddStringToObject(information.get(), "language", "zh-CN");
    cJSON_AddStringToObject(information.get(), "mac_address",
                            device.get().c_str());
    cJSON_AddStringToObject(information.get(), "uuid", client.get().c_str());
    auto application =
        cJSON_AddObjectToObject(information.get(), "application");
    cJSON_AddStringToObject(application, "name", "pxa-xiaozhi");
    cJSON_AddStringToObject(application, "version", "0.3.3");
    auto board = cJSON_AddObjectToObject(information.get(), "board");
    cJSON_AddStringToObject(board, "type", "generic");
    cJSON_AddStringToObject(board, "name", "pxa");
    cJSON_AddStringToObject(board, "version", "1.0");
    const std::string body = xiaozhi::encode(information.get());
    if (body.empty())
      co_return std::unexpected(Error::resource_limit);
    NetHeaderView headers[] = {{"Device-Id", device.get()},
                               {"Client-Id", client.get()},
                               {"Content-Type", "application/json"},
                               {"User-Agent", "PXA-Xiaozhi/0.2"}};
    auto exchange = co_await context->http().open(
        {.url = endpoint.get(),
         .method = HttpMethod::post,
         .headers = headers,
         .body = std::as_bytes(std::span{body.data(), body.size()}),
         .upload_bytes = body.size(),
         .options = {.buffer_bytes = 8192,
                     .header_bytes = 2048,
                     .max_body_bytes = 8192}},
        network_permission);
    if (!exchange)
      co_return std::unexpected(exchange.error());
    auto response = co_await std::move(*exchange).response();
    if (!response)
      co_return std::unexpected(response.error());
    if (response->status_code != 200) {
      fail("获取配置失败，HTTP " + std::to_string(response->status_code));
      co_return false;
    }
    auto data = std::make_unique<std::array<std::byte, 8193>>();
    std::size_t used = 0;
    while (used < 8192) {
      auto n = co_await response->body.read_some(
          std::span{*data}.subspan(used, 8192 - used));
      if (!n)
        co_return std::unexpected(n.error());
      if (!*n)
        break;
      used += *n;
    }
    auto root =
        xiaozhi::parse({reinterpret_cast<const char *>(data->data()), used});
    if (!root)
      co_return std::unexpected(Error::protocol_error);
    auto ws = cJSON_GetObjectItemCaseSensitive(root.get(), "websocket");
    const auto ws_url = xiaozhi::text(ws, "url");
    const auto ws_token = xiaozhi::text(ws, "token");
    auto version = cJSON_GetObjectItemCaseSensitive(ws, "version");
    if (version && (!cJSON_IsNumber(version) || version->valuedouble != 1)) {
      manual_retry = true;
      fail("服务端需要其他协议版本，请使用 WebSocket v1");
      co_return false;
    }
    auto activation =
        cJSON_GetObjectItemCaseSensitive(root.get(), "activation");
    auto code = xiaozhi::text(activation, "code");
    if (!code.empty()) {
      manual_retry = true;
      met_before = false;
      activation_code = std::string(code.substr(0, 64));
      activation_message =
          std::string(xiaozhi::text(activation, "message").substr(0, 512));
      status.set("激活码：" + std::string(code) + "\n" +
                 std::string(xiaozhi::text(activation, "message")) +
                 "\n绑定后再次获取配置");
    } else {
      if (!xiaozhi::safe_header(ws_url, 512) ||
          !(ws_url.starts_with("ws://") || ws_url.starts_with("wss://")) ||
          !xiaozhi::safe_header(ws_token, 1024)) {
        manual_retry = true;
        fail("服务端未提供有效的 WebSocket 配置\n长按可检查接入设置");
        co_return false;
      }
      // A failed discovery must never reuse credentials from an older session.
      official_url.assign(ws_url);
      official_token.assign(ws_token);
      status.set("正在连接小智…");
    }
    busy.set(false);
    connect_deadline = 0;
    co_return code.empty();
  }
  Task<void> connect() {
    if (busy.get() || !valid_config())
      co_return Result<void>{};
    busy.set(true);
    disconnect();
    status.set("正在连接…");
    if (!socket_permission) {
      auto grant =
          co_await context->permissions().acquire("net.socket", "outbound");
      if (!grant)
        co_return std::unexpected(grant.error());
      socket_permission = std::move(*grant);
    }
    auto saved = co_await save();
    if (!saved)
      co_return std::unexpected(saved.error());
    std::string auth = official_server.get() ? official_token : token.get();
    if (!auth.empty() && auth.find(' ') == std::string::npos)
      auth = "Bearer " + auth;
    NetHeaderView headers[] = {{"Protocol-Version", "1"},
                               {"Device-Id", device.get()},
                               {"Client-Id", client.get()},
                               {"Authorization", auth}};
    connect_deadline = now_ms() + 20000;
    auto opened = co_await context->websockets().open(
        official_server.get() ? official_url : url.get(), socket_permission,
        std::span{headers}.first(auth.empty() ? 3 : 4));
    if (!opened)
      co_return std::unexpected(opened.error());
    socket = std::move(*opened);
    connect_deadline = 0;
    opened_at = last_received = now_ms();
    pending_text = xiaozhi::hello_for(capture_frame_ms);
    busy.set(false);
    co_return Result<void>{};
  }
  bool queue(std::string message) {
    if (!socket || !pending_text.empty() || message.empty()) {
      status.set("正在发送，请稍后重试");
      return false;
    }
    pending_text = std::move(message);
    return true;
  }
  Task<void> record() {
    if (busy.get() || !ready) {
      status.set("请先连接小智");
      co_return Result<void>{};
    }
    if (listening) {
      co_return Result<void>{};
    }
    if (speaking) {
      status.set("请先打断当前回复");
      co_return Result<void>{};
    }
    if (!pending_text.empty())
      co_return Result<void>{};
    busy.set(true);
    if (!mic_permission) {
      auto grant = co_await context->permissions().acquire("audio.capture",
                                                           "microphone");
      if (!grant)
        co_return std::unexpected(grant.error());
      mic_permission = std::move(*grant);
    }
    if (playback) {
      auto flushed = co_await playback->flush();
      if (!flushed)
        co_return std::unexpected(flushed.error());
      pcm_count = pcm_offset = 0;
    }
    auto opened = co_await context->capture().open(mic_permission);
    if (!opened) {
      busy.set(false);
      if (opened.error() == Error::unavailable || opened.error() == Error::busy ||
          opened.error() == Error::would_block) {
        if (++capture_retries <= 20) {
          want_listen = true;
          capture_retry_after = now_ms() + 500;
          status.set("正在准备麦克风…");
        } else {
          want_listen = false;
          status.set("麦克风暂时未就绪\n轻触重试，长按可发送文字");
        }
        co_return Result<void>{};
      }
      capture_supported = false;
      status.set("麦克风暂不可用\n长按可在设置中输入文字");
      co_return Result<void>{};
    }
    microphone = std::move(*opened);
    capture_supported = true;
    capture_retries = 0;
    capture_retry_after = 0;
    if (microphone.frame_ms() != capture_frame_ms) {
      capture_frame_ms = microphone.frame_ms();
      disconnect();
      resume_after_format = true;
      retry_after = 0;
      busy.set(false);
      status.set("正在调整录音格式…");
      co_return Result<void>{};
    }
    listening = true;
    record_label.set("停止录音");
    accept_audio = false;
    auto command = xiaozhi::command(session, "listen", "start");
    auto root = xiaozhi::parse(command);
    cJSON_SetValuestring(cJSON_GetObjectItemCaseSensitive(root.get(), "mode"),
                         "auto");
    queue(xiaozhi::encode(root.get()));
    waiting = false;
    emotion = 0;
    status.set("正在听你说…");
    busy.set(false);
    co_return Result<void>{};
  }
  void send_text() {
    if (!ready || busy.get() || listening || speaking ||
        draft.get().empty())
      return;
    if (queue(xiaozhi::command(session, "listen", "detect",
                               draft.get().c_str()))) {
      heard.set(draft.get());
      draft.set("");
      waiting = true;
      response_wait.think(now_ms());
      emotion = 4;
      status.set("小智正在想…");
    }
  }
  Task<void> enable_playback() {
    if (playback || !voice_enabled.get())
      co_return Result<void>{};
    busy.set(true);
    if (!play_permission) {
      auto grant =
          co_await context->permissions().acquire("audio.playback", "media");
      if (!grant) {
        (void)context->log().write(LogLevel::warning, "Xiaozhi playback permission unavailable");
        co_return std::unexpected(grant.error());
      }
      play_permission = std::move(*grant);
    }
    auto opened = co_await context->audio().open_pcm(play_permission, 16000, 1);
    if (!opened)
      co_return std::unexpected(opened.error());
    playback.emplace(std::move(*opened));
    auto graph = co_await playback->graph(0);
    if (!graph)
      co_return std::unexpected(graph.error());
    busy.set(false);
    (void)context->log().write(LogLevel::info, "Xiaozhi playback ready");
    status.set("小智已准备好");
    co_return Result<void>{};
  }
  Task<void> abort() {
    if (!ready)
      co_return Result<void>{};
    stop_recording();
    tts_finished = false;
    drain_deadline = 0;
    speaking = false;
    accept_audio = false;
    pcm_count = pcm_offset = 0;
    if (playback) {
      auto flushed = co_await playback->flush();
      if (!flushed)
        co_return std::unexpected(flushed.error());
    }
    if (!pending_text.empty())
      pending_text.clear();
    queue(xiaozhi::command(session, "abort"));
    waiting = false;
    response_wait.reset();
    emotion = 0;
    status.set("我在听…");
    co_return Result<void>{};
  }
  void message(std::string_view value) {
    auto root = xiaozhi::parse(value);
    if (!root) {
      fail("服务端消息格式错误");
      return;
    }
    auto type = xiaozhi::text(root.get(), "type");
    if (type == "hello") {
      if (ready) {
        fail("收到重复握手，请重新连接");
        return;
      }
      auto version = cJSON_GetObjectItemCaseSensitive(root.get(), "version");
      if (version && (!cJSON_IsNumber(version) || version->valueint != 1)) {
        fail("服务端需要其他协议版本，请使用 WebSocket v1");
        return;
      }
      auto id = xiaozhi::text(root.get(), "session_id");
      auto params =
          cJSON_GetObjectItemCaseSensitive(root.get(), "audio_params");
      auto format = xiaozhi::text(params, "format");
      if (!xiaozhi::session_valid(id) ||
          xiaozhi::text(root.get(), "transport") != "websocket" ||
          (!format.empty() && format != "opus")) {
        fail("服务端会话或音频格式不兼容");
        return;
      }
      session = std::string(id);
      ready = true;
      met_before = true;
      activation_code.clear();
      activation_message.clear();
      int error = 0;
      decoder = opus_decoder_create(16000, 1, &error);
      if (!decoder) {
        fail("无法初始化 Opus 解码器");
        return;
      }
      status.set("轻触开始说话");
      prepare_audio = true;
      retry_after = 0;
      return;
    }
    if (!ready) {
      fail("会话握手尚未完成，轻触重试");
      return;
    }
    auto id = xiaozhi::text(root.get(), "session_id");
    if (!id.empty() && id != session)
      return;
    if (type == "stt") {
      const auto text = xiaozhi::text(root.get(), "text");
      if (text.find_first_not_of(" \t\r\n") == std::string_view::npos) {
        recover_response("没有听清刚才的话");
        return;
      }
      stop_recording();
      heard.set(std::string(text));
      subtitle.set("");
      waiting = true;
      response_wait.think(now_ms());
      emotion = 4;
      status.set("小智正在想…\n轻触可重新说");
    } else if (type == "tts") {
      auto state = xiaozhi::text(root.get(), "state");
      if (state == "start") {
        stop_recording();
        reply_packets = reply_samples = reply_submitted = 0;
        reply_peak = 0;
        waiting = false;
        response_wait.reset();
        speaking = true;
        tts_finished = false;
        drain_deadline = 0;
        accept_audio = true;
        subtitle.set("");
        if (decoder)
          opus_decoder_ctl(decoder, OPUS_RESET_STATE);
        status.set("小智正在回复…");
      } else if (state == "sentence_start")
        subtitle.set(std::string(xiaozhi::text(root.get(), "text")));
      else if (state == "stop") {
        if (!speaking)
          return;
        accept_audio = false;
        tts_finished = true;
        drain_deadline = now_ms() + 5000;
        next_drain_check = 0;
        if (!playback)
          finish_reply();
      }
    } else if (type == "llm") {
      emotion = xiaozhi::emotion_frame(xiaozhi::text(root.get(), "emotion"));
    } else if (type == "goodbye")
      fail("服务端已结束会话，点击重新连接");
    else if (type == "error")
      fail("服务端返回错误，请检查连接配置");
  }
  void finish_reply() {
    char diagnostic[160];
    std::snprintf(diagnostic, sizeof(diagnostic),
                  "Xiaozhi audio: playback=%u packets=%u decoded=%u submitted=%u peak=%d",
                  unsigned(bool(playback)), reply_packets, reply_samples,
                  reply_submitted, reply_peak);
    (void)context->log().write(LogLevel::info, diagnostic);
    tts_finished = false;
    drain_deadline = 0;
    speaking = false;
    waiting = false;
    response_wait.reset();
    status.set("轻触开始说话");
    want_listen = auto_listen.get() && capture_supported;
  }
  Task<void> check_audio_drain() {
    drain_pending = true;
    auto state = co_await playback->query();
    drain_pending = false;
    if (!state)
      co_return std::unexpected(state.error());
    if (tts_finished && pcm_offset == pcm_count && state->queued_samples == 0)
      finish_reply();
    co_return Result<void>{};
  }
  void pump() {
    if (!socket)
      return;
    // Do not consume and discard an early reply while its PCM session opens.
    if (ready && prepare_audio)
      return;
    if ((!ready && now_ms() - opened_at > 20000) ||
        (ready && now_ms() - last_received > 120000)) {
      fail("连接超时，请重新连接");
      return;
    }
    if (!pending_text.empty()) {
      if (pending_text.size() > 4096) {
        fail("消息过长");
        return;
      }
      std::array<std::byte, 4097> out;
      out[0] = std::byte{1};
      std::memcpy(out.data() + 1, pending_text.data(), pending_text.size());
      auto sent = socket.write(std::span{out}.first(pending_text.size() + 1));
      if (sent) {
        if (!hello_sent)
          hello_sent = true;
        pending_text.clear();
      } else if (sent.error() != Error::would_block) {
        fail("连接已断开，请重新连接");
        return;
      }
    }
    if (listening && pending_text.empty()) {
      if (!mic_size) {
        auto n = microphone.read(std::span{mic_packet}.subspan(1));
        if (n) {
          mic_size = *n + 1;
          mic_packet[0] = std::byte{2};
        } else if (n.error() != Error::would_block) {
          stop_recording();
          queue(xiaozhi::command(session, "listen", "stop"));
          status.set("录音中断");
        }
      }
    }
    if (mic_size) {
      auto n = socket.write(std::span{mic_packet}.first(mic_size));
      if (n)
        mic_size = 0;
      else if (n.error() != Error::would_block) {
        fail("录音发送失败");
        return;
      }
    }
    // Drain pending PCM before consuming the next compressed packet.
    for (unsigned work = 0; work < 8 && socket; work++) {
      if (pcm_offset < pcm_count && playback) {
        auto count = std::min<std::size_t>(320, pcm_count - pcm_offset);
        auto n = playback->write_pcm(
            std::as_bytes(std::span{pcm}.subspan(pcm_offset, count)));
        if (!n) {
          if (n.error() != Error::would_block)
            fail("语音播放失败");
          return;
        }
        pcm_offset += *n / 2;
        reply_submitted += unsigned(*n / 2);
        if (pcm_offset < pcm_count)
          continue;
      }
      pcm_count = pcm_offset = 0;
      auto n = socket.read(incoming);
      if (!n) {
        if (n.error() != Error::would_block)
          fail("连接已断开，请重新连接");
        return;
      }
      if (!*n) {
        fail("连接已关闭");
        return;
      }
      last_received = now_ms();
      if (incoming[0] == std::byte{1})
        message({reinterpret_cast<const char *>(incoming.data() + 1), *n - 1});
      else if (ready && accept_audio && playback && decoder && *n > 1) {
        int decoded = opus_decode(
            decoder,
            reinterpret_cast<const unsigned char *>(incoming.data() + 1),
            int(*n - 1), pcm.data(), int(pcm.size()), 0);
        if (decoded < 0) {
          fail("收到无效的 Opus 音频");
          return;
        }
        pcm_count = std::size_t(decoded);
        ++reply_packets;
        reply_samples += unsigned(decoded);
        for (std::size_t i = 0; i < pcm_count; ++i)
          reply_peak = std::max(reply_peak, std::abs(int(pcm[i])));
      }
      if (prepare_audio)
        return;
    }
  }
  Task<void> refresh_scene_time() {
    auto wall = co_await context->clock().wall_time();
    time_pending = false;
    // Unsynchronized devices use daytime until the system clock becomes valid.
    if (wall &&
        wall->utc.time_since_epoch() >= std::chrono::seconds{1577836800}) {
      const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
                               wall->local().time_since_epoch())
                               .count();
      automatic_scene = xiaozhi::scene_for_local_seconds(seconds);
    }
    co_return Result<void>{};
  }
  void sync_labels() {
    constexpr const char *scenes[] = {"场景：跟随系统时间", "场景：清晨",
                                      "场景：白天", "场景：傍晚", "场景：夜晚"};
    scene_label.set(scenes[scene_mode]);
    server_label.set(official_server.get() ? "官方小智 · 已选择"
                                           : "自定义服务器 · 已选择");
    voice_label.set(voice_enabled.get() ? "语音回复：开启" : "语音回复：关闭");
    listen_label.set(auto_listen.get() ? "自动聆听：开启" : "自动聆听：关闭");
  }
  Task<void> bootstrap() {
    if (busy.get() || settings_open.get() || !profile_loaded)
      co_return Result<void>{};
    manual_retry = false;
    retry_after = now_ms() + 10000;
    if (official_server.get()) {
      auto discovered = co_await discover();
      if (!discovered)
        co_return std::unexpected(discovered.error());
      if (!*discovered)
        co_return Result<void>{};
    }
    auto connected = co_await connect();
    if (!connected)
      co_return std::unexpected(connected.error());
    co_return Result<void>{};
  }
  Task<void> check_network() {
    network_pending = true;
    auto info = co_await context->net().connectivity();
    network_pending = false;
    auto previous = connectivity;
    connectivity = info ? *info : Connectivity::unknown;
    if (connectivity == Connectivity::offline) {
      network_lost =
          previous != Connectivity::offline || bool(socket) || busy.get();
    } else if (previous == Connectivity::offline && !user_paused) {
      retry_after = 0;
      manual_retry = false;
      status.set("网络已连接，正在连接小智…");
    }
    co_return Result<void>{};
  }
  Task<void> prepare_conversation() {
    struct Guard {
      App &app;
      ~Guard() { app.conversation_preparing = false; app.busy.set(false); }
    } guard{*this};
    (void)context->log().write(LogLevel::info, voice_enabled.get()
        ? "Xiaozhi preparing voice playback" : "Xiaozhi preparing text-only conversation");
    if (voice_enabled.get()) {
      auto sound = co_await enable_playback();
      if (!sound)
        co_return std::unexpected(sound.error());
    }
    want_listen = (auto_listen.get() || resume_after_format) && capture_supported && !send_draft;
    resume_after_format = false;
    prepare_audio = false;
    if (!want_listen && !send_draft)
      status.set(std::string(idle_message));
    co_return Result<void>{};
  }
  Task<void> interrupt_and_listen() {
    const bool cancelled_wait = waiting;
    auto stopped = co_await abort();
    if (!stopped)
      co_return std::unexpected(stopped.error());
    if (cancelled_wait) {
      // A fresh connection prevents an old untagged reply from reviving a
      // cancelled turn; the protocol has no per-utterance response ID.
      disconnect();
      resume_after_format = true;
      manual_retry = false;
      retry_after = 0;
      status.set("正在重新准备聆听…");
    } else {
      want_listen = capture_supported;
    }
    co_return Result<void>{};
  }
  void interact() {
    if (now_ms() < tap_debounce_until)
      return;
    if (settings_open.get())
      return;
    if (user_paused) {
      tap_debounce_until = now_ms() + 400;
      user_paused = false;
      manual_retry = false;
      resume_after_format = capture_supported;
      retry_after = 0;
      next_network_check = 0;
      status.set("我来听你说…");
      return;
    }
    if (connectivity == Connectivity::offline) {
      next_network_check = 0;
      return;
    }
    if (!ready) {
      if (!busy.get()) {
        manual_retry = false;
        resume_after_format = capture_supported;
        retry_after = 0;
        next_network_check = 0;
      }
      return;
    }
    if (busy.get() || prepare_audio || conversation_preparing)
      return;
    tap_debounce_until = now_ms() + 400;
    capture_retries = 0;
    capture_retry_after = 0;
    if (listening)
      pause_conversation();
    else if (speaking || waiting)
      launch(interrupt_and_listen());
    else
      launch(record());
  }
  void enter_settings() {
    if (settings_open.get())
      return;
    settings_snapshot.emplace(SettingsSnapshot{
        url.get(), token.get(), endpoint.get(), device.get(), client.get(),
        activation_code, activation_message, official_server.get(),
        voice_enabled.get(), auto_listen.get(), keep_awake.get(), met_before, scene_mode});
    context->foreground_tasks().cancel();
    network_pending = false;
    network_lost = false;
    disconnect();
    busy.set(false);
    press.reset();
    button_began = 0;
    settings_page = 0;
    settings_notice.clear();
    settings_ui.keyboard_focus = false;
    settings_ui.previous_hash = 0;
    settings_open.set(true);
    status.set("对话已暂停，返回后继续连接");
    sync_labels();
  }
  void leave_settings(bool commit = false) {
    if (settings_saving && !commit)
      return;
    if (editing != EditField::none)
      finish_edit(false);
    if (settings_snapshot && !commit) {
      auto &original = *settings_snapshot;
      url.set(std::move(original.url));
      token.set(std::move(original.token));
      endpoint.set(std::move(original.endpoint));
      device.set(std::move(original.device));
      client.set(std::move(original.client));
      activation_code = std::move(original.activation_code);
      activation_message = std::move(original.activation_message);
      official_server.set(original.official);
      voice_enabled.set(original.voice);
      auto_listen.set(original.listen);
      keep_awake.set(original.awake);
      scene_mode = original.scene;
      met_before = original.met;
      sync_labels();
    }
    if (commit && settings_snapshot) {
      const auto &old = *settings_snapshot;
      if (old.official != official_server.get() || old.endpoint != endpoint.get() ||
          old.device != device.get() || old.client != client.get() ||
          (!official_server.get() && (old.url != url.get() || old.token != token.get()))) {
        heard.set("");
        subtitle.set("");
      }
    }
    settings_snapshot.reset();
    settings_open.set(false);
    tap_debounce_until = now_ms() + 350;
    press.reset();
    companion.previous_hash = 0;
    manual_retry = false;
    retry_after = 0;
    next_network_check = 0;
    if (user_paused)
      status.set(std::string(idle_message));
  }
  Task<void> apply_settings(bool send_after_save = false) {
    if (settings_saving || !settings_open.get())
      co_return Result<void>{};
    settings_saving = true;
    struct SaveGuard { bool &saving; ~SaveGuard() { saving = false; } } guard{settings_saving};
    if (!valid_config(!official_server.get())) {
      settings_notice = "请检查服务器地址、令牌和设备标识";
      co_return Result<void>{};
    }
    auto saved = co_await save();
    if (!saved) {
      settings_notice = saved.error() == Error::limit_exceeded
                            ? "配置太长，请缩短地址或令牌"
                            : "未能保存，请稍后再试";
      co_return Result<void>{};
    }
    capture_supported = true;
    if (send_after_save && !draft.get().empty()) {
      send_draft = true;
      user_paused = false;
    }
    leave_settings(true);
    co_return Result<void>{};
  }
  void start_settings_save(bool send_after_save = false) {
    // A foreground change must not abandon a configuration write halfway.
    auto started = context->tasks().start(apply_settings(send_after_save));
    if (!started)
      settings_notice = "暂时无法保存，请稍后重试";
  }
  State<std::string> &edit_target() {
    switch (editing) {
    case EditField::url:
      return url;
    case EditField::token:
      return token;
    case EditField::endpoint:
      return endpoint;
    case EditField::device:
      return device;
    case EditField::client:
      return client;
    default:
      return draft;
    }
  }
  std::string_view edit_title() const {
    switch (editing) {
    case EditField::url:
      return "服务器地址";
    case EditField::token:
      return "访问令牌";
    case EditField::endpoint:
      return "配置服务地址";
    case EditField::device:
      return "Device-Id";
    case EditField::client:
      return "Client-Id";
    default:
      return "写一句话给她";
    }
  }
  void sync_editor_box() {
    auto r = settings_ui.editor_bounds();
    const auto d = std::max(1u, environment.display().get().density_q16);
    editor_box.set({{int(std::int64_t(r.x) * 65536 / d),
                     int(std::int64_t(r.y) * 65536 / d),
                     int(std::int64_t(r.width) * 65536 / d),
                     int(std::int64_t(r.height) * 65536 / d)},
                    editing != EditField::none,
                    Font::body,
                    xiaozhi::SettingsUi::ink});
  }
  void begin_edit(EditField field) {
    if (settings_saving)
      return;
    editing = field;
    edit_value.set(edit_target().get());
    edit_secret.set(field == EditField::token);
    edit_error.clear();
    settings_ui.keyboard_focus = false;
    sync_editor_box();
    focus_pending = true;
  }
  void finish_edit(bool apply) {
    if (editing == EditField::none)
      return;
    bool text = editing == EditField::text;
    auto limit = editing == EditField::token ? 1024u
                 : editing == EditField::device || editing == EditField::client
                     ? 64u
                     : 512u;
    if (apply && edit_value.get().size() > limit) {
      edit_error = "内容过长，请缩短后再确认";
      return;
    }
    if (apply) {
      if (!text && edit_target().get() != edit_value.get()) {
        met_before = false;
        activation_code.clear();
        activation_message.clear();
      }
      edit_target().set(edit_value.get());
    }
    (void)editor.hide_keyboard();
    editing = EditField::none;
    edit_value.set("");
    focus_pending = false;
    sync_editor_box();
    settings_ui.previous_hash = 0;
    if (apply && text && !draft.get().empty()) {
      start_settings_save(true);
    }
  }
  void setting_action(xiaozhi::SettingHit hit) {
    if (settings_saving)
      return;
    using A = xiaozhi::SettingAction;
    settings_notice.clear();
    switch (hit.action) {
    case A::awake:
      keep_awake.set(!keep_awake.get());
      break;
    case A::back:
      if (settings_page == 3)
        settings_page = 0;
      else
        leave_settings();
      break;
    case A::tab:
      settings_page = hit.value;
      settings_ui.keyboard_focus = false;
      break;
    case A::details:
      settings_page = 3;
      break;
    case A::official:
      if (!official_server.get()) {
        official_server.set(true);
        url.set("");
        token.set("");
        endpoint.set("https://api.tenclass.net/xiaozhi/ota/");
        met_before = false;
        activation_code.clear();
      }
      break;
    case A::custom:
      official_server.set(false);
      met_before = false;
      activation_code.clear();
      break;
    case A::voice:
      voice_enabled.set(!voice_enabled.get());
      break;
    case A::listen:
      auto_listen.set(!auto_listen.get());
      break;
    case A::auto_scene:
      scene_mode = scene_mode ? 0 : automatic_scene + 1;
      break;
    case A::scene:
      scene_mode = hit.value;
      break;
    case A::edit_url:
      begin_edit(EditField::url);
      break;
    case A::edit_token:
      begin_edit(EditField::token);
      break;
    case A::edit_endpoint:
      begin_edit(EditField::endpoint);
      break;
    case A::edit_device:
      begin_edit(EditField::device);
      break;
    case A::edit_client:
      begin_edit(EditField::client);
      break;
    case A::edit_text:
      begin_edit(EditField::text);
      break;
    case A::save:
      start_settings_save();
      break;
    case A::edit_done:
      finish_edit(true);
      break;
    case A::edit_cancel:
      finish_edit(false);
      break;
    default:
      break;
    }
    sync_labels();
    settings_ui.previous_hash = 0;
  }
  void draw_settings() {
    settings_ui.draw(companion.canvas, environment.display().get(),
                     companion.background,
                     {settings_page, scene_mode, official_server.get(),
                      voice_enabled.get(), auto_listen.get(),
                      editing != EditField::none, editing == EditField::text,
                      keep_awake.get(), settings_saving,
                      url.get(), token.get(), endpoint.get(), device.get(),
                      client.get(), settings_notice, edit_title(), edit_error});
    if (editing != EditField::none)
      sync_editor_box();
    if (focus_pending && editor.mounted()) {
      focus_pending = false;
      (void)editor.show_keyboard();
    }
    companion.update(*context, 0, scene_mode ? scene_mode - 1 : automatic_scene,
                     false, now_ms(), false);
  }
  void pointer(const CanvasPointer &event) {
    auto timed = event;
    timed.timestamp_us = now_ms() * 1000;
    auto result = press.update(timed, {std::chrono::milliseconds(700), 12});
    if (settings_open.get()) {
      if (result == PressResult::tap) {
        const auto &m = environment.display().get();
        setting_action(
            settings_ui.hit(canvas_to_surface_coordinate(event.x, m),
                            canvas_to_surface_coordinate(event.y, m)));
      }
    } else if (result == PressResult::tap)
      interact();
    else if (result == PressResult::long_press)
      enter_settings();
  }
  void on_controller(Context &, const input::ControllerState &event) {
    if (!controller.update(event))
      return;
    if (settings_open.get()) {
      if (controller.pressed(input::controller_b)) {
        if (editing != EditField::none)
          finish_edit(false);
        else
          setting_action({{}, xiaozhi::SettingAction::back});
      } else if (editing != EditField::none) {
        if (controller.pressed(input::controller_a | input::controller_start))
          finish_edit(true);
      } else {
        if (controller.pressed(input::controller_left))
          setting_action(
              {{}, xiaozhi::SettingAction::tab, (settings_page + 2) % 3});
        if (controller.pressed(input::controller_right))
          setting_action(
              {{}, xiaozhi::SettingAction::tab, (settings_page + 1) % 3});
        if (controller.pressed(input::controller_down))
          settings_ui.move_focus(1);
        if (controller.pressed(input::controller_up))
          settings_ui.move_focus(-1);
        if (controller.pressed(input::controller_a))
          setting_action(settings_ui.selected());
        if (controller.pressed(input::controller_start))
          start_settings_save();
      }
      return;
    }
    constexpr auto action = input::controller_a | input::controller_start;
    if (controller.pressed(action)) {
      button_began = now_ms();
      button_long_fired = false;
    }
    if (controller.released(action)) {
      if (button_began && !button_long_fired)
        interact();
      button_began = 0;
    }
    if (controller.pressed(input::controller_select))
      enter_settings();
  }
  BackAction on_back() {
    if (settings_open.get()) {
      if (editing != EditField::none)
        finish_edit(false);
      else
        setting_action({{}, xiaozhi::SettingAction::back});
      return BackAction::stay;
    }
    return BackAction::close;
  }
  void tick() {
    const auto now = now_ms();
    const bool stay_awake = keep_awake.get() && !settings_open.get() && ready &&
                            (listening || waiting || speaking);
    if (!awake_request || *awake_request != stay_awake) {
      if (context->window().keep_screen_on(stay_awake))
        awake_request = stay_awake;
    }
    if (!settings_open.get() &&
        (press.advance(now * 1000, {std::chrono::milliseconds(700), 12}) ||
         (button_began && !button_long_fired && now - button_began >= 700))) {
      button_long_fired = true;
      enter_settings();
    }
    if (settings_open.get()) {
      draw_settings();
      return;
    }
    if (waiting && response_wait.expired(now)) {
      recover_response("这次回复等得有点久");
    }
    if (network_lost) {
      network_lost = false;
      context->foreground_tasks().cancel();
      network_pending = false;
      disconnect();
      busy.set(false);
      status.set(met_before
                     ? "网络断开了，我在这里等你\n连接 Wi-Fi 后会自动继续"
                     : "未连接网络\n请先在系统设置中连接 Wi-Fi");
    }
    if (connect_deadline && now > connect_deadline) {
      context->foreground_tasks().cancel();
      network_pending = false;
      fail("连接超时，轻触重试\n长按可更改服务器");
    } else
      pump();
    if (profile_loaded && now >= next_network_check && !network_pending) {
      next_network_check = now + 2000;
      launch(check_network());
    }
    if (profile_loaded && !user_paused && !busy.get() && !network_pending && !socket &&
        connectivity != Connectivity::offline && !manual_retry &&
        now >= retry_after)
      launch(bootstrap());
    if (ready && tts_finished && playback && !drain_pending &&
        now >= next_drain_check) {
      if (now >= drain_deadline) {
        fail("语音播放超时，轻触重试");
      } else {
        next_drain_check = now + 40;
        launch(check_audio_drain());
      }
    }
    if (ready && !busy.get() && prepare_audio && !conversation_preparing) {
      conversation_preparing = true;
      busy.set(true);
      launch(prepare_conversation());
    }
    if (ready && !busy.get() && send_draft && !prepare_audio && !speaking) {
      want_listen = false;
      send_text();
      if (draft.get().empty())
        send_draft = false;
    }
    if (ready && !busy.get() && want_listen && !speaking && !listening &&
        pending_text.empty() && now >= capture_retry_after) {
      want_listen = false;
      launch(record());
    }
    auto room = xiaozhi::RoomMode::arriving;
    if (connectivity == Connectivity::offline)
      room =
          met_before ? xiaozhi::RoomMode::present : xiaozhi::RoomMode::offline;
    else if (!activation_code.empty())
      room = xiaozhi::RoomMode::activation;
    else if (met_before)
      room = xiaozhi::RoomMode::present;
    const auto shown_emotion = user_paused ? 0 : !ready && met_before ? 3
        : waiting && response_wait.phase() == xiaozhi::ResponseWait::Phase::thinking ? 4 : emotion;
    companion.draw(environment.display().get(), status.get(), heard.get(),
                   subtitle.get(), speaking, ready, listening, waiting, now,
                   room, activation_code, activation_message, !voice_enabled.get(),
                   user_paused || (ready && !busy.get() && !listening && !waiting &&
                       !speaking && !prepare_audio && subtitle.get().empty()));
    companion.update(*context, shown_emotion,
                     scene_mode ? scene_mode - 1 : automatic_scene, speaking,
                     now, room == xiaozhi::RoomMode::present);
    if (profile_loaded && now >= next_time_check && !time_pending) {
      next_time_check = now + 60000;
      time_pending = true;
      auto query = context->tasks().start(refresh_scene_time());
      if (!query)
        time_pending = false;
    }
  }
  Result<void> on_start(Context &ctx, std::span<const std::byte> config) {
    context = &ctx;
    auto initialized = environment.initialize(config);
    if (!initialized)
      return initialized;
    auto full =
        ctx.window().configure({.edge_to_edge = true,
                                .status_bar_mode = WindowBarMode::hidden,
                                .navigation_bar_mode = WindowBarMode::hidden});
    if (!full)
      return full;
    auto clock = ctx.clock().set_period(20);
    if (!clock)
      return clock;
    auto art = ctx.tasks().start(companion.load(ctx));
    if (!art)
      return art;
    return ctx.tasks().start(load());
  }
  void on_frame(Context &, game::FrameTick) { tick(); }
  Result<bool> on_event(Context &, const Event &event) {
    auto updated = environment.update(event);
    if (!updated)
      return updated;
    if (event.service == 11 && event.opcode == 0x8001) {
      context->foreground_tasks().cancel();
      disconnect();
      network_permission = Permission{};
      socket_permission = Permission{};
      play_permission = Permission{};
      mic_permission = Permission{};
      manual_retry = true;
      fail("权限已撤销，轻触可重新授权");
    }
    return true;
  }
  void on_background(Context &) {
    awake_request.reset();
    if (editing != EditField::none)
      finish_edit(false);
    press.reset();
    controller.reset();
    button_began = 0;
    network_pending = false;
    disconnect();
    busy.set(false);
    companion.previous_hash = 0;
  }
  void on_foreground(Context &) {
    awake_request.reset();
    next_time_check = 0;
    press.reset();
    controller.reset();
    network_pending = false;
    next_network_check = 0;
    retry_after = 0;
    companion.previous_hash = 0;
    if (user_paused)
      status.set(std::string(idle_message));
  }
  void on_stop(StopReason) { disconnect(); }
  struct ScenePointer {
    App *app = nullptr;
    void operator()(const CanvasPointer &p) const { app->pointer(p); }
  };
  struct EditorSubmit {
    App *app = nullptr;
    void operator()() const { app->finish_edit(true); }
  };
  auto view() {
    return Stack(Canvas(companion.canvas).on_pointer(ScenePointer{this}),
                 TextInput(edit_value, editor)
                     .box(editor_box)
                     .max_bytes(1024)
                     .single_line()
                     .on_submit(EditorSubmit{this})
                     .password(edit_secret)
                     .background(0xfffcf6ff)
                     .rgba(xiaozhi::SettingsUi::ink)
                     .border_width(0_dp)
                     .radius(2_dp))
        .fill()
        .fill_height();
  }
};
} // namespace
PXA_APPLICATION(App)
