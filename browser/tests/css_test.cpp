#include "../browser_core.hpp"
#include <cassert>
#include <iostream>
using namespace browser;

static Document parse(std::string_view html, std::size_t chunk = 4096) {
    Document d; d.clear("https://css.test/article"); Parser parser(d);
    for (std::size_t at = 0; at < html.size(); at += chunk) parser.feed(html.substr(at,chunk));
    parser.finish(); return d;
}
static const Span &at(const Document &d, std::string_view word) {
    const auto p = d.text.find(word); assert(p != d.text.npos);
    for (const auto &s : d.spans) if (p >= s.offset && p < s.offset+s.size) return s;
    std::abort();
}
static void same(const Document &a, const Document &b) {
    assert(a.text == b.text && a.spans.size() == b.spans.size() && a.truncated == b.truncated);
    for (std::size_t i = 0; i < a.spans.size(); ++i) {
        const auto &s = a.spans[i], &t = b.spans[i];
        assert(s.offset == t.offset && s.size == t.size && s.style == t.style && s.text_style == t.text_style);
    }
}
int main() {
    assert(css::readable_color(0xffffffff) == 0x787878ff);
    assert(css::readable_color(0x2468acff) == 0x2468acff);
    const auto root = parse("<html id=root><head><style>html#root{color:#2468ac;font-size:120%}</style>"
                            "<style media=print>p{display:none}</style></head><body><p>Root inheritance</p></body></html>");
    assert(at(root,"Root").text_style.color == 0x2468acff && Layout::font(at(root,"Root"),15) == 18);
    const auto percent_heading = parse("<h1 style='font-size:120%'>Heading</h1>");
    assert(Layout::font(at(percent_heading,"Heading"),15) == 18);
    const std::string page = "<head><style>/*comment*/ p {color:blue} .note {color:green} "
        "p.note.hot {color:#123} #lead {color:#a02030; font-size:120%}"
        ".gone {display:none} .pre {white-space:pre-wrap} .lines{white-space:pre-line}"
        "@media print {p {color:red}} @supports(display:grid){.note{display:none}} "
        "p:hover {display:none} div p{display:none}"
        "</style></head><body><p id='lead' class='note hot'>Lead "
        "<span style='color:#2468ac;font-size:150%'>Nested</span> restored</p>"
        "<p class='note hot'>Compound</p><p style='color: purple'>Inline</p>"
        "<div class='gone'>Hidden<img src='/secret.png'><a href='/secret'>Link</a></div>"
        "<p class='pre'>  A\tB\r\nC  D</p><p class='lines'> E  F\n G  H</p><p>Tail</p></body>";
    const auto d = parse(page);
    assert(d.images.empty() && d.links.empty() && d.text.find("Hidden") == d.text.npos);
    assert(at(d,"Lead").text_style.color == 0xa02030ff && Layout::font(at(d,"Lead"),15) == 18);
    assert(at(d,"Nested").text_style.color == 0x2468acff && Layout::font(at(d,"Nested"),15) == 27);
    assert(at(d,"restored").text_style == at(d,"Lead").text_style);
    assert(at(d,"Compound").text_style.color == 0x112233ff);
    assert(at(d,"Inline").text_style.color == 0x800080ff);
    assert(at(d,"Tail").text_style.color == 0x0000ffff && Layout::font(at(d,"Tail"),15) == 15);
    assert(d.text.find("  A    B\nC  D") != d.text.npos);
    assert(d.text.find("E F\nG H") != d.text.npos);
    for (auto chunk : {1,2,3,5,17,64,2048,4096}) same(d,parse(page,chunk));

    const auto priority = parse("<style>.x {color:green!important;color:red} #id {color:blue}"
        ".x {color:purple} .hide{display:none;display:block}</style>"
        "<p id=id class=x style='color:red'>Sheet important</p>"
        "<p class=x style='color:red!important'>Inline important</p>"
        "<div class=hide>Shown</div><p style='display:none;display:block'>Also shown</p>");
    assert(at(priority,"Sheet").text_style.color == 0x008000ff);
    assert(at(priority,"Inline").text_style.color == 0xff0000ff);
    assert(priority.text.find("Shown") != priority.text.npos && priority.text.find("Also shown") != priority.text.npos);

    const auto malformed = parse("<head><title>Omitted head</title><h1>Heading <span>nested</span> still heading</h1>"
        "<p style='color:red'>One<span> child<p>Two</p>"
        "<div style='color:green'>Outer<span style='color:blue'>Inner</div>After");
    assert(malformed.title == "Omitted head");
    assert(Layout::font(at(malformed,"still"),16) == 20);
    assert(at(malformed,"One").text_style.color == 0xff0000ff && at(malformed,"Two").text_style.color == 0);
    assert(at(malformed,"Outer").text_style.color == 0x008000ff);
    assert(at(malformed,"Inner").text_style.color == 0x0000ffff && at(malformed,"After").text_style.color == 0);

    auto aligned = parse("<p style='text-align:center'>AB<a href='/x'>CD</a></p>"
        "<p style='text-align:right'>EF</p><p>GH</p>");
    Layout layout; layout.build(aligned,200,16);
    assert(layout.pieces.size() == 4);
    assert(layout.pieces[0].x == 76 && layout.pieces[1].x == 100);
    assert(layout.pieces[2].x == 176 && layout.pieces[3].x == 0);
    auto wrap = parse("<p style='text-align:right;font-size:30px'>AB CD EF</p><p>End</p>");
    layout.build(wrap,80,15);
    for (const auto &p : layout.pieces) assert(int(p.x)+p.width <= 80);

    std::string rules;
    for (unsigned i = 0; i < 1000; ++i) rules += ".n"+std::to_string(i)+"{color:red}";
    const auto capped = parse("<style>"+rules+"</style><p class=n0>First</p><p class=n999>Last</p>");
    assert(at(capped,"First").text_style.color == 0xff0000ff);
    assert(at(capped,"Last").text_style.color == 0 && !capped.truncated);
    const auto huge = parse("<style>"+std::string(1000000,'x')+"</style><p>Still readable</p>",7);
    assert(huge.text == "Still readable\n");
    const auto raw = parse("<style>.x{unknown:'<tag>;\\\"';color:red}</stylex>.y{color:blue}</style><p class=x>Raw</p>",1);
    assert(raw.text == "Raw\n");
    std::string deep;
    for (int i = 0; i < 100; ++i) deep += "<span style='font-size:200%'>";
    deep += "Deep";
    for (int i = 0; i < 100; ++i) deep += "</span>";
    deep += "Tail";
    const auto nested = parse(deep,3);
    assert(Layout::font(at(nested,"Deep"),15) == 30 && Layout::font(at(nested,"Tail"),15) == 15);
    const auto cr = parse("<pre>A\r\nB\rC\nD</pre>",1);
    assert(cr.text == "A\nB\nC\nD\n");
    std::cout << "CSS PASS: cascade, priority, nesting, hidden resources, whitespace, line alignment, streaming and budgets\n";
}
