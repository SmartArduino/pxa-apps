#include "../browser_find.hpp"
#include <cassert>
#include <iostream>
#include <random>
using namespace browser;

int main() {
    Document d;
    Layout layout;
    PageFind find;
    auto parse = [&](std::string_view html, int width = 200) {
        d.clear("https://find.test/");
        Parser parser(d);
        for (char c : html) parser.feed({&c, 1});
        parser.finish();
        layout.build(d, width, 15);
    };
    parse("<h1>中文查找</h1><p>First NEEDLE, second needle, third Needle.</p>");
    assert(!find.active() && find.count() == 0);
    assert(find.start(d, layout, "  nEeDlE  "));
    assert(find.query() == "nEeDlE" && find.count() == 3 && find.ordinal() == 1);
    const auto first = find.offset();
    find.move(d, layout, true);
    assert(find.ordinal() == 3 && find.offset() > first);
    find.move(d, layout, false);
    assert(find.ordinal() == 1 && find.offset() == first);
    find.move(d, layout, false);
    assert(find.ordinal() == 2);
    find.move(d, layout, false);
    assert(find.ordinal() == 3);
    assert(find.start(d, layout, "中文"));
    assert(find.count() == 1 && find.ordinal() == 1 && find.offset() == 0);
    assert(!find.start(d, layout, " ") && !find.start(d, layout, "x\ny"));
    assert(!find.start(d, layout, std::string(PageFind::max_query + 1, 'x')));
    assert(find.query() == "中文"); // invalid edits preserve the previous search

    parse("<p>Chinese 中文<a href='/'>跨行</a>查找 phrase.</p>", 45);
    assert(find.start(d, layout, "中文跨行查找"));
    assert(find.count() == 1);
    std::size_t highlighted_bytes = 0;
    unsigned highlighted_pieces = 0;
    for (const auto &piece : layout.pieces) {
        if (find.overlaps(piece)) {
            highlighted_bytes += std::min<std::size_t>(piece.offset + piece.size,
                                                       find.offset() + find.query().size()) -
                                 std::max<std::size_t>(piece.offset, find.offset());
            ++highlighted_pieces;
        }
    }
    assert(highlighted_bytes == find.query().size() && highlighted_pieces >= 3);

    parse("<p>public needle</p><form><input name='needle' value='needle'>"
          "<input type='password' name='needle' value='needle'>"
          "<input type='hidden' name='secret' value='needle'>"
          "<input type='submit' value='needle'></form>"
          "<img src='/pic.png' alt='needle'><div hidden>needle</div>"
          "<script>needle</script><style>needle</style><p>public NEEDLE</p>");
    assert(find.start(d, layout, "needle") && find.count() == 2);
    assert(find.start(d, layout, "secret") && find.count() == 0 && find.ordinal() == 0);
    find.move(d, layout, true);
    assert(find.offset() == std::string::npos);

    parse("<p>aaaaa</p>");
    assert(find.start(d, layout, "aa") && find.count() == 2);
    find.move(d, layout, false);
    assert(find.offset() == 2);
    parse("<p>aa</p>");
    find.refresh(d, layout);
    assert(find.count() == 1 && find.ordinal() == 1 && find.offset() == 0);
    parse("<p>Nothing</p>");
    find.refresh(d, layout);
    assert(find.active() && find.count() == 0 && find.ordinal() == 0);
    parse("<p>aa again</p>");
    find.refresh(d, layout);
    assert(find.count() == 1 && find.ordinal() == 1 && find.offset() == 0);
    layout.clear();
    find.refresh(d, layout);
    assert(find.count() == 0 && find.offset() == std::string::npos);
    find.clear();
    assert(!find.active() && find.query().empty() && find.count() == 0);

    d.clear("https://find.test/");
    d.append(std::string(max_text, 'x'));
    layout.build(d, 64, 24);
    assert(layout.truncated);
    assert(find.start(d, layout, "x"));
    const auto &last = layout.pieces.back();
    assert(find.count() == last.offset + last.size && find.count() < max_text);
    assert(find.start(d, layout, std::string(PageFind::max_query, 'x')));
    assert(find.count() == (last.offset + last.size) / PageFind::max_query);

    std::mt19937 random(71);
    for (unsigned round = 0; round < 200; ++round) {
        std::string text, query;
        for (unsigned i = 0; i < 1000; ++i) text += "aAbBcC"[random() % 6];
        for (unsigned i = 0; i < 1 + round % 8; ++i) query += "aAbBcC"[random() % 6];
        d.clear("https://find.test/"); d.append(text); layout.build(d, 300, 15);
        assert(find.start(d, layout, query));
        const auto folded = lowercase(text), needle = lowercase(query);
        std::vector<std::size_t> matches;
        for (auto at = folded.find(needle); at != folded.npos; at = folded.find(needle, at + needle.size()))
            matches.push_back(at);
        assert(find.count() == matches.size());
        for (std::size_t i = 0; i < matches.size(); ++i) {
            assert(find.offset() == matches[i] && find.ordinal() == i + 1);
            find.move(d, layout, false);
        }
        if (!matches.empty()) assert(find.offset() == matches.front());
    }
    std::cout << "page find PASS: Chinese, ASCII case, wraps, inline spans, privacy, limits, updates, randomized navigation\n";
}
