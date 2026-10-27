// SPDX-License-Identifier: MIT
#include "radio_model.hpp"
#include <climits>
#include <cstdio>
#include <cstring>
#include <initializer_list>
namespace radio {
bool valid_url(const char *p) {
  if (!p || (std::strncmp(p, "https://", 8) && std::strncmp(p, "http://", 7)))
    return false;
  const char *host = std::strstr(p, "://") + 3;
  if (!*host || *host == '/' || *host == '?' || *host == '#')
    return false;
  for (const char *c = p; *c; ++c)
    if ((unsigned char)*c < 33 || (unsigned char)*c > 126 || *c == '\\')
      return false;
  const char *end = host + std::strcspn(host, "/?#");
  for (const char *c = host; c < end; ++c)
    if (*c == '@')
      return false;
  return std::strlen(p) <= 512;
}
bool same_station(const Station &a, const Station &b) {
  return a.uuid[0] && b.uuid[0] ? std::strcmp(a.uuid, b.uuid) == 0
                                : std::strcmp(a.url, b.url) == 0;
}
struct Json {
  const uint8_t *p, *end;
  void ws() {
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'))
      ++p;
  }
  bool take(char c) {
    ws();
    if (p == end || *p != (uint8_t)c)
      return false;
    ++p;
    return true;
  }
  int hex() {
    if (p == end)
      return -1;
    int c = *p++;
    if (c >= '0' && c <= '9')
      return c - '0';
    if (c >= 'a' && c <= 'f')
      return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
      return c - 'A' + 10;
    return -1;
  }
  int code() {
    int n = 0;
    for (int i = 0; i < 4; ++i) {
      int h = hex();
      if (h < 0)
        return -1;
      n = n * 16 + h;
    }
    return n;
  }
  bool str(char *out, size_t cap, bool *truncated = nullptr) {
    if (!take('"'))
      return false;
    size_t used = 0;
    bool full = false;
    while (p < end && *p != '"') {
      uint32_t c = *p++;
      if (c < 32)
        return false;
      if (c == '\\') {
        if (p == end)
          return false;
        int e = *p++;
        if (e == 'u') {
          int h = code();
          if (h < 0)
            return false;
          c = (uint32_t)h;
          if (c >= 0xd800 && c <= 0xdbff) {
            if (end - p < 2 || *p++ != '\\' || *p++ != 'u')
              return false;
            int l = code();
            if (l < 0xdc00 || l > 0xdfff)
              return false;
            c = 0x10000 + ((c - 0xd800) << 10) + (l - 0xdc00);
          } else if (c >= 0xdc00 && c <= 0xdfff)
            return false;
        } else if (e == '"' || e == '\\' || e == '/')
          c = e;
        else if (e == 'b' || e == 'f' || e == 'n' || e == 'r' || e == 't')
          c = ' ';
        else
          return false;
      } else if (c >= 128) {
        unsigned n = c >= 0xf0 ? 3 : c >= 0xe0 ? 2 : c >= 0xc2 ? 1 : 99;
        if (n == 99 || n > size_t(end - p))
          return false;
        uint32_t value = c & ((1u << (6 - n)) - 1);
        for (unsigned i = 0; i < n; ++i) {
          unsigned v = *p++;
          if ((v & 0xc0) != 0x80)
            return false;
          value = (value << 6) | (v & 63);
        }
        if ((n == 1 && value < 128) || (n == 2 && value < 2048) ||
            (n == 3 && value < 65536) || value > 0x10ffff ||
            (value >= 0xd800 && value <= 0xdfff))
          return false;
        c = value;
      }
      if (!c)
        c = ' '; // PXA strings cannot contain embedded NUL.
      uint8_t b[4];
      unsigned n;
      if (c < 128) {
        b[0] = uint8_t(c);
        n = 1;
      } else if (c < 2048) {
        b[0] = 0xc0 | (c >> 6);
        b[1] = 0x80 | (c & 63);
        n = 2;
      } else if (c < 65536) {
        b[0] = 0xe0 | (c >> 12);
        b[1] = 0x80 | ((c >> 6) & 63);
        b[2] = 0x80 | (c & 63);
        n = 3;
      } else {
        b[0] = 0xf0 | (c >> 18);
        b[1] = 0x80 | ((c >> 12) & 63);
        b[2] = 0x80 | ((c >> 6) & 63);
        b[3] = 0x80 | (c & 63);
        n = 4;
      }
      if (out && !full) {
        if (used + n < cap) {
          std::memcpy(out + used, b, n);
          used += n;
        } else
          full = true;
      }
    }
    if (p == end)
      return false;
    ++p;
    if (out) {
      if (!cap)
        return false;
      out[used] = 0;
    }
    if (truncated)
      *truncated = full;
    return true;
  }
  bool number(unsigned &v) {
    ws();
    if (p == end || *p < '0' || *p > '9')
      return false;
    v = 0;
    while (p < end && *p >= '0' && *p <= '9') {
      unsigned c = *p++ - '0';
      if (v > (UINT_MAX - c) / 10)
        return false;
      v = v * 10 + c;
    }
    return true;
  }
  bool skip(unsigned depth = 0) {
    ws();
    if (p == end || depth > 16)
      return false;
    if (*p == '"')
      return str(nullptr, 0);
    if (*p == '{' || *p == '[') {
      bool obj = *p++ == '{';
      ws();
      if (take(obj ? '}' : ']'))
        return true;
      do {
        if (obj && (!str(nullptr, 0) || !take(':')))
          return false;
        if (!skip(depth + 1))
          return false;
        if (take(obj ? '}' : ']'))
          return true;
      } while (take(','));
      return false;
    }
    for (const char *s : {"true", "false", "null"}) {
      size_t n = std::strlen(s);
      if (size_t(end - p) >= n && !std::memcmp(p, s, n)) {
        p += n;
        return true;
      }
    }
    if (*p == '-')
      ++p;
    unsigned dummy;
    if (!number(dummy))
      return false;
    if (p < end && *p == '.') {
      ++p;
      const uint8_t *begin = p;
      while (p < end && *p >= '0' && *p <= '9')
        ++p;
      if (begin == p)
        return false;
    }
    if (p < end && (*p == 'e' || *p == 'E')) {
      ++p;
      if (p < end && (*p == '+' || *p == '-'))
        ++p;
      const uint8_t *begin = p;
      while (p < end && *p >= '0' && *p <= '9')
        ++p;
      if (begin == p)
        return false;
    }
    return true;
  }
};
bool parse_stations(const uint8_t *data, size_t size, Station *rows, size_t cap,
                    unsigned &count) {
  count = 0;
  if (!data || !rows)
    return false;
  Json j{data, data + size};
  if (!j.take('['))
    return false;
  if (j.take(']')) {
    j.ws();
    return j.p == j.end;
  }
  do {
    Station s;
    char key[64], codec[24]{}, original[513]{};
    bool url_cut = false, original_cut = false;
    unsigned hls = 0, ok = 1;
    if (!j.take('{'))
      return false;
    if (!j.take('}'))
      do {
        if (!j.str(key, sizeof(key)) || !j.take(':'))
          return false;
        if (!std::strcmp(key, "name")) {
          if (!j.str(s.name, sizeof(s.name)))
            return false;
        } else if (!std::strcmp(key, "stationuuid")) {
          if (!j.str(s.uuid, sizeof(s.uuid)))
            return false;
        } else if (!std::strcmp(key, "url_resolved")) {
          if (!j.str(s.url, sizeof(s.url), &url_cut))
            return false;
        } else if (!std::strcmp(key, "url")) {
          if (!j.str(original, sizeof(original), &original_cut))
            return false;
        } else if (!std::strcmp(key, "country")) {
          if (!j.str(s.country, sizeof(s.country)))
            return false;
        } else if (!std::strcmp(key, "codec")) {
          if (!j.str(codec, sizeof(codec)))
            return false;
        } else if (!std::strcmp(key, "hls")) {
          if (!j.number(hls))
            return false;
        } else if (!std::strcmp(key, "lastcheckok")) {
          if (!j.number(ok))
            return false;
        } else if (!std::strcmp(key, "bitrate")) {
          unsigned n;
          if (!j.number(n))
            return false;
          s.bitrate = n > 65535 ? 0 : uint16_t(n);
        } else if (!j.skip())
          return false;
        if (j.take('}'))
          break;
        if (!j.take(','))
          return false;
      } while (true);
    if (!s.url[0] && !original_cut)
      std::memcpy(s.url, original, sizeof(original));
    for (char *c = codec; *c; ++c)
      if (*c >= 'a' && *c <= 'z')
        *c -= 32;
    bool duplicate = false;
    for (unsigned i = 0; i < count; ++i)
      if (same_station(rows[i], s))
        duplicate = true;
    if (count < cap && s.name[0] && !url_cut && !hls && ok &&
        !std::strcmp(codec, "MP3") && valid_url(s.url) && !duplicate)
      rows[count++] = s;
    if (j.take(']')) {
      j.ws();
      return j.p == j.end;
    }
  } while (j.take(','));
  return false;
}
bool search_url(char *out, size_t cap, const char *server, const char *query,
                bool chinese, unsigned offset) {
  if (!out || !server || !query)
    return false;
  char encoded[385];
  size_t n = 0;
  for (const unsigned char *p = (const unsigned char *)query; *p; ++p) {
    if (n + 3 >= sizeof(encoded))
      return false;
    unsigned c = *p;
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~')
      encoded[n++] = c;
    else {
      constexpr char hex[] = "0123456789ABCDEF";
      encoded[n++] = '%';
      encoded[n++] = hex[c >> 4];
      encoded[n++] = hex[c & 15];
    }
  }
  encoded[n] = 0;
  int len = std::snprintf(out, cap,
                          "https://%s/json/stations/"
                          "search?codec=MP3&hidebroken=true&order=clickcount&"
                          "reverse=true&limit=16&offset=%u%s&name=%s",
                          server, offset, chinese ? "&language=chinese" : "",
                          encoded);
  return len > 0 && size_t(len) < cap;
}
size_t save_station(const Station &s, uint8_t *out, size_t cap) {
  if (!out || cap < 6 || !valid_url(s.url) || !s.name[0])
    return 0;
  std::memcpy(out, "NRF1", 4);
  size_t n = 4;
  for (const char *p : {s.uuid, s.name, s.url, s.country}) {
    size_t len = std::strlen(p);
    if (n + 2 + len + 2 > cap)
      return 0;
    out[n++] = len & 255;
    out[n++] = len >> 8;
    std::memcpy(out + n, p, len);
    n += len;
  }
  out[n++] = s.bitrate & 255;
  out[n++] = s.bitrate >> 8;
  return n;
}
bool load_station(const uint8_t *data, size_t size, Station &s) {
  if (!data || size < 14 || std::memcmp(data, "NRF1", 4))
    return false;
  Station v;
  size_t n = 4;
  char *dest[] = {v.uuid, v.name, v.url, v.country};
  size_t caps[] = {sizeof(v.uuid), sizeof(v.name), sizeof(v.url),
                   sizeof(v.country)};
  for (unsigned i = 0; i < 4; ++i) {
    if (n + 2 > size)
      return false;
    size_t len = data[n] | unsigned(data[n + 1]) << 8;
    n += 2;
    if (len >= caps[i] || len > size - n || std::memchr(data + n, 0, len))
      return false;
    std::memcpy(dest[i], data + n, len);
    dest[i][len] = 0;
    n += len;
  }
  if (n + 2 != size || !v.name[0] || !valid_url(v.url))
    return false;
  v.bitrate = data[n] | unsigned(data[n + 1]) << 8;
  s = v;
  return true;
}
size_t IcyFilter::consume(const uint8_t *in, size_t n, uint8_t *out) {
  if (!interval) {
    std::memcpy(out, in, n);
    return n;
  }
  size_t used = 0;
  while (n) {
    if (meta_left) {
      size_t k = n < meta_left ? n : meta_left;
      in += k;
      n -= k;
      meta_left -= k;
    } else if (!audio_left) {
      meta_left = unsigned(*in++) * 16;
      --n;
      audio_left = interval;
    } else {
      size_t k = n < audio_left ? n : audio_left;
      std::memcpy(out + used, in, k);
      used += k;
      in += k;
      n -= k;
      audio_left -= k;
    }
  }
  return used;
}
} // namespace radio
