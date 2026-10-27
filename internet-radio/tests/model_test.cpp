// SPDX-License-Identifier: MIT
#include "radio_model.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
int main() {
  using namespace radio;
  Station rows[16];
  unsigned count = 0;
  const char *input =
      R"([{"name":"\u4e2d\u6587 \ud83d\udcfb","stationuuid":"test","url":"https://old.example/x","url_resolved":"https://example.org/live.mp3","country":"中国","codec":"mp3","bitrate":64,"hls":0,"lastcheckok":1,"geo":-1.25,"extra":{"list":[true,null,1e2]}},{"name":"HLS","url_resolved":"https://example.org/x","codec":"MP3","hls":1},{"name":"AAC","url_resolved":"https://example.org/y","codec":"AAC","hls":0},{"name":"duplicate","stationuuid":"test","url_resolved":"https://example.org/z","codec":"MP3","hls":0}])";
  assert(parse_stations((const uint8_t *)input, std::strlen(input), rows, 16,
                        count));
  assert(count == 1);
  assert(std::string(rows[0].name) == "中文 📻");
  assert(rows[0].bitrate == 64);
  assert(!std::strcmp(rows[0].url, "https://example.org/live.mp3"));
  const char *bad[] = {"[", R"([{"name":"\ud800"}])", R"([{"name":"\udc00"}])",
                       R"([{"a":[[[[[[[[[[[[[[[[[[[[]]]]]]]]]]]]]]]]]]]]}])",
                       R"([{"name":"bad
literal"}])"};
  for (auto p : bad)
    assert(
        !parse_stations((const uint8_t *)p, std::strlen(p), rows, 16, count));
  assert(!valid_url("file:///etc/passwd"));
  assert(!valid_url("https://a\r\nb"));
  assert(!valid_url("https://user@a/"));
  assert(!valid_url("https:///x"));
  assert(valid_url("http://radio.example:8000/live"));
  char url[513];
  assert(search_url(url, sizeof(url), "de1.api.radio-browser.info",
                    "中文 &爵士", true, 16));
  assert(std::strstr(url, "%E4%B8%AD%E6%96%87%20%26"));
  assert(std::strstr(url, "offset=16"));
  assert(std::strstr(url, "language=chinese"));
  Station s{};
  std::strcpy(s.name, "测试电台");
  std::strcpy(s.url, "https://radio.example/live");
  std::strcpy(s.uuid, "id");
  s.bitrate = 128;
  uint8_t saved[1024];
  size_t size = save_station(s, saved, sizeof(saved));
  assert(size);
  Station loaded;
  assert(load_station(saved, size, loaded));
  assert(same_station(s, loaded) && loaded.bitrate == 128 &&
         !std::strcmp(s.name, loaded.name));
  for (size_t n = 0; n < size; ++n)
    assert(!load_station(saved, n, loaded));
  saved[0] = 0;
  assert(!load_station(saved, size, loaded));
  const uint8_t wire[] = {'a', 'b', 'c', 1,   'x', 'x', 'x', 'x', 'x',
                          'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x',
                          'x', 'x', 'd', 'e', 'f', 0,   'g', 'h', 'i'};
  for (unsigned block = 1; block <= sizeof(wire); ++block) {
    IcyFilter filter;
    filter.reset(3);
    std::string result;
    for (unsigned at = 0; at < sizeof(wire);) {
      unsigned n = std::min(block, unsigned(sizeof(wire) - at));
      uint8_t out[64];
      size_t used = filter.consume(wire + at, n, out);
      result.append((char *)out, used);
      at += n;
    }
    assert(result == "abcdefghi");
  }
  std::puts("radio model: JSON/UTF-8, codec filtering, URL encoding, favorites "
            "and segmented ICY metadata passed");
}
