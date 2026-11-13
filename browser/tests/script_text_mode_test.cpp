#include "../browser_script.hpp"
#include "../browser_core.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
using namespace browser::script;
static Bootstrap bootstrap;
static unsigned case_number=0;
static Page open(std::string html) {
    ++case_number;Page p;Limits l;l.heap_bytes=512*1024;l.turn_us=200000;
    assert(p.open(std::move(html),"https://example.test/",bootstrap,{},{},l));return p;
}
static std::string settle(Page& p) {
    std::string html;
    for(unsigned i=0;i<4096 && p.active();++i) {
        p.step();if(auto v=p.take_markup())html=std::move(*v);
        if(p.ready() && p.next_wake_us()!=0) {
            if(p.stats().errors)std::cerr<<"Text case "<<case_number<<": "<<p.error()<<'\n';
            assert(!p.stats().errors);return html;
        }
    }
    assert(false && "text page did not settle");return {};
}
int main(int argc,char** argv) {
    assert(argc==2);std::ifstream f(argv[1],std::ios::binary);bootstrap.runtime.assign(std::istreambuf_iterator<char>(f),{});
    for(auto name:{"title","textarea","xmp","iframe","noembed","noframes","plaintext"}) {
        const std::string opening="<"+std::string(name)+">";
        for(std::size_t chunk:{1,2,7,4096}) {
            const auto source=opening+"<script src=/ghost.js></script><img src=/ghost.png>"+(std::string(name)=="plaintext"?"":"</"+std::string(name)+">");
            Capture capture;for(std::size_t at=0;at<source.size();at+=chunk)capture.append(std::string_view(source).substr(at,chunk));
            assert(!capture.has_script() && capture.take()==source);
            Capture real;real.append("<script>void 0</script>"+source);assert(real.has_script());
        }
    }
    {
        const auto source="<textarea><style>"+std::string(12000,'a')+"</style><script>bad()</script></textarea>";
        Capture capture;capture.append(source);assert(!capture.limited() && !capture.has_script() && capture.take()==source);
    }
    for(auto name:{"title","textarea","xmp","iframe","noembed","noframes"}) {
        const bool refs=std::string_view(name)=="title" || std::string_view(name)=="textarea";
        const std::string expected="A<b>B</b>"+std::string(refs?"&":"&amp;")+"\nEnd<script src=/ghost.js></script>";
        auto p=open("<html><body><"+std::string(name)+" id=raw onclick=\"void 0\">A<b>B</b>&amp;\r\nEnd<script src=/ghost.js></script></"+name+">"
            "<script>const n=document.getElementById('raw');if(n.children.length || document.querySelector('img'))throw Error('inert children');"
            "if(n.textContent.indexOf('A<b>B</b>')!==0 || n.textContent.indexOf('End')<0)throw Error('literal text');"
            "location.href='/checked?text='+encodeURIComponent(n.textContent);</script></body></html>");
        const auto html=settle(p);const auto navigation=p.take_navigation();assert(navigation && navigation->url=="https://example.test/checked?text="+browser::encode(expected) && !p.take_request());
        if(std::string_view(name)=="title" || std::string_view(name)=="iframe" || std::string_view(name)=="noembed" || std::string_view(name)=="noframes") {
            browser::Document d;d.clear("https://example.test/");browser::Parser parsed(d,true);parsed.feed(html);parsed.finish();assert(d.text.find("ghost")==d.text.npos);
        }
        // Exercise element-context innerHTML against the Chromium oracle cases.
        auto fragment=open("<html><body><div id=host></div><script>const n=document.createElement('"+std::string(name)+"');"
            "n.id='raw';n.innerHTML='A<b>B</b>&amp;\\r\\nEnd';document.getElementById('host').appendChild(n);"
            "if(n.children.length || n.textContent!==\"A<b>B</b>"+std::string(refs?"&":"&amp;")+"\\nEnd\")throw Error('context text');"
            "n.innerHTML='A</"+name+"><img src=/ghost.png>';if(n.children.length)throw Error('context close');"
            "if(n.textContent!=='A</"+name+"><img src=/ghost.png>')throw Error('context ending');"
            "location.href='/fragment';</script></body></html>");
        const auto projected=settle(fragment);const auto navigation2=fragment.take_navigation();assert(navigation2 && navigation2->url=="https://example.test/fragment" && !fragment.take_request());
        browser::Document d;d.clear("https://example.test/");browser::Parser parsed(d,true);parsed.feed(projected);parsed.finish();
        assert(d.images.empty());
        (void)html;
    }
    {
        auto p=open(R"HTML(<html><body><textarea id=raw>

Line</textarea><script>const t=document.getElementById('raw');
        if(t.textContent!=='\nLine')throw Error('initial LF');
        t.innerHTML='&#13;A';if(t.textContent!=='\rA')throw Error('entity CR');
        t.innerHTML='\r\nAlpha';if(t.textContent!=='\nAlpha')throw Error('fragment LF');
        </script></body></html>)HTML");
        const auto html=settle(p);browser::Document d;d.clear("https://example.test/");browser::Parser parsed(d,true);parsed.feed(html);parsed.finish();
        assert(d.text.find("\nAlpha")!=d.text.npos);
    }
    {
        auto p=open(R"HTML(<html><body><script>document.addEventListener('DOMContentLoaded',()=>{
        const n=document.getElementById('raw');if(n.children.length || n.textContent!=='A</plaintext><img src=/ghost>')throw Error('plaintext EOF');
        location.href='/plaintext';});</script><plaintext id=raw>A</plaintext><img src=/ghost>)HTML");
        const auto html=settle(p);const auto navigation=p.take_navigation();assert(navigation && navigation->url=="https://example.test/plaintext" && !p.take_request());
        browser::Document d;d.clear("https://example.test/");browser::Parser parsed(d,true);parsed.feed(html);parsed.finish();assert(d.images.empty());
    }
    {
        auto p=open(R"HTML(<html><head><style id=s></style></head><body><p class=proof>Proof</p><script>
        const s=document.getElementById('s');s.innerHTML='/* <'+ '/style><img src=/ghost> */ .proof{color:green}';
        if(s.children.length || s.innerHTML.indexOf('<'+ '/style>')<0)throw Error('raw style');
        </script></body></html>)HTML");
        const auto html=settle(p);browser::Document d;d.clear("https://example.test/");browser::Parser parsed(d,true);parsed.feed(html);parsed.finish();assert(d.images.empty());
        assert(!d.spans.empty() && d.spans[0].text_style.color==0x008000ff);
    }
    std::cout<<"script text modes PASS: inert capture/DOM, context fragments, newlines, raw serialization and safe repaint\n";
}
