// Independent behavioral fixture plus native no-op and quota coverage.
#include "../browser_script.hpp"
#include <cassert>
#include <ctime>
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
    std::cerr<<p.error()<<'\n';assert(false && "classList page did not settle");return {};
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
static unsigned hit(const std::string& html) {
    const auto at=html.find("data-pxa-click=\"");assert(at!=html.npos);return unsigned(std::stoul(html.substr(at+16)));
}
static void measure(bool cached) {
    std::string source="const n=document.getElementById('results');n.className=Array.from({length:63},(_,i)=>'token'+i).join(' ')+' target';";
    if(cached)source+="const list=n.classList;";
    source+="let count=0;n.onclick=function(){let sum=0;for(let i=0;i<100;++i){sum+=";
    source+=cached?"list.contains('target')":"n.className.split(' ').includes('target')";
    source+="}if(sum!==100)throw Error('class read');location.href='/reads/'+(++count)}";
    auto p=open(source);const auto html=settle(p);const auto id=hit(html);assert(p.click(id));settle(p);assert(p.take_navigation());
    const auto before=p.stats().markup_turns;const auto begin=std::clock();
    for(unsigned i=0;i<64;++i){assert(p.click(id));assert(settle(p).empty());const auto nav=p.take_navigation();assert(nav && nav->url=="https://example.test/reads/"+std::to_string(i+2));}
    std::cout<<"{\"reads\":6400,\"cpu_ms\":"<<1000.0*double(std::clock()-begin)/CLOCKS_PER_SEC
        <<",\"vm_peak_bytes\":"<<p.stats().peak_bytes<<",\"markup_batches\":"<<p.stats().markup_turns-before<<"}\n";
}
int main(int argc,char** argv) {
    assert(argc==4 || argc==3);bootstrap.runtime=read(argv[1]);
    if(argc==3){assert(std::string_view(argv[2])=="--measure-cached" || std::string_view(argv[2])=="--measure-split");measure(std::string_view(argv[2])=="--measure-cached");return 0;}
    const auto source=read(argv[2]);const auto first=source.find("    test(");assert(first!=source.npos);
    const auto helpers=source.substr(0,first);std::string actual="[";unsigned cases=0;
    for(std::size_t at=first;at<source.size();) {
        const auto end=source.find('\n',at);const auto line=source.substr(at,end==source.npos?source.npos:end-at);
        if(line.starts_with("    test(")) {
            auto p=open(helpers+line+"\nreturn rows;}\ndocument.getElementById('results').textContent=JSON.stringify(runClassListCases());");
            auto row=result(settle(p));assert(row.starts_with('[') && row.ends_with(']'));
            if(cases++)actual+=',';actual+=row.substr(1,row.size()-2);
        }
        if(end==source.npos)break;at=end+1;
    }
    actual+=']';auto expected=read(argv[3]);if(expected.ends_with('\n'))expected.pop_back();
    if(actual!=expected)std::cerr<<"ACTUAL "<<actual<<'\n';assert(actual==expected && cases>=30);
    {
        auto p=open(R"JS(
        const n=document.getElementById('results');n.className='ready active';const list=n.classList;let count=0;
        n.onclick=function(){
          if(!list.contains('ready') || list.item(0)!=='ready' || list.length!==2)throw Error('cached reads');
          list.add('ready');list.remove('missing');if(!list.toggle('ready',true) || list.toggle('missing',false) || list.replace('missing','other'))throw Error('no-op result');
          list.value='ready active';n.className='ready active';
          location.href='/idle/'+(++count);
        };
        )JS");const auto html=settle(p);const auto id=hit(html);const auto batches=p.stats().markup_turns;
        for(unsigned i=0;i<12;++i){assert(p.click(id));assert(settle(p).empty());const auto nav=p.take_navigation();assert(nav && nav->url=="https://example.test/idle/"+std::to_string(i+1));}
        assert(p.stats().markup_turns==batches);
    }
    {
        auto p=open(R"JS(
        const n=document.getElementById('results'),list=n.classList;
        function reject(fn){const before=n.className,old=list[0],length=list.length;let caught=false;
          try{fn()}catch(e){caught=e.name==='RangeError'}
          if(!caught || n.className!==before || list[0]!==old || list.length!==length)throw Error('atomic class quota');
        }
        n.className='x'.repeat(4094);reject(()=>list.add('bb'));reject(()=>list.value='x'.repeat(4097));
        reject(()=>list.replace(n.className,'y'.repeat(4097)));
        n.className='__proto__ constructor __proto__ toString';
        if(list.length!==3 || !list.contains('__proto__') || list[1]!=='constructor')throw Error('token dictionary');
        n.removeAttribute('class');for(let i=0;i<23;++i)n.setAttribute('data-'+i,'x');
        list.add();list.remove('missing');list.toggle('missing',false);
        if(n.hasAttribute('class') || list.length || list[0]!==undefined)throw Error('absent empty class');
        reject(()=>list.add('ready'));reject(()=>list.value='');
        n.removeAttribute('data-0');list.add('ready');
        if(list[0]!=='ready' || list.length!==1)throw Error('class quota recovery');
        n.setAttribute('class','a b');if(list[0]!=='a' || list[1]!=='b')throw Error('index refresh');
        n.removeAttribute('class');if(list[0]!==undefined || list.length)throw Error('index removal');
        )JS");settle(p);
    }
    std::cout<<"classList behavioral fixture PASS: "<<cases<<" cases; live state, quotas and no-op snapshots\n";
}
