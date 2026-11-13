#include "../browser_core.hpp"
#include "../browser_search.hpp"
#include <cassert>
#include <iostream>
using namespace browser;

static Document parse(std::string_view html, std::size_t chunk = 4096, bool plain = false) {
    Document d; d.clear("https://compat.test/original/page");
    Parser parser(d);
    for (std::size_t at = 0; at < html.size(); at += chunk) {
        const auto part = html.substr(at, chunk);
        if (plain) parser.feed_plain(part); else parser.feed(part);
    }
    parser.finish(); return d;
}
static void same(const Document &a, const Document &b) {
    assert(a.text == b.text && a.title == b.title && a.base == b.base);
    assert(a.truncated == b.truncated && a.spans.size() == b.spans.size());
    assert(a.links.size() == b.links.size() && a.fields.size() == b.fields.size());
    for (std::size_t i = 0; i < a.spans.size(); ++i) {
        const auto &x = a.spans[i], &y = b.spans[i];
        assert(x.offset == y.offset && x.size == y.size && x.link == y.link &&
               x.field == y.field && x.style == y.style && x.image == y.image && x.text_style == y.text_style);
    }
}
int main() {
    const std::string raw = "<head><title>Title 中文</title><style>discard </stylex> and <x>"
        "</STYLE data-ignored='a>b'><script>discard </scripture> and <x>"
        "</ScRiPt ignored=\"" + std::string(4000, 'x') + ">\"><body><p>正文 &ldquo;中文&rdquo; &times; 2</p>";
    for (std::size_t chunk : {1, 2, 3, 5, 17, 64, 2048, 4096, 16384}) {
        const auto d = parse(raw, chunk);
        assert(d.text == "正文 “中文” × 2\n");
        same(d, parse(raw, 1));
    }
    const std::string comments = "<!--" + std::string(10000, 'x') + "----><p>after</p><!--end";
    for (std::size_t chunk : {1, 2, 3, 7, 4096}) assert(parse(comments, chunk).text == "after\n");
    for (auto html : {"<style>not closed<p>not content", "<script>not closed<p>not content"})
        assert(parse(html).text.empty());
    assert(parse("<template><p>hidden</p><template>nested</template>hidden</template><p>visible</p>"
                 "<dialog>closed</dialog><dialog open>open</dialog>").text == "visible\nopen\n");
    assert(parse("<head><base target='_blank'><base href='../first/'><base href='/wrong/'></head>"
                 "<a href='next'>next</a>").links[0].target == "https://compat.test/first/next");
    assert(parse("<head><base href='javascript:bad'><base href='/wrong/'></head>"
                 "<a href='next'>next</a>").links[0].target == "https://compat.test/original/next");
    assert(entities("&lsquo;&rsquo;&middot;&bull;&divide;&deg;&reg;&trade;&euro;") == "‘’·•÷°®™€");
    auto icons = parse("<img src='/icon.png' width=25 height=25>"
                       "<img src='/portrait.jpg' width=130 height=242>"
                       "<img src='/auto.jpg'><img src='/height.jpg' height=30>");
    assert(icons.images.size() == 4);
    icons.images[0].intrinsic_size(200, 200);
    icons.images[1].intrinsic_size(250, 465);
    icons.images[2].intrinsic_size(96, 1536);
    icons.images[3].intrinsic_size(200, 100);
    assert(icons.images[0].width == 25 && icons.images[0].height == 25);
    assert(icons.images[1].width == 130 && icons.images[1].height == 241);
    assert(icons.images[2].width == 96 && icons.images[2].height == 1536);
    assert(icons.images[3].width == 60 && icons.images[3].height == 30);

    std::string article;
    for (unsigned i = 0; i < 1500; ++i)
        article += "<p>Chinese 中文阅读 paragraph <a href='/next'>next</a> tail &amp; end</p>";
    const auto reference = parse(article, 1);
    for (std::size_t chunk : {2, 3, 7, 128, 4096, 65536}) same(reference, parse(article, chunk));
    for (bool plain : {false, true}) {
        std::string text;
        for (unsigned i = 0; i < 5000; ++i) text += "A中文\xff<> &amp; \t\n";
        const auto expected = parse(text, 1, plain);
        for (std::size_t chunk : {2, 3, 7, 4096, 65536}) same(expected, parse(text, chunk, plain));
        assert(expected.text.size() <= max_text);
    }

    auto form = parse("<form action='/echo' method='post'>"
        "<input type='radio' name='mode' value='old' checked>"
        "<input type='radio' name='mode' value='new' checked aria-label='New mode'>"
        "<input type='radio' name='other' checked>"
        "<input type='checkbox' name='empty' value='' checked>"
        "<input type='file' name='upload' disabled>"
        "<select disabled><option>not submitted</option></select>"
        "<textarea disabled>not submitted</textarea>"
        "<button name='go' value='yes'>Send</button></form>"
        "<form><input type='radio' name='mode' checked></form>");
    assert(form.forms.size() == 2 && form.forms[0].supported && form.fields.size() == 6);
    assert(!form.fields[0].checked && form.fields[1].checked && form.fields[5].checked);
    assert(form.fields[1].label == "New mode" && form.fields[2].value == "on");
    assert(submit(form, 0, 4)->body == "mode=new&other=on&empty=&go=yes");
    check_field(form, 0, true);
    assert(form.fields[0].checked && !form.fields[1].checked && form.fields[5].checked);
    assert(submit(form, 0, 4)->body == "mode=old&other=on&empty=&go=yes");
    check_field(form, 0, true);
    assert(form.fields[0].checked); // a second tap does not deselect a radio
    check_field(form, 3, false);
    assert(submit(form, 0, 4)->body == "mode=old&other=on&go=yes");
    auto unsupported = parse("<form><input type=file><input type=submit></form>");
    assert(!submit(unsupported, 0, 0));
    auto disabled = parse("<form>" + std::string(100, ' ') +
        "<input disabled type=file><input type=checkbox name=q checked value=''><input type=submit></form>");
    assert(submit(disabled, 0, 1)->url == "https://compat.test/original/page?q=");
    const std::string atom = R"XML(<feed><entry xml:lang='zh'>
        <title type='html'>&lt;b&gt;中文标题&lt;/b&gt;</title>
        <link rel='self' href='/not-the-article'/>
        <link title='a>b' rel='alternate' href='/article?a=1&amp;b=2'/>
        <summary type='html'>&lt;p&gt;正文&lt;/p&gt;</summary></entry>
        <entry><title>Next</title><link href='/next'/><content><![CDATA[<p>literal </entry> end</p>]]></content></entry>
        <entry/></feed>)XML";
    for (std::size_t chunk : {1, 2, 3, 7, 32, 4096}) {
        Document d;d.clear("https://compat.test/feed");RssParser feed(d);
        for (std::size_t at = 0; at < atom.size(); at += chunk) feed.feed(std::string_view(atom).substr(at, chunk));
        feed.finish();
        assert(d.links.size() == 2 && d.links[0].target == "https://compat.test/article?a=1&b=2");
        assert(d.links[1].target == "https://compat.test/next");
        assert(d.text == "中文标题\n正文\nNext\nliteral end\n");
    }
    const std::string oversized = "<feed><entry><title>Skip</title><content>" + std::string(10000, 'x') +
        "</content></entry><entry><title>Kept</title><link href='/kept'/></entry></feed>";
    Document bounded_feed;bounded_feed.clear("https://compat.test/");RssParser rss(bounded_feed);
    rss.feed(oversized);rss.finish();
    assert(bounded_feed.links.size() == 1 && bounded_feed.text == "Kept\n");
    std::cout << "compatibility PASS: bulk/scalar equivalence, UTF-8 boundaries, quotas, raw ends, base, templates, entities, radio forms\n";
}
