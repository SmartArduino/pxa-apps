#pragma once
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <optional>
#include <pxa/ui_display.hpp>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace reader {
// Share integer formatting instead of linking the general printf formatter or
// expanding the same conversion in every progress label and URI.
[[gnu::noinline]] inline std::string number_string(std::int64_t value) {
  std::array<char, 24> bytes{};
  auto end = std::to_chars(bytes.data(), bytes.data() + bytes.size(), value).ptr;
  return {bytes.data(), std::size_t(end - bytes.data())};
}

inline constexpr std::size_t max_books = 32, max_shelf = 16, max_sources = 8;
inline constexpr std::size_t max_chapters = 1024, chunk_bytes = 32768,
                             max_book_bytes = 8 * 1024 * 1024;
inline constexpr std::size_t max_json_bytes = 96 * 1024;

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
  bool append(std::string_view text) {
    for (unsigned char c : text) {
      if (remaining) {
        if ((c & 0xc0) != 0x80)
          return ok = false;
        value = (value << 6) | (c & 63);
        if (!--remaining && (value < minimum || value > 0x10ffff ||
                             (value >= 0xd800 && value <= 0xdfff)))
          return ok = false;
      } else if (c < 128)
        continue;
      else if ((c & 0xe0) == 0xc0) {
        remaining = 1;
        minimum = 128;
        value = c & 31;
      } else if ((c & 0xf0) == 0xe0) {
        remaining = 2;
        minimum = 2048;
        value = c & 15;
      } else if ((c & 0xf8) == 0xf0) {
        remaining = 3;
        minimum = 65536;
        value = c & 7;
      } else
        return ok = false;
    }
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
inline std::string origin(std::string_view url) {
  if (!url.starts_with("https://") && !url.starts_with("http://"))
    return {};
  auto start = url.find("://") + 3, end = url.find_first_of("/?#", start);
  if (end == url.npos)
    end = url.size();
  auto host = url.substr(start, end - start);
  if (host.empty() || host.find('@') != host.npos)
    return {};
  for (unsigned char c : url)
    if (c <= 32 || c == 127 || c == '\\')
      return {};
  return std::string(url.substr(0, end));
}
inline std::string resolve(std::string_view base, std::string_view path) {
  if (!origin(path).empty())
    return std::string(path);
  auto root = origin(base);
  if (root.empty() || path.empty() || path.starts_with("//"))
    return {};
  if (path.front() == '/')
    return root + std::string(path);
  auto end = base.find_last_of('/');
  if (end < root.size())
    return root + "/" + std::string(path);
  return std::string(base.substr(0, end + 1)) + std::string(path);
}
inline std::uint64_t hash(std::string_view text) {
  std::uint64_t h = 14695981039346656037ull;
  for (unsigned char c : text) {
    h ^= c;
    h *= 1099511628211ull;
  }
  return h;
}
inline std::string cache_key(std::string_view text) {
  char b[17];
  auto [end, ec] = std::to_chars(b, b + 16, hash(text), 16);
  (void)ec;
  return {b, end};
}

// Bounded-depth JSON views, no token tree or unbounded recursive allocations.
class Json {
public:
  Json() = default;
  explicit Json(std::string_view s) : text_(s) {}
  bool valid() const {
    std::size_t at = 0;
    return skip(text_, at, 0) && space(text_, at) == text_.size();
  }
  std::string_view raw() const { return text_; }
  [[gnu::noinline]] Json get(std::string_view key) const {
    std::size_t at = 0;
    space(text_, at);
    if (at >= text_.size() || text_[at++] != '{')
      return {};
    while (at < text_.size()) {
      space(text_, at);
      if (at >= text_.size() || text_[at] == '}')
        return {};
      std::size_t begin = at;
      if (!skip_string(text_, at))
        return {};
      auto name = Json(text_.substr(begin, at - begin)).string();
      space(text_, at);
      if (at >= text_.size() || text_[at++] != ':')
        return {};
      space(text_, at);
      begin = at;
      if (!skip(text_, at, 0))
        return {};
      if (name && *name == key)
        return Json(text_.substr(begin, at - begin));
      space(text_, at);
      if (at >= text_.size() || text_[at++] != ',')
        return {};
    }
    return {};
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

enum class SourceKind : std::uint8_t { gutenberg, json, classics, gitenberg };
struct Source {
  std::string name, url;
  SourceKind kind = SourceKind::json;
  bool enabled = true;
};
struct Book {
  std::string title, author, url, toc;
  std::uint32_t position = 0, total = 0;
  bool complete = false;
  std::string validator{}; // strong ETag or Last-Modified for safe range resume
};
struct Chapter {
  std::string title, url;
  std::uint32_t offset = 0;
};
struct Settings {
  std::uint8_t font = 16, line_gap = 6, theme = 3, auto_seconds = 15;
  bool automatic = false;
};
struct Rect {
  int x, y, w, h;
  bool contains(int px, int py) const {
    return px >= x && py >= y && px < x + w && py < y + h;
  }
};
struct Layout {
  pxa::ui::DisplayMetrics metrics;
  float density = 1;
  int width = 296, height = 240, top = 8, bottom = 232, margin = 10, row = 32,
      ui_font = 14, caption = 24;
  [[gnu::noinline]] explicit Layout(pxa::ui::DisplayMetrics m, bool reserve_navigation = true) : metrics(m) {
    density = float(m.density_q16) / 65536.f;
    width = int(m.width);
    height = int(m.height);
    // Small panels reduce the dp layout to preserve usable text/control rows.
    density = std::min(density, std::min(width / 180.f, height / 176.f));
    margin = std::max(6, int(10 * density));
    row = std::max(22, int(32 * density));
    ui_font = std::max(10, int(14 * density));
    caption = height < 176 ? 0 : std::max(16, int(22 * density));
    top = std::max(int(m.safe.top) + 4, int(8 * density));
    bottom = height - std::max(int(m.safe.bottom) + 4, int(8 * density));
    row = std::max(16, std::min(row, (bottom - top - caption - 5) / 6));
    ui_font = std::max(8, std::min(ui_font, int((row - 2) / 1.6f)));
    if (!reserve_navigation) {
      // The book name is a quiet caption, not a menu/control row. Match the
      // Host's existing caption role, including density and system font scale.
      ui_font = std::clamp(int(12.f * float(m.density_q16) / 65536.f *
                               float(m.font_scale_q16) / 65536.f + .5f), 1, 256);
      row = (ui_font * 8 + 4) / 5;
      top = std::max(int(m.safe.top) + 2, int(6 * density));
      bottom = height - std::max(int(m.safe.bottom), int(4 * density));
    }
    // Three two-character navigation labels must fit above a curved edge.
    const int navigation_width = std::min(width - margin * 2,
                                          3 * (ui_font * 2 + 12));
    while (reserve_navigation && bottom - row > top + row + caption &&
           box(bottom - row, row - 2).w < navigation_width)
      bottom -= std::max(1, int(4 * density));
  }
  std::pair<int, int> span(int y, int h) const {
    int left = std::max(int(metrics.safe.left), margin),
        right = width - std::max(int(metrics.safe.right), margin);
    auto corner = [](int r, int d) {
      if (r <= 0 || d >= r)
        return 0;
      d = std::max(0, d);
      return int(std::ceil(
          r -
          std::sqrt(std::max(0., double(r) * r - double(r - d) * (r - d)))));
    };
    left = std::max({left, corner(metrics.corners[0], y),
                     corner(metrics.corners[3], height - y - h)});
    right = std::min({right, width - corner(metrics.corners[1], y),
                      width - corner(metrics.corners[2], height - y - h)});
    if (metrics.shape == 2) {
      double cx = width / 2., cy = height / 2., rx = width / 2.,
             ry = height / 2.;
      double d = std::max(std::abs(y - cy), std::abs(y + h - cy)) / ry;
      int inset = int(std::ceil(cx - rx * std::sqrt(std::max(0., 1 - d * d))));
      left = std::max(left, inset + 2);
      right = std::min(right, width - inset - 2);
    }
    return {left, std::max(left, right)};
  }
  Rect box(int y, int h) const {
    auto [l, r] = span(y, h);
    return {l, y, r - l, h};
  }
  [[gnu::noinline]] Rect page_number_box() const {
    const int pixels = std::max(8, int(10 * density));
    const int h = (pixels * 8 + 4) / 5;
    int y = bottom - h;
    // A short centered label needs much less room than the removed buttons.
    while (y > top + row + 2 * h && box(y,h).w < std::min(width-2*margin,14*pixels))
      y -= std::max(1,int(2*density));
    return box(y,h);
  }
  int body_end() const { return page_number_box().y - std::max(2,int(3*density)); }
  int body_start() const { return top + row + std::max(2,int(3*density)); }
};
// Conservative single-line geometry, shared by labels and body pagination.
// No Guest font atlas or heap-backed measurement cache is required.
inline int text_line_height(int pixels) { return (pixels * 8 + 4) / 5; }
inline int text_width(std::string_view value, int pixels) {
  int units = 0;
  for (std::size_t at = 0; at < value.size();) {
    auto r = rune(value.substr(at));
    units += r.value == ' ' ? 1 : 2;
    at += r.bytes;
  }
  return (units * pixels + 1) / 2;
}
inline int fit_text_size(std::string_view value, int preferred, Rect box) {
  int pixels = std::min(preferred, box.h * 5 / 8);
  while (pixels > 8 && text_width(value, pixels) > box.w) --pixels;
  return std::max(8, pixels);
}
// Long data labels are visibly shortened; controls keep their complete labels.
// The caller owns the fixed scratch span and consumes the result synchronously.
inline std::string_view ellipsize(std::string_view value, int pixels, int width,
                                 std::span<char> scratch) {
  if (text_width(value, pixels) <= width) return value;
  std::size_t at = 0; int used = 0;
  while (at < value.size()) {
    auto r = rune(value.substr(at));
    int advance = r.value == ' ' ? (pixels + 1) / 2 : pixels;
    if (used + advance + pixels > width || at + r.bytes + 3 > scratch.size()) break;
    used += advance; at += r.bytes;
  }
  if (width < pixels || scratch.size() < 3) return {};
  std::copy_n(value.data(), at, scratch.data());
  constexpr std::string_view dots = "…";
  std::copy(dots.begin(), dots.end(), scratch.data() + at);
  return {scratch.data(), at + dots.size()};
}
struct PageLine {
  std::uint32_t begin = 0, end = 0;
  Rect box{};
};
struct Page {
  std::array<PageLine, 64> lines{};
  std::size_t count = 0, consumed = 0;
  bool invalid_utf8 = false;
};
// Chapter headings have a numeral followed by a unit; normal prose beginning
// with 第一 must not create phantom directory entries.
[[gnu::noinline]] inline bool chapter_heading(std::string_view s) {
  while (!s.empty()) {
    auto c = rune(s);
    if (c.value != ' ' && c.value != '\t' && c.value != 0x3000 && c.value != 0xfeff) break;
    s.remove_prefix(c.bytes);
  }
  if (s.empty() || s.size() > 240) return false;
  if (!s.starts_with("第"))
    return s.starts_with("CHAPTER ") || s.starts_with("Chapter ") ||
           s.starts_with("BOOK ") || s.starts_with("Book ");
  s.remove_prefix(3);
  bool digit = false;
  while (!s.empty()) {
    auto c = rune(s);
    if (c.value == 0x56de || c.value == 0x7ae0 || c.value == 0x5377 || c.value == 0x8282 || c.value == 0x7bc0)
      return digit;
    if (!(c.value >= '0' && c.value <= '9') && !(c.value >= 0xff10 && c.value <= 0xff19) &&
        std::string_view("零〇○一二三四五六七八九十百千万萬两兩壹贰貳叁參肆伍陆陸柒捌玖拾佰仟").find(s.substr(0,c.bytes)) == std::string_view::npos)
      return false;
    digit = true; s.remove_prefix(c.bytes);
  }
  return false;
}
// Bounded scanner shared by cold indexing and streamed downloads. A heading is
// emitted only after its newline, never from an incomplete range/UTF-8 suffix.
struct ChapterScanner {
  std::array<char,240> line{};
  std::uint32_t offset = 0, line_start = 0;
  std::uint16_t used = 0;
  bool overflow = false;
private:
  using EmitFn = void(*)(void*,std::string_view,std::uint32_t);
  [[gnu::noinline]] void finish_impl(void* context, EmitFn emit) {
    auto count = used;
    while (count && line[count-1] == '\r') --count;
    auto text = std::string_view(line.data(), count);
    if (!overflow && valid_utf8(text) && chapter_heading(text)) emit(context,text,line_start);
    used = 0; overflow = false;
  }
  [[gnu::noinline]] void append_impl(std::string_view text,void* context, EmitFn emit) {
    for (char c : text) {
      if (c == '\n') { finish_impl(context,emit); line_start = offset + 1; }
      else if (used < line.size()) line[used++] = c;
      else overflow = true;
      ++offset;
    }
  }
public:
  template<class Emit> void finish(Emit&& emit) {
    finish_impl(&emit,[](void* context,std::string_view text,std::uint32_t offset) {
      (*static_cast<std::remove_reference_t<Emit>*>(context))(text,offset);
    });
  }
  template<class Emit> void append(std::string_view text, Emit&& emit) {
    append_impl(text,&emit,[](void* context,std::string_view line,std::uint32_t offset) {
      (*static_cast<std::remove_reference_t<Emit>*>(context))(line,offset);
    });
  }
};
inline bool opening_punctuation(std::uint32_t c) {
  return c == '(' || c == '[' || c == '{' || c == 0x2018 || c == 0x201c || c == 0x3008 || c == 0x300a || c == 0x300c || c == 0x300e || c == 0x3010 || c == 0xff08;
}
inline bool closing_punctuation(std::uint32_t c) {
  return c == ',' || c == '.' || c == '!' || c == '?' || c == ':' || c == ';' || c == ')' || c == ']' || c == '}' ||
    c == 0x2019 || c == 0x201d || c == 0x3001 || c == 0x3002 || c == 0x3009 || c == 0x300b || c == 0x300d || c == 0x300f || c == 0x3011 ||
    c == 0xff01 || c == 0xff09 || c == 0xff0c || c == 0xff0e || c == 0xff1a || c == 0xff1b || c == 0xff1f || c == 0xfe54;
}
inline bool hardwrapped_source(std::string_view url) {
  return url.starts_with("https://www.gutenberg.org/") || url.starts_with("https://raw.githubusercontent.com/GITenberg/");
}
struct PaginationCursor {
  int y = 0;
  std::uint16_t lines = 0;
  std::uint16_t command_bytes = 0;
  std::uint8_t indent = 0;
  bool paragraph_start = true, indented_source = false, command_full = false;
  bool full(const Layout& layout,const Settings& settings) const {
    return command_full || lines == 64 || y + text_line_height(std::max(10,int(settings.font*layout.density))) > layout.body_end();
  }
};
// The caption is native UI. Reserve the Canvas clear and the longest footer
// before encoding body lines; growing the usable area must not grow its buffer.
inline constexpr std::size_t page_command_capacity = 3600;
inline constexpr std::size_t page_fixed_command_bytes = 32 + 28 + 96;
static_assert(sizeof(PaginationCursor) == 12);
// Rendering and background counting share this walker. A non-final window
// commits whole lines only; the unread suffix is read again, with no extra
// text buffer or page-offset table proportional to the book's length.
[[gnu::noinline]] inline Page paginate_window(std::string_view text, const Layout &layout,
                     const Settings &settings, PaginationCursor& cursor,
                     bool reflow = false, bool complete = true) {
  Page page;
  const int size = std::max(10, int(settings.font * layout.density));
  const int height = text_line_height(size), gap = int(settings.line_gap * layout.density), step = height + gap;
  if (!cursor.y) cursor.y = layout.body_start();
  int& y = cursor.y;
  const int end = layout.body_end();
  auto& paragraph_start = cursor.paragraph_start;
  auto& indented_source = cursor.indented_source;
  std::size_t at = 0;
  while (at < text.size() && y + height <= end && cursor.lines < page.lines.size() && !cursor.command_full) {
    const auto line_start = at;
    const auto old_indent = cursor.indent;
    const bool old_indented_source = indented_source;
    // Collapse blank rows and normalize leading indent without changing file
    // offsets. A paragraph gets two em; wrapped continuations get none.
    unsigned indent = cursor.indent;
    if (paragraph_start) {
      while (at < text.size()) {
        auto c = rune(text.substr(at));
        if (c.value == '\r' || c.value == '\n') { at += c.bytes; continue; }
        if (c.value == ' ' || c.value == '\t' || c.value == 0x3000 || c.value == 0xfeff) {
          indent += c.value == 0x3000 ? 2 : c.value == '\t' ? 4 : c.value == 0xfeff ? 0 : 1;
          at += c.bytes; continue;
        }
        break;
      }
      indented_source = indent != 0;
    }
    cursor.indent = std::uint8_t(std::min(8u,indent));
    if (at == text.size()) break;
    if (!complete && text.size()-at < 256) break;
    auto source_end = text.find_first_of("\r\n",at);
    if (source_end == text.npos) source_end = text.size();
    const bool heading = paragraph_start && chapter_heading(text.substr(at,source_end-at));
    Rect box = layout.box(y,height);
    box.x += 2; box.w -= 4;
    if (paragraph_start && !heading && box.w >= size * 6) {
      // Four-em poetry indentation remains distinct from ordinary prose.
      const int padding = size * (indent >= 8 ? 4 : 2);
      box.x += padding; box.w -= padding;
    }
    if (box.w < size) break;
    const std::size_t start = at;
    std::size_t visible = at, last_legal = at;
    int used = 0;
    bool newline = false;
    bool incomplete_line = false;
    while (at < text.size()) {
      if (!complete && text.size()-at < 256) { incomplete_line = true; break; }
      auto r = rune(text.substr(at));
      if (r.value == 0xfffd && r.bytes == 1) page.invalid_utf8 = true;
      if (r.value == '\r' || r.value == '\n') {
        auto after = at + r.bytes;
        if (r.value == '\r' && after < text.size() && text[after] == '\n') ++after;
        const auto next = rune(text.substr(after));
        auto e = text.find_first_of("\r\n",after); if (e == text.npos) e = text.size();
        // Gutenberg's plain text hardwrap is not a paragraph. Keep blank lines,
        // poetry indentation and chapter boundaries as explicit breaks.
        if (reflow && !heading && !indented_source && next.bytes && next.value >= 128 && next.value != 0x3000 && next.value != 0xfeff &&
            !chapter_heading(text.substr(after,e-after))) {
          at = after; continue;
        }
        at = after; newline = true; break;
      }
      const int advance = r.value == ' ' ? (size + 1) / 2 : size;
      auto next_at = at+r.bytes;
      auto next = rune(text.substr(next_at));
      if (reflow && !heading && !indented_source && (next.value == '\r' || next.value == '\n')) {
        next_at += next.bytes;
        if (next.value == '\r' && next_at < text.size() && text[next_at] == '\n') ++next_at;
        auto following = rune(text.substr(next_at));
        if (following.value >= 128 && following.value != 0x3000 && following.value != 0xfeff) next = following;
      }
      if (used + advance > box.w) {
        if (last_legal > start) at = visible = last_legal;
        break;
      }
      used += advance; at += r.bytes; visible = at;
      // Keep the last legal boundary, including closing-mark clusters. Rolling
      // back never overhangs the rounded-screen span or consumes hidden text.
      if (!opening_punctuation(r.value) && !closing_punctuation(next.value)) last_legal = at;
    }
    if (incomplete_line) { at = start; break; }
    if (at == start) break;
    // Count original bytes conservatively: compaction can remove hardwraps.
    const auto bytes = 28 + visible - start;
    if (cursor.command_bytes + bytes > page_command_capacity - page_fixed_command_bytes) {
      at = line_start; cursor.indent = old_indent;
      indented_source = old_indented_source; cursor.command_full = true;
      break;
    }
    cursor.command_bytes += std::uint16_t(bytes);
    page.lines[page.count++] = {std::uint32_t(start),std::uint32_t(visible),box};
    ++cursor.lines;
    cursor.indent = 0;
    y += step;
    paragraph_start = newline;
    if (newline && at < text.size() && (text[at] == '\r' || text[at] == '\n')) y += gap;
  }
  page.consumed = at;
  return page;
}
[[gnu::noinline]] inline Page paginate(std::string_view text,const Layout& layout,
                     const Settings& settings,bool paragraph_start=true,bool reflow=false) {
  PaginationCursor cursor; cursor.paragraph_start=paragraph_start;
  return paginate_window(text,layout,settings,cursor,reflow);
}
// Compact only the displayed lines in the existing page buffer. The paginator's
// consumed count is an ORIGINAL file offset, including removed CRLF hardwraps.
inline void compact_page_text(std::string& text, Page& page) {
  std::size_t write = 0;
  for (std::size_t i=0; i<page.count; ++i) {
    auto& line = page.lines[i]; const auto begin = line.begin, end = line.end;
    line.begin = std::uint32_t(write);
    for (auto at=begin; at<end; ++at)
      if (text[at] != '\r' && text[at] != '\n') text[write++] = text[at];
    line.end = std::uint32_t(write);
  }
  text.resize(write);
}
struct PreferencePreview {
  Rect first, second;
  int pixels = 0, step = 0;
};
[[gnu::noinline]] inline PreferencePreview preference_preview(const Layout& l, const Settings& s) {
  const int size = std::max(10,int(s.font*l.density));
  const int height = text_line_height(size), step = height + int(s.line_gap*l.density);
  const int bottom = l.bottom-l.row-4;
  const int y = bottom-height-step;
  return {l.box(y,height),l.box(y+step,height),size,step};
}
inline std::vector<Book> parse_books(std::string_view raw,
                                     const Source &source) {
  Json root(raw);
  std::vector<Book> out;
  if (!root.valid())
    return out;
  out.reserve(max_books);
  auto list =
      root.get(source.kind == SourceKind::gutenberg ? "results" : "books");
  list.each([&](Json j) {
    Book b;
    auto title = j.get("title").string(192);
    if (!title)
      return true;
    b.title = *title;
    if (source.kind == SourceKind::gutenberg) {
      if (j.get("copyright").raw() != "false")
        return true;
      auto url = j.get("formats").get("text/plain; charset=utf-8").string(512);
      if (!url)
        return true;
      b.url = *url;
      j.get("authors").each([&](Json a) {
        auto author = a.get("name").string(128);
        if (author)
          b.author = *author;
        return false;
      });
    } else {
      auto url = j.get("url").string(512), toc = j.get("toc").string(512),
           author = j.get("author").string(128);
      if (url)
        b.url = resolve(source.url, *url);
      if (toc)
        b.toc = resolve(source.url, *toc);
      if (author)
        b.author = *author;
    }
    if (origin(b.url).empty() && origin(b.toc).empty())
      return true;
    out.push_back(std::move(b));
    return out.size() < max_books;
  });
  return out;
}
inline std::vector<Chapter> parse_toc(std::string_view raw,
                                      std::string_view base) {
  std::vector<Chapter> out;
  Json j(raw);
  if (!j.valid())
    return out;
  bool valid = true;
  auto array_ok = j.get("chapters")
                      .each(
                          [&](Json value) {
                            auto title = value.get("title").string(192),
                                 url = value.get("url").string(512);
                            auto resolved =
                                url ? resolve(base, *url) : std::string{};
                            if (!title || title->empty() || resolved.empty() ||
                                out.size() >= max_chapters) {
                              valid = false;
                              return false;
                            }
                            out.push_back({*title, resolved, 0});
                            return true;
                          },
                          max_chapters + 1);
  if (!valid || !array_ok)
    out.clear();
  return out;
}
struct ContentRange {
  std::uint32_t start, end, total;
};
inline std::optional<ContentRange> content_range(std::string_view value) {
  if (!value.starts_with("bytes "))
    return {};
  value.remove_prefix(6);
  ContentRange r{};
  auto take = [&](std::uint32_t &n, char delimiter) {
    auto [at, ec] =
        std::from_chars(value.data(), value.data() + value.size(), n);
    if (ec != std::errc{} || at == value.data() + value.size() ||
        *at != delimiter)
      return false;
    value.remove_prefix(at - value.data() + 1);
    return true;
  };
  if (!take(r.start, '-') || !take(r.end, '/'))
    return {};
  auto [at, ec] =
      std::from_chars(value.data(), value.data() + value.size(), r.total);
  if (ec != std::errc{} || at != value.data() + value.size() ||
      r.start > r.end || r.end >= r.total || r.total > max_book_bytes)
    return {};
  return r;
}
} // namespace reader
