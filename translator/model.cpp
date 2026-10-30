// SPDX-License-Identifier: MIT
#include "model.hpp"
#include "../common/media/json.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
namespace translate {
namespace {
size_t unit(const unsigned char *s, size_t n) {
  if (!n)
    return 0;
  unsigned c = s[0], k = c < 0x80                 ? 1
                         : c >= 0xc2 && c <= 0xdf ? 2
                         : c >= 0xe0 && c <= 0xef ? 3
                         : c >= 0xf0 && c <= 0xf4 ? 4
                                                  : 0;
  if (!k || k > n)
    return 0;
  uint32_t value = c & ((1u << (7 - k)) - 1);
  if (k == 1)
    return c ? 1 : 0;
  for (unsigned i = 1; i < k; ++i) {
    if ((s[i] & 0xc0) != 0x80)
      return 0;
    value = (value << 6) | (s[i] & 63);
  }
  if ((k == 2 && value < 128) || (k == 3 && value < 2048) ||
      (k == 4 && value < 65536) || value > 0x10ffff ||
      (value >= 0xd800 && value <= 0xdfff))
    return 0;
  return k;
}
bool safe(unsigned c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~';
}
size_t encoded(const char *s, size_t n) {
  size_t result = 0;
  for (size_t i = 0; i < n; ++i)
    result += safe((unsigned char)s[i]) ? 1 : 3;
  return result;
}
bool encode(const char *s, size_t n, char *out, size_t cap) {
  const char hex[] = "0123456789ABCDEF";
  size_t at = 0;
  for (size_t i = 0; i < n; ++i) {
    unsigned c = (unsigned char)s[i];
    if (at + (safe(c) ? 1 : 3) >= cap)
      return false;
    if (safe(c))
      out[at++] = char(c);
    else {
      out[at++] = '%';
      out[at++] = hex[c >> 4];
      out[at++] = hex[c & 15];
    }
  }
  out[at] = 0;
  return true;
}
size_t segment(const char *s, size_t start, size_t maximum) {
  size_t n = std::strlen(s), end = start, last = 0;
  while (end < n) {
    size_t k = unit((const unsigned char *)s + end, n - end);
    if (!k || encoded(s + start, end - start + k) > maximum)
      break;
    end += k;
    if (s[end - 1] == ' ' || s[end - 1] == '\n' || s[end - 1] == '.' ||
        s[end - 1] == '!' || s[end - 1] == '?' ||
        (k == 3 && !std::memcmp(s + end - 3, "。", 3)))
      last = end;
  }
  if (end < n && last > start + (end - start) / 3)
    end = last;
  return end;
}
bool document(const uint8_t *p, size_t n) {
  if (!p || !n)
    return false;
  media::json::Reader r{p, p + n};
  r.ws();
  if (r.p == r.end || *r.p != '{' || !r.skip())
    return false;
  r.ws();
  return r.p == r.end;
}
void put16(uint8_t *p, unsigned v) {
  p[0] = uint8_t(v);
  p[1] = uint8_t(v >> 8);
}
unsigned get16(const uint8_t *p) { return p[0] | unsigned(p[1]) << 8; }
void put32(uint8_t *p, uint32_t v) {
  for (unsigned i = 0; i < 4; ++i)
    p[i] = uint8_t(v >> (i * 8));
}
uint32_t get32(const uint8_t *p) {
  uint32_t v = 0;
  for (unsigned i = 0; i < 4; ++i)
    v |= uint32_t(p[i]) << (i * 8);
  return v;
}
// MyMemory segments can contain inline translation-memory formatting tags.
void clean_markup(char *text) {
  size_t to = 0;
  for (size_t i = 0; text[i];) {
    if (text[i] == '<' &&
        (text[i + 1] == '/' || (text[i + 1] >= 'a' && text[i + 1] <= 'z'))) {
      const char *end = std::strchr(text + i, '>');
      if (end && size_t(end - text - i) < 128) {
        i = size_t(end - text) + 1;
        continue;
      }
    }
    struct Entity {
      const char *from, *to;
    };
    static constexpr Entity entities[] = {
        {"&amp;", "&"}, {"&quot;", "\""}, {"&apos;", "'"}, {"&#39;", "'"},
        {"&lt;", "<"},  {"&gt;", ">"},    {"&nbsp;", " "}};
    bool found = false;
    for (auto &e : entities) {
      size_t n = std::strlen(e.from);
      if (!std::strncmp(text + i, e.from, n)) {
        text[to++] = e.to[0];
        i += n;
        found = true;
        break;
      }
    }
    if (!found)
      text[to++] = text[i++];
  }
  text[to] = 0;
}
bool write_string(uint8_t *p, size_t cap, size_t &at, const char *s) {
  size_t n = std::strlen(s);
  if (n > 65535 || at + 2 + n > cap)
    return false;
  put16(p + at, n);
  at += 2;
  std::memcpy(p + at, s, n);
  at += n;
  return true;
}
bool read_string(const uint8_t *p, size_t n, size_t &at, char *s, size_t cap) {
  if (at + 2 > n)
    return false;
  size_t k = get16(p + at);
  at += 2;
  if (k >= cap || at + k > n || !utf8((const char *)p + at, k))
    return false;
  std::memcpy(s, p + at, k);
  s[k] = 0;
  at += k;
  return true;
}
} // namespace
bool utf8(const char *s, size_t n, size_t *characters) {
  if (!s && n)
    return false;
  size_t at = 0, count = 0;
  while (at < n) {
    auto k = unit((const unsigned char *)s + at, n - at);
    if (!k)
      return false;
    at += k;
    ++count;
  }
  if (characters)
    *characters = count;
  return true;
}
bool valid_input(const char *s) {
  size_t chars = 0, n = std::strlen(s);
  bool content = false;
  for (size_t i = 0; i < n; ++i)
    content |= (unsigned char)s[i] > 32;
  return content && n < InputBytes && utf8(s, n, &chars) && chars <= 500;
}
bool header_value(const char *s) {
  for (; *s; ++s)
    if ((unsigned char)*s < 32 || (unsigned char)*s >= 127)
      return false;
  return true;
}
bool endpoint(const char *s, bool secret) {
  size_t n = std::strlen(s);
  const char *host = nullptr;
  if (!std::strncmp(s, "https://", 8))
    host = s + 8;
  else if (!secret && !std::strncmp(s, "http://", 7))
    host = s + 7;
  else
    return false;
  if (n < 9 || n > 256 || !host || !*host || *host == '/' || *host == '?' ||
      *host == '#')
    return false;
  for (const char *p = s; *p; ++p)
    if ((unsigned char)*p <= 32 || (unsigned char)*p >= 127 || *p == '@' ||
        *p == '#' || *p == '\\')
      return false;
  return true;
}
bool model_name(const char *s) {
  if (!*s || std::strlen(s) > 96)
    return false;
  for (; *s; ++s)
    if (!safe((unsigned char)*s) && *s != '/' && *s != ':')
      return false;
  return true;
}
bool json_quote(const char *s, char *out, size_t cap) {
  size_t at = 0;
  auto put = [&](char c) {
    if (at + 1 >= cap)
      return false;
    out[at++] = c;
    return true;
  };
  if (!put('"'))
    return false;
  const char hex[] = "0123456789abcdef";
  for (const unsigned char *p = (const unsigned char *)s; *p; ++p) {
    unsigned c = *p;
    if (c == '"' || c == '\\') {
      if (!put('\\') || !put(char(c)))
        return false;
    } else if (c < 32) {
      if (at + 6 >= cap)
        return false;
      out[at++] = '\\';
      out[at++] = 'u';
      out[at++] = '0';
      out[at++] = '0';
      out[at++] = hex[c >> 4];
      out[at++] = hex[c & 15];
    } else if (!put(char(c)))
      return false;
  }
  if (!put('"'))
    return false;
  out[at] = 0;
  return true;
}
bool translation_url(const char *s, size_t at, unsigned from, unsigned to,
                     char *out, size_t cap, size_t &next) {
  if (from >= LanguageCount || to >= LanguageCount || at >= std::strlen(s) ||
      cap < 128)
    return false;
  int prefix = std::snprintf(
      out, cap, "https://api.mymemory.translated.net/get?langpair=%s%%7C%s&q=",
      Languages[from].code, Languages[to].code);
  if (prefix <= 0 || size_t(prefix) >= cap)
    return false;
  next = segment(s, at, cap - size_t(prefix) - 1);
  return next > at && encode(s + at, next - at, out + prefix, cap - prefix);
}
bool tts_url(const char *s, size_t at, unsigned language, char *out, size_t cap,
             size_t &next) {
  if (language >= LanguageCount || at >= std::strlen(s) || cap < 160)
    return false;
  int prefix = std::snprintf(out, cap,
                             "https://translate.google.com/"
                             "translate_tts?ie=UTF-8&client=tw-ob&tl=%s&q=",
                             Languages[language].code);
  if (prefix <= 0 || size_t(prefix) >= cap)
    return false;
  next = segment(s, at, std::min<size_t>(180, cap - prefix - 1));
  return next > at && encode(s + at, next - at, out + prefix, cap - prefix);
}
bool libre_body(const char *s, const Settings &settings, char *out,
                size_t cap) {
  if (settings.source >= LanguageCount || settings.target >= LanguageCount)
    return false;
  char q[InputBytes * 6 + 2], key[1600];
  if (!json_quote(s, q, sizeof(q)) ||
      !json_quote(settings.libre_key, key, sizeof(key)))
    return false;
  int n = std::snprintf(out, cap,
                        "{\"q\":%s,\"source\":\"%s\",\"target\":\"%s\","
                        "\"format\":\"text\",\"api_key\":%s}",
                        q, Languages[settings.source].libre,
                        Languages[settings.target].libre, key);
  return n > 0 && size_t(n) < cap;
}
Parse parse_translation(const uint8_t *p, size_t n, bool libre, char *out,
                        size_t cap) {
  if (!document(p, n))
    return Parse::Invalid;
  media::json::Value value{p, n};
  if (value.get("quotaFinished").boolean())
    return Parse::Quota;
  if (!libre && value.get("responseStatus").number() != 200)
    return Parse::Invalid;
  auto result = libre ? value.get("translatedText")
                      : value.get("responseData").get("translatedText");
  bool truncated = false;
  if (!result.string(out, cap, &truncated))
    return Parse::Invalid;
  if (truncated)
    return Parse::TooLong;
  if (!libre)
    clean_markup(out);
  if (!out[0])
    return Parse::Empty;
  return Parse::Ok;
}
Parse parse_transcription(const uint8_t *p, size_t n, char *out, size_t cap) {
  if (!document(p, n))
    return Parse::Invalid;
  media::json::Value value{p, n};
  bool truncated = false;
  if (!value.get("text").string(out, cap, &truncated))
    return Parse::Invalid;
  if (truncated)
    return Parse::TooLong;
  return out[0] ? Parse::Ok : Parse::Empty;
}
uint32_t hash(const uint8_t *p, size_t n) {
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < n; ++i)
    h = (h ^ p[i]) * 16777619u;
  return h;
}
void asr_preset(Settings &s, unsigned preset) {
  if (preset > 2)
    return;
  s.asr_preset = uint8_t(preset);
  s.asr_key[0] = 0;
  if (preset == 0) {
    std::strcpy(s.asr_url,
                "https://api.groq.com/openai/v1/audio/transcriptions");
    std::strcpy(s.asr_model, "whisper-large-v3-turbo");
  } else if (preset == 1) {
    std::strcpy(s.asr_url, "https://api.openai.com/v1/audio/transcriptions");
    std::strcpy(s.asr_model, "gpt-transcribe");
  }
}
size_t save_settings(const Settings &s, uint8_t *p, size_t cap) {
  if (cap < 16 || s.source >= LanguageCount || s.target >= LanguageCount ||
      s.provider > 1 || s.asr_preset > 2)
    return 0;
  std::memcpy(p, "TRSC", 4);
  p[4] = 1;
  p[5] = s.source;
  p[6] = s.target;
  p[7] = s.provider;
  p[8] = s.history;
  p[9] = s.asr_preset;
  size_t at = 10;
  for (const char *v :
       {s.libre_url, s.libre_key, s.asr_url, s.asr_key, s.asr_model})
    if (!write_string(p, cap - 4, at, v))
      return 0;
  put32(p + at, hash(p, at));
  return at + 4;
}
bool load_settings(const uint8_t *p, size_t n, Settings &out) {
  if (n < 24 || std::memcmp(p, "TRSC", 4) || p[4] != 1 ||
      p[5] >= LanguageCount || p[6] >= LanguageCount || p[7] > 1 || p[8] > 1 ||
      p[9] > 2 || get32(p + n - 4) != hash(p, n - 4))
    return false;
  Settings s;
  s.source = p[5];
  s.target = p[6];
  s.provider = p[7];
  s.history = p[8];
  s.asr_preset = p[9];
  size_t at = 10;
  if (!read_string(p, n - 4, at, s.libre_url, sizeof(s.libre_url)) ||
      !read_string(p, n - 4, at, s.libre_key, sizeof(s.libre_key)) ||
      !read_string(p, n - 4, at, s.asr_url, sizeof(s.asr_url)) ||
      !read_string(p, n - 4, at, s.asr_key, sizeof(s.asr_key)) ||
      !read_string(p, n - 4, at, s.asr_model, sizeof(s.asr_model)) ||
      at != n - 4)
    return false;
  if ((s.libre_url[0] && !endpoint(s.libre_url, s.libre_key[0])) ||
      !endpoint(s.asr_url, s.asr_key[0]) || !header_value(s.libre_key) ||
      !header_value(s.asr_key) || !model_name(s.asr_model))
    return false;
  out = s;
  return true;
}
size_t save_record(const Record &r, uint8_t *p, size_t cap) {
  if (cap < 14 || r.source >= LanguageCount || r.target >= LanguageCount ||
      !valid_input(r.input) || !utf8(r.output, std::strlen(r.output)) ||
      !r.output[0])
    return 0;
  std::memcpy(p, "TRHR", 4);
  p[4] = 1;
  p[5] = r.source;
  p[6] = r.target;
  p[7] = 0;
  size_t at = 8;
  if (!write_string(p, cap - 4, at, r.input) ||
      !write_string(p, cap - 4, at, r.output))
    return 0;
  put32(p + at, hash(p, at));
  return at + 4;
}
bool load_record(const uint8_t *p, size_t n, Record &out) {
  if (n < 16 || std::memcmp(p, "TRHR", 4) || p[4] != 1 ||
      p[5] >= LanguageCount || p[6] >= LanguageCount || p[7] ||
      get32(p + n - 4) != hash(p, n - 4))
    return false;
  Record r;
  r.source = p[5];
  r.target = p[6];
  size_t at = 8;
  if (!read_string(p, n - 4, at, r.input, sizeof(r.input)) ||
      !read_string(p, n - 4, at, r.output, sizeof(r.output)) || at != n - 4 ||
      !valid_input(r.input) || !r.output[0])
    return false;
  out = r;
  return true;
}
} // namespace translate
