#pragma once
#include "reader_core.hpp"
#include <charconv>
namespace reader {
[[gnu::noinline]] inline void append_quoted(std::string& out, std::string_view value) {
  out += '"';
  for (unsigned char c : value) {
    if (c == '"' || c == '\\') { out += '\\'; out += char(c); }
    else if (c == '\n') out += "\\n";
    else if (c == '\r') out += "\\r";
    else if (c == '\t') out += "\\t";
    else out += c < 32 ? ' ' : char(c);
  }
  out += '"';
}
inline std::string quote(std::string_view value) { std::string out; append_quoted(out, value); return out; }
[[gnu::noinline]] inline void append_number(std::string& out, std::uint32_t value) {
  std::array<char, 16> buffer{};
  const auto end = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value).ptr;
  out.append(buffer.data(), std::size_t(end - buffer.data()));
}
struct SavedState {
  Settings settings;
  std::vector<Book> shelf;
  std::vector<Source> sources{
      {"经典直读 · 即时搜索", "https://www.gutenberg.org/cache/epub/", SourceKind::classics},
      {"GITenberg · 即时搜索", "https://raw.githubusercontent.com/GITenberg/", SourceKind::gitenberg},
      {"古腾堡中文小说",
       "https://gutendex.com/books/"
       "?languages=zh&copyright=false&mime_type=text%2Fplain",
       SourceKind::gutenberg},
      {"古腾堡世界文学",
       "https://gutendex.com/books/"
       "?copyright=false&mime_type=text%2Fplain&topic=fiction",
       SourceKind::gutenberg}};
  std::string encode() const {
    std::string out = "{\"version\":2,\"settings\":{\"font\":";
    append_number(out, settings.font); out += ",\"gap\":"; append_number(out, settings.line_gap);
    out += ",\"theme\":"; append_number(out, settings.theme);
    out += ",\"seconds\":"; append_number(out, settings.auto_seconds);
    out += "},\"sources\":[";
    bool comma = false;
    for (const auto& source : sources) {
      if (comma) out += ',';
      comma = true;
      out += "{\"name\":"; append_quoted(out, source.name);
      out += ",\"url\":"; append_quoted(out, source.url);
      out += ",\"kind\":"; append_number(out, unsigned(source.kind));
      out += ",\"enabled\":"; out += source.enabled ? "true}" : "false}";
    }
    out += "],\"shelf\":["; comma = false;
    for (const auto& book : shelf) {
      if (comma) out += ',';
      comma = true;
      out += "{\"title\":"; append_quoted(out, book.title);
      out += ",\"author\":"; append_quoted(out, book.author);
      out += ",\"url\":"; append_quoted(out, book.url);
      out += ",\"toc\":"; append_quoted(out, book.toc);
      out += ",\"validator\":"; append_quoted(out, book.validator);
      out += ",\"position\":"; append_number(out, book.position);
      out += ",\"total\":"; append_number(out, book.total);
      out += ",\"complete\":"; out += book.complete ? "true}" : "false}";
    }
    out += "]}";
    return out;
  }
  bool decode(std::string_view text) {
    Json j(text);
    auto version = j.get("version").number();
    if (text.size() > 32768 || !j.valid() || !version || (*version != 1 && *version != 2))
      return false;
    SavedState candidate;
    auto s = j.get("settings");
    auto font = s.get("font").number(), gap = s.get("gap").number(),
         theme = s.get("theme").number(), seconds = s.get("seconds").number();
    if (!font || *font < 12 || *font > 28 || !gap || *gap > 12 || !theme ||
        *theme > 3 || !seconds || *seconds < 5 || *seconds > 60)
      return false;
    candidate.settings = {std::uint8_t(*font), std::uint8_t(*gap),
                          std::uint8_t(*theme), std::uint8_t(*seconds), false};
    candidate.sources.clear();
    bool ok = true;
    bool sources_valid = j.get("sources").each(
        [&](Json row) {
          auto name = row.get("name").string(96),
               url = row.get("url").string(512);
          auto kind = row.get("kind").number();
          if (!name || !url || origin(*url).empty() || !kind || *kind > 3 ||
              candidate.sources.size() >= max_sources) {
            ok = false;
            return false;
          }
          candidate.sources.push_back({*name, *url, SourceKind(*kind),
                                       row.get("enabled").raw() != "false"});
          return true;
        },
        max_sources + 1);
    bool shelf_valid = j.get("shelf").each(
        [&](Json row) {
          Book b;
          auto title = row.get("title").string(192),
               author = row.get("author").string(128),
               url = row.get("url").string(512),
               toc = row.get("toc").string(512);
          auto pos = row.get("position").number(),
               total = row.get("total").number();
          if (!title || !author || !url || !toc ||
              (!url->starts_with("sample:") && origin(*url).empty() &&
               origin(*toc).empty()) ||
              !pos || !total || *total > max_book_bytes || *pos > *total ||
              candidate.shelf.size() >= max_shelf) {
            ok = false;
            return false;
          }
          b = {*title,
               *author,
               *url,
               *toc,
               *pos,
               *total,
               row.get("complete").raw() == "true"};
          b.validator = row.get("validator").string(128).value_or("");
          candidate.shelf.push_back(std::move(b));
          return true;
        },
        max_shelf + 1);
    if (!ok || !sources_valid || !shelf_valid || candidate.sources.empty())
      return false;
    if (*version == 1) {
      for (const auto& extra : std::array{
          Source{"经典直读 · 即时搜索", "https://www.gutenberg.org/cache/epub/", SourceKind::classics},
          Source{"GITenberg · 即时搜索", "https://raw.githubusercontent.com/GITenberg/", SourceKind::gitenberg}}) {
        if (candidate.sources.size() < max_sources &&
            std::none_of(candidate.sources.begin(), candidate.sources.end(), [&](const auto& source) { return source.kind == extra.kind; }))
          candidate.sources.push_back(extra);
      }
    }
    *this = std::move(candidate);
    return true;
  }
};
inline constexpr std::string_view sample_text =
    "第一章 "
    "山间来信\n\n清晨的风沿着山路吹来，林舟推开木窗，看见一只白鸟停在门前。白鸟"
    "的脚上系着一封信。\n"
    "他解下细绳，信纸上只有一行字：请在太阳落山之前，到河边的旧书屋来。\n"
    "村里的人都知道，那间书屋已经关闭了许多年。林舟收好信，带上水壶，沿小路走向"
    "山外。\n\n"
    "第二章 "
    "河边书屋\n\n午后的河水映着云影。旧书屋的门半掩着，门上的铜铃轻轻响了一声。"
    "\n"
    "屋里有一位老人，正在整理一排没有书名的书。他说，每本书都在等待一个愿意读完"
    "它的人。\n"
    "林舟翻开第一页，纸上慢慢浮现出他刚才走过的山路。路的尽头，还有一个他从未见"
    "过的村庄。\n\n"
    "第三章 "
    "未完的旅程\n\n老人递给他一本空白的书：把沿途遇见的故事写下来，你就能找到回"
    "家的路。\n"
    "林舟把书放进背包。夕阳落在河面上，白鸟再次飞起，引着他走向那座陌生的村庄。"
    "\n"
    "他知道，这一次的旅程才刚刚开始。\n\n本篇为书页阅读验收而创作，采用 "
    "CC0，允许免费阅读、复制与修改。\n";
} // namespace reader
