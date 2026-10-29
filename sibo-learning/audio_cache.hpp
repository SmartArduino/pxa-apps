// SPDX-License-Identifier: MIT
#pragma once
#include "model.hpp"
#include <cstring>
#include <functional>
#include <memory>
#include <optional>
#include <pxa/app.hpp>

namespace sibo {
// Encoded MP3 only. Full sources use bounded RAM; short sources/word prefixes
// share a strictly bounded private cache, subordinate to personal records.
// Small private-file slots are disposable. A final checked header commits a file;
// cancellation, power loss, truncation and checksum failure produce a miss.
struct AudioCache {
  static constexpr size_t limit = 1024 * 1024, disk_limit = 24 * 1024;
  struct Memory {
    std::string url;
    std::unique_ptr<std::byte[]> data;
    size_t capacity = 0, used = 0;
    uint32_t age = 0, coverage = 0;
    bool complete = false;
  };
  struct Disk {
    std::string url;
    uint32_t size = 0, hash = 0, age = 0, coverage = 0;
    size_t bytes = 0;
  };
  pxa::Context *ctx = nullptr;
  pxa::TaskScope tasks;
  std::array<Memory, 3> memory;
  std::array<Disk, 3> disk;
  std::optional<pxa::HttpExchange> stream;
  std::optional<pxa::Permission> permission;
  std::string url, label;
  std::optional<pxa::Error> error;
  std::function<void()> changed;
  std::function<size_t()> disk_budget;
  std::function<pxa::Task<void>(size_t)> reserve_disk;
  unsigned requested_end = 0, clip_coverage = 0;
  size_t clip_bytes = 0;
  uint64_t generation = 0;
  uint32_t clock = 0;
  int current = -1;
  bool checked = false, loading = false, downloading = false,
       streaming = false, ended = false, saving = false, pending_save = false;
  ~AudioCache() {
    // Pending frames borrow buffers/flags below; destroy them before members.
    tasks.cancel();stream.reset();permission.reset();
  }
  static uint32_t hash(std::span<const std::byte> bytes,
                       uint32_t h = 2166136261u) {
    for (auto c : bytes) h = (h ^ std::to_integer<uint8_t>(c)) * 16777619u;
    return h;
  }
  static std::string path(unsigned slot) {
    return ".sibo-audio-" + std::to_string(slot) + ".bin";
  }
  bool busy() const { return loading || downloading || saving || pending_save; }
  void notify(std::string text) {
    label = std::move(text);
    if (changed) changed();
  }
  void log(std::string text) {
    if (ctx) (void)ctx->log().write(pxa::LogLevel::info, text);
  }
  void cancel_pending() {
    ++generation;
    tasks.cancel();
    stream.reset(); permission.reset();
    loading = downloading = streaming = saving = pending_save = false;
    ended = false; clip_coverage = 0; clip_bytes = 0;
    if (current >= 0 && !memory[current].complete) memory[current] = {};
    current = -1;
    url.clear();
    error.reset();
  }
  void fail(pxa::Error e) {
    stream.reset(); permission.reset();
    loading = downloading = streaming = false;
    error = e;
  }
  pxa::Task<void> invalidate(std::string key) {
    struct Reset { bool &v; ~Reset(){v=false;} } reset{loading};
    for(unsigned i=0;i<disk.size();++i)if(disk[i].url==key)(void)co_await discard(i);
    co_return pxa::Result<void>{};
  }
  void reject() {
    auto key=url;cancel_pending();
    for(auto &m:memory)if(m.url==key)m={};
    loading=true;
    if(!tasks.start(invalidate(std::move(key))))loading=false;
  }
  bool allocate(std::string_view address, size_t bytes) {
    if (!bytes || bytes > limit) return false;
    size_t total = bytes;
    for (const auto &m : memory) total += m.capacity;
    for (;;) {
      int oldest = -1;
      for (unsigned i = 0; i < memory.size(); ++i)
        if (memory[i].data && (oldest < 0 || memory[i].age < memory[oldest].age)) oldest = i;
      if (total <= limit && std::any_of(memory.begin(), memory.end(),
                                       [](const auto &m) { return !m.data; })) break;
      if (oldest < 0) return false;
      total -= memory[oldest].capacity;
      memory[oldest] = {};
    }
    current = int(std::find_if(memory.begin(), memory.end(),
                               [](const auto &m) { return !m.data; }) - memory.begin());
    auto &m = memory[current];
    m.data.reset(new (std::nothrow) std::byte[bytes]);
    if (!m.data) { current = -1; return false; }
    m.url = address; m.capacity = bytes; m.age = ++clock;
    return true;
  }
  void select(pxa::Context &context, std::string address, unsigned end_ms = 0) {
    ctx = &context;
    tasks.reap();
    requested_end = end_ms;
    auto covers = [&](unsigned end) { return !end || (end_ms && end_ms<=end); };
    if (url == address && (current<0 || covers(memory[current].coverage)) && !error &&
        (busy() || streaming || (current >= 0 && memory[current].complete))) {
      if (current >= 0 && memory[current].complete) {
        memory[current].age=++clock;
        log("sibo audio cache hit memory"); notify("缓存播放");
      }
      return;
    }
    cancel_pending();
    url = std::move(address);
    if (!trusted(url)) { fail(pxa::Error::invalid_argument); return; }
    for (unsigned i = 0; i < memory.size(); ++i)
      if (memory[i].complete && memory[i].url == url && covers(memory[i].coverage)) {
        current = i; memory[i].age = ++clock;
        pending_save = std::none_of(disk.begin(), disk.end(),
                                    [&](const auto &d) { return d.url == url; });
        log("sibo audio cache hit memory"); notify("缓存播放"); return;
      }
    loading = true;
    notify("首次加载中");
    if (!tasks.start(open(generation))) fail(pxa::Error::resource_limit);
  }
  size_t disk_bytes() const { size_t n=0; for(const auto &d:disk)n+=d.bytes; return n; }
  pxa::Task<void> discard(unsigned slot) {
    auto r=co_await ctx->fs().remove(path(slot));
    if(!r && r.error()!=pxa::Error::not_found)co_return r;
    disk[slot]={};co_return pxa::Result<void>{};
  }
  void note_clip(unsigned end_ms,size_t bytes) {
    if(current<0 || !end_ms || memory[current].coverage || bytes>memory[current].used)return;
    clip_coverage=end_ms;clip_bytes=bytes;
    if(memory[current].complete)
      pending_save=std::none_of(disk.begin(),disk.end(),[&](const auto &d) {
        return d.url==url && (!d.coverage || d.coverage>=end_ms);
      });
  }
  pxa::Task<void> evict_disk() {
    if(!ctx)co_return pxa::Result<void>{};
    defer_persist();pending_save=false;
    (void)co_await ctx->fs().remove("sibo-audio.tmp");
    for(unsigned i=0;i<disk.size();++i)(void)co_await discard(i);
    co_return pxa::Result<void>{};
  }
  pxa::Task<void> trim_disk() {
    const auto budget=std::min(disk_limit,disk_budget?disk_budget():size_t(0));
    while(disk_bytes()>budget) {
      int oldest=-1;
      for(unsigned i=0;i<disk.size();++i)
        if(disk[i].bytes && (oldest<0 || disk[i].age<disk[oldest].age))oldest=i;
      if(oldest<0 || !(co_await discard(oldest)))break;
    }
    co_return pxa::Result<void>{};
  }
  static pxa::Result<void> read_exact(pxa::File &file, std::span<std::byte> out) {
    while (!out.empty()) {
      auto n = file.read(out.first(std::min<size_t>(4096, out.size())));
      if (!n || !*n) return std::unexpected(n ? pxa::Error::io_error : n.error());
      out = out.subspan(*n);
    }
    return {};
  }
  pxa::Task<void> inspect() {
    if (checked) co_return pxa::Result<void>{};
    (void)co_await ctx->fs().remove("sibo-audio.tmp");
    for (unsigned i = 0; i < disk.size(); ++i) {
      auto file = co_await ctx->fs().open(path(i));
      if (!file) continue;
      auto size = co_await file->seek(0, pxa::SeekOrigin::end);
      auto start = co_await file->seek(0);
      std::array<std::byte, 32> h{};
      std::string key;
      bool valid = size && start && *size >= 32 && *size <= disk_limit && read_exact(*file, h);
      if (valid) {
        auto n = pxa::wire::get16(h.data() + 16);
        auto bytes = pxa::wire::get32(h.data() + 8);
        valid = !std::memcmp(h.data(), "SIBOAUD1", 8) && bytes && bytes <= disk_limit && n && n <= 1024 &&
                pxa::wire::get16(h.data() + 18) == 1 && *size == 32u + n + bytes;
        if (valid) {
          key.resize(n);
          valid = bool(read_exact(*file, std::as_writable_bytes(std::span{key.data(), key.size()}))) &&
                  trusted(key) && hash(std::as_bytes(std::span{key.data(), key.size()}),
                    hash(std::span{h}.first(28))) == pxa::wire::get32(h.data() + 28);
        }
      }
      (void)file->close();
      if (valid) {
        disk[i] = {std::move(key), pxa::wire::get32(h.data() + 8),
                   pxa::wire::get32(h.data() + 20), pxa::wire::get32(h.data() + 24),
                   pxa::wire::get32(h.data() + 12), size_t(*size)};
        clock = std::max(clock, disk[i].age);
      } else if (size && *size) (void)co_await discard(i);
    }
    checked = true;
    co_return pxa::Result<void>{};
  }
  pxa::Task<void> open(uint64_t tag) {
    using namespace pxa;
    (void)co_await inspect();
    if (tag != generation) co_return Result<void>{};
    for (unsigned i = 0; i < disk.size(); ++i) {
      if (disk[i].url != url || (disk[i].coverage &&
          (!requested_end || requested_end>disk[i].coverage)) || !allocate(url, disk[i].size)) continue;
      auto file = co_await ctx->fs().open(path(i));
      bool valid = bool(file);
      if (valid) {
        auto seek = co_await file->seek(32 + url.size());
        valid = bool(seek);
      }
      auto &m = memory[current];
      while (valid && m.used < m.capacity) {
        auto n = file->read(std::span{m.data.get() + m.used, std::min<size_t>(4096, m.capacity - m.used)});
        valid = n && *n;
        if (!valid) break;
        m.used += *n;
        if (m.used % 16384 < *n) (void)co_await ctx->clock().yield();
      }
      if (tag != generation) co_return Result<void>{};
      if (valid && hash({m.data.get(), m.used}) == disk[i].hash) {
        m.complete = true; m.age = disk[i].age = ++clock; m.coverage=disk[i].coverage;
        loading = false;
        log("sibo audio cache hit disk"); notify("缓存播放");
        co_return Result<void>{};
      }
      if (file) (void)file->close();
      memory[current] = {}; current = -1;
      (void)co_await discard(i);
      log("sibo audio cache invalid; retry network");
    }
    auto grant = co_await ctx->permissions().acquire("net.client", "web");
    if (tag != generation) co_return Result<void>{};
    if (!grant) { fail(grant.error()); co_return Result<void>{}; }
    static constexpr std::array<std::string_view, 1> wanted{"content-type"};
    auto request = co_await ctx->http().open({.url=url, .wanted_headers=wanted,
        .options={.buffer_bytes=8192,.header_bytes=512,.max_body_bytes=64*1024*1024,.timeout_ms=3600000}}, *grant);
    if (tag != generation) co_return Result<void>{};
    if (!request) { fail(request.error()); co_return Result<void>{}; }
    auto response = co_await std::move(*request).response();
    if (tag != generation) co_return Result<void>{};
    if (!response || response->status_code != 200) {
      fail(response ? Error::unavailable : response.error()); co_return Result<void>{};
    }
    if(auto mime=response->headers.first("content-type")) {
      std::string type(*mime);
      for(auto &c:type)if(c>='A' && c<='Z')c=char(c-'A'+'a');
      if(type.starts_with("text/") || type.find("json")!=type.npos || type.find("xml")!=type.npos) {
        fail(Error::unsupported);co_return Result<void>{};
      }
    }
    const bool retain = response->body_length && *response->body_length <= limit &&
                        allocate(url, size_t(*response->body_length));
    stream.emplace(std::move(response->body)); permission.emplace(std::move(*grant));
    loading = false; downloading = retain; streaming = !retain;
    log("sibo audio cache network"); notify(retain ? "边听边缓存" : "在线播放");
    co_return Result<void>{};
  }
  void defer_persist() {
    if (saving) { tasks.cancel(); saving=false; pending_save=true; }
  }
  void tick(bool allow_save) {
    tasks.reap();
    if (allow_save && pending_save && !saving && current>=0 && memory[current].complete) {
      pending_save=false; saving=true;
      if (!tasks.start(persist(current,generation))) saving=false;
    }
    if (!downloading || current < 0 || !stream) return;
    auto &m = memory[current];
    for (unsigned work=0;work<4;++work) {
      std::byte probe;
      auto out = m.used < m.capacity
          ? std::span{m.data.get()+m.used,std::min<size_t>(4096,m.capacity-m.used)}
          : std::span{&probe,size_t(1)};
      auto source=stream->readable(); auto n=source.transport->io(source.handle,1,out);
      if (!n) { if(n.error()!=pxa::Error::would_block) fail(n.error()); return; }
      if (*n>out.size() || (*n && m.used==m.capacity)) { fail(pxa::Error::protocol_error); return; }
      if (!*n) {
        if (m.used!=m.capacity) { fail(pxa::Error::io_error); return; }
        m.complete=true; downloading=false; stream.reset(); permission.reset();
        log("sibo audio cache complete bytes="+std::to_string(m.used)); notify("已缓存");
        pending_save=true;
        return;
      }
      m.used+=*n;
    }
  }
  pxa::Result<uint32_t> read(size_t offset, std::span<std::byte> out) {
    if (error) return std::unexpected(*error);
    if (loading) return std::unexpected(pxa::Error::would_block);
    if (current >= 0) {
      auto &m=memory[current];
      if(offset>m.used) return std::unexpected(pxa::Error::bad_state);
      auto n=std::min(out.size(),m.used-offset);
      if(n) { std::memcpy(out.data(),m.data.get()+offset,n); return uint32_t(n); }
      if(m.complete) return 0;
    } else if (streaming && stream) {
      auto s=stream->readable(); auto n=s.transport->io(s.handle,1,out);
      if(n && !*n) { ended=true; stream.reset(); permission.reset(); }
      return n;
    } else if(ended) return 0;
    return std::unexpected(pxa::Error::would_block);
  }
  pxa::Task<void> persist(int index, uint64_t tag) {
    using namespace pxa;
    struct Reset { bool &v; ~Reset(){v=false;} } reset{saving};
    if(!memory[index].complete || url.size()>1024)co_return Result<void>{};
    auto &m=memory[index];
    const auto budget=std::min(disk_limit,disk_budget?disk_budget():size_t(0));
    size_t payload=m.used;unsigned coverage=m.coverage;
    if(32+url.size()+payload>budget && clip_coverage && clip_bytes) {
      payload=clip_bytes;coverage=clip_coverage;
    }
    const size_t bytes=32+url.size()+payload;
    if(bytes>budget)co_return Result<void>{};
    unsigned slot=0;
    for(unsigned i=0;i<disk.size();++i) {
      if(disk[i].url==url) {slot=i;break;}
      if(disk[i].age<disk[slot].age)slot=i;
    }
    if(!(co_await discard(slot)))co_return Result<void>{};
    while(disk_bytes()+bytes>budget) {
      int oldest=-1;
      for(unsigned i=0;i<disk.size();++i)
        if(disk[i].bytes && (oldest<0 || disk[i].age<disk[oldest].age))oldest=i;
      if(oldest<0 || !(co_await discard(oldest)))co_return Result<void>{};
    }
    if(reserve_disk)(void)co_await reserve_disk(bytes);
    if(tag!=generation)co_return Result<void>{};
    auto file=co_await ctx->fs().open("sibo-audio.tmp",OpenMode::write|OpenMode::create|OpenMode::truncate);
    if(!file)co_return Result<void>{};
    std::array<std::byte,32> h{};
    auto write=[&](std::span<const std::byte> data)->bool {
      while(!data.empty()) {
        auto n=file->write(data.first(std::min<size_t>(4096,data.size())));
        if(!n || !*n)return false;data=data.subspan(*n);
      }return true;
    };
    bool ok=write(h) && write(std::as_bytes(std::span{url.data(),url.size()}));
    for(size_t at=0;ok && at<payload;) {
      auto n=std::min<size_t>(4096,payload-at);ok=write({m.data.get()+at,n});at+=n;
      (void)co_await ctx->clock().yield();
    }
    if(ok) {
      std::memcpy(h.data(),"SIBOAUD1",8);wire::put32(h.data()+8,payload);
      wire::put32(h.data()+12,coverage);wire::put16(h.data()+16,url.size());wire::put16(h.data()+18,1);
      const auto sum=hash({m.data.get(),payload});wire::put32(h.data()+20,sum);wire::put32(h.data()+24,m.age);
      wire::put32(h.data()+28,hash(std::as_bytes(std::span{url.data(),url.size()}),hash(std::span{h}.first(28))));
      auto seek=co_await file->seek(0);ok=bool(seek);
      if(ok) {auto n=file->write(h);ok=n && *n==h.size();}
      ok=bool(file->close()) && ok;
      if(ok) {
        auto replaced=co_await ctx->fs().replace("sibo-audio.tmp",path(slot));ok=bool(replaced);
      }
      if(ok) {
        disk[slot]={url,uint32_t(payload),sum,m.age,coverage,bytes};
        log("sibo audio cache saved bytes="+std::to_string(payload));notify("已保存缓存");
      }
    }
    (void)file->close();
    if(!ok)(void)co_await ctx->fs().remove("sibo-audio.tmp");
    co_return Result<void>{};
  }
  pxa::Task<void> clear() {
    cancel_pending();
    for(auto &m:memory)m={};
    if(ctx) {
      (void)co_await inspect();
      for(unsigned i=0;i<disk.size();++i)(void)co_await discard(i);
    }
    notify("");co_return pxa::Result<void>{};
  }
};
} // namespace sibo
