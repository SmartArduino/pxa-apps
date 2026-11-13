// Streaming HTML text containers and an optional host-only title benchmark.
#ifdef BROWSER_BENCH_HEADER
#include BROWSER_BENCH_HEADER
#else
#include "../browser_core.hpp"
#endif
#include <cassert>
#include <ctime>
#include <iostream>
using namespace browser;
static Document parse(std::string_view source,std::size_t chunk=4096) {
    Document d;d.clear("https://example.test/");Parser p(d);
    for(std::size_t at=0;at<source.size();at+=chunk)p.feed(source.substr(at,chunk));
    p.finish();return d;
}
int main(int argc,char** argv) {
    if(argc==2 && std::string_view(argv[1])=="--measure") {
        const std::string html="<head><title>"+std::string(100000,'a')+"</title></head><p>After</p>";
        std::size_t buffers=0;const auto begin=std::clock();
        for(unsigned i=0;i<64;++i) {
            const auto d=parse(html);assert(d.title==std::string(256,'a') && d.text=="After\n" && d.images.empty());
            buffers=std::max(buffers,d.buffer_bytes());
        }
        std::cout<<"{\"pages\":64,\"input_bytes\":"<<html.size()<<",\"cpu_ms\":"
            <<1000.0*double(std::clock()-begin)/CLOCKS_PER_SEC<<",\"document_buffers\":"<<buffers<<"}\n";return 0;
    }
    for(std::size_t chunk:{1,2,7,64,4096}) {
        auto title=parse("<head><title>A<b>B</b> &amp; 中文 <img src=/ghost></title></head><p>After</p>",chunk);
        assert(title.title=="A<b>B</b> & 中文 <img src=/ghost>" && title.images.empty() && title.text=="After\n");
        auto area=parse("<textarea>\r\n  A<b>B</b> &amp;\n&lt;img src=/ghost&gt;\n</textarea><p>After</p>",chunk);
        assert(area.text=="  A<b>B</b> &\n<img src=/ghost>\nAfter\n" && area.images.empty());
        auto xmp=parse("<xmp>A<b>B</b>&amp;</xmp><p>After</p>",chunk);
        assert(xmp.text=="A<b>B</b>&amp;\nAfter\n" && xmp.links.empty());
        auto plain=parse("<plaintext>A<b>B</b>&amp;</plaintext><img src=/ghost>",chunk);
        assert(plain.text=="A<b>B</b>&amp;</plaintext><img src=/ghost>\n" && plain.images.empty());
        for(auto name:{"iframe","noembed","noframes"}) {
            auto d=parse("<"+std::string(name)+"><script>bad()</script><img src=/ghost><form><input name=bad>"
                         "</"+name+"><p>After</p>",chunk);
            assert(d.text=="After\n" && d.images.empty() && d.fields.empty() && d.forms.empty());
        }
        auto frame=parse("<iframe src=/frame><img src=/ghost></iframe><p>After</p>",chunk);
        assert(frame.links.size()==1 && frame.links[0].target=="https://example.test/frame" && frame.images.empty());
        assert(parse("<iframe src=/ignored srcdoc='text'>fallback</iframe><p>After</p>",chunk).links.empty());
        assert(parse("<textarea hidden>A<textarea>literal</textarea><p>Visible</p>",chunk).text=="Visible\n");
        assert(parse("<div hidden><textarea></div>literal</textarea></div><p>Visible</p>",chunk).text=="Visible\n");
        assert(parse("<textarea>A</textareax>B</TeXtArEa ignored='a>b'><p>After</p>",chunk).text=="A</textareax>B\nAfter\n");
        assert(parse("<textarea>A</textarea",chunk).text=="A</textarea\n");
        assert(parse("<textarea>A</textarea ",chunk).text=="A\n");
        assert(parse("<textarea/>&#10;Alpha</textarea><p>After</p>",chunk).text=="Alpha\nAfter\n");
        auto nul=parse(std::string("<xmp>A\0B</xmp>",14),chunk);
        assert(nul.text=="A\xef\xbf\xbd" "B\n");
    }
    const auto large=parse("<textarea>"+std::string(max_text+100,'x')+"</textarea>");
    assert(large.truncated && large.text.size()<=max_text);
    std::cout<<"HTML text modes PASS: RCDATA, literal/hidden/EOF text, fragments, resources, streaming and quotas\n";
}
