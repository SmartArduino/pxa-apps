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
  Json get(std::string_view key) const {
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

enum class SourceKind : std::uint8_t { gutenberg, json };
struct Source {
  std::string name, url;
  SourceKind kind = SourceKind::json;
  bool enabled = true;
};
struct Book {
  std::string title, author, url, toc;
  std::uint32_t position = 0, total = 0;
  bool complete = false;
};
struct Chapter {
  std::string title, url;
  std::uint32_t offset = 0;
};
struct Settings {
  std::uint8_t font = 16, line_gap = 6, theme = 0, auto_seconds = 15;
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
  explicit Layout(pxa::ui::DisplayMetrics m) : metrics(m) {
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
    // Three two-character navigation labels must fit above a curved edge.
    const int navigation_width = std::min(width - margin * 2,
                                          3 * (ui_font * 2 + 12));
    while (bottom - row > top + row + caption &&
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
};
struct PageLine {
  std::uint32_t begin = 0, end = 0;
  Rect box{};
};
struct Page {
  std::array<PageLine, 64> lines{};
  std::size_t count = 0, consumed = 0;
  bool invalid_utf8 = false;
};
inline Page paginate(std::string_view text, const Layout &layout,
                     const Settings &settings) {
  Page page;
  int size = std::max(10, int(settings.font * layout.density)),
      step =
          int(std::ceil(size * 1.6f)) + int(settings.line_gap * layout.density);
  int y = layout.top + layout.row + 4, end = layout.bottom - layout.row - 2;
  std::size_t at = 0;
  while (at < text.size() && y + step <= end &&
         page.count < page.lines.size()) {
    Rect box = layout.box(y, step);
    box.x += 2;
    box.w -= 4;
    std::size_t start = at, visible = at;
    int used = 0;
    while (at < text.size()) {
      auto r = rune(text.substr(at));
      if (r.value == 0xfffd && r.bytes == 1)
        page.invalid_utf8 = true;
      if (r.value == '\r') {
        at += r.bytes;
        continue;
      }
      if (r.value == '\n') {
        at += r.bytes;
        break;
      }
      // Reserve one em per code point, including Latin, to avoid overlap
      // without a Guest font atlas or text measurement cache.
      int advance = size;
      if (used + advance > box.w && at > start)
        break;
      if (box.w < size)
        break;
      used += advance;
      at += r.bytes;
      visible = at;
    }
    if (at == start)
      break;
    page.lines[page.count++] = {std::uint32_t(start), std::uint32_t(visible),
                                box};
    y += step;
  }
  page.consumed = at;
  return page;
}
inline bool chapter_heading(std::string_view s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t'))
    s.remove_prefix(1);
  if (s.size() > 240 || s.empty())
    return false;
  if (s.starts_with("第")) {
    auto end = s.find_first_of(" \t");
    if (end == s.npos)
      end = s.size();
    return s.substr(0, end).find("回") != s.npos ||
           s.substr(0, end).find("章") != s.npos ||
           s.substr(0, end).find("卷") != s.npos;
  }
  return s.starts_with("CHAPTER ") || s.starts_with("Chapter ") ||
         s.starts_with("BOOK ") || s.starts_with("Book ");
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
