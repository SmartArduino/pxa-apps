// SPDX-License-Identifier: MIT
#pragma once
#include "../common/media/decoder.hpp"
#include "../common/media/pcm_packet.hpp"
#include "model.hpp"
#include <cstring>
#include <ctime>
#include <functional>
#include <optional>
#include <pxa/app.hpp>
namespace sibo {
inline uint64_t now_ms() {
  timespec t{};
  clock_gettime(CLOCK_MONOTONIC, &t);
  return uint64_t(t.tv_sec) * 1000 + t.tv_nsec / 1000000;
}
// Decode bounded MP3 frames in the Guest; the Host only needs its PCM sink.
// No Music decoder, filesystem audio download, or SD card is required.
struct AudioPlayer {
  pxa::Context *ctx = nullptr;
  pxa::TaskScope scope;
  // Permission handles own their authority: closing one also closes the
  // network/audio resources issued under it. Keep both for the whole stream.
  std::optional<pxa::Permission> net_permission, audio_permission;
  std::optional<pxa::HttpExchange> stream;
  std::optional<pxa::AudioSession> session;
  media::Decoder decoder;
  media::PcmPacket packet;
  std::array<uint8_t, 32768> compressed{};
  size_t head = 0, size = 0;
  unsigned pcm_count = 0, pcm_offset = 0, rate = 0, channels = 0;
  bool active = false, paused = false, eof = false, opening = false,
       draining = false, querying = false;
  uint64_t generation = 0, sent = 0, drain_at = 0, last_ui = 0;
  std::string status = "准备好，就听一听";
  std::function<void()> changed;
  void notify() {
    if (changed)
      changed();
  }
  void tick_period(bool value) {
    if (ctx)
      (void)ctx->clock().set_period(value ? 20 : 0);
  }
  void stop() {
    ++generation;
    scope.cancel();
    stream.reset();
    session.reset();
    net_permission.reset();
    audio_permission.reset();
    active = paused = opening = draining = querying = false;
    head = size = pcm_count = pcm_offset = 0;
    tick_period(false);
    status = "已停止";
    notify();
  }
  void finish(std::string text) {
    stream.reset();
    session.reset();
    net_permission.reset();
    audio_permission.reset();
    active = paused = opening = draining = querying = false;
    tick_period(false);
    status = std::move(text);
    if (ctx)
      (void)ctx->log().write(pxa::LogLevel::info, "sibo audio: " + status);
    notify();
  }
  void start(pxa::Context &context, std::string url) {
    stop();
    ctx = &context;
    active = true;
    eof = false;
    sent = 0;
    rate = channels = 0;
    decoder.reset();
    packet.reset();
    status = "正在连接朗读…";
    tick_period(true);
    auto tag = generation;
    auto launched = scope.start(open(std::move(url), tag));
    if (!launched)
      finish("无法启动朗读，请重试");
    notify();
  }
  void pause_resume() {
    if (!active)
      return;
    paused = !paused;
    status = paused ? "暂停了，想听时再继续" : "继续听故事吧";
    notify();
  }
  pxa::Task<void> open(std::string url, uint64_t tag) {
    using namespace pxa;
    if (!trusted(url)) {
      finish("朗读地址无效");
      co_return Result<void>{};
    }
    auto permission = co_await ctx->permissions().acquire("net.client", "web");
    if (tag != generation)
      co_return Result<void>{};
    if (!permission) {
      finish("需要联网授权才能听朗读");
      co_return Result<void>{};
    }
    static constexpr std::array<std::string_view, 1> wanted{"content-type"};
    auto request = co_await ctx->http().open(
        {.url = url,
         .wanted_headers = wanted,
         .options = {.buffer_bytes = 8192,
                     .header_bytes = 512,
                     .max_body_bytes = 64 * 1024 * 1024,
                     .timeout_ms = 3600000}},
        *permission);
    if (tag != generation)
      co_return Result<void>{};
    if (!request) {
      finish("朗读连接失败，请检查网络");
      co_return Result<void>{};
    }
    auto response = co_await std::move(*request).response();
    if (tag != generation)
      co_return Result<void>{};
    if (!response || response->status_code != 200) {
      finish("这段朗读暂时不能播放");
      co_return Result<void>{};
    }
    net_permission.emplace(std::move(*permission));
    stream.emplace(std::move(response->body));
    status = "正在准备声音…";
    notify();
    co_return Result<void>{};
  }
  pxa::Task<void> open_output(uint64_t tag) {
    using namespace pxa;
    auto permission =
        co_await ctx->permissions().acquire("audio.playback", "media");
    if (tag != generation)
      co_return Result<void>{};
    if (!permission) {
      finish("需要播放授权才能听朗读");
      co_return Result<void>{};
    }
    auto value =
        co_await ctx->audio().open_pcm(*permission, rate, uint8_t(channels));
    if (tag != generation)
      co_return Result<void>{};
    if (!value) {
      finish("设备暂不能输出这段声音");
      co_return Result<void>{};
    }
    audio_permission.emplace(std::move(*permission));
    session.emplace(std::move(*value));
    auto gain = co_await session->graph(-3 * 256);
    (void)gain;
    if (tag != generation)
      co_return Result<void>{};
    opening = false;
    status = "声音来了，慢慢听";
    notify();
    co_return Result<void>{};
  }
  pxa::Task<void> query_end(uint64_t tag) {
    using namespace pxa;
    auto q = co_await session->query();
    if (tag != generation)
      co_return Result<void>{};
    querying = false;
    if (!q || q->queued_samples == 0)
      finish("这一段听完啦");
    co_return Result<void>{};
  }
  void tick() {
    scope.reap();
    if (!active)
      return;
    const auto now = now_ms();
    if (now - last_ui >= 500) {
      last_ui = now;
      notify();
    }
    if (paused || opening || !stream)
      return;
    if (draining) {
      if (now - drain_at > 4000) {
        finish("这一段听完啦");
        return;
      }
      if (now - drain_at > 160 && !querying) {
        querying = true;
        if (!scope.start(query_end(generation)))
          finish("这一段听完啦");
      }
      return;
    }
    if (!eof && size < 24576) {
      if (head && head + size + 4096 > compressed.size()) {
        std::memmove(compressed.data(), compressed.data() + head, size);
        head = 0;
      }
      for (unsigned i = 0; i < 4 && size < 24576; ++i) {
        auto source = stream->readable();
        auto available =
            std::min<size_t>(4096, compressed.size() - head - size);
        if (!available)
          break;
        auto bytes = std::span<std::byte>(
            reinterpret_cast<std::byte *>(compressed.data() + head + size),
            available);
        auto read = source.transport->io(source.handle, 1, bytes);
        if (!read) {
          if (read.error() != pxa::Error::would_block) {
            if (ctx)
              (void)ctx->log().write(
                  pxa::LogLevel::warning,
                  "sibo HTTP body error=" + std::to_string(int(read.error())) +
                      " buffered=" + std::to_string(size));
            finish("网络中断了，可以点朗读重新听");
          }
          break;
        }
        if (*read > available) {
          finish("朗读数据异常，请重试");
          return;
        }
        if (!*read) {
          eof = true;
          break;
        }
        size += *read;
      }
    }
    if (!active)
      return;
    auto begin = now_ms();
    for (unsigned work = 0; work < 12 && now_ms() - begin < 5; ++work) {
      if (packet.full() ||
          (eof && !size && pcm_offset == pcm_count && packet.frames)) {
        if (!session)
          return;
        const auto count = packet.frames * packet.channels;
        auto written = session->write_pcm(
            std::as_bytes(std::span{packet.samples, size_t(count)}));
        if (!written) {
          if (written.error() != pxa::Error::would_block)
            finish("声音输出中断，请重试");
          return;
        }
        if (*written != count * sizeof(int16_t)) {
          finish("声音输出格式异常");
          return;
        }
        if (!sent && ctx)
          (void)ctx->log().write(pxa::LogLevel::info,
                                 "sibo PCM started " + std::to_string(rate) +
                                     "Hz " + std::to_string(channels) + "ch");
        sent += packet.frames;
        packet.frames = 0;
        continue;
      }
      if (pcm_offset < pcm_count) {
        if (!session)
          return;
        pcm_offset += packet.append(decoder.pcm + pcm_offset * channels,
                                    pcm_count - pcm_offset);
        continue;
      }
      if (!size) {
        if (eof) {
          if (!sent) {
            finish("未识别到 MP3 朗读");
            return;
          }
          draining = true;
          drain_at = now_ms();
        }
        return;
      }
      if (size < 4096 && !eof)
        return;
      mp3dec_frame_info_t info{};
      int n = decoder.decode(compressed.data() + head, int(size), info);
      if (info.frame_bytes <= 0) {
        if (eof) {
          size = 0;
          continue;
        }
        return;
      }
      const auto consumed = std::min<size_t>(size, unsigned(info.frame_bytes));
      head += consumed;
      size -= consumed;
      if (!size)
        head = 0;
      if (!n)
        continue;
      if (info.hz < 8000 || info.hz > 48000 ||
          (info.channels != 1 && info.channels != 2)) {
        finish("这段朗读的格式暂不支持");
        return;
      }
      if (rate &&
          (rate != unsigned(info.hz) || channels != unsigned(info.channels))) {
        finish("朗读格式发生变化，请重试");
        return;
      }
      pcm_count = unsigned(n);
      pcm_offset = 0;
      if (!rate) {
        rate = unsigned(info.hz);
        channels = unsigned(info.channels);
        packet.configure(rate, channels);
        opening = true;
        if (!scope.start(open_output(generation)))
          finish("无法开启声音输出");
        return;
      }
    }
  }
  std::string position() const {
    const auto sec = rate ? sent / rate : 0;
    char out[32];
    std::snprintf(out, sizeof(out), "%02u:%02u", unsigned(sec / 60),
                  unsigned(sec % 60));
    return out;
  }
};
} // namespace sibo
