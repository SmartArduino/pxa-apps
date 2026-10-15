#include "../reader_catalog.hpp"
#include "../reader_state.hpp"
#include "../reader_window.hpp"
#include <cassert>
#include <cstdio>

int main() {
  using namespace reader;
  assert(search_address("https://x.test/?q={{key}}&also={{key}}", "书") ==
         "https://x.test/?q=%E4%B9%A6&also=%E4%B9%A6");
  assert(!search_address(std::string(510, 'a') + "{{key}}", "书"));
  const std::string config =
      R"({"format":"pxa-reader-source-2","name":"静态书站","kind":"html","searchUrl":"/search?q={{key}}","search":{"items":"#books .book","title":"a.title","link":"a.title@href","author":".author","next":"a.next@href"},"toc":{"items":"#chapters a","title":"","link":"@href"},"content":{"selector":"#content","remove":[".ad"]}})";
  auto r = web::rules(config);
  assert(r);
  Source source{"静态书站", "https://x.test/search?q={{key}}", SourceKind::html, true, config};
  const auto books = web::books(
      R"(<html><div id="books"><article class="book extra"><a class="title" href="../book/1">山间 &amp; 来信</a><span class="author">林舟</span></article><article class="book"><a class="title" href="javascript:evil">危险链接</a></article></div><a class="next" href="?page=2">下一页</a></html>)",
      source, *r, "https://x.test/list/search?q=1");
  assert(books.size() == 1 && books[0].title == "山间 & 来信" && books[0].author == "林舟");
  assert(books[0].toc == "https://x.test/book/1" && books[0].source == source.url);
  auto chapters = web::chapters(
      R"(<nav id="chapters"><a href="/chapter/1">第一章</a><a href="//cdn.test/chapter/2">第二章</a><a href="/chapter/1">第一章</a></nav>)",
      *r, "https://x.test/book/1");
  assert(chapters.size() == 2 && chapters[1].url == "https://cdn.test/chapter/2");
  assert(
      web::chapters("<div id='chapters'><a href='javascript:x'>bad</a></div>", *r, "https://x.test")
          .empty());
  const auto body = web::content(
      R"(<h1>outside</h1><DIV id="content"><p>清晨&nbsp;来信&#x3002;</p><!-- secret --><script>if(a<b){"<div>"}</script><style>bad</style><div class="ad">广告<p>广告</p></div><p>河边<br>书屋&#10;下一行</p></DIV>)",
      *r);
  assert(body == "清晨 来信。\n河边\n书屋\n下一行");
  assert(web::text("<script>unfinished").empty());
  assert(web::entities("&#x1f600;") == "😀");
  assert(web::entities("&unknown;") == "&unknown;");
  assert(!web::rules("{}"));
  assert(web::Selector("link[rel=canonical]").count==1);
  assert(web::Selector("a[title='a b@c'] .title").count==2);
  for(auto bad:{"a[x~=y]","a[x=]","a[x='bad]","a[x]junk","a[x=[y]]"})assert(!web::Selector(bad).count);
  web::Element attrs{{},R"(<a title="a b@c" href="/ok" disabled>结果</a><a href="/other">其他</a>)"};
  assert(web::field(attrs,"a[title='a b@c']@href")=="/ok");
  assert(web::field(attrs,"a[disabled]")=="结果");
  const auto defaults=default_sources();
  assert(defaults.size()==4 && !old_default_source(defaults[0]));
  for(const auto& source:defaults)assert(source.rules.empty() && web::rules(source_rules(source)));
  const auto website=web::rules(source_rules(defaults[0]));
  assert(website && !website->post_body.empty() && website->paged_toc);
  assert(web::books(R"(<div class="s-list-wrap"><div class="s-nv-list""><ul><li><a href="/book/1/"><h3 class="book-title"><span class="title">山间来信</span></h3><h5 class="book-author">林舟</h5></a></li></ul></div></div>)",
    defaults[0],*website,defaults[0].url).size()==1);
  // Ignore dangling attribute quotes, but preserve > inside a quoted value.
  assert(web::text(R"(<a title="a > b">书</a>)")=="书");
  const auto& paged=*website->paged_toc;
  assert(web::toc_page_address(paged,"https://x.test/book/1/","7",2)==
    "https://x.test/e/extend/bookpage/pages.php?id=7&pageNum=2&dz=asc");
  assert(!web::toc_page_address(paged,"https://x.test/","",0));
  auto remote_page=web::toc_page(R"({"total":"2","totalPage":2,"list":[{"title":"第一章","pic":"/1.html"}]})",paged,"https://x.test/book/1/");
  assert(remote_page && remote_page->total==2 && remote_page->pages==2 && remote_page->chapters[0].url=="https://x.test/1.html");
  for(auto bad:{R"({"total":1025,"totalPage":1,"list":[]})",R"({"total":"2x","totalPage":1,"list":[]})",
    R"({"total":2,"totalPage":129,"list":[]})",R"({"total":2,"totalPage":1,"list":[]})",
    R"({"total":2,"totalPage":1,"list":[{"title":"第一章","pic":"javascript:bad"}]})"})assert(!web::toc_page(bad,paged,"https://x.test/"));
  web::TocCursor cursor{"book-id"};std::vector<Chapter> prefix;
  assert(cursor.append(prefix,{3,2,{{"一","https://x.test/1",UINT32_MAX},{"二","https://x.test/2",UINT32_MAX}}}));
  assert(!cursor.complete() && cursor.next==1 && prefix.size()==2);
  assert(!cursor.append(prefix,{3,2,{{"重复","https://x.test/1",UINT32_MAX}}}));
  assert(!cursor.append(prefix,{4,2,{{"变动","https://x.test/3",UINT32_MAX}}}));
  assert(!cursor.append(prefix,{3,2,{}}));
  assert(cursor.next==1 && prefix.size()==2); // Failed pages preserve a retryable prefix.
  assert(cursor.append(prefix,{3,2,{{"三","https://x.test/3",UINT32_MAX}}}) && cursor.complete());
  assert(!cursor.append(prefix,{3,2,{{"多余","https://x.test/4",UINT32_MAX}}}));
  SavedState no_sources;no_sources.sources.clear();SavedState empty_restored;
  assert(empty_restored.decode(no_sources.encode()) && empty_restored.sources.empty());
  {
    SavedState old;old.source_revision=1;old.sources.resize(1);old.sources[0].rules=std::string(nunu_rules);
    auto raw=old.encode(); // An older install has only the first built-in rule.
    SavedState upgraded;assert(upgraded.decode(raw) && upgraded.sources.size()==defaults.size());
    assert(upgraded.sources[0].rules.empty() && upgraded.sources[0].rules.capacity()==std::string{}.capacity());
    assert(source_rules(upgraded.sources[0]).data()==nunu_rules.data());
    upgraded.sources.erase(upgraded.sources.begin()+1);
    assert(upgraded.decode(upgraded.encode()) && upgraded.sources.size()==defaults.size()-1);
    old.sources.clear();assert(upgraded.decode(old.encode()) && upgraded.sources.empty());
    old.sources={{"用户规则","https://custom.test/",SourceKind::html,true,config}};
    assert(upgraded.decode(old.encode()) && upgraded.sources.size()==1 && upgraded.sources[0].rules==config);
    old.sources=defaults;old.sources[0].url="https://unknown.test/";
    assert(!upgraded.decode(old.encode())); // Compact rules must refer to this app's registry.
  }
  {
    const auto counted=web::rules(source_rules(defaults[2]));assert(counted && counted->paged_toc->html && counted->paged_content);
    const auto html=R"(<p class="pagination"><a>1/3</a></p><article class="font_max">第(1/2)页<br>正文第一页</article>)";
    assert(web::page_count(html,"article.font_max")->total==2);
    auto part=web::chapter_part(html,*counted,"https://x.test/ch1.html","https://x.test/ch1.html",0);
    assert(part && part->pages==2 && part->next=="https://x.test/ch1_2.html" && part->text.find("第(1/2)页")==std::string::npos);
    assert(!web::chapter_part(html,*counted,"https://x.test/ch1.html","https://x.test/ch1.html",1));
    const auto linked=web::rules(R"({"format":"pxa-reader-source-2","kind":"html","search":{"items":"a","title":"","link":"@href"},"toc":{"items":"a","link":"@href"},"content":{"selector":"#TextContent","paged":{"next":"#next_url@href","text":"下一页"}}})");assert(linked && linked->paged_content);
    part=web::chapter_part(R"(<div id="TextContent"><p>前半</p></div><a id="next_url" href="./ch1_2.html">下一页</a>)",*linked,"https://x.test/ch1.html","https://x.test/ch1.html",0);
    assert(part && part->next=="https://x.test/ch1_2.html");
    part=web::chapter_part(R"(<div id="TextContent"><p>后半</p></div><a id="next_url" href="./ch2.html">下一章</a>)",*linked,"https://x.test/ch1.html","https://x.test/ch1_2.html",1);
    assert(part && part->next.empty());
    web::TocCursor cursor;std::vector<Chapter> prefix;
    assert(cursor.append(prefix,{0,2,{{"一","https://x.test/1",UINT32_MAX}}}) && cursor.total==0 && !cursor.complete());
    assert(cursor.append(prefix,{0,2,{{"二","https://x.test/2",UINT32_MAX}}}) && cursor.total==2 && cursor.complete());
  }
  for(unsigned version:{1u,2u,3u}) {
    SavedState old;
    old.sources={{"用户书源","https://x.test/api",SourceKind::json},
      {"经典直读","https://www.gutenberg.org/cache/epub/",SourceKind::classics},
      {"镜像","https://raw.githubusercontent.com/GITenberg/",SourceKind::gitenberg},
      {"古腾堡中文小说","https://gutendex.com/books/?languages=zh&copyright=false&mime_type=text%2Fplain",SourceKind::gutenberg},
      {"古腾堡世界文学","https://gutendex.com/books/?copyright=false&mime_type=text%2Fplain&topic=fiction",SourceKind::gutenberg}};
    old.shelf.push_back({"旧书","作者","https://old.test/book.txt","",15,100,true,"v1"});
    old.remember_search("旧搜索");old.settings.offline=true;
    auto raw=old.encode();raw.replace(raw.find("\"version\":4"),11,"\"version\":"+number_string(version));
    SavedState upgraded;assert(upgraded.decode(raw));
    assert(upgraded.sources.size()==1+defaults.size() && upgraded.sources[0].url=="https://x.test/api" && upgraded.sources[1].url==defaults[0].url);
    assert(upgraded.shelf[0].position==15 && upgraded.shelf[0].complete && upgraded.shelf[0].validator=="v1");
    assert(upgraded.searches==old.searches && upgraded.settings.offline);
    assert(upgraded.decode(upgraded.encode()) && upgraded.sources.size()==1+defaults.size());
    // A custom Gutendex query is not mistaken for an old built-in source.
    old.sources={{"自定义筛选","https://gutendex.com/books/?languages=en",SourceKind::gutenberg,false}};
    raw=old.encode();raw.replace(raw.find("\"version\":4"),11,"\"version\":3");
    assert(upgraded.decode(raw) && upgraded.sources.size()==1 && !upgraded.sources[0].enabled);
  }
  auto invalid = config;
  invalid.replace(invalid.find("#books .book"), 12, "#books > a");
  assert(!web::rules(invalid));
  assert(!web::Selector("a..title").count);
  assert(!web::Selector("a:nth-child(2)").count);
  assert(resolve("https://x.test/a/b?q=1", "../c/./d#f") == "https://x.test/c/d");
  assert(resolve("https://x.test/a/b?q=1", "?page=2") == "https://x.test/a/b?page=2");
  assert(resolve("https://x.test/a/b", "//cdn.test/x") == "https://cdn.test/x");
  for (auto bad : {"javascript:x", "data:text/plain,hi", "file:///tmp/x", "https://u@x.test/",
                   "https://x.test/a\nb"})
    assert(resolve("https://x.test/", bad).empty());
  SavedState saved;
  saved.sources.push_back(source);
  saved.shelf.push_back(books[0]);
  saved.shelf[0].chapter = 1;
  saved.remember_search("山间");
  saved.remember_search("林舟");
  saved.remember_search("山间");
  SavedState restored;
  assert(restored.decode(saved.encode()));
  assert(restored.searches.size() == 2 && restored.searches[0] == "山间" &&
         !restored.settings.offline);
  assert(restored.sources.back().rules == config && restored.shelf[0].source == source.url &&
         restored.shelf[0].chapter == 1);
  auto large = saved;
  large.sources.clear();
  large.shelf.clear();
  std::string padded = config;
  padded.pop_back();
  padded += ",\"unused\":\"";
  for (unsigned i = 0; i < 1600; ++i)
    padded += "\\\\";
  padded += "\"}";
  assert(web::rules(padded));
  for (unsigned i = 0; i < max_sources; ++i) {
    auto extra = source;
    extra.name = std::string(96, '"');
    extra.url = "https://x.test/" + std::string(490, 'a');
    extra.rules = padded;
    large.sources.push_back(std::move(extra));
  }
  for (unsigned i = 0; i < max_shelf; ++i) {
    Book b{std::string(192, '"'), std::string(128, '"'), "https://x.test/" + std::string(490, 'a'),
           "https://x.test/" + std::string(490, 'b')};
    b.source = "https://x.test/" + std::string(490, 'c');
    b.validator = std::string(128, '"');
    large.shelf.push_back(std::move(b));
  }
  const auto full_state = large.encode();
  assert(full_state.size() > 65536 && full_state.size() < SavedState::max_state_bytes);
  assert(restored.decode(full_state));
  for (unsigned i = 0; i < 10; ++i)
    saved.remember_search(number_string(i));
  assert(saved.searches.size() == 6);
  ReadingWindow window;
  window.text = "书页\n正文";
  window.offset = 30;
  window.paragraph = false;
  assert(window.covers(30, window.offset + window.text.size()));
  assert(!window.covers(29, 1000));
  assert(!window.covers(30, 1000));
  assert(window.paragraph_at(37));
  pxa::ui::DisplayMetrics metrics;
  metrics.width = 296;
  metrics.height = 240;
  std::string raw = "第一章\n\n";
  for (unsigned i = 0; i < 100; ++i)
    raw += "清晨的风沿着山路吹来，白鸟带来一封信。\n";
  auto page = paginate(raw, Layout(metrics, false), Settings{});
  const auto consumed = page.consumed;
  std::string display;
  copy_page_text(raw, display, page);
  assert(page.consumed == consumed && display.size() < page_command_capacity);
  assert(raw.size() > display.size() && display.find('\n') == display.npos);
  std::uint32_t seed = 81;
  for (unsigned run = 0; run < 8000; ++run) {
    std::string input;
    for (unsigned i = 0; i < run % 80; ++i) {
      seed = seed * 1664525 + 1013904223;
      input += char(seed >> 24);
    }
    web::select(input, "a.x", [](const web::Element &e) {
      (void)web::field(e, "@href");
      return true;
    });
    (void)web::text(input);
    (void)web::entities(input);
  }
  puts("Reader web rules, UTF-8/entities, URL resolution, state and reading window OK");
}
