#pragma once
#include "reader_core.hpp"

namespace reader::web {
inline constexpr std::size_t max_rules_bytes = 4096;
inline bool space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f'; }
inline char lower(char c) { return c >= 'A' && c <= 'Z' ? char(c + 32) : c; }
inline bool equal(std::string_view a, std::string_view b) {
  return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(),
                                            [](char x, char y) { return lower(x) == lower(y); });
}
inline std::string_view trim(std::string_view s) {
  while(!s.empty()) {
    if(space(s.front()))s.remove_prefix(1);
    else if(s.starts_with("\xc2\xa0"))s.remove_prefix(2);
    else if(s.starts_with("　"))s.remove_prefix(3);
    else break;
  }
  while(!s.empty()) {
    if(space(s.back()))s.remove_suffix(1);
    else if(s.ends_with("\xc2\xa0"))s.remove_suffix(2);
    else if(s.ends_with("　"))s.remove_suffix(3);
    else break;
  }
  return s;
}
inline bool name_char(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' ||
         c == '_' || c == ':';
}
// Decode text/attributes only, not markup; unknown entities remain visible.
inline std::string entities(std::string_view s) {
  std::string out;
  for (std::size_t at = 0; at < s.size();) {
    if (s[at] != '&') {
      out += s[at++];
      continue;
    }
    const auto end = s.find(';', at + 1);
    if (end == s.npos || end - at > 16) {
      out += s[at++];
      continue;
    }
    auto key = s.substr(at + 1, end - at - 1);
    std::uint32_t cp = 0;
    if (key == "amp")
      cp = '&';
    else if (key == "lt")
      cp = '<';
    else if (key == "gt")
      cp = '>';
    else if (key == "quot")
      cp = '"';
    else if (key == "apos")
      cp = '\'';
    else if (key == "nbsp")
      cp = ' ';
    else if (key == "hellip")
      cp = 0x2026;
    else if (key == "mdash")
      cp = 0x2014;
    else if (key == "ndash")
      cp = 0x2013;
    else if (key.starts_with('#')) {
      key.remove_prefix(1);
      int base = 10;
      if (key.starts_with('x') || key.starts_with('X')) {
        key.remove_prefix(1);
        base = 16;
      }
      auto [last, ec] = std::from_chars(key.data(), key.data() + key.size(), cp, base);
      if (key.empty() || ec != std::errc{} || last != key.data() + key.size() || !cp ||
          cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff))
        cp = 0xfffd;
    }
    if (!cp) {
      out += s[at++];
      continue;
    }
    append_rune(out, cp);
    at = end + 1;
  }
  return out;
}
// Website links support dot segments, protocol-relative URLs and queries.
// Non-HTTP schemes, credentials and control characters are rejected.
inline std::string url(std::string_view base, std::string_view ref) {
  return reader::resolve(base, ref);
}
struct Tag {
  std::string_view name, attrs;
  std::size_t begin = 0, end = 0;
  bool closing = false, single = false;
  std::optional<std::string_view> find_attribute(std::string_view key) const {
    std::size_t at = 0;
    while (at < attrs.size()) {
      while (at < attrs.size() && (space(attrs[at]) || attrs[at] == '/'))
        ++at;
      auto begin = at;
      while (at < attrs.size() && name_char(attrs[at]))
        ++at;
      if (begin == at) {
        ++at;
        continue;
      }
      auto name = attrs.substr(begin, at - begin);
      while (at < attrs.size() && space(attrs[at]))
        ++at;
      std::string_view value;
      if (at < attrs.size() && attrs[at] == '=') {
        ++at;
        while (at < attrs.size() && space(attrs[at]))
          ++at;
        if (at < attrs.size() && (attrs[at] == '\'' || attrs[at] == '"')) {
          const char quote = attrs[at++];
          begin = at;
          while (at < attrs.size() && attrs[at] != quote)
            ++at;
          value = attrs.substr(begin, at - begin);
          if (at < attrs.size())
            ++at;
        } else {
          begin = at;
          while (at < attrs.size() && !space(attrs[at]) && attrs[at] != '>')
            ++at;
          value = attrs.substr(begin, at - begin);
        }
      }
      if (equal(name, key))
        return value;
    }
    return {};
  }
  std::string_view attribute(std::string_view key) const {return find_attribute(key).value_or(std::string_view{});}
};
inline bool next_tag(std::string_view html, std::size_t &at, Tag &tag) {
  while (at < html.size()) {
    const auto next = html.find('<', at);
    if (next == html.npos) {
      at = html.size();
      return false;
    }
    at = next;
    if (html.substr(at).starts_with("<!--")) {
      const auto begin = at;
      auto end = html.find("-->", at + 4);
      at = end == html.npos ? html.size() : end + 3;
      tag = {"!comment", {}, begin, at, false, true};
      return true;
    }
    if (at + 1 < html.size() && (html[at + 1] == '!' || html[at + 1] == '?')) {
      const auto begin = at;
      auto end = html.find('>', at + 2);
      at = end == html.npos ? html.size() : end + 1;
      tag = {"!comment", {}, begin, at, false, true};
      return true;
    }
    const auto begin = at++;
    bool closing = at < html.size() && html[at] == '/';
    if (closing)
      ++at;
    const auto name = at;
    while (at < html.size() && name_char(html[at]))
      ++at;
    if (name == at)
      continue;
    const auto attrs = at;
    char quote = 0, previous = 0;
    while (at < html.size()) {
      char c = html[at];
      if (quote) {
        if (c == quote)
          quote = 0;
      } else if ((c == '\'' || c == '"') && previous == '=')
        quote = c;
      else if (c == '>')
        break;
      if(!space(c))previous=c;
      ++at;
    }
    if (at == html.size())
      return false;
    tag = {html.substr(name, attrs - name),  html.substr(attrs, at - attrs), begin, at + 1, closing,
           at > attrs && html[at - 1] == '/'};
    ++at;
    for (auto name_ : {"br", "hr", "img", "input", "meta", "link", "source", "wbr", "area", "base",
                       "embed", "param", "col"})
      if (equal(tag.name, name_))
        tag.single = true;
    return true;
  }
  return false;
}
// A null tag validates syntax without matching. The same parser is used by
// rule admission and matching, including quoted attribute values.
inline bool compound_match(const Tag *tag, std::string_view selector) {
  if (selector.empty())
    return false;
  std::size_t at = 0;
  if (selector[0] == '*')
    ++at;
  else if (name_char(selector[0])) {
    while (at < selector.size() && name_char(selector[at]))
      ++at;
    if (tag && !equal(tag->name, selector.substr(0, at)))
      return false;
  }
  while (at < selector.size()) {
    const char type = selector[at++];
    if(type=='[') {
      while(at<selector.size() && space(selector[at]))++at;
      const auto begin=at;while(at<selector.size() && name_char(selector[at]))++at;
      if(at==begin)return false;
      const auto name=selector.substr(begin,at-begin);
      while(at<selector.size() && space(selector[at]))++at;
      bool equality=false;std::string_view expected;
      if(at<selector.size() && selector[at]=='=') {
        equality=true;++at;while(at<selector.size() && space(selector[at]))++at;
        if(at==selector.size())return false;
        const char quote=selector[at]=='\'' || selector[at]=='"'?selector[at++]:0;
        const auto value=at;
        while(at<selector.size() && (quote?selector[at]!=quote:name_char(selector[at])))++at;
        expected=selector.substr(value,at-value);
        if(quote){if(at==selector.size())return false;++at;}
        else if(expected.empty())return false;
        while(at<selector.size() && space(selector[at]))++at;
      }
      if(at==selector.size() || selector[at++]!=']')return false;
      if(tag) {const auto value=tag->find_attribute(name);if(!value || (equality && *value!=expected))return false;}
      continue;
    }
    const auto begin = at;
    while (at < selector.size() && name_char(selector[at]))
      ++at;
    if (begin == at || (type != '.' && type != '#'))
      return false;
    const auto token = selector.substr(begin, at - begin);
    if (type == '#') {
      if (tag && tag->attribute("id") != token)
        return false;
    } else if(tag) {
      auto classes = tag->attribute("class");
      bool found = false;
      while (!classes.empty()) {
        classes = trim(classes);
        auto end = classes.find_first_of(" \t\n\r\f");
        if (classes.substr(0, end) == token)
          found = true;
        if (end == classes.npos)
          break;
        classes.remove_prefix(end);
      }
      if (!found)
        return false;
    }
  }
  return true;
}
inline bool compound(const Tag& tag,std::string_view selector){return compound_match(&tag,selector);}
struct Selector {
  std::array<std::string_view, 6> parts{};
  std::size_t count = 0;
  explicit Selector(std::string_view text) {
    text = trim(text);
    while (!text.empty() && count < parts.size()) {
      std::size_t end=0;bool bracket=false;char quote=0;
      for(;end<text.size();++end) {
        const char c=text[end];
        if(quote){if(c==quote)quote=0;else if(c=='\\'){count=0;return;}continue;}
        if(bracket && (c=='\'' || c=='"'))quote=c;
        else if(c=='['){if(bracket){count=0;return;}bracket=true;}
        else if(c==']'){if(!bracket){count=0;return;}bracket=false;}
        else if(!bracket && space(c))break;
      }
      if(bracket || quote){count=0;return;}
      auto part = text.substr(0, end);
      if(!compound_match(nullptr,part)) {
        count = 0;
        return;
      }
      parts[count++] = part;
      if (end == text.size()) {
        text = {};
        break;
      }
      text = trim(text.substr(end));
    }
    if (!text.empty())
      count = 0;
  }
  bool matches(std::span<const Tag> ancestors, const Tag &current) const {
    if (!count || !compound(current, parts[count - 1]))
      return false;
    auto part = count - 1;
    std::size_t at = ancestors.size();
    while (part) {
      bool found = false;
      while (at) {
        if (compound(ancestors[--at], parts[part - 1])) {
          found = true;
          break;
        }
      }
      if (!found)
        return false;
      --part;
    }
    return true;
  }
};
struct Element {
  Tag tag;
  std::string_view inner;
};
inline Element element(std::string_view html, const Tag &start) {
  if (start.single)
    return {start, {}};
  std::size_t at = start.end;
  unsigned level = 1;
  Tag tag;
  const bool raw =
      equal(start.name, "script") || equal(start.name, "style") || equal(start.name, "noscript");
  if (raw) {
    while ((at = html.find("</", at)) != html.npos) {
      const auto close = at;
      if (next_tag(html, at, tag) && tag.closing && equal(tag.name, start.name))
        return {start, html.substr(start.end, close - start.end)};
      if (at == html.npos)
        break;
    }
    return {start, html.substr(start.end)};
  }
  while (next_tag(html, at, tag)) {
    if (!tag.closing &&
        (equal(tag.name, "script") || equal(tag.name, "style") || equal(tag.name, "noscript"))) {
      at = tag.end + element(html, tag).inner.size();
      continue;
    }
    if (equal(tag.name, start.name)) {
      if (tag.closing && !--level)
        return {start, html.substr(start.end, tag.begin - start.end)};
      if (!tag.closing && !tag.single)
        ++level;
    }
  }
  return {start, html.substr(start.end)};
}
template <class F> bool select(std::string_view html, std::string_view rule, F &&callback) {
  const Selector selector(rule);
  if (!selector.count)
    return false;
  std::array<Tag, 32> parents{};
  std::size_t depth = 0, at = 0;
  Tag tag;
  while (next_tag(html, at, tag)) {
    if (tag.closing) {
      for (auto i = depth; i; --i)
        if (equal(parents[i - 1].name, tag.name)) {
          depth = i - 1;
          break;
        }
      continue;
    }
    if (selector.matches(std::span{parents}.first(depth), tag) && !callback(element(html, tag)))
      return true;
    if (equal(tag.name, "script") || equal(tag.name, "style") || equal(tag.name, "noscript")) {
      at = tag.end + element(html, tag).inner.size();
      continue;
    }
    if (!tag.single) {
      if (depth == parents.size())
        return false;
      parents[depth++] = tag;
    }
  }
  return true;
}
inline std::string text(std::string_view html, std::span<const std::string> remove = {},
                        std::size_t maximum = 65536) {
  std::string out;
  auto append = [&](std::string_view part) {
    if (part.size() > maximum - out.size())
      return false;
    out += part;
    return true;
  };
  auto newline = [&] {
    if (!out.empty() && out.back() != '\n' && out.size() < maximum)
      out += '\n';
  };
  std::size_t at = 0, previous = 0;
  Tag tag;
  while (next_tag(html, at, tag)) {
    if (!append(entities(html.substr(previous, tag.begin - previous))))
      return {};
    bool skip =
        equal(tag.name, "script") || equal(tag.name, "style") || equal(tag.name, "noscript");
    for (const auto &rule : remove)
      if (compound(tag, rule))
        skip = true;
    if (skip && !tag.closing) {
      const auto node = element(html, tag);
      at = tag.end + node.inner.size();
      Tag end;
      (void)next_tag(html, at, end);
      previous = at;
      continue;
    }
    if (equal(tag.name, "br") || equal(tag.name, "p") || equal(tag.name, "div") ||
        equal(tag.name, "li") || equal(tag.name, "h1") || equal(tag.name, "h2"))
      newline();
    previous = at;
  }
  if (!append(entities(html.substr(previous))))
    return {};
  auto clean = trim(out);
  return std::string(clean);
}
// Field: "a.title" extracts text; "a.title@href" extracts an attribute;
// "@href" and "" address the selected item itself.
inline std::size_t field_separator(std::string_view rule) {
  bool bracket=false;char quote=0;
  for(std::size_t at=0;at<rule.size();++at) {
    const char c=rule[at];
    if(quote){if(c==quote)quote=0;continue;}
    if(bracket && (c=='\'' || c=='"'))quote=c;
    else if(c=='[')bracket=true;
    else if(c==']')bracket=false;
    else if(c=='@' && !bracket)return at;
  }
  return rule.npos;
}
inline std::string field(const Element &item, std::string_view rule, std::size_t maximum = 512) {
  const auto attr = field_separator(rule);
  const auto selector = rule.substr(0, attr);
  auto read = [&](const Element &node) {
    return attr == rule.npos ? text(node.inner, {}, maximum)
                             : entities(node.tag.attribute(rule.substr(attr + 1)));
  };
  std::string out;
  if (selector.empty())
    out = read(item);
  else
    select(item.inner, selector, [&](const Element &node) {
      out = read(node);
      return false;
    });
  return out.size() <= maximum ? out : std::string{};
}
struct PageCount {std::uint32_t current=0,total=0;};
// Read a bounded visible prefix; some sites put "第(1/3)页" inside the
// article itself. Never copy a whole chapter just to discover its pagination.
inline std::optional<PageCount> page_count(std::string_view html,std::string_view selector) {
  std::string prefix;
  select(html,selector,[&](const Element& node){prefix=text(node.inner.substr(0,1024),{},2048);return false;});
  const auto slash=prefix.find('/');if(slash==prefix.npos)return {};
  auto begin=slash,end=slash+1;
  while(begin && prefix[begin-1]>='0' && prefix[begin-1]<='9')--begin;
  while(end<prefix.size() && prefix[end]>='0' && prefix[end]<='9')++end;
  PageCount count;const auto a=std::string_view(prefix).substr(begin,slash-begin),b=std::string_view(prefix).substr(slash+1,end-slash-1);
  auto read=[](std::string_view s,std::uint32_t& n){auto [end,ec]=std::from_chars(s.data(),s.data()+s.size(),n);return !s.empty() && ec==std::errc{} && end==s.data()+s.size();};
  if(!read(a,count.current) || !read(b,count.total) || !count.current || count.current>count.total || count.total>128)return {};
  return count;
}
struct Rules {
  std::string items, title, link, author, next, toc_items, toc_title, toc_link, content;
  std::string post_body;
  std::string single_title,single_author,single_link,single_suffix,referer,user_agent;
  struct PagedToc {
    std::string url, id, items, title, link, pages, total;
    bool html=false;
    std::uint32_t start=0;
  };
  std::optional<PagedToc> paged_toc;
  struct PagedContent {std::string url,pages,next,next_text,header;std::uint32_t start=1;bool detect_link=false;};
  std::optional<PagedContent> paged_content;
  std::array<std::string, 8> remove{};
  std::size_t remove_count = 0;
};
inline std::optional<Rules> rules(std::string_view raw) {
  Json j(raw);
  if (raw.size() > max_rules_bytes || !j.valid() ||
      j.get("format").string() != "pxa-reader-source-2" || j.get("kind").string() != "html")
    return {};
  Rules r;
  auto read = [](Json object, std::string_view key) {
    return object.get(key).string(160).value_or("");
  };
  const auto search = j.get("search"), toc = j.get("toc"), content = j.get("content");
  r.items = read(search, "items");
  r.title = read(search, "title");
  r.link = read(search, "link");
  r.author = read(search, "author");
  r.next = read(search, "next");
  r.toc_items = read(toc, "items");
  r.toc_title = read(toc, "title");
  r.toc_link = read(toc, "link");
  r.content = read(content, "selector");
  const auto single=search.get("single");
  r.single_title=read(single,"title");r.single_author=read(single,"author");r.single_link=read(single,"link");
  r.single_suffix=read(single,"stripTitleSuffix");
  if(!single.raw().empty() && (r.single_title.empty() || r.single_link.empty()))return {};
  r.referer=j.get("referer").string(256).value_or("");
  if(!r.referer.empty() && (origin(r.referer).empty() || !std::all_of(r.referer.begin(),r.referer.end(),
    [](unsigned char c){return c>=0x21 && c<=0x7e;})))return {};
  r.user_agent=j.get("userAgent").string(256).value_or("");
  if(!std::all_of(r.user_agent.begin(),r.user_agent.end(),[](unsigned char c){return c>=0x20 && c<=0x7e;}))return {};
  auto valid_field = [](std::string_view field_) {
    const auto at = field_separator(field_);
    return (field_.substr(0, at).empty() || Selector(field_.substr(0, at)).count) &&
           (at == field_.npos || (!field_.substr(at + 1).empty() &&
                                  std::all_of(field_.begin() + at + 1, field_.end(), name_char)));
  };
  auto method=search.get("method").string(8).value_or("GET");
  if(method=="POST") {
    auto body=search.get("body").string(512);
    if(!body || body->empty() || body->find("{{key}}")==body->npos)return {};
    r.post_body=std::move(*body);
  } else if(method!="GET" || !search.get("body").raw().empty())return {};
  if(!toc.get("paged").raw().empty()) {
    const auto p=toc.get("paged");
    Rules::PagedToc page{p.get("url").string(512).value_or(""),read(p,"id"),
      read(p,"items"),read(p,"title"),read(p,"link"),read(p,"pages"),read(p,"total")};
    const auto format=p.get("format").string(8).value_or("json");
    if(format!="json" && format!="html")return {};
    page.html=format=="html";page.start=p.get("start").number().value_or(page.html?1:0);
    if(page.start>1 || page.url.find("{{page}}")==page.url.npos)return {};
    if(page.html) {
      if(page.pages.empty() || !Selector(page.pages).count)return {};
    } else {
      if(page.url.find("{{id}}")==page.url.npos || page.id.empty() || !valid_field(page.id))return {};
      for(const auto* key:{&page.items,&page.title,&page.link,&page.pages,&page.total})
        if(key->empty() || !std::all_of(key->begin(),key->end(),name_char))return {};
    }
    r.paged_toc=std::move(page);
  }
  if(!content.get("paged").raw().empty()) {
    const auto p=content.get("paged");
    Rules::PagedContent page{p.get("url").string(512).value_or(""),read(p,"pages"),read(p,"next"),read(p,"text"),read(p,"header"),p.get("start").number().value_or(1)};
    page.detect_link=p.get("detectLink").raw()=="true";
    if(page.start>1)return {};
    if(!page.next.empty()) {
      if(!page.url.empty() || !page.pages.empty() || !valid_field(page.next) || page.next_text.empty())return {};
    } else if(page.url.find("{{page}}")==page.url.npos || !Selector(page.pages).count)return {};
    r.paged_content=std::move(page);
  }
  const bool html_toc=!r.paged_toc || r.paged_toc->html;
  if (!Selector(r.items).count || (html_toc && !Selector(r.toc_items).count) || !Selector(r.content).count ||
      r.link.empty() || (html_toc && r.toc_link.empty()))
    return {};
  for (const auto *f : {&r.title, &r.link, &r.author, &r.next, &r.toc_title, &r.toc_link,&r.single_title,&r.single_author,&r.single_link})
    if (!valid_field(*f))
      return {};
  if (!content.get("remove").raw().empty()) {
    bool ok = true;
    if (!content.get("remove").each(
            [&](Json v) {
              auto s = v.string(160);
              if (!s || r.remove_count == r.remove.size() || Selector(*s).count != 1) {
                ok = false;
                return false;
              }
              r.remove[r.remove_count++] = *s;
              return true;
            },
            9) ||
        !ok)
      return {};
  }
  return r;
}
inline std::optional<std::string> page_address(std::string_view pattern,
    std::string_view base,std::string_view id,std::uint32_t page) {
  if(id.size()>96)return {};
  std::string address;
  while(!pattern.empty()) {
    const auto marker=pattern.find("{{");
    address+=pattern.substr(0,marker);
    if(marker==pattern.npos)break;
    pattern.remove_prefix(marker);
    if(pattern.starts_with("{{id}}")){if(id.empty())return {};address+=percent(id);pattern.remove_prefix(6);}
    else if(pattern.starts_with("{{page}}")){address+=number_string(page);pattern.remove_prefix(8);}
    else if(pattern.starts_with("{{stem}}")) {
      const auto path=base.substr(0,base.find_first_of("?#"));
      const auto dot=path.rfind('.'),slash=path.rfind('/');
      address+=dot!=path.npos && dot>slash?path.substr(0,dot):path;
      pattern.remove_prefix(8);
    }
    else return {};
    if(address.size()>512)return {};
  }
  address=url(base,address);
  if(address.empty())return {};
  return address;
}
inline std::optional<std::string> toc_page_address(const Rules::PagedToc& r,
    std::string_view base,std::string_view id,std::uint32_t page) {
  return page_address(r.url,base,id,page+r.start);
}
struct TocPage {
  std::uint32_t total=0,pages=0;
  std::vector<Chapter> chapters;
};
struct TocCursor {
  std::string id;
  std::uint32_t total=0,pages=0,next=0;
  bool complete() const {return pages && next==pages;}
  // Validate a whole page before changing the prefix; retries remain safe.
  bool append(std::vector<Chapter>& prefix,TocPage page) {
    if(complete() || (next && (total!=page.total || pages!=page.pages)))return false;
    const auto count=prefix.size()+page.chapters.size();
    if(!page.pages || page.pages>128 || page.chapters.empty() || count>max_chapters ||
       (page.total && (count>page.total || (next+1==page.pages ? count!=page.total : count>=page.total))))return false;
    for(std::size_t i=0;i<page.chapters.size();++i) {
      const auto& link=page.chapters[i].url;
      if(std::any_of(prefix.begin(),prefix.end(),[&](const auto& c){return c.url==link;}) ||
         std::any_of(page.chapters.begin(),page.chapters.begin()+i,[&](const auto& c){return c.url==link;}))return false;
    }
    for(auto& chapter:page.chapters)prefix.push_back(std::move(chapter));
    total=page.total;pages=page.pages;++next;
    if(!total && complete())total=std::uint32_t(prefix.size());
    return true;
  }
};
inline std::optional<TocPage> toc_page(std::string_view raw,const Rules::PagedToc& r,std::string_view base) {
  Json j(raw);if(!j.valid())return {};
  // Some sites send counts as decimal strings; accept neither signs nor junk.
  auto count=[](Json v)->std::optional<std::uint32_t> {
    if(auto n=v.number())return n;
    auto s=v.string(10);if(!s || s->empty())return {};
    std::uint32_t n=0;auto [end,ec]=std::from_chars(s->data(),s->data()+s->size(),n);
    return ec==std::errc{} && end==s->data()+s->size()?std::optional{n}:std::nullopt;
  };
  auto total=count(j.get(r.total)),pages=count(j.get(r.pages));
  if(!total || !*total || *total>max_chapters || !pages || !*pages || *pages>128 || *pages>*total)return {};
  TocPage result{*total,*pages,{}};bool ok=true;
  if(!j.get(r.items).each([&](Json row){
      auto title=row.get(r.title).string(192),link=row.get(r.link).string(512);
      if(!title || title->empty() || !link || result.chapters.size()>=*total){ok=false;return false;}
      auto address=url(base,*link);if(address.empty()){ok=false;return false;}
      result.chapters.push_back({std::move(*title),std::move(address),UINT32_MAX});return true;
    },max_chapters+1) || !ok || result.chapters.empty())return {};
  return result;
}
inline std::vector<Book> books(std::string_view html, const Source &source, const Rules &r,
                               std::string_view base) {
  std::vector<Book> out;
  if (!valid_utf8(html))
    return out;
  const auto parsed = select(html, r.items, [&](const Element &item) {
    auto title = field(item, r.title, 192), link = url(base, field(item, r.link));
    if (!title.empty() && !link.empty() &&
        std::none_of(out.begin(), out.end(), [&](const Book &b) { return b.toc == link; })) {
      Book book{std::move(title), r.author.empty() ? "" : field(item, r.author, 128), "",
                std::move(link)};
      book.source = source.url;
      out.push_back(std::move(book));
    }
    return out.size() < max_books;
  });
  if (!parsed)
    out.clear();
  if(parsed && out.empty() && !r.single_title.empty()) {
    Element document{{},html};
    auto title=field(document,r.single_title,192),link=url(base,field(document,r.single_link));
    if(!r.single_suffix.empty() && title.ends_with(r.single_suffix))title.resize(title.size()-r.single_suffix.size());
    if(!title.empty() && !link.empty()) {
      Book book{std::move(title),r.single_author.empty()?"":field(document,r.single_author,128),"",std::move(link)};
      book.source=source.url;out.push_back(std::move(book));
    }
  }
  return out;
}
inline std::vector<Chapter> chapters(std::string_view html, const Rules &r, std::string_view base) {
  std::vector<Chapter> out;
  bool ok = valid_utf8(html);
  const bool parsed = select(html, r.toc_items, [&](const Element &item) {
    auto title = field(item, r.toc_title, 192), link = url(base, field(item, r.toc_link));
    if (title.empty() || link.empty() || out.size() == max_chapters) {
      ok = false;
      return false;
    }
    if (std::none_of(out.begin(), out.end(), [&](const Chapter &c) { return c.url == link; }))
      out.push_back({std::move(title), std::move(link), UINT32_MAX});
    return true;
  });
  if (!ok || !parsed)
    out.clear();
  return out;
}
inline std::optional<TocPage> html_toc_page(std::string_view html,const Rules& r,
    std::string_view base,std::uint32_t index) {
  if(!r.paged_toc || !r.paged_toc->html)return {};
  const auto count=page_count(html,r.paged_toc->pages);
  if(!count || count->current!=index+1)return {};
  auto entries=chapters(html,r,base);if(entries.empty())return {};
  return TocPage{0,count->total,std::move(entries)};
}
inline std::string content(std::string_view html, const Rules &r) {
  if (!valid_utf8(html))
    return {};
  std::string result;
  select(html, r.content, [&](const Element &item) {
    result = text(item.inner, std::span{r.remove}.first(r.remove_count));
    return false;
  });
  return result;
}
struct ChapterPart {std::string text,next;std::uint32_t pages=0;};
inline std::string page_header(std::string_view pattern,std::uint32_t current,std::uint32_t total) {
  std::string header(pattern);
  for(auto [key,value]:std::array{std::pair{std::string_view("{{page}}"),current},std::pair{std::string_view("{{total}}"),total}}) {
    const auto at=header.find(key);if(at!=header.npos)header.replace(at,key.size(),number_string(value));
  }
  return header;
}
inline std::optional<ChapterPart> chapter_part(std::string_view html,const Rules& r,
    std::string_view original,std::string_view current,std::uint32_t index) {
  ChapterPart part;part.text=content(html,r);if(part.text.empty())return {};
  if(r.paged_content) {
    const auto& p=*r.paged_content;
    if(!p.next.empty()) {
      Element document{{},html};const auto at=field_separator(p.next);
      if(at==p.next.npos)return {};
      if(field(document,std::string_view(p.next).substr(0,at),160)==p.next_text) {
        part.next=url(current,field(document,p.next));
        if(part.next.empty())return {};
      }
    } else {
      auto count=page_count(html,p.pages);
      if(count && !p.header.empty() && !part.text.starts_with(page_header(p.header,count->current,count->total)))count.reset();
      if(!count) {
        if(!p.detect_link)return {};
        const auto expected=page_address(p.url,original,{},index+1+p.start);if(!expected)return {};
        select(html,"a",[&](const Element& node){
          if(url(current,entities(node.tag.attribute("href")))==*expected){part.next=*expected;return false;}
          return true;
        });
      } else if(count->current!=index+1)return {};
      else if(count->current<count->total) {
        auto address=page_address(p.url,original,{},index+1+p.start);
        if(!address)return {};
        part.next=std::move(*address);
      }
      if(count)part.pages=count->total;
    }
  } else if(index)return {};
  if(r.paged_content && !r.paged_content->header.empty()) {
    const auto header=page_header(r.paged_content->header,index+1,part.pages);
    if(part.text.starts_with(header))part.text.erase(0,header.size());
  }
  return part;
}
} // namespace reader::web
