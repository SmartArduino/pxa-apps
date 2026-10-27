// SPDX-License-Identifier: MIT
#include "model.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
namespace music {
using media::json::Array;
using media::json::Value;
bool parse_track(Value v, Track &t) {
  Track s;
  s.id = v.get("id").number();
  if (!s.id || !v.get("name").string(s.name, sizeof(s.name)))
    return false;
  Value ar = v.get("artists");
  if (!ar)
    ar = v.get("ar");
  Array a(ar);
  Value artist;
  size_t used = 0;
  while (a.next(artist)) {
    char n[128]{};
    artist.get("name").string(n, sizeof(n));
    if (n[0]) {
      int k = std::snprintf(s.artist + used, sizeof(s.artist) - used, "%s%s",
                            used ? " / " : "", n);
      if (k < 0 || size_t(k) >= sizeof(s.artist) - used)
        break;
      used += k;
    }
  }
  Value album = v.get("album");
  if (!album)
    album = v.get("al");
  album.get("name").string(s.album, sizeof(s.album));
  album.get("picUrl").string(s.cover, sizeof(s.cover));
  s.duration = v.get("duration").number(v.get("dt").number());
  s.fee = v.get("fee").number();
  t = s;
  return true;
}
unsigned parse_tracks(Value root, Track *tracks, unsigned cap) {
  Value list = root.get("result").get("songs");
  if (!list)
    list = root.get("songs");
  if (!list)
    list = root;
  Array a(list);
  Value v;
  unsigned n = 0;
  while (n < cap && a.next(v)) {
    Track t;
    if (parse_track(v, t))
      tracks[n++] = t;
  }
  return n;
}
unsigned parse_discovery(Value root, Track *tracks, unsigned cap) {
  if (!root || !tracks || root.get("code").number() != 200)
    return 0;
  media::json::Reader reader{root.p, root.p + root.n};
  if (!reader.skip())
    return 0;
  reader.ws();
  if (reader.p != reader.end)
    return 0;
  Array items(root.get("result"));
  Value item;
  unsigned n = 0;
  while (n < cap && items.next(item)) {
    Track track;
    if (!parse_track(item.get("song"), track))
      continue;
    bool duplicate = false;
    for (unsigned i = 0; i < n; ++i)
      duplicate |= tracks[i].id == track.id;
    if (!duplicate)
      tracks[n++] = track;
  }
  return n;
}
// One atomic Storage value, under its 2048-byte limit. Keep titles/artists and
// identities; album art and full metadata are fetched again when playing.
size_t save_discovery(const Track *tracks, unsigned count, uint64_t fetched_at,
                      uint8_t *out, size_t cap) {
  if (!tracks || !out || !count || count > MaxTracks || cap < 16)
    return 0;
  std::memcpy(out, "MDC1", 4);
  out[4] = count;
  out[5] = out[6] = out[7] = 0;
  for (unsigned i = 0; i < 8; ++i)
    out[8 + i] = fetched_at >> (8 * i);
  size_t at = 16;
  for (unsigned i = 0; i < count; ++i) {
    Track compact = tracks[i];
    compact.album[0] = compact.cover[0] = 0;
    for (char *s : {compact.name, compact.artist}) {
      size_t n = std::min(size_t(63), std::strlen(s));
      while (n && (uint8_t(s[n]) & 0xc0) == 0x80)
        --n;
      s[n] = 0;
    }
    if (cap - at < 2)
      return 0;
    size_t size = save_track(compact, out + at + 2, cap - at - 2);
    if (!size)
      return 0;
    out[at] = size;
    out[at + 1] = size >> 8;
    at += size + 2;
  }
  return at;
}
unsigned load_discovery(const uint8_t *in, size_t size, Track *tracks,
                        unsigned cap, uint64_t &fetched_at) {
  if (!in || !tracks || size < 16 || std::memcmp(in, "MDC1", 4) || !in[4] ||
      in[4] > MaxTracks || in[4] > cap || in[5] || in[6] || in[7])
    return 0;
  unsigned count = in[4];
  size_t at = 16;
  for (unsigned i = 0; i < count; ++i) {
    if (size - at < 2)
      return 0;
    size_t length = in[at] | unsigned(in[at + 1]) << 8;
    at += 2;
    if (length > size - at || !load_track(in + at, length, tracks[i]))
      return 0;
    for (unsigned j = 0; j < i; ++j)
      if (tracks[j].id == tracks[i].id)
        return 0;
    at += length;
  }
  if (at != size)
    return 0;
  fetched_at = 0;
  for (unsigned i = 0; i < 8; ++i)
    fetched_at |= uint64_t(in[8 + i]) << (8 * i);
  return count;
}
void Lyrics::parse(const char *source) {
  count = 0;
  timed = false;
  offset = 0;
  text[0] = 0;
  size_t used = 0;
  if (!source)
    return;
  const char *p = source;
  while (*p && count < 256) {
    const char *end = std::strchr(p, '\n');
    if (!end)
      end = p + std::strlen(p);
    const char *q = p;
    uint32_t stamps[12];
    unsigned n = 0;
    while (q < end && *q == '[') {
      const char *close =
          static_cast<const char *>(std::memchr(q, ']', end - q));
      if (!close)
        break;
      if (!std::strncmp(q, "[offset:", 8)) {
        char *stop;
        long v = std::strtol(q + 8, &stop, 10);
        if (stop == close && v >= -600000 && v <= 600000)
          offset = v;
      } else {
        unsigned min = 0, sec = 0, frac = 0, digits = 0;
        const char *t = q + 1;
        bool ok = t < close && *t >= '0' && *t <= '9';
        while (t < close && *t >= '0' && *t <= '9') {
          if (min > 1440) {
            ok = false;
            break;
          }
          min = min * 10 + *t++ - '0';
        }
        ok = ok && t < close && *t++ == ':';
        const char *ss = t;
        while (t < close && *t >= '0' && *t <= '9') {
          sec = sec * 10 + *t++ - '0';
          if (t - ss > 2) {
            ok = false;
            break;
          }
        }
        ok = ok && t > ss && sec < 60;
        if (t < close && (*t == '.' || *t == ':')) {
          ++t;
          while (t < close && *t >= '0' && *t <= '9') {
            if (digits < 3)
              frac = frac * 10 + *t - '0';
            ++digits;
            ++t;
          }
          if (!digits)
            ok = false;
          while (digits < 3) {
            frac *= 10;
            ++digits;
          }
        }
        if (ok && t == close && n < 12)
          stamps[n++] = (min * 60 + sec) * 1000 + frac;
      }
      q = close + 1;
    }
    while (q < end && (*q == ' ' || *q == '\r' || *q == '\t'))
      ++q;
    while (end > q && (end[-1] == '\r' || end[-1] == ' '))
      --end;
    size_t len = end - q;
    if (len && len < 1024 && used + len + 1 < sizeof(text)) {
      if (n) {
        timed = true;
        for (unsigned i = 0; i < n && count < 256; ++i)
          lines[count++] = {stamps[i], uint16_t(used), uint16_t(len)};
      } else if (q == p)
        lines[count++] = {UINT32_MAX, uint16_t(used), uint16_t(len)};
      if (n || q == p) {
        std::memcpy(text + used, q, len);
        text[used + len] = 0;
        used += len + 1;
      }
    }
    p = *end == '\n' ? end + 1 : std::strchr(p, '\n');
    if (!p)
      break;
    if (*p == '\n')
      ++p;
  }
  if (timed) {
    unsigned out = 0;
    for (unsigned i = 0; i < count; ++i)
      if (lines[i].time != UINT32_MAX)
        lines[out++] = lines[i];
    count = out;
    for (unsigned i = 1; i < count; ++i) {
      Line v = lines[i];
      unsigned j = i;
      while (j && lines[j - 1].time > v.time) {
        lines[j] = lines[j - 1];
        --j;
      }
      lines[j] = v;
    }
  }
}
int Lyrics::current(uint64_t ms) const {
  if (!count)
    return -1;
  if (!timed)
    return 0;
  int64_t adjusted = int64_t(ms) + offset;
  int at = -1;
  for (unsigned i = 0; i < count && int64_t(lines[i].time) <= adjusted; ++i)
    at = i;
  return at;
}
const char *Lyrics::get(unsigned i, char *out, size_t cap) const {
  if (!cap)
    return "";
  out[0] = 0;
  if (i >= count)
    return out;
  size_t n = std::min(cap - 1, size_t(lines[i].length));
  while (n < lines[i].length && n && (text[lines[i].offset + n] & 0xc0) == 0x80)
    --n;
  std::memcpy(out, text + lines[i].offset, n);
  out[n] = 0;
  return out;
}
size_t save_track(const Track &t, uint8_t *out, size_t cap) {
  if (!t.id || cap < 18)
    return 0;
  std::memcpy(out, "MSF1", 4);
  for (unsigned i = 0; i < 8; ++i)
    out[4 + i] = t.id >> (8 * i);
  for (unsigned i = 0; i < 4; ++i)
    out[12 + i] = t.duration >> (8 * i);
  out[16] = t.fee;
  size_t n = 17;
  for (auto s : {t.name, t.artist, t.album, t.cover}) {
    size_t z = std::strlen(s);
    if (n + 2 + z > cap)
      return 0;
    out[n++] = z & 255;
    out[n++] = z >> 8;
    std::memcpy(out + n, s, z);
    n += z;
  }
  return n;
}
bool load_track(const uint8_t *in, size_t size, Track &out) {
  if (size < 25 || std::memcmp(in, "MSF1", 4))
    return false;
  Track t;
  for (unsigned i = 0; i < 8; ++i)
    t.id |= uint64_t(in[4 + i]) << (8 * i);
  for (unsigned i = 0; i < 4; ++i)
    t.duration |= uint32_t(in[12 + i]) << (8 * i);
  t.fee = in[16];
  size_t n = 17;
  char *dest[] = {t.name, t.artist, t.album, t.cover};
  size_t caps[] = {sizeof(t.name), sizeof(t.artist), sizeof(t.album),
                   sizeof(t.cover)};
  for (unsigned i = 0; i < 4; ++i) {
    if (n + 2 > size)
      return false;
    size_t z = in[n] | unsigned(in[n + 1]) << 8;
    n += 2;
    if (z >= caps[i] || z > size - n || std::memchr(in + n, 0, z))
      return false;
    std::memcpy(dest[i], in + n, z);
    dest[i][z] = 0;
    n += z;
  }
  if (n != size || !t.id || !t.name[0])
    return false;
  out = t;
  return true;
}
} // namespace music
