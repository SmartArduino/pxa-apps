// SPDX-License-Identifier: MIT
#pragma once
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
namespace media::json {
struct Reader {
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
          c = e == 'n' ? '\n' : e == 'r' ? '\r' : e == 't' ? '\t' : ' ';
        else
          return false;
      } else if (c >= 128) {
        if (c > 0xf4)
          return false;
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
    const uint8_t *number_start = p;
    if (p < end && *p == '0')
      ++p;
    else
      while (p < end && *p >= '0' && *p <= '9')
        ++p;
    if (number_start == p)
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

struct Value {
  const uint8_t *p = nullptr;
  size_t n = 0;
  explicit operator bool() const { return p && n; }
  bool string(char *out, size_t cap, bool *truncated = nullptr) const {
    if (!p || !cap) {
      if (cap)
        out[0] = 0;
      return false;
    }
    Reader r{p, p + n};
    return r.str(out, cap, truncated);
  }
  uint64_t number(uint64_t fallback = 0) const {
    if (!p || !n || *p < '0' || *p > '9')
      return fallback;
    uint64_t v = 0;
    for (size_t i = 0; i < n; ++i) {
      if (p[i] < '0' || p[i] > '9' || v > (UINT64_MAX - (p[i] - '0')) / 10)
        return fallback;
      v = v * 10 + p[i] - '0';
    }
    return v;
  }
  bool boolean() const { return p && n == 4 && !std::memcmp(p, "true", 4); }
  bool is_null() const { return !p || (n == 4 && !std::memcmp(p, "null", 4)); }
  Value get(const char *name) const {
    if (!p)
      return {};
    Reader r{p, p + n};
    if (!r.take('{'))
      return {};
    if (r.take('}'))
      return {};
    do {
      char key[96];
      if (!r.str(key, sizeof(key)) || !r.take(':'))
        return {};
      r.ws();
      const uint8_t *start = r.p;
      if (!r.skip())
        return {};
      if (!std::strcmp(key, name))
        return {start, size_t(r.p - start)};
      if (r.take('}'))
        return {};
    } while (r.take(','));
    return {};
  }
};
struct Array {
  Reader r;
  bool ready = false;
  Array(Value v) : r{v.p, v.p ? v.p + v.n : nullptr} {
    if (v)
      ready = r.take('[');
  }
  bool next(Value &out) {
    if (!ready || r.take(']')) {
      ready = false;
      return false;
    }
    r.ws();
    const uint8_t *begin = r.p;
    if (!r.skip()) {
      ready = false;
      return false;
    }
    out = {begin, size_t(r.p - begin)};
    if (!r.take(','))
      ready = false;
    return true;
  }
};
inline bool encode(const char *s, char *out, size_t cap) {
  size_t at = 0;
  constexpr char hex[] = "0123456789ABCDEF";
  for (const unsigned char *p = (const unsigned char *)s; *p; ++p) {
    unsigned c = *p;
    bool safe = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
                c == '~';
    unsigned n = safe ? 1 : 3;
    if (at + n >= cap)
      return false;
    if (safe)
      out[at++] = c;
    else {
      out[at++] = '%';
      out[at++] = hex[c >> 4];
      out[at++] = hex[c & 15];
    }
  }
  if (at >= cap)
    return false;
  out[at] = 0;
  return true;
}
} // namespace media::json
