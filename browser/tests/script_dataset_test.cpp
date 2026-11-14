// Independent named-property fixture plus bounded native mutation checks.
#include "../browser_script.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
using namespace browser::script;
static Bootstrap bootstrap;
static std::string read(const char* path) {std::ifstream f(path,std::ios::binary);assert(f);return {std::istreambuf_iterator<char>(f),{}};}
static Page open(std::string source) {
    Page p;Limits limits;limits.heap_bytes=512*1024;limits.turn_us=200000;
    assert(p.open("<html><body><pre id=results>Running</pre><script>"+source+"</script></body></html>","https://example.test/",bootstrap,{}, {},limits));return p;
}
static std::string settle(Page& p) {
    std::string html;
    for(unsigned i=0;i<4096 && p.active();++i) {
        p.step();if(auto v=p.take_markup())html=std::move(*v);
        if(p.ready() && p.next_wake_us()!=0){if(p.stats().errors)std::cerr<<p.error()<<'\n';assert(!p.stats().errors);return html;}
    }
    std::cerr<<p.error()<<'\n';assert(false && "dataset page did not settle");return {};
}
static std::string result(std::string html) {
    const auto id=html.find("id=\"results\"");assert(id!=html.npos);
    const auto start=html.find('>',id)+1,end=html.find("</pre>",start);assert(end!=html.npos);
    html=html.substr(start,end-start);
    for(const auto& pair:{std::pair{"&quot;","\""},std::pair{"&lt;","<"},std::pair{"&gt;",">"},std::pair{"&amp;","&"}}) {
        std::size_t at=0;while((at=html.find(pair.first,at))!=html.npos){html.replace(at,std::char_traits<char>::length(pair.first),pair.second);at+=std::char_traits<char>::length(pair.second);}
    }
    return html;
}
int main(int argc,char** argv) {
    assert(argc==4);bootstrap.runtime=read(argv[1]);
    const auto source=read(argv[2]);const auto first=source.find("    test(");assert(first!=source.npos);
    const auto helpers=source.substr(0,first);std::string actual="[";unsigned cases=0;
    for(std::size_t at=first;at<source.size();) {
        const auto end=source.find('\n',at);const auto line=source.substr(at,end==source.npos?source.npos:end-at);
        if(line.starts_with("    test(")) {
            auto p=open(helpers+line+"\nreturn rows;}\ndocument.getElementById('results').textContent=JSON.stringify(runDatasetCases());");
            auto row=result(settle(p));assert(row.starts_with('[') && row.ends_with(']'));
            if(cases++)actual+=',';actual+=row.substr(1,row.size()-2);
        }
        if(end==source.npos)break;at=end+1;
    }
    actual+=']';auto expected=read(argv[3]);if(expected.ends_with('\n'))expected.pop_back();
    if(actual!=expected)std::cerr<<"ACTUAL "<<actual<<'\n';assert(actual==expected && cases>=35);
    {
        // Also exercise conversion directly in an ordinary page script.
        auto p=open(R"JS(
        const n=document.getElementById('results'),d=n.dataset;let caught=false;
        try{d['bad-key']={toString(){d.side='effect';return 'value'}}}catch(e){caught=e.name==='SyntaxError'}
        if(!caught || d.side!=='effect' || n.getAttribute('data-side')!=='effect')throw Error('reentrant conversion');
        if(n.tagName!=='PRE' || Reflect.set(n,'tagName','WRONG') || document.createTextNode('text').tagName!==undefined)throw Error('tag name reflection');
        )JS");settle(p);
    }
    {
        auto p=open(R"JS(
        const n=document.getElementById('results'),d=n.dataset;
        function check(ok){if(!ok)throw Error('dataset limit')}
        function reject(fn){const before=JSON.stringify(d);let caught=false;try{fn()}catch(e){caught=e.name==='RangeError'}check(caught && JSON.stringify(d)===before)}
        d.x='old';reject(()=>d.x='x'.repeat(4097));reject(()=>d['a'.repeat(60)]='v');reject(()=>d['A'.repeat(30)]='v');
        d['a'.repeat(59)]='boundary';check(n.getAttribute('data-'+'a'.repeat(59))==='boundary');delete d['a'.repeat(59)];
        d['A'.repeat(29)]='upper';check(d['A'.repeat(29)]==='upper');delete d['A'.repeat(29)];
        for(let i=0;i<22;++i)d['n'+i]='v';reject(()=>d.overflow='v');d.x='update';delete d.n0;d.overflow='recovered';check(d.overflow==='recovered');
        check(!Reflect.defineProperty(d,'x',{value:'bad',configurable:false}) && d.x==='update');
        check(!Reflect.defineProperty(d,'x',{get(){return 'bad'},configurable:true}) && d.x==='update');
        check(document.dataset===undefined && document.createTextNode('text').dataset===undefined);
        )JS");settle(p);
    }
    {
        auto p=open(R"JS(
        const n=document.getElementById('results');n.innerHTML='<img id=photo data-src="/MiXeD.png" data-page-number=2>';
        const image=document.getElementById('photo'),d=image.dataset;
        if(d.src!=='/MiXeD.png' || d.pageNumber!=='2')throw Error('parsed dataset');
        image.src=d.src;delete d.src;d.pageNumber='3';
        if(image.src!=='/MiXeD.png' || image.hasAttribute('data-src') || !image.matches('[data-page-number="3"]'))throw Error('image dataset');
        )JS");const auto html=settle(p);assert(html.find("src=\"/MiXeD.png\"")!=html.npos && html.find("data-page-number=\"3\"")!=html.npos);
    }
    {
        auto p=open(R"JS(
        const n=document.getElementById('results'),d=n.dataset;d.src='unchanged';let count=0;
        n.onclick=function(){if(d.src!=='unchanged' || !('src' in d) || Object.keys(d).length!==1)throw Error('dataset read');
          d.src='unchanged';delete d.missing;location.href='/idle/'+(++count)};
        )JS");const auto html=settle(p);const auto at=html.find("data-pxa-click=\"");assert(at!=html.npos);
        const auto id=unsigned(std::stoul(html.substr(at+16))),batches=p.stats().markup_turns;
        for(unsigned i=0;i<12;++i){assert(p.click(id));assert(settle(p).empty());const auto nav=p.take_navigation();assert(nav && nav->url=="https://example.test/idle/"+std::to_string(i+1));}
        assert(p.stats().markup_turns==batches);
    }
    std::cout<<"dataset behavioral fixture PASS: "<<cases<<" cases; live attributes, quotas, parsed images and idle snapshots\n";
}
