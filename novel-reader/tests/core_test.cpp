#include "../reader_state.hpp"
#include "../reader_catalog.hpp"
#include "../reader_paging.hpp"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iterator>
int main(int argc,char** argv) {
  using namespace reader;
  assert(parse_toc(R"({"chapters":[{"title":"missing url"}]})",
                   "https://x.test/toc")
             .empty());
  std::uint32_t random = 1234;
  for (unsigned run = 0; run < 10000; ++run) {
    std::string bytes;
    for (unsigned i = 0; i < run % 64; ++i) {
      random = random * 1664525 + 1013904223;
      bytes += char(random >> 24);
    }
    Json j(bytes);
    (void)j.valid();
    (void)j.string();
    (void)j.get("x").number();
    (void)j.each([](Json) { return true; });
  }

  assert(Json("{\"a\":[1,true,null,{\"x\":\"\\uD83D\\uDE00\"}]}").valid());
  assert(*Json("{\"x\":\"\\u4e66\\u9875\"}").get("x").string() == "书页");
  assert(!Json("{\"x\":\"\\uD800\"}").get("x").string());
  for (auto s :
       {"[1,]", "{\"a\":1,}", "01", "{\"x\":\"\\q\"}", "[", "true false"})
    assert(!Json(s).valid());
  Utf8Validator streaming;
  assert(streaming.append("a\xe4"));
  assert(!streaming.complete());
  assert(streaming.append("\xb9\xa6"));
  assert(streaming.complete());
  Utf8Validator overlong;
  assert(!overlong.append("\xc0\xaf"));
  Utf8Validator surrogate;
  assert(!surrogate.append("\xed\xa0\x80"));
  bool invalid = false;
  assert(utf8_prefix("a\xe4", invalid) == 1 && !invalid);
  assert(utf8_prefix("a\xe4\xb9", invalid) == 1 && !invalid);
  assert(valid_utf8("a书"));
  assert(!valid_utf8("a\xff"));
  assert(utf8_prefix("a\xff", invalid) == 1 && invalid);
  assert(!Json(std::string("\"a") + char(255) + "\"").string());
  assert(percent("西游记") == "%E8%A5%BF%E6%B8%B8%E8%AE%B0");
  assert(origin("https://example.com/a") == "https://example.com");
  assert(origin("file:///a").empty());
  assert(origin("https://user@example.com/a").empty());
  assert(resolve("https://example.com/list.json", "/book") ==
         "https://example.com/book");
  assert(content_range("bytes 0-32767/100000"));
  assert(!content_range("bytes 10-9/20"));
  assert(!content_range("bytes 0-20/20"));
  SavedState original;
  original.shelf.push_back(
      {"林间书屋", "书页", "sample:book", "", 123, 888, true});
  auto encoded = original.encode();
  SavedState restored;
  assert(restored.decode(encoded));
  assert(restored.shelf[0].position == 123);
  assert(restored.shelf[0].complete);
  assert(!restored.decode("{\"version\":3}"));
  assert(restored.settings.theme == 3);
  assert(search_classics("三国", SourceKind::classics).size() == 1);
  assert(search_classics("西遊記", SourceKind::classics).size() == 1);
  assert(search_classics("吴承恩", SourceKind::gitenberg).size() == 1);
  assert(search_classics("journey", SourceKind::gitenberg).size() == 1);
  assert(search_classics("", SourceKind::classics).size() == 4);
  assert(search_classics("", SourceKind::gitenberg).size() == 3);
  assert(search_classics("nothing", SourceKind::classics).empty());
  auto old = encoded;
  old.replace(old.find("\"version\":2"), 11, "\"version\":1");
  // Version 1 shelves remain intact and receive the optional fast catalogs.
  assert(restored.decode(old) && restored.shelf[0].position == 123);

  for (auto dims :
       {std::pair{296u, 240u}, std::pair{480u, 480u}, std::pair{240u, 296u},
        std::pair{412u, 412u}, std::pair{128u, 128u}, std::pair{800u, 480u}})
    for (unsigned dpi : {65536u, 98304u, 124928u, 196608u})
      for (unsigned shape : {0u, 1u, 2u}) {
        pxa::ui::DisplayMetrics m;
        m.width = dims.first;
        m.height = dims.second;
        m.density_q16 = dpi;
        m.safe = {8, 8, 8, 8};
        m.corners = {58, 58, 58, 58};
        m.shape = shape;
        Layout l(m,false);
        Settings settings;
        std::size_t at = 0, iterations = 0;
        while (at < sample_text.size()) {
          auto page = paginate(sample_text.substr(at), l, settings);
          if (!page.consumed) {
            assert(dims.first == 128 || shape == 2);
            break;
          }
          assert(!page.invalid_utf8);
          assert(page.count <= 64);
          for (std::size_t i = 0; i < page.count; ++i) {
            auto line = page.lines[i];
            auto bounds = l.span(line.box.y, line.box.h);
            assert(line.box.x >= bounds.first &&
                   line.box.x + line.box.w <= bounds.second);
            assert(line.box.y + line.box.h <= l.body_end());
            assert(line.end >= line.begin);
          }
          at += page.consumed;
          assert(++iterations < 1000);
        }
      }
  // Labels retain whole UTF-8 glyphs and signal omitted data explicitly.
  std::array<char, 64> scratch{};
  assert(ellipsize("阅读偏好", 24, 100, scratch) == "阅读偏好");
  assert(ellipsize("阅读偏好", 24, 80, scratch) == "阅读…");
  assert(ellipsize("很长很长的书名", 16, 16, scratch) == "…");
  assert(ellipsize("书名", 16, 15, scratch).empty());
  assert(text_line_height(28) >= 44);
  for (auto label : {"阅读偏好", "A−  字号 28", "A+  增大字号", "自动翻页 · 关", "15 秒 / 页"}) {
    int size = fit_text_size(label, 28, {0, 0, 134, 58});
    assert(text_width(label, size) <= 134);
    assert(text_line_height(size) <= 58);
  }
  // A page consumes exactly the text sent to the body renderer, at every font.
  for (unsigned font = 12; font <= 28; ++font) {
    pxa::ui::DisplayMetrics m; m.width = 800; m.height = 480;
    Layout l(m,false); Settings setting; setting.font = font;
    const std::string mixed = "第一章 ABC 世界\n" + std::string(sample_text);
    std::size_t at = 0;
    while (at < mixed.size()) {
      auto page = paginate(std::string_view(mixed).substr(at), l, setting);
      assert(page.consumed > 0);
      for (std::size_t i = 0; i < page.count; ++i) {
        auto& line = page.lines[i];
        assert(valid_utf8(std::string_view(mixed).substr(at + line.begin, line.end - line.begin)));
        assert(line.box.h >= text_line_height(font));
      }
      at += page.consumed;
    }
    assert(at == mixed.size());
  }
  std::string dense_text;
  for (unsigned i = 0; i < 4000; ++i) dense_text += "书";
  for (auto dims : {std::pair{800u, 480u}, std::pair{480u, 480u}, std::pair{296u, 240u}})
    for (unsigned font : {12u, 16u, 28u}) for (unsigned gap : {0u,6u,12u}) {
      pxa::ui::DisplayMetrics m; m.width = dims.first; m.height = dims.second;
      Layout l(m,false); Settings setting; setting.font = font; setting.line_gap=gap;
      auto page = paginate(dense_text, l, setting);
      std::size_t command_bytes = 32+28+96; // clear + bounded page-number label
      for (std::size_t i = 0; i < page.count; ++i)
        command_bytes += 28 + page.lines[i].end - page.lines[i].begin;
      assert(command_bytes <= 3600);
      assert(page.lines[page.count-1].box.y + page.lines[page.count-1].box.h <= l.page_number_box().y-2);
    }
  assert(chapter_heading("第一章 山间来信"));
  assert(chapter_heading("Chapter IV. The house"));
  assert(!chapter_heading("这是一段正常的小说正文。"));
  assert(!chapter_heading("第一天他说了这句话然后回家"));
  assert(!chapter_heading("第章 没有章号"));
  assert(chapter_heading("第一○○回 五圣成真"));
  assert(chapter_heading("第１０章 全角章号"));
  assert(chapter_heading("\xef\xbb\xbf　第一百回 最后旅程"));
  // Every split, including inside a multibyte numeral/newline, has identical
  // titles and original byte offsets. Incomplete last headings stay hidden.
  const std::string streamed = "\xef\xbb\xbf第一章 开始\r\n" + std::string(300,'x') + "\n　第二回 江边\n第三章 尚未缓存";
  std::vector<std::pair<std::string,std::uint32_t>> expected_chapters;
  for (std::size_t chunk=1;chunk<streamed.size();++chunk) {
    ChapterScanner scanner; decltype(expected_chapters) headings;
    auto emit = [&](std::string_view title,std::uint32_t offset) { headings.emplace_back(title,offset); };
    for (std::size_t at=0;at<streamed.size();at+=chunk) scanner.append(std::string_view(streamed).substr(at,chunk),emit);
    assert(headings.size()==2);
    if (expected_chapters.empty()) expected_chapters=headings;
    assert(headings==expected_chapters);
    scanner.finish(emit); assert(headings.size()==3);
    assert(headings.back().second==streamed.find("第三章"));
  }
  {
    pxa::ui::DisplayMetrics m; m.width=296;m.height=240;
    Layout l(m); Settings s;
    auto a=preference_preview(l,s);s.font=28;auto b=preference_preview(l,s);
    assert(b.pixels>a.pixels && b.first.h>a.first.h);
    s.line_gap=12;auto c=preference_preview(l,s);assert(c.step>b.step);
    assert(c.second.y+c.second.h<l.bottom-l.row);
    std::string raw="　　这是开头的一段正文，应该有缩进。\r\n连续折行合并成同一段正文。\r\n\r\n\r\n第二段正文在这里结束。";
    auto p=paginate(raw,l,s,true,true);assert(p.count>0);
    for (std::size_t i=0;i<p.count;++i) {
      auto str=std::string_view(raw).substr(p.lines[i].begin,p.lines[i].end-p.lines[i].begin);
      if (!str.empty()) {
        assert(!closing_punctuation(rune(str).value));
        std::size_t at=0;Rune last{};while(at<str.size()){last=rune(str.substr(at));at+=last.bytes;}
        assert(!opening_punctuation(last.value));
      }
    }
    const auto consumed=p.consumed; compact_page_text(raw,p);
    assert(p.consumed==consumed && raw.find('\r')==raw.npos && raw.find('\n')==raw.npos && valid_utf8(raw));
    auto continuation=paginate("续写的正文没有重新缩进。",l,s,false);
    assert(continuation.lines[0].box.x< p.lines[0].box.x);
  }
  auto books = parse_books(
      R"({"results":[{"title":"西遊記","copyright":false,"authors":[{"name":"Wu"}],"formats":{"text/plain; charset=utf-8":"https://www.gutenberg.org/ebooks/23962.txt.utf-8"}},{"title":"Paid","copyright":true,"formats":{"text/plain; charset=utf-8":"https://x.test/a"}}]})",
      Source{"PG", "https://gutendex.com/books", SourceKind::gutenberg});
  assert(books.size() == 1 && books[0].author == "Wu");
  auto check_numbers=[](std::string_view text,const Layout& layout,const Settings& settings,
      bool reflow,std::size_t window) {
    std::vector<std::uint32_t> starts;
    for (std::size_t at=0;at<text.size();) {
      auto p=paginate(text.substr(at),layout,settings,!at || text[at-1]=='\n' || text[at-1]=='\r',reflow);
      assert(p.consumed);
      if (p.count) starts.push_back(std::uint32_t(at));
      at+=p.consumed;
    }
    PageNumbers n; n.available=std::uint32_t(text.size());
    while (n.offset<text.size()) {
      const auto input=text.substr(n.offset,window);
      auto used=n.consume(input,layout,settings,0,reflow,n.offset+input.size()==text.size(),false);
      assert(used && used<=input.size());
    }
    assert(n.total==starts.size() && n.mark_count<=n.marks.size());
    n.finished=n.exact_total=true;
    std::array<char,96> label{};
    assert(n.label(0,label).starts_with("1 / "));
    for (std::size_t i=0;i<starts.size();i+=std::max<std::size_t>(1,starts.size()/13)) {
      for (auto target : {starts[i],std::min<std::uint32_t>(starts[i]+3,std::uint32_t(text.size()-1))}) {
        n.locate(target);
        n.cursor.paragraph_start=!n.offset || text[n.offset-1]=='\n' || text[n.offset-1]=='\r';
        while (n.current_offset!=target) {
          const auto input=text.substr(n.offset,window);
          const auto used=n.consume(input,layout,settings,target,reflow,n.offset+input.size()==text.size(),true);
          if (!used) std::fprintf(stderr,"count seek stuck target=%u offset=%u page=%zu total=%u width=%d font=%u window=%zu bytes=%zu\n",target,n.offset,i+1,n.total,layout.width,settings.font,window,text.size());
          assert(used);
        }
        assert(n.current==i+1 && n.current_boundary==(target==starts[i]));
        assert(n.current_start==starts[i]);
        assert(n.total==starts.size());
      }
    }
  };
  for (auto dims : {std::pair{296u,240u},std::pair{480u,480u},std::pair{800u,480u}})
    for (unsigned font : {12u,16u,28u}) for (unsigned gap : {0u,6u,12u}) {
      pxa::ui::DisplayMetrics m;m.width=dims.first;m.height=dims.second;
      Layout l(m,false);Settings s;s.font=font;s.line_gap=gap;
      for (std::size_t window : {512u,769u,4096u}) {
        check_numbers(sample_text,l,s,false,window);
        check_numbers(dense_text,l,s,false,window);
        check_numbers(std::string(1200,' ')+std::string(sample_text),l,s,false,window);
      }
    }
  {
    PageNumbers n;n.total=12;n.finished=true;n.current=2;n.current_offset=128;
    std::array<char,96> scratch{};
    assert(n.label(128,scratch)=="2 / 已缓存 12+ 页");
    n.exact_total=true;assert(n.label(128,scratch)=="2 / 12 页");
    n.running=true;const auto generation=n.generation;n.reset();
    assert(n.running && n.generation==generation+1 && !n.total && !n.finished);
  }
  if (argc == 2) {
    std::ifstream file(argv[1],std::ios::binary);
    const std::string novel((std::istreambuf_iterator<char>(file)),{});
    assert(!novel.empty() && valid_utf8(novel));
    {
      // The user's 296x240 / 12 dp / zero-gap page used to miss the ninth
      // complete row by two pixels despite unused space below the footer.
      pxa::ui::DisplayMetrics m;m.width=296;m.height=240;
      m.safe={10,8,10,8};m.corners={58,58,58,58};
      Layout l(m,false);Settings s;s.font=12;s.line_gap=0;
      const auto p=paginate(std::string_view(novel).substr(604),l,s,true,true);
      assert(p.count>=9);
      assert(p.lines[p.count-1].box.y+p.lines[p.count-1].box.h<l.page_number_box().y);
      std::printf("User reading preferences: %zu complete rows, footer separated OK\n",p.count);
    }
    auto digest = [](std::uint64_t& h,std::string_view text) {
      for (std::size_t at=0;at<text.size();) {
        auto r=rune(text.substr(at));
        if (r.value!=' ' && r.value!='\r' && r.value!='\n' && r.value!='\t' && r.value!=0x3000 && r.value!=0xfeff)
          for (unsigned char c:text.substr(at,r.bytes)) h=(h^c)*1099511628211ull;
        at+=r.bytes;
      }
    };
    std::uint64_t expected=14695981039346656037ull;digest(expected,novel);
    ChapterScanner scanner;std::size_t chapter_count=0;
    auto emit=[&](std::string_view,std::uint32_t offset){assert(offset<novel.size());++chapter_count;};
    for (std::size_t at=0;at<novel.size();at+=4096) scanner.append(std::string_view(novel).substr(at,4096),emit);
    scanner.finish(emit);assert(chapter_count==100);
    for (auto dimensions: {std::pair{296u,240u},std::pair{480u,480u},std::pair{360u,480u},std::pair{800u,480u}})
      for (unsigned font:{12u,16u,28u}) {
        pxa::ui::DisplayMetrics m;m.width=dimensions.first;m.height=dimensions.second;m.density_q16=dimensions.first==296 || dimensions.first==800?65536:124928;
        Layout l(m,false);Settings setting;setting.font=font;
        std::uint64_t actual=14695981039346656037ull;std::size_t at=0,pages=0;
        while (at<novel.size()) {
          auto p=paginate(std::string_view(novel).substr(at),l,setting,at==0 || novel[at-1]=='\n',true);
          assert(p.consumed>0 && !p.invalid_utf8 && p.consumed<=novel.size()-at);
          for (std::size_t i=0;i<p.count;++i) {
            const auto& line=p.lines[i];
            assert(line.begin<=line.end && line.end<=p.consumed);
            const auto value=std::string_view(novel).substr(at+line.begin,line.end-line.begin);
            assert(valid_utf8(value));digest(actual,value);
          }
          at+=p.consumed;++pages;
        }
        assert(actual==expected); // No lost, duplicated or reordered non-space characters.
        check_numbers(novel,l,setting,true,4096);
        std::printf("Real novel %ux%u font=%u pages=%zu: complete text digest OK\n",dimensions.first,dimensions.second,font,pages);
      }
    std::printf("Real novel bytes=%zu chapters=%zu: streamed index OK\n",novel.size(),chapter_count);
  }
  std::puts("Reader core: JSON/unicode, portable state, sources, ranges and "
            "shaped/DPI pagination OK");
}
