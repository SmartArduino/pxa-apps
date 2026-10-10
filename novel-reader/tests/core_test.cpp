#include "../reader_state.hpp"
#include <cassert>
#include <cstdio>
int main() {
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
  assert(!restored.decode("{\"version\":2}"));
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
        Layout l(m);
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
            assert(line.end >= line.begin);
          }
          at += page.consumed;
          assert(++iterations < 1000);
        }
      }
  assert(chapter_heading("第一章 山间来信"));
  assert(chapter_heading("Chapter IV. The house"));
  assert(!chapter_heading("这是一段正常的小说正文。"));
  auto books = parse_books(
      R"({"results":[{"title":"西遊記","copyright":false,"authors":[{"name":"Wu"}],"formats":{"text/plain; charset=utf-8":"https://www.gutenberg.org/ebooks/23962.txt.utf-8"}},{"title":"Paid","copyright":true,"formats":{"text/plain; charset=utf-8":"https://x.test/a"}}]})",
      original.sources[0]);
  assert(books.size() == 1 && books[0].author == "Wu");
  std::puts("Reader core: JSON/unicode, portable state, sources, ranges and "
            "shaped/DPI pagination OK");
}
