// SPDX-License-Identifier: MIT
#include "model.hpp"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
int main(int argc, char **argv) {
  using namespace music;
  Lyrics l;
  l.parse("[ar:Author]\n[offset:+120]\n[00:01.20][00:03.456]第一句\r\n[00:02."
          "000]Second line\n[00:59.9]Last\n");
  assert(l.count == 4 && l.timed && l.offset == 120);
  assert(l.lines[0].time == 1200 && l.lines[1].time == 2000 &&
         l.lines[2].time == 3456 && l.lines[3].time == 59900);
  assert(l.current(1079) == -1 && l.current(1080) == 0 && l.current(1900) == 1);
  char line[100];
  assert(std::string(l.get(2, line, sizeof(line))) == "第一句");
  l.parse("Plain lyrics\nSecond line\n");
  assert(l.count == 2 && !l.timed && l.current(100) == 0);
  l.parse("[00:99]bad\n[bogus]bad\n[offset:-300]\n[00:00.01]A\n");
  assert(l.count == 1 && l.current(299) == -1 && l.current(310) == 0);
  l.parse("");
  assert(!l.count);
  const char *raw =
      R"({"result":{"songs":[{"id":123,"name":"hello \ud83d\udc9c","artists":[{"name":"中文"},{"name":"Other"}],"album":{"name":"Album","picUrl":"https://a/b","picId":109951172349668373},"duration":123456,"fee":8}]}})";
  Track t[12];
  assert(parse_tracks({(const uint8_t *)raw, std::strlen(raw)}, t, 12) == 1);
  assert(t[0].id == 123 && t[0].duration == 123456 &&
         std::string(t[0].artist) == "中文 / Other");
  uint8_t buf[1024];
  size_t n = save_track(t[0], buf, sizeof(buf));
  assert(n);
  Track restored;
  assert(load_track(buf, n, restored) && restored.id == 123 &&
         !std::strcmp(restored.name, t[0].name));
  for (size_t i = 0; i < n; ++i)
    assert(!load_track(buf, i, restored));
  const char *escaped = R"("[00:00]first\n[00:01]next")";
  char text[100];
  assert(
      media::json::Value{(const uint8_t *)escaped, std::strlen(escaped)}.string(
          text, sizeof(text)));
  l.parse(text);
  assert(l.count == 2);
  if (argc > 1) {
    std::ifstream f(argv[1]);
    std::string j{std::istreambuf_iterator<char>(f), {}};
    assert(parse_tracks({(const uint8_t *)j.data(), j.size()}, t, 12) > 0);
  }
  if (argc > 2) {
    std::ifstream f(argv[2]);
    std::string j{std::istreambuf_iterator<char>(f), {}};
    media::json::Value v{(const uint8_t *)j.data(), j.size()};
    char text[16384];
    assert(v.get("lrc").get("lyric").string(text, sizeof(text)));
    l.parse(text);
    assert(l.count > 10 && l.timed);
  }
}
