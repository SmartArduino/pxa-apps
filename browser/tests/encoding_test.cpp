#include "../browser_encoding.hpp"
#include <cassert>
#include <fstream>
#include <iterator>
#include <iostream>
using namespace browser;
int main(int argc, char **argv) {
    assert(argc == 2);
    std::ifstream input(argv[1], std::ios::binary);
    std::string raw{std::istreambuf_iterator<char>(input), {}};
    assert(raw.size() == gb_map_bytes);
    auto map = std::as_bytes(std::span{raw});
    auto decode = [&](std::string_view bytes, std::size_t chunk) {
        Gb18030Decoder d(map); std::string output;
        for (std::size_t at = 0; at < bytes.size(); at += chunk)
            d.feed(bytes.substr(at, chunk), output);
        d.finish(output); return output;
    };
    // Known text independently encoded as GB2312, and extended GBK/GB18030.
    const auto text = "网址导航 · 当前位置：手机漫画";
    const std::string gb = "\xcd\xf8\xd6\xb7\xb5\xbc\xba\xbd \xa1\xa4 \xb5\xb1\xc7\xb0\xce\xbb\xd6\xc3\xa3\xba\xca\xd6\xbb\xfa\xc2\xfe\xbb\xad";
    for (std::size_t n = 1; n < gb.size()+2; ++n) assert(decode(gb,n) == text);
    assert(decode("\x81\x40",1) == "丂");
    assert(decode("\x80",1) == "€");
    assert(decode("\x90\x30\x81\x30",1) == "𐀀");
    assert(decode("\xe3\x32\x9a\x35",1) == "\xf4\x8f\xbf\xbf");
    assert(decode("\x81\x35\xf4\x37",1) == "\xee\x9f\x87"); // pointer 7457
    assert(decode("\x81\"<p>ok",1) == "�\"<p>ok");
    assert(decode("\x81\x30\x81\"",1) == "�0�\"");
    assert(decode("\x81\x30\"",1) == "�0\"");
    assert(decode("\x81\x30\x81",1) == "�");
    assert(decode("\xff",1) == "�");
    assert(sniff_encoding("text/html", "<META charset=GB2312>", true) == TextEncoding::gb18030);
    assert(sniff_encoding("text/html", "<meta http-equiv=Content-Type content='text/html; charset = \"gbk\"'>", true) == TextEncoding::gb18030);
    assert(sniff_encoding("text/html; charset='utf-8'", "<meta charset=gbk>", true) == TextEncoding::utf8);
    assert(sniff_encoding("text/html; charset=big5", "\xef\xbb\xbf", true) == TextEncoding::utf8);
    assert(sniff_encoding("text/html", "<!--<meta charset=gbk>--><meta charset=utf8>", true) == TextEncoding::utf8);
    assert(sniff_encoding("text/html", "<script>var s='<meta charset=gbk>';</script><meta charset=utf8>", true) == TextEncoding::utf8);
    assert(sniff_encoding("text/plain", "<meta charset=gbk>", false) == TextEncoding::utf8);
    assert(sniff_encoding("text/html", "<meta charset=big5>", true) == TextEncoding::unsupported);
    std::string late(1024,' ');late += "<meta charset=gbk>";
    assert(sniff_encoding("text/html", late, true) == TextEncoding::utf8);
    // Every two-byte combination: chunk boundaries do not change decoding or
    // accidentally consume following markup. ASan checks all table accesses.
    for (unsigned first=0; first<256; ++first)
        for (unsigned second=0; second<256; ++second) {
            std::string bytes{char(first),char(second)};bytes += "<a>";
            assert(decode(bytes,1) == decode(bytes,bytes.size()));
        }
    Document doc; doc.clear("https://example.org/"); Parser parser(doc);
    const std::string html = "<div style='width:400px; display: none'><div>正在加载,请稍候...</div><input name=x><script>\"</div>\"</script></div><p>正文</p><span hidden>隐藏</span><img hidden src='/bad'><a href='/ok'>继续</a>";
    for (char c : html) parser.feed({&c,1});
    parser.finish();
    assert(doc.text.find("正在加载") == doc.text.npos && doc.text.find("隐藏") == doc.text.npos);
    assert(doc.text.find("正文") != doc.text.npos && doc.text.find("继续") != doc.text.npos);
    assert(doc.images.empty() && doc.fields.empty() && doc.links.size()==1);
    std::cout << "encoding and hidden-content tests passed\n";
}
