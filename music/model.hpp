// SPDX-License-Identifier: MIT
#pragma once
#include "../common/media/json.hpp"
#include <cstddef>
#include <cstdint>
namespace music {
constexpr unsigned MaxTracks = 12, MaxFavorites = 16;
struct Track {
  uint64_t id = 0;
  char name[128]{}, artist[128]{}, album[128]{}, cover[513]{};
  uint32_t duration = 0;
  uint8_t fee = 0;
};
unsigned parse_tracks(media::json::Value, Track *, unsigned);
unsigned parse_discovery(media::json::Value, Track *, unsigned);
size_t save_discovery(const Track *, unsigned, uint64_t fetched_at, uint8_t *,
                      size_t);
unsigned load_discovery(const uint8_t *, size_t, Track *, unsigned,
                        uint64_t &fetched_at);
bool parse_track(media::json::Value, Track &);
struct Line {
  uint32_t time;
  uint16_t offset, length;
};
struct Lyrics {
  Line lines[256]{};
  char text[32768]{};
  unsigned count = 0;
  bool timed = false;
  int32_t offset = 0;
  void parse(const char *);
  int current(uint64_t) const;
  const char *get(unsigned, char *, size_t) const;
};
size_t save_track(const Track &, uint8_t *, size_t);
bool load_track(const uint8_t *, size_t, Track &);
} // namespace music
