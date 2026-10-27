// SPDX-License-Identifier: MIT
#include "../main.cpp"
#include <cassert>
#include <fstream>
#include <string>
#include <vector>
static unsigned cache_writes = 0;
extern "C" int32_t pxa_call(uint32_t operation, uint64_t token, const uint8_t *,
                            uint32_t) {
  if (operation == (PXA_STORAGE_SET << 16 | PXA_STORAGE_SERVICE) && token == 8)
    ++cache_writes;
  return 0;
}
extern "C" int32_t pxa_io(uint64_t, uint32_t, uint8_t *, uint32_t) {
  return PXA_STATUS_WOULD_BLOCK;
}
static void feed(const std::string &json) {
  assert(json.size() < sizeof(json_data));
  std::memcpy(json_data, json.data(), json.size());
  json_size = json.size();
  job = Discover;
  job_song = current.id;
  job_done();
}
static void cached(const uint8_t *bytes, size_t size) {
  std::vector<uint8_t> payload(size + 8), wire(size + 40);
  uint32_t n = 0;
  pxa_store_u16(payload.data() + 4, 2);
  pxa_store_u16(payload.data() + 6, size);
  std::memcpy(payload.data() + 8, bytes, size);
  assert(pxa_build_message(wire.data(), wire.size(), PXA_STORAGE_SERVICE,
                           PXA_STORAGE_GET, 7, payload.data(), payload.size(),
                           &n));
  pxa_app_on_event(wire.data(), n);
}
int main(int argc, char **argv) {
  assert(argc == 2);
  std::ifstream f(argv[1]);
  std::string fixture{std::istreambuf_iterator<char>(f), {}};
  assert(!fixture.empty());
  Track parsed[12];
  unsigned n = music::parse_discovery(
      {(const uint8_t *)fixture.data(), fixture.size()}, parsed, 12);
  assert(n == 12 && parsed[0].id && parsed[0].artist[0] && parsed[0].cover[0]);
  const char *valid =
      R"({"code":200,"result":[{"song":{"id":1,"name":"真实歌曲甲","artists":[{"name":"歌手甲"}],"duration":12000}},{"song":{"id":1,"name":"duplicate"}},{"song":{"id":0,"name":"invalid"}},{"song":{"id":2,"name":"真实歌曲乙","artists":[{"name":"歌手乙"}],"fee":1}}]})";
  assert(music::parse_discovery({(const uint8_t *)valid, std::strlen(valid)},
                                parsed, 12) == 2 &&
         parsed[1].fee == 1);
  const char *bad =
      R"({"code":200,"result":[{"song":{"id":42,"name":"must not accept partial JSON"}}])";
  assert(!music::parse_discovery({(const uint8_t *)bad, std::strlen(bad)},
                                 parsed, 12));
  assert(music::parse_discovery({(const uint8_t *)valid, std::strlen(valid)},
                                parsed, 1) == 1);
  feed(fixture);
  assert(discovery_live && discovery_count == 12 && !discovery_cached);
  assert(discovery_cache_pending);
  flush_discovery_cache();
  assert(cache_writes == 1 && !discovery_cache_pending);
  uint64_t first = discovery[0].id;
  assert(list_count() == 12 && list(0)->id == first);
  uint8_t cache[2048];
  size_t bytes =
      music::save_discovery(discovery, 12, 1790000000, cache, sizeof(cache));
  assert(bytes && bytes <= 2048);
  Track restored[12];
  uint64_t stamp = 0;
  assert(music::load_discovery(cache, bytes, restored, 12, stamp) == 12 &&
         stamp == 1790000000 && restored[0].id == first &&
         !std::strcmp(restored[0].name, discovery[0].name));
  for (size_t i = 0; i < bytes; ++i)
    assert(!music::load_discovery(cache, i, restored, 12, stamp));
  assert(!music::load_discovery(cache, bytes, restored, 1, stamp));
  // Worst-case UTF-8 titles/artists stay complete and fit one atomic value.
  for (unsigned i = 0; i < 12; ++i) {
    restored[i] = discovery[i];
    std::memset(restored[i].name, 0, sizeof(restored[i].name));
    std::memset(restored[i].artist, 0, sizeof(restored[i].artist));
    for (int j = 0; j < 30; ++j) {
      std::strcat(restored[i].name, "音");
      std::strcat(restored[i].artist, "乐");
    }
  }
  size_t long_bytes = music::save_discovery(restored, 12, 1, discovery_cache,
                                            sizeof(discovery_cache));
  assert(long_bytes && long_bytes <= 2048);
  assert(music::load_discovery(discovery_cache, long_bytes, parsed, 12,
                               stamp) == 12 &&
         std::strlen(parsed[0].name) == 63);
  discovery_live = false;
  discovery_count = 0;
  scene.initialized = 1;
  dirty = false;
  cached(cache, bytes);
  assert(discovery_cached && discovery_count == 12 && list(0)->id == first);
  // A late disk read cannot replace a newer network result.
  discovery_live = true;
  discovery[0].id = 999;
  cached(cache, bytes);
  assert(discovery[0].id == 999);
  discovery[0].id = first;
  // Search results remain separate; entering discovery restores the network
  // list.
  tracks[0].id = 888;
  count = 1;
  view = 1;
  assert(list_count() == 1 && list(0)->id == 888);
  view = 0;
  assert(list_count() == 12 && list(0)->id == first);
  net_permission = audio_permission = UINT64_C(0x100000001);
  choose(discovery[0]);
  assert(queue_count == 12 && queue[0].id == first);
  view = 0;
  uint64_t queue_first = queue[0].id;
  position = 3456;
  audio = UINT64_C(0x100000009);
  job = Discover;
  job_failed(false);
  assert(want_play && audio && position == 3456 && discovery_count == 12 &&
         discovery_cached && queue[0].id == queue_first);
  feed("{\"code\":503,\"result\":[]}");
  assert(discovery_count == 12 && discovery[0].id == first && want_play &&
         audio);
  feed(valid);
  assert(discovery_count == 2 && discovery[0].id == 1 && current.id == first &&
         queue[0].id == first && want_play && audio);
  unsigned before_cache = cache_writes;
  assert(discovery_cache_pending);
  flush_discovery_cache();
  assert(cache_writes == before_cache && discovery_cache_pending);
  paused = true;
  flush_discovery_cache();
  assert(cache_writes == before_cache + 1 && !discovery_cache_pending);
  paused = false;
  job = Resolve;
  request_discovery(true);
  pump_discovery();
  assert(job == Resolve && discovery_pending);
  job = None;
  pump_discovery();
  assert(job == Discover && play_token == 0 &&
         std::strstr(job_url, "personalized/newsong"));
  cancel_job();
  discovery_pending = false;
  request_discovery(true);
  assert(!discovery_pending); // refresh rate limited
  stop();
  std::puts("discovery: live schema, malformed data, duplicate filtering, "
            "bounded atomic UTF-8 cache, cache race, search isolation, "
            "queue/playback preservation, deferred refresh passed");
}
