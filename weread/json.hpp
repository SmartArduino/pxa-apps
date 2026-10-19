#pragma once
// Bounded data views and UTF-8 primitives, derived from the maintained native
// reader. This module has no UI, SDK transport, JSON tree or JS runtime.
#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
namespace weread {
struct Rune {
  std::uint32_t value;
  std::size_t bytes;
};
inline Rune rune(std::string_view s) {
  if (s.empty())
    return {0, 0};
  auto a = static_cast<unsigned char>(s[0]);
  if (a < 128)
    return {a, 1};
  unsigned n = 0, min = 0, c = 0;
  if ((a & 0xe0) == 0xc0) {
    n = 2;
    min = 128;
    c = a & 31;
  } else if ((a & 0xf0) == 0xe0) {
    n = 3;
    min = 2048;
    c = a & 15;
  } else if ((a & 0xf8) == 0xf0) {
    n = 4;
    min = 65536;
    c = a & 7;
  } else
    return {0xfffd, 1};
  if (s.size() < n)
    return {0xfffd, 1};
  for (unsigned i = 1; i < n; ++i) {
    unsigned b = static_cast<unsigned char>(s[i]);
    if ((b & 0xc0) != 0x80)
      return {0xfffd, 1};
    c = (c << 6) | (b & 63);
  }
  if (c < min || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff))
    return {0xfffd, 1};
  return {c, n};
}
// Keep only complete, valid UTF-8 sequences. A truncated final sequence can be
// retried at the next file boundary; malformed bytes inside the text are
// errors.
inline std::size_t utf8_prefix(std::string_view text, bool &invalid) {
  std::size_t at = 0;
  invalid = false;
  while (at < text.size()) {
    auto c = static_cast<unsigned char>(text[at]);
    unsigned n = c < 128              ? 1
                 : (c & 0xe0) == 0xc0 ? 2
                 : (c & 0xf0) == 0xe0 ? 3
                 : (c & 0xf8) == 0xf0 ? 4
                                      : 0;
    if (n && text.size() - at < n)
      return at;
    auto r = rune(text.substr(at));
    if (!n || (r.value == 0xfffd && r.bytes == 1)) {
      invalid = true;
      return at;
    }
    at += r.bytes;
  }
  return at;
}
inline bool valid_utf8(std::string_view text) {
  bool invalid = false;
  return utf8_prefix(text, invalid) == text.size() && !invalid;
}
struct Utf8Validator {
  std::uint32_t value = 0, minimum = 0;
  unsigned remaining = 0;
  bool ok = true;
  bool append(unsigned char c) {
    if (!ok) return false;
    if (remaining) {
      if ((c & 0xc0) != 0x80) return ok = false;
      value = (value << 6) | (c & 63);
      if (!--remaining && (value < minimum || value > 0x10ffff ||
                           (value >= 0xd800 && value <= 0xdfff)))
        return ok = false;
    } else if (c < 128) {
      return true;
    } else if ((c & 0xe0) == 0xc0) {
      remaining = 1; minimum = 128; value = c & 31;
    } else if ((c & 0xf0) == 0xe0) {
      remaining = 2; minimum = 2048; value = c & 15;
    } else if ((c & 0xf8) == 0xf0) {
      remaining = 3; minimum = 65536; value = c & 7;
    } else return ok = false;
    return true;
  }
  bool append(std::string_view text) {
    for (unsigned char c : text) if (!append(c)) return false;
    return ok;
  }
  bool complete() const { return ok && !remaining; }
};
inline void append_rune(std::string &out, std::uint32_t c) {
  if (c < 128)
    out += char(c);
  else if (c < 2048) {
    out += char(0xc0 | (c >> 6));
    out += char(0x80 | (c & 63));
  } else if (c < 65536) {
    out += char(0xe0 | (c >> 12));
    out += char(0x80 | ((c >> 6) & 63));
    out += char(0x80 | (c & 63));
  } else {
    out += char(0xf0 | (c >> 18));
    out += char(0x80 | ((c >> 12) & 63));
    out += char(0x80 | ((c >> 6) & 63));
    out += char(0x80 | (c & 63));
  }
}
inline std::string shorten(std::string_view text, std::size_t bytes) {
  std::size_t at = 0;
  while (at < text.size()) {
    auto r = rune(text.substr(at));
    if (at + r.bytes > bytes)
      break;
    at += r.bytes;
  }
  return std::string(text.substr(0, at));
}
inline std::string percent(std::string_view text) {
  constexpr char hex[] = "0123456789ABCDEF";
  std::string out;
  out.reserve(text.size() * 3);
  for (unsigned char c : text) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~')
      out += char(c);
    else {
      out += '%';
      out += hex[c >> 4];
      out += hex[c & 15];
    }
  }
  return out;
}
class Json {
public:
  Json() = default;
  explicit Json(std::string_view s) : text_(s) {}
  bool valid() const {
    std::size_t at = 0;
    return skip(text_, at, 0) && space(text_, at) == text_.size();
  }
  std::string_view raw() const { return text_; }
  // Borrow several fields in one pass. Large reader objects contain arrays;
  // repeated get() would walk those arrays for every later scalar field.
  // Like get(), the first occurrence wins. No JSON tree or key cache is kept.
  template <std::size_t N>
  std::array<Json, N> select(const std::string_view (&keys)[N]) const {
    std::array<Json, N> result{};
    std::size_t at = 0, remaining = N;
    space(text_, at);
    if (at >= text_.size() || text_[at++] != '{') return result;
    while (at < text_.size() && remaining) {
      space(text_, at);
      if (at >= text_.size() || text_[at] == '}') break;
      const auto key_begin = at;
      if (!skip_string(text_, at)) break;
      const auto key = text_.substr(key_begin + 1, at - key_begin - 2);
      std::optional<std::string> decoded;
      if (key.find('\\') != key.npos) {
        decoded = Json(text_.substr(key_begin, at - key_begin)).string();
        if (!decoded) break;
      }
      const auto name = decoded ? std::string_view(*decoded) : key;
      space(text_, at);
      if (at >= text_.size() || text_[at++] != ':') break;
      space(text_, at);
      const auto begin = at;
      if (!skip(text_, at, 0)) break;
      for (std::size_t i = 0; i < N; ++i)
        if (result[i].raw().empty() && name == keys[i]) {
          result[i] = Json(text_.substr(begin, at - begin));
          --remaining;
        }
      space(text_, at);
      if (at >= text_.size() || text_[at++] != ',') break;
    }
    return result;
  }
  // Single and multi-field lookups share one scanner. Unescaped keys borrow
  // the input; only escaped names need a temporary decoded string.
  [[gnu::noinline]] Json get(std::string_view key) const {
    const std::string_view keys[]={key};
    return select(keys)[0];
  }
  template <class F> bool each(F &&callback, std::size_t limit = 2048) const {
    std::size_t at = 0, count = 0;
    space(text_, at);
    if (at >= text_.size() || text_[at++] != '[')
      return false;
    space(text_, at);
    if (at < text_.size() && text_[at] == ']')
      return true;
    while (at < text_.size()) {
      space(text_, at);
      auto begin = at;
      if (!skip(text_, at, 0) || ++count > limit)
        return false;
      if (!callback(Json(text_.substr(begin, at - begin))))
        return true;
      space(text_, at);
      if (at >= text_.size())
        return false;
      if (text_[at++] == ']')
        return true;
      if (text_[at - 1] != ',')
        return false;
    }
    return false;
  }
  std::optional<std::string> string(std::size_t maximum = 4096) const {
    std::size_t at = 0;
    space(text_, at);
    if (at >= text_.size() || text_[at++] != '"')
      return {};
    std::string out;
    out.reserve(std::min(text_.size(), maximum));
    auto hex = [&](unsigned &c) {
      if (text_.size() - at < 4)
        return false;
      c = 0;
      for (int i = 0; i < 4; ++i) {
        char h = text_[at++];
        unsigned v = h >= '0' && h <= '9'   ? h - '0'
                     : h >= 'a' && h <= 'f' ? h - 'a' + 10
                     : h >= 'A' && h <= 'F' ? h - 'A' + 10
                                            : 99;
        if (v > 15)
          return false;
        c = (c << 4) | v;
      }
      return true;
    };
    while (at < text_.size()) {
      unsigned char c = text_[at++];
      if (c == '"')
        return valid_utf8(out) ? std::optional<std::string>(std::move(out))
                               : std::nullopt;
      if (c < 32)
        return {};
      if (c == '\\') {
        if (at >= text_.size())
          return {};
        c = text_[at++];
        if (c == 'u') {
          unsigned value;
          if (!hex(value))
            return {};
          if (value >= 0xd800 && value <= 0xdbff) {
            if (text_.substr(at, 2) != "\\u")
              return {};
            at += 2;
            unsigned low;
            if (!hex(low) || low < 0xdc00 || low > 0xdfff)
              return {};
            value = 0x10000 + ((value - 0xd800) << 10) + low - 0xdc00;
          } else if (value >= 0xdc00 && value <= 0xdfff)
            return {};
          append_rune(out, value);
        } else if (c == 'n')
          out += '\n';
        else if (c == 'r')
          out += '\r';
        else if (c == 't')
          out += '\t';
        else if (c == 'b')
          out += '\b';
        else if (c == 'f')
          out += '\f';
        else if (c == '"' || c == '\\' || c == '/')
          out += char(c);
        else
          return {};
      } else
        out += char(c);
      if (out.size() > maximum)
        return {};
    }
    return {};
  }
  std::optional<std::uint32_t> number() const {
    if (text_.empty())
      return {};
    std::uint32_t n = 0;
    auto [end, ec] =
        std::from_chars(text_.data(), text_.data() + text_.size(), n);
    if (ec != std::errc{} || end != text_.data() + text_.size())
      return {};
    return n;
  }

private:
  static std::size_t space(std::string_view s, std::size_t &at) {
    while (at < s.size() &&
           (s[at] == ' ' || s[at] == '\n' || s[at] == '\r' || s[at] == '\t'))
      ++at;
    return at;
  }
  static bool skip_string(std::string_view s, std::size_t &at) {
    if (at >= s.size() || s[at++] != '"')
      return false;
    while (at < s.size()) {
      unsigned char c = s[at++];
      if (c == '"')
        return true;
      if (c < 32)
        return false;
      if (c == '\\') {
        if (at >= s.size())
          return false;
        c = s[at++];
        if (c == 'u') {
          for (int i = 0; i < 4; ++i) {
            if (at >= s.size())
              return false;
            char h = s[at++];
            if (!((h >= '0' && h <= '9') || (h >= 'a' && h <= 'f') ||
                  (h >= 'A' && h <= 'F')))
              return false;
          }
        } else if (c != '"' && c != '\\' && c != '/' && c != 'n' && c != 'r' &&
                   c != 't' && c != 'b' && c != 'f')
          return false;
      }
    }
    return false;
  }
  static bool skip(std::string_view s, std::size_t &at, unsigned depth) {
    if (depth > 24)
      return false;
    space(s, at);
    if (at >= s.size())
      return false;
    char c = s[at];
    if (c == '"')
      return skip_string(s, at);
    if (c == '{' || c == '[') {
      char close = c == '{' ? '}' : ']';
      ++at;
      space(s, at);
      if (at < s.size() && s[at] == close) {
        ++at;
        return true;
      }
      while (at < s.size()) {
        if (c == '{') {
          if (!skip_string(s, at))
            return false;
          space(s, at);
          if (at >= s.size() || s[at++] != ':')
            return false;
        }
        if (!skip(s, at, depth + 1))
          return false;
        space(s, at);
        if (at >= s.size())
          return false;
        if (s[at] == close) {
          ++at;
          return true;
        }
        if (s[at++] != ',')
          return false;
        space(s, at);
      }
      return false;
    }
    for (auto value : {std::string_view("true"), std::string_view("false"),
                       std::string_view("null")})
      if (s.substr(at).starts_with(value)) {
        at += value.size();
        return true;
      }
    auto begin = at;
    if (s[at] == '-')
      ++at;
    if (at >= s.size())
      return false;
    if (s[at] == '0')
      ++at;
    else {
      if (s[at] < '1' || s[at] > '9')
        return false;
      while (at < s.size() && s[at] >= '0' && s[at] <= '9')
        ++at;
    }
    if (at < s.size() && s[at] == '.') {
      ++at;
      auto b = at;
      while (at < s.size() && s[at] >= '0' && s[at] <= '9')
        ++at;
      if (at == b)
        return false;
    }
    if (at < s.size() && (s[at] == 'e' || s[at] == 'E')) {
      ++at;
      if (at < s.size() && (s[at] == '+' || s[at] == '-'))
        ++at;
      auto b = at;
      while (at < s.size() && s[at] >= '0' && s[at] <= '9')
        ++at;
      if (at == b)
        return false;
    }
    return at > begin;
  }
  std::string_view text_;
};

} // namespace weread
