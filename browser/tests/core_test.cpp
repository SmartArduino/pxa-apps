#include "../browser_core.hpp"
#include "../browser_search.hpp"
#include "../browser_image_view.hpp"
#include "../browser_geometry.hpp"
#include "../browser_scrollbar.hpp"
#include "../browser_cookies.hpp"
#include "../browser_site_forms.hpp"
#include "../browser_sites.hpp"
#include "../browser_memory.hpp"
#include <pxa/ui_resource_pressure.hpp>
#include <cassert>
#include <iostream>
#include <random>
using namespace browser;
int main() {
    {
        using enum ReferrerPolicy;
        const auto page = "https://EXAMPLE.test:443/chapter?key=x#private";
        assert(referrer_policy("origin, unknown, NO-REFERRER") == no_referrer);
        assert(!referrer_policy("unknown"));
        assert(request_referrer(page,"https://cdn.test/a") == "https://example.test/");
        assert(request_referrer(page,"https://example.test/b") ==
               "https://example.test:443/chapter?key=x");
        assert(request_referrer(page,"https://cdn.test/a",no_referrer_when_downgrade) ==
               "https://example.test:443/chapter?key=x");
        for (auto policy : {no_referrer_when_downgrade,strict_origin,strict_origin_when_cross_origin})
            assert(request_referrer(page,"http://cdn.test/a",policy).empty());
        assert(request_referrer(page,"https://cdn.test/a",same_origin).empty());
        assert(request_referrer(page,"https://example.test/a",origin) == "https://example.test/");
        assert(request_referrer(page,"http://cdn.test/a",unsafe_url) ==
               "https://example.test:443/chapter?key=x");
        assert(request_referrer("https://user:pass@example.test/a","https://cdn.test/a").empty());
        assert(request_referrer("about:home","https://cdn.test/a").empty());
        Document d;d.clear(page);Parser p(d);
        p.feed("<head><meta name=referrer content=no-referrer-when-downgrade></head>"
               "<img src='https://cdn.test/p.jpg' referrerpolicy=no-referrer>"
               "<img src='https://cdn.test/q.jpg'>");p.finish();
        assert(d.referrer == no_referrer_when_downgrade && d.images.size() == 2);
        assert(d.images[0].referrer == no_referrer && !d.images[1].referrer);
        d.clear(page);assert(d.referrer == strict_origin_when_cross_origin);
    }
    {
        assert(memory::budget(0)==1024*1024 && memory::budget(5)==4*1024*1024);
        assert(memory::budget(99)==memory::budget(0));
        assert(memory::budget(5,1)==256*1024 && memory::budget(3,2)==256*1024);
        std::array<std::byte,4> bytes{std::byte{1}};
        pxa::Event event{3,0x8003,0,bytes};
        assert(pxa::ui::decode_resource_pressure(event)==pxa::ui::ResourcePressure::constrained);
        bytes[1]=std::byte{1};assert(!pxa::ui::decode_resource_pressure(event));bytes[1]=std::byte{};
        bytes[0]=std::byte{3};assert(!pxa::ui::decode_resource_pressure(event));
        event.service=2;assert(!pxa::ui::decode_resource_pressure(event));
    }
    {
        using namespace browser::inline_image;
        auto p=payload("data:image/png;base64,iVBORw0KGgo=");assert(p);
        std::array<std::byte,12> output{};assert(decode(*p,output)==8);
        assert(output[0]==std::byte{0x89} && output[1]==std::byte{'P'});
        for(auto uri:{"data:image/svg+xml;base64,PHN2Zz4=","data:text/html;base64,AAAA","data:image/png;base64,A===","data:image/png;base64,AB==","data:image/png;base64,AAAA=","data:image/png;base64,AA A","data:image/png;base64,"}) assert(!payload(uri));
        Document d;d.clear("https://example.test/");Parser parser(d);
        const std::string large="data:image/png;base64,"+std::string(3000,'A');
        for(int i=0;i<8;++i)parser.feed("<img src='"+large+"' width=180 height=180>");
        parser.finish();assert(d.images.size()==2);
        assert(!resolve(d.base,"data:image/png;base64,AAAA"));
    }
    {
        assert(sites.size() == 8);
        assert(site_user_agent("https://ink.qq.com/", "default").find("Kindle/3.0+") != std::string_view::npos);
        assert(site_user_agent("https://weread.qq.com:443/wrwebsimplenjlogic/login", "default").find("Kindle/3.0+") != std::string_view::npos);
        for (auto url : {"https://weread.qq.com.evil.test/", "https://evil.test/?weread.qq.com", "about:home"})
            assert(site_user_agent(url, "default") == "default");
    }
    {
        Document d; d.clear("https://example.org/"); Parser p(d);
        for (int i=0; i<200; ++i) p.feed("<a href='/book'>书封面</a><a href='/book'>书名</a>");
        p.finish();
        assert(d.links.size() == 1 && !d.links_limited);
        for (std::size_t i=1; i<max_links; ++i) assert(d.add_link(std::to_string(i)) == int(i));
        assert(d.add_link("over-limit") == -1 && d.links_limited);
        assert(d.add_link("https://example.org/book") == 0);
        d.clear("here"); assert(!d.links_limited);
    }
    {
        assert(static_navigation("window.location.href='/book/1.html'") == "/book/1.html");
        assert(static_navigation(" location.href = \"/book/2\" ; ") == "/book/2");
        assert(!static_navigation("location.href=getURL()"));
        assert(!static_navigation("location.href='/ok';doOtherThing()"));
        assert(!static_navigation("location.href='/ok\\x';"));
    }
    {
        Cookies jar;
        jar.receive("https://example.org/a/login", "sid=a; Path=/; Secure; HttpOnly", 100, 1792038000);
        jar.receive("https://example.org/a/login", "sid=b; Path=/a; Max-Age=30", 100, 1792038000);
        assert(jar.request("https://example.org/a/page", 100) == "sid=b; sid=a");
        assert(jar.request("http://example.org/a/page", 100) == "sid=b");
        assert(jar.request("https://evil.org/a", 100).empty());
        assert(jar.request("https://sub.example.org/a", 100).empty());
        assert(jar.request("https://example.org/ab", 100) == "sid=a");
        jar.receive("https://example.org/", "steal=x; Domain=evil.org", 100, 1792038000);
        jar.receive("http://example.org/", "sid=bad; Path=/", 100, 1792038000);
        assert(jar.request("https://example.org/a", 130) == "sid=a");
        assert(jar.size() == 1);
        jar.receive("https://example.org/", "old=x; Expires=Wed, 09 Jun 2021 10:18:14 GMT", 100, 1792038000);
        assert(jar.size() == 1);
        jar.receive("https://example.org/", "ok=x; Expires=Tue, 19 Jan 2038 03:14:07 GMT", 100, 1792038000);
        assert(jar.size() == 2);
        jar.receive("https://example.org/", "ok=; Max-Age=0", 100, 1792038000);
        jar.receive("http://example.org/", "__Secure-a=x; Secure", 100, 1792038000);
        jar.receive("https://example.org/", "__Host-a=x; Secure; Path=/; Domain=example.org", 100, 1792038000);
        assert(jar.size() == 1);
        jar.receive("https://example.org/", "bad=x\r\nInjected: 1", 100, 1792038000);
        for (int i=0; i<40; ++i) jar.receive("https://example.org/", "name"+std::to_string(i)+"=value", 100, 1792038000);
        assert(jar.size() == 16 && jar.bytes() <= 4096);
        jar.clear(); assert(jar.size() == 0 && jar.request("https://example.org/", 100).empty());
    }
    {
        Document d; d.clear("https://example.org/login"); Parser p(d);
        p.feed("<form method=post><input name=user><input type=password name=p><input type=submit value=Login></form>"); p.finish();
        assert(d.forms.size() == 1 && d.forms[0].supported && d.fields.size() == 3);
        d.fields[0].value = "tester"; d.fields[1].value = "a&= +\"b";
        auto sent = submit(d, 0, 2); assert(sent && sent->post && sent->body == "user=tester&p=a%26%3D+%2B%22b");
        assert(d.text.find(d.fields[1].value) == d.text.npos);
        d.forms[0].post = false; assert(!submit(d, 0, 2));
        d.clear("http://wap.example.org/"); Parser w(d);
        w.feed("<wml><card id=main title=Tools><input name='q'/><anchor>Search<go href='/find?q=$(q)'/></anchor></card></wml>"); w.finish();
        assert(d.wml && d.fields.size() == 1 && d.links.size() == 1);
        d.fields[0].value = "a & b";
        assert(wml_target(d, d.links[0].target) == "http://wap.example.org/find?q=a%20%26%20b");
        assert(!wml_target(d, "/find?q=$(missing)"));
        History h; for(int i=0;i<40;++i) h.visit(std::to_string(i));
        assert(h.entries().size() == 24 && h.entries()[0].url == "16");
        assert(h.select(0)->url == "16" && !h.back() && h.forward());
        assert(!h.select(24)); h.clear("here", 12); assert(h.entries().size()==1 && h.entries()[0].scroll==12);
        d.clear("http://www.bookshuku.org/"); site_forms(d, d.base);
        assert(d.forms.size()==1 && d.fields.size()==2 && d.links.size()==1);
        d.fields[0].value="书"; assert(submit(d,0,1)->body=="searchkey=%E4%B9%A6");
    }

    {
        for (int height : {1, 20, 120, 480})
            for (int total : {0, 20, 120, 480, 1000000}) {
                Scrollbar bar{35, height, total, 0, 20};
                assert(bar.thumb_height() <= height && bar.thumb_height() > 0);
                assert(bar.thumb_top() == 35);
                assert(bar.position_at(INT32_MIN, 0) == 0);
                if (height > bar.thumb_height()) {
                    assert(bar.position_at(INT32_MAX, 0) == bar.range());
                    bar.position = bar.range();
                    assert(bar.thumb_top() + bar.thumb_height() == bar.top + height);
                    assert(bar.position_at(bar.thumb_top() + 3, 3) == bar.range());
                    int last = 0;
                    for (int y = bar.top; y <= bar.top + height; ++y) {
                        int next = bar.position_at(y, 3);
                        assert(next >= last && next <= bar.range()); last = next;
                    }
                }
            }
    }
    {
        pxa::ui::DisplayMetrics display;
        display.shape = 1;
        display.corners = {58, 58, 58, 58};
        display.safe = {10, 8, 10, 8};
        auto boxes = viewport(display, {26, 0, 0, 20});
        assert(boxes.content.x == 30 && boxes.content.width == 252 && boxes.content.height == 138);
        assert(boxes.address.y == 10 && boxes.toolbar.y == 186);
        // Every corner of every band remains within the real panel outline.
        // Include asymmetric corners, fractional DPI, circles and letterboxing.
        for (auto [w, h] : {std::pair{296u, 240u}, {240u, 240u}, {480u, 480u},
                            {320u, 240u}, {240u, 320u}})
            for (unsigned shape : {0u, 1u, 2u})
                for (unsigned density : {53248u, 65536u, 131072u}) {
                    display.width = w; display.height = h; display.shape = shape;
                    display.density_q16 = density;
                    display.corners = {58, 31, 64, 20};
                    boxes = viewport(display, {16, 0, 0, 20});
                    for (auto box : {boxes.address, boxes.content, boxes.toolbar, boxes.status}) {
                        assert(box.width >= 0 && box.height >= 0);
                        assert(box.x >= 16 && box.x + box.width <= int(w) - 10);
                        assert(box.y >= 8 && box.y + box.height <= int(h) - 20);
                        for (int x : {box.x, box.x + box.width})
                            for (int y : {box.y, box.y + box.height}) {
                                if (shape == 2) {
                                    const double dx = x - w/2., dy = y - h/2.;
                                    assert(dx*dx + dy*dy <= std::min(w,h)*std::min(w,h)/4.);
                                } else if (shape == 1) {
                                    for (unsigned corner = 0; corner < 4; ++corner) {
                                        const int radius = std::min(display.corners[corner], std::min(w,h)/2);
                                        const bool right = corner == 1 || corner == 2;
                                        const bool bottom = corner >= 2;
                                        const int dx = radius - (right ? int(w) - x : x);
                                        const int dy = radius - (bottom ? int(h) - y : y);
                                        if (dx > 0 && dy > 0) assert(dx*dx + dy*dy <= radius*radius);
                                    }
                                }
                            }
                    }
                    for (auto box : {boxes.address, boxes.toolbar})
                        if (box.width) assert(box.x == int(w) - box.x - box.width);
                    auto occupied_display = display;
                    occupied_display.safe.left = std::max(16u, display.safe.left);
                    occupied_display.safe.bottom = std::max(20u, display.safe.bottom);
                    const auto safe_content = horizontal_band(occupied_display, boxes.content.y, boxes.content.height);
                    const int padding = std::max(1, int(std::int64_t(4) * density / 65536));
                    assert(boxes.content.width == std::max(0, safe_content.width - 2 * padding));
                    assert(boxes.address.y + boxes.address.height <= boxes.content.y);
                    assert(boxes.content.y + boxes.content.height <= boxes.toolbar.y);
                    assert(boxes.status.y >= boxes.content.y);
                    if (boxes.status.y < boxes.toolbar.y + boxes.toolbar.height)
                        assert(boxes.status.y + boxes.status.height <= boxes.content.y + boxes.content.height);
                }
        display.safe = {UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX};
        boxes = viewport(display, {});
        assert(!boxes.content.width && !boxes.content.height);
        display.width = display.height = 0;
        boxes = viewport(display, {});
        assert(!boxes.content.width && !boxes.content.height);
    }
    {
        ImageView view;
        assert(!view.active());
        view.open(3);
        assert(view.active());
        assert((view.rectangle(320, 180, 240, 120) == std::array{13, 0, 213, 120}));
        assert(!view.pan(100, -100, 320, 180, 240, 120));
        view.resize(1, 320, 180, 240, 120);
        assert(view.zoom == 2 && view.pan(-10000, 10000, 320, 180, 240, 120));
        assert((view.rectangle(320, 180, 240, 120) == std::array{-186, 0, 426, 240}));
        for (int i = 0; i < 10; ++i) view.resize(1, 320, 180, 240, 120);
        assert(view.zoom == 8);
        view.fit();
        assert(view.zoom == 1 && !view.x && !view.y);
        assert((view.rectangle(320, 900, 240, 120) == std::array{99, 0, 42, 120}));
        assert((view.rectangle(0, 0, 0, 0) == std::array{0, 0, 1, 1}));
        for (int w : {1, 96, 240, 480, 640, 4096})
            for (int h : {1, 180, 900, 4096})
                for (int zoom : {1, 2, 4, 8}) {
                    view = {0, zoom, 100000, -100000};
                    view.clamp(w, h, 240, 120);
                    auto box = view.rectangle(w, h, 240, 120);
                    assert(box[2] > 0 && box[3] > 0);
                    if (box[2] >= 240) assert(box[0] <= 0 && box[0] + box[2] >= 240);
                    if (box[3] >= 120) assert(box[1] <= 0 && box[1] + box[3] >= 120);
                }
    }
    assert(resolve("https://a.test/dir/page?q=old", "../next?x=1#ok") ==
           "https://a.test/next?x=1#ok");
    assert(resolve("https://a.test/", "?q=中文") == "https://a.test/?q=%E4%B8%AD%E6%96%87");
    assert(resolve("https://a.test/", "EXAMPLE.com", true) == "https://example.com/");
    assert(resolve("https://a.test", "next") == "https://a.test/next");
    assert(resolve("https://a.test/", "/a//b") == "https://a.test/a//b");
    assert(address_target("esp32 中文", SearchEngine::bing) ==
           "https://www.bing.com/search?format=rss&q=esp32+%E4%B8%AD%E6%96%87");
    assert(address_target("example.com", SearchEngine::bing) == "https://example.com/");
    assert(address_target("?example.com", SearchEngine::duckduckgo) ==
           "https://lite.duckduckgo.com/lite/?q=example.com");
    assert(!resolve("https://a.test/", "javascript:alert(1)"));
    assert(!resolve("https://a.test/", "http://name:pass@host/"));
    assert(!resolve("https://a.test/", "http://host/\nInjected"));
    assert(entities("&amp; &#x4e2d;&#25991; &unknown;") == "& 中文 &unknown;");
    static_assert(fragment_matches("目录", "%E7%9B%AE%e5%bd%95"));
    static_assert(fragment_matches("目錄", "%E7%9B%AE%E9%8C%84"));
    assert(fragment_matches("目录", "目录"));
    assert(fragment_matches("a b+c", "a%20b%2Bc"));
    assert(fragment_matches("a+b", "a+b"));
    assert(!fragment_matches("a b", "a+b"));
    assert(fragment_matches("%20", "%2520"));
    assert(fragment_matches("%x%1", "%x%1"));
    assert(fragment_matches("", "") && !fragment_matches("", "%00"));
    assert(!fragment_matches("目录", "%E7%9B%AE") && !fragment_matches("a", "ab"));
    const std::string html =
        "<!doctype html><head><title>PXA 浏览器</title><style>bad</style></head><body><h1>你好 "
        "&amp; Hello</h1><p>Before <a href='../next'>中文 link</a> after.</p><!-- invisible "
        "--><script>bad()</script><img alt='海边'><form action='/search' method='post'><input "
        "name='q' placeholder='关键词'><input type='checkbox' name='ok' checked><input "
        "type='hidden' name='token' value='x&amp;y'><button name='go' "
        "value='1'>发送</button></form></body>";
    Document whole;
    whole.clear("https://a.test/dir/page");
    Parser p(whole);
    p.feed(html);
    p.finish();
    assert(whole.text.find("invisible") == std::string::npos);
    assert(whole.text.find("bad") == std::string::npos);
    assert(whole.links.size() == 1 && whole.links[0].target == "https://a.test/next");
    assert(whole.title == "PXA 浏览器");
    assert(whole.fields.size() == 4);
    static_assert(sizeof(Span)==16 && sizeof(Piece)==16);
    Document pictures;
    pictures.clear("https://a.test/dir/page");
    Parser image_parser(pictures);
    image_parser.feed("<a href='/full'><img src='../photo.png' alt='山水' width='320' height='180'></a>"
                      "<p>after</p><img data-src='/lazy.jpg'><img src='/pixel' width='1' height='1'>"
                      "<img src='data:image/png,not-supported' alt='替代文字'>");
    image_parser.finish();
    assert(pictures.images.size()==2 && pictures.images[0].source=="https://a.test/photo.png");
    assert(pictures.images[1].source=="https://a.test/lazy.jpg");
    assert(pictures.images[0].width==320 && pictures.images[0].height==180);
    assert(pictures.text.find("替代文字")!=std::string::npos);
    assert(std::ranges::count_if(pictures.spans,[](auto span){return span.image>=0;})==2);
    Layout image_layout;image_layout.build(pictures,200,15);
    assert((Layout::image_size(pictures.images[0],200,15)==std::pair{200,112}));
    assert(image_layout.height>=262 && !image_layout.truncated);
    for(std::size_t i=0;i<max_images+8;++i) image_parser.feed("<img src='/same.png'>");
    image_parser.finish();
    assert(pictures.images.size()==max_images);
    pictures.clear("https://a.test/");assert(pictures.images.empty());
    whole.fields[0].value = "中文 & a";
    auto post = submit(whole, 0, 3);
    assert(post && post->post && post->url == "https://a.test/search");
    assert(post->body == "q=%E4%B8%AD%E6%96%87+%26+a&ok=on&token=x%26y&go=1");
    for (std::size_t chunk = 1; chunk <= html.size(); ++chunk) {
        Document d;
        d.clear("https://a.test/dir/page");
        Parser parser(d);
        for (std::size_t i = 0; i < html.size(); i += chunk)
            parser.feed(std::string_view(html).substr(i, chunk));
        parser.finish();
        assert(d.text == whole.text && d.title == whole.title && d.links.size() == 1 &&
               d.spans.size() == whole.spans.size());
    }
    Document large;
    large.clear("https://a.test/");
    Parser lp(large);
    for (int i = 0; i < 10000; ++i)
        lp.feed("<p>hello 世界 <a href='/a'>link</a></p>");
    lp.finish();
    assert(large.truncated && large.text.size() <= max_text && large.links.size() <= max_links &&
           large.spans.size() <= max_spans);
    std::string capacity_reference;
    capacity_reference.reserve(max_text);
    for (std::size_t split : {17u, 16384u, 27000u, 40000u}) {
        Document bounded;
        bounded.append(std::string(split, 'a'));
        bounded.append(std::string(max_text-split, 'b'));
        assert(bounded.text.size()==max_text && !bounded.truncated);
        assert(bounded.text.capacity()<=capacity_reference.capacity());
        assert(bounded.text.substr(split)==std::string(max_text-split,'b'));
        bounded.append("x");assert(bounded.truncated && bounded.text.size()==max_text);
    }
    Document aliased;
    aliased.append(std::string(max_text/2+1,'x'));
    aliased.append(std::string_view(aliased.text).substr(0,max_text-aliased.text.size()));
    assert(aliased.text==std::string(max_text,'x'));
    assert(aliased.text.capacity()<=capacity_reference.capacity());
    Layout layout;
    assert(user_agent(false).find("TextMode") != std::string_view::npos);
    assert(user_agent(true).find("Mobile Safari/") != std::string_view::npos);
    Document words;
    words.append("alpha beta gamma");
    layout.build(words, 60, 10);
    assert(layout.pieces.size() == 3);
    for (std::size_t i = 0; i < 3; ++i) {
        const auto &piece = layout.pieces[i];
        assert((words.text.substr(piece.offset, piece.size) ==
                std::array<std::string_view, 3>{"alpha ", "beta ", "gamma"}[i]));
        assert(piece.width <= 60 && piece.x == 0);
    }
    words.text.clear(); words.spans.clear();
    words.append("abcdefghijklmnopqrstuvwxyz");
    layout.build(words, 25, 10);
    std::size_t covered = 0;
    for (const auto &piece : layout.pieces) {
        assert(piece.width <= 25 && piece.offset == covered);
        covered += piece.size;
    }
    assert(covered == words.text.size());
    assert(line_prefix("中文abc", 20, 10) == "中文");
    assert(line_prefix("中文abc", 19, 10) == "中");
    assert(line_prefix("abc\ndef", 100, 10) == "abc");
    assert(line_prefix(std::string(1024, 'W'), 100, 10).size() == 10);
    assert(line_prefix("中文", 0, 10).empty());
    layout.build(whole, 96, 16);
    assert(layout.height > 16 && !layout.truncated);
    for (auto &piece : layout.pieces) {
        assert(piece.offset + piece.size <= whole.text.size());
        assert(piece.x + piece.width <= 96);
        assert(piece.span < whole.spans.size());
    }
    History history;
    history.visit("a");
    history.position(42);
    history.visit("b");
    assert(history.move(-1)->scroll == 42);
    history.visit("c");
    assert(!history.forward());
    const std::string rss =
        "<?xml version='1.0'?><rss><channel><title>ignored</title><item><title>中文 &amp; "
        "Hello</title><link>https://example.com/?a=1&amp;b=2</link><description>说明 "
        "&lt;b&gt;文字&lt;/b&gt;</description></item></channel></rss>";
    for (std::size_t chunk = 1; chunk <= rss.size(); ++chunk) {
        Document d;
        d.clear("https://www.bing.com/search");
        RssParser parser(d);
        for (std::size_t i = 0; i < rss.size(); i += chunk)
            parser.feed(std::string_view(rss).substr(i, chunk));
        parser.finish();
        assert(d.links.size() == 1 && d.text == "中文 & Hello\n说明 文字\n" &&
               d.links[0].target == "https://example.com/?a=1&b=2");
    }
    Document malformed;
    malformed.clear("https://a.test/");
    Parser bad(malformed);
    bad.feed("<script>if (a < b) { x=\"<z\"; }</script><p>2 < 3 &amp; 中文</p>");
    bad.finish();
    assert(malformed.text == "2 < 3 & 中文\n");
    std::mt19937 random(42);
    for (int n = 0; n < 1000; ++n) {
        Document d;
        d.clear("https://a.test/");
        Parser parser(d);
        std::string noise;
        for (int i = 0; i < 2048; ++i)
            noise += char(random() % 256);
        for (char c : noise)
            parser.feed({&c, 1});
        parser.finish();
        Layout l;
        l.build(d, 32, 12);
        assert(d.text.size() <= max_text && l.pieces.size() <= max_pieces);
    }
    std::cout << "browser core PASS: URL, entities, UTF-8 splits, HTML, forms, limits, layout, "
                 "history, randomized input\n";
}
