#pragma once
#include "reader_web.hpp"

namespace reader {
// Built-in documents stay in mapped read-only storage. A Source owns rules
// only when the user imports an override; neither state nor RAM duplicates them.
inline constexpr std::string_view nunu_rules=R"({
  "format":"pxa-reader-source-2","name":"努努书坊","kind":"html",
  "searchUrl":"https://www.nunubook.com/e/search/index.php",
  "search":{"method":"POST","body":"tbname=bookname&show=title%2Cwriter&tempid=1&keyboard={{key}}",
    "items":".s-list-wrap li","title":".book-title .title","link":"a@href","author":".book-author","next":".page_next@href"},
  "toc":{"paged":{"url":"/e/extend/bookpage/pages.php?id={{id}}&pageNum={{page}}&dz=asc",
    "id":"#bookDetails@data-bookid","items":"list","title":"title","link":"pic","pages":"totalPage","total":"total"}},
  "content":{"selector":"#text","remove":["script","style",".ad"]}
})";
inline constexpr std::string_view shuzhaige_rules=R"PXA({"format":"pxa-reader-source-2","kind":"html","name":"书斋阁","searchUrl":"https://www.shuzhaige.com/e/search/index.php","search":{"method":"POST","body":"tbname=bookname&show=title%2Cwriter&tempid=1&keyboard={{key}}","items":".table tr","title":"a","link":"a@href","author":"td.col-sm-4"},"toc":{"items":"#play_0 a","title":"","link":"@href"},"content":{"selector":"#content","remove":["script","style"]},"userAgent":"Mozilla/5.0"})PXA";
inline constexpr std::string_view biquche_rules=R"PXA({"format":"pxa-reader-source-2","kind":"html","name":"笔趣阁 · biquche","searchUrl":"https://www.biquche.com/search.php?q={{key}}","search":{"items":"dl","title":"dt img@alt","link":"h3 a@href","author":".book_other span"},"toc":{"items":".book_list2 a","title":"","link":"@href","paged":{"format":"html","url":"index_{{page}}.html","start":1,"pages":".pagination .disabled a"}},"content":{"selector":"article.font_max","remove":["ins","script","style"],"paged":{"url":"{{stem}}_{{page}}.html","pages":"article.font_max","start":1,"header":"第({{page}}/{{total}})页","detectLink":true}},"userAgent":"Mozilla/5.0"})PXA";
inline constexpr std::string_view wenlinshe_rules=R"PXA({"format":"pxa-reader-source-2","kind":"html","name":"文林社","searchUrl":"https://www.wenlinshe.com/search?q={{key}}","referer":"https://www.wenlinshe.com/","search":{"items":".book-search-results-item","title":".book-search-results-name","link":"a.book-search-results-link@href","single":{"title":"h1","link":"link[rel=canonical]@href","author":".book-author-name","stripTitleSuffix":"全文在线阅读"}},"toc":{"items":"#chapterList a","title":"","link":"@href"},"content":{"selector":"#chapterContent","remove":["h2.structural",".chapter-inline-ad"]},"userAgent":"Mozilla/5.0"})PXA";
inline constexpr std::uint32_t current_source_revision=2;
struct BuiltinSource {std::string_view key,name,url,rules;std::uint32_t introduced;};
inline constexpr std::array builtin_sources{
  BuiltinSource{"nunu","努努书坊","https://www.nunubook.com/e/search/index.php",nunu_rules,1},
  BuiltinSource{"shuzhaige","书斋阁","https://www.shuzhaige.com/e/search/index.php",shuzhaige_rules,2},
  BuiltinSource{"biquche","笔趣阁 · biquche","https://www.biquche.com/search.php?q={{key}}",biquche_rules,2},
  BuiltinSource{"wenlinshe","文林社","https://www.wenlinshe.com/search?q={{key}}",wenlinshe_rules,2},
};
inline const BuiltinSource* builtin_source(std::string_view url) {
  for(const auto& source:builtin_sources)if(source.url==url)return &source;
  return nullptr;
}
inline std::string_view source_rules(const Source& source) {
  if(!source.rules.empty())return source.rules;
  if(const auto* builtin=builtin_source(source.url))return builtin->rules;
  return {};
}
inline Source make_source(const BuiltinSource& source) {
  return {std::string(source.name),std::string(source.url),SourceKind::html,true,{}};
}
inline std::vector<Source> default_sources() {
  std::vector<Source> out;
  for(const auto& source:builtin_sources)out.push_back(make_source(source));
  return out;
}
inline bool old_default_source(const Source& source) {
  switch(source.kind) {
  case SourceKind::classics:return source.url=="https://www.gutenberg.org/cache/epub/";
  case SourceKind::gitenberg:return source.url=="https://raw.githubusercontent.com/GITenberg/";
  case SourceKind::gutenberg:return source.url=="https://gutendex.com/books/?languages=zh&copyright=false&mime_type=text%2Fplain" ||
      source.url=="https://gutendex.com/books/?copyright=false&mime_type=text%2Fplain&topic=fiction";
  default:return false;
  }
}
} // namespace reader
