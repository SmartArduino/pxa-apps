#pragma once
#include "reader_core.hpp"
namespace reader {
inline std::string quote(std::string_view value) {
  std::string out = "\"";
  for (unsigned char c : value) {
    if (c == '"' || c == '\\') {
      out += '\\';
      out += char(c);
    } else if (c == '\n')
      out += "\\n";
    else if (c == '\r')
      out += "\\r";
    else if (c == '\t')
      out += "\\t";
    else if (c < 32)
      out += ' ';
    else
      out += char(c);
  }
  return out + '"';
}
struct SavedState {
  Settings settings;
  std::vector<Book> shelf;
  std::vector<Source> sources{
      {"古腾堡中文小说",
       "https://gutendex.com/books/"
       "?languages=zh&copyright=false&mime_type=text%2Fplain",
       SourceKind::gutenberg},
      {"古腾堡世界文学",
       "https://gutendex.com/books/"
       "?copyright=false&mime_type=text%2Fplain&topic=fiction",
       SourceKind::gutenberg}};
  std::string encode() const {
    auto out = std::string("{\"version\":1,\"settings\":{") +
               "\"font\":" + std::to_string(settings.font) +
               ",\"gap\":" + std::to_string(settings.line_gap) +
               ",\"theme\":" + std::to_string(settings.theme) +
               ",\"seconds\":" + std::to_string(settings.auto_seconds) +
               "},\"sources\":[";
    bool comma = false;
    for (auto &s : sources) {
      if (comma)
        out += ',';
      comma = true;
      out += "{\"name\":" + quote(s.name) + ",\"url\":" + quote(s.url) +
             ",\"kind\":" + std::to_string(unsigned(s.kind)) +
             ",\"enabled\":" + (s.enabled ? "true" : "false") + "}";
    }
    out += "],\"shelf\":[";
    comma = false;
    for (auto &b : shelf) {
      if (comma)
        out += ',';
      comma = true;
      out += "{\"title\":" + quote(b.title) + ",\"author\":" + quote(b.author) +
             ",\"url\":" + quote(b.url) + ",\"toc\":" + quote(b.toc) +
             ",\"position\":" + std::to_string(b.position) +
             ",\"total\":" + std::to_string(b.total) +
             ",\"complete\":" + (b.complete ? "true" : "false") + "}";
    }
    return out + "]}";
  }
  bool decode(std::string_view text) {
    Json j(text);
    if (text.size() > 32768 || !j.valid() || j.get("version").number() != 1)
      return false;
    SavedState candidate;
    auto s = j.get("settings");
    auto font = s.get("font").number(), gap = s.get("gap").number(),
         theme = s.get("theme").number(), seconds = s.get("seconds").number();
    if (!font || *font < 12 || *font > 28 || !gap || *gap > 12 || !theme ||
        *theme > 2 || !seconds || *seconds < 5 || *seconds > 60)
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
          if (!name || !url || origin(*url).empty() || !kind || *kind > 1 ||
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
          candidate.shelf.push_back(std::move(b));
          return true;
        },
        max_shelf + 1);
    if (!ok || !sources_valid || !shelf_valid || candidate.sources.empty())
      return false;
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
