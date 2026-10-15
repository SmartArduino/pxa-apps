#pragma once
#include "reader_core.hpp"
#include "reader_sources.hpp"
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
  // Eight escaped 4 KiB rule documents plus sixteen bounded book records.
  // This is an input limit, never a preallocated buffer or per-frame reserve.
  static constexpr std::size_t max_state_bytes = 160 * 1024;
  Settings settings;
  std::uint32_t source_revision=current_source_revision;
  std::vector<Book> shelf;
  std::vector<std::string> searches;
  void remember_search(std::string_view key) {
    key=web::trim(key);
    if(key.empty() || key.size()>96)return;
    std::erase(searches,key);
    searches.insert(searches.begin(),std::string(key));
    if(searches.size()>6)searches.pop_back();
  }
  std::vector<Source> sources=default_sources();
  std::string encode() const {
    std::string out = "{\"version\":4,\"settings\":{\"font\":";
    append_number(out, settings.font); out += ",\"gap\":"; append_number(out, settings.line_gap);
    out += ",\"theme\":"; append_number(out, settings.theme);
    out += ",\"seconds\":"; append_number(out, settings.auto_seconds);
    out += ",\"offline\":";out += settings.offline ? "true" : "false";
    out += "},\"sourceRevision\":";append_number(out,source_revision);out+=",\"searches\":[";
    bool history_comma=false;
    for(const auto& key:searches){if(history_comma)out+=',';history_comma=true;append_quoted(out,key);}
    out += "],\"sources\":[";
    bool comma = false;
    for (const auto& source : sources) {
      if (comma) out += ',';
      comma = true;
      out += "{\"name\":"; append_quoted(out, source.name);
      out += ",\"url\":"; append_quoted(out, source.url);
      out += ",\"kind\":"; append_number(out, unsigned(source.kind));
      out += ",\"enabled\":"; out += source.enabled ? "true" : "false";
      if(!source.rules.empty()){out+=",\"rules\":";append_quoted(out,source.rules);}
      out+='}';
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
      out += ",\"complete\":"; out += book.complete ? "true" : "false";
      if(!book.source.empty()){out+=",\"source\":";append_quoted(out,book.source);}
      out+=",\"chapter\":";append_number(out,book.chapter);out+='}';
    }
    out += "]}";
    return out;
  }
  bool decode(std::string_view text) {
    Json j(text);
    auto version = j.get("version").number();
    if (text.size() > max_state_bytes || !j.valid() || !version || *version<1 || *version>4)
      return false;
    SavedState candidate{.settings={},.shelf={},.searches={},.sources={}};
    const auto previous_revision=*version<4?0:j.get("sourceRevision").number().value_or(1);
    candidate.source_revision=std::max(previous_revision,current_source_revision);
    auto s = j.get("settings");
    auto font = s.get("font").number(), gap = s.get("gap").number(),
         theme = s.get("theme").number(), seconds = s.get("seconds").number();
    if (!font || *font < 12 || *font > 28 || !gap || *gap > 12 || !theme ||
        *theme > 3 || !seconds || *seconds < 5 || *seconds > 60)
      return false;
    candidate.settings = {std::uint8_t(*font), std::uint8_t(*gap),
                          std::uint8_t(*theme), std::uint8_t(*seconds), false};
    candidate.settings.offline=s.get("offline").raw()=="true";
    candidate.sources.clear();
    bool ok = true;
    bool sources_valid = j.get("sources").each(
        [&](Json row) {
          auto name = row.get("name").string(96),
               url = row.get("url").string(512);
          auto kind = row.get("kind").number();
          if (!name || !url || origin(*url).empty() || !kind || *kind > 4 ||
              candidate.sources.size() >= max_sources) {
            ok = false;
            return false;
          }
          candidate.sources.push_back({*name, *url, SourceKind(*kind),
                                       row.get("enabled").raw() != "false"});
          if(*kind==4) {
            auto rules=row.get("rules").string(web::max_rules_bytes);
            auto& source=candidate.sources.back();
            if(rules)source.rules=std::move(*rules);
            if(!web::rules(source_rules(source))){ok=false;return false;}
            if(const auto* builtin=builtin_source(source.url);builtin && source.rules==builtin->rules)std::string{}.swap(source.rules);
          }
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
          b.source=row.get("source").string(512).value_or("");
          b.chapter=row.get("chapter").number().value_or(0);
          if((!b.source.empty() && origin(b.source).empty()) || b.chapter>=max_chapters){ok=false;return false;}
          candidate.shelf.push_back(std::move(b));
          return true;
        },
        max_shelf + 1);
    if (!ok || !sources_valid || !shelf_valid)
      return false;
    if(!j.get("searches").raw().empty()) {
      if(!j.get("searches").each([&](Json value){auto key=value.string(96);if(!key || key->empty() || candidate.searches.size()==6){ok=false;return false;}candidate.searches.push_back(std::move(*key));return true;},7) || !ok)return false;
    }
    const bool replace_legacy=*version<4 && std::erase_if(candidate.sources,old_default_source);
    const bool uses_builtin=std::any_of(candidate.sources.begin(),candidate.sources.end(),
      [](const auto& source){return source.kind==SourceKind::html && builtin_source(source.url);});
    if(replace_legacy || (uses_builtin && previous_revision<current_source_revision)) {
      for(const auto& extra:builtin_sources)
        if((replace_legacy || extra.introduced>previous_revision) && candidate.sources.size()<max_sources &&
           std::none_of(candidate.sources.begin(),candidate.sources.end(),[&](const auto& source){return source.url==extra.url;}))
          candidate.sources.push_back(make_source(extra));
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
