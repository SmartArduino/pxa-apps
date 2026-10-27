// SPDX-License-Identifier: MIT
#pragma once
#include <cstddef>
#include <cstdint>
namespace radio {
constexpr unsigned PageSize = 16, FavoriteCount = 12;
struct Station {
  char uuid[40]{}, name[128]{}, url[513]{}, country[80]{};
  uint16_t bitrate{};
};
bool valid_url(const char *url);
bool same_station(const Station &, const Station &);
bool parse_stations(const uint8_t *, size_t, Station *, size_t, unsigned &);
bool search_url(char *, size_t, const char *server, const char *query,
                bool chinese, unsigned offset);
size_t save_station(const Station &, uint8_t *, size_t);
bool load_station(const uint8_t *, size_t, Station &);
struct IcyFilter {
  uint32_t interval{}, audio_left{}, meta_left{};
  void reset(uint32_t n) {
    interval = n;
    audio_left = n;
    meta_left = 0;
  }
  // The caller reserves output space at least as large as input.
  size_t consume(const uint8_t *, size_t, uint8_t *);
};
} // namespace radio
