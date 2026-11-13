// Functional event notifications plus an optional host-only idle benchmark.
#include "../browser_script.hpp"
#include <cassert>
#include <ctime>
#include <fstream>
#include <iostream>
#include <iterator>
using namespace browser::script;

struct Clock {
    std::uint64_t us=0;
    Hooks hooks() { Hooks h;h.owner=this;h.now_us=[](void* p){return static_cast<Clock*>(p)->us;};return h; }
};
static Bootstrap bootstrap;
static Page open(std::string body, Clock& clock) {
    Page p;Limits l;l.heap_bytes=512*1024;l.turn_us=200000;
    assert(p.open("<html><body>"+body+"</body></html>","https://example.test/",bootstrap,clock.hooks(),{},l));return p;
}
static std::string settle(Page& p) {
    std::string result;
    for (unsigned i=0;i<4096 && p.active();++i) {
        p.step();if(auto out=p.take_markup())result=std::move(*out);
        if(p.ready() && p.next_wake_us()!=0) {assert(!p.stats().errors);return result;}
    }
    assert(false && "event page did not settle");return {};
}
static void click(Page& p, unsigned id) { assert(p.click(id)); }
static bool target(const std::string& html, unsigned id) {
    return html.find("data-pxa-click=\""+std::to_string(id)+"\"")!=html.npos;
}
static void measure() {
    for (const auto* mode : {"timer","click"}) {
        Clock clock;std::string body;
        for(int i=0;i<40;++i)body+="<p>Unchanged paragraph</p>";
        body+="<p id=t style='color:red'>Same</p><button onclick=noop()>No change</button><script>"
            "var n=0;function noop(){++n;document.getElementById('t').textContent='Same';"
            "document.getElementById('t').style.color='red';location.href='/count/'+n}";
        if(std::string_view(mode)=="timer")body+="setInterval(noop,10);";
        body+="</script>";
        auto p=open(std::move(body),clock);assert(!settle(p).empty());
        // Warm lazy style access and inline-handler compilation outside timing.
        if(std::string_view(mode)=="timer")clock.us+=20000;else click(p,1);
        assert(settle(p).empty());assert(p.take_navigation());
        const auto before=p.stats();const auto begin=std::clock();
        for(unsigned i=0;i<96;++i) {
            if(std::string_view(mode)=="timer")clock.us+=20000;else click(p,1);
            assert(settle(p).empty());
            const auto nav=p.take_navigation();assert(nav && nav->url=="https://example.test/count/"+std::to_string(i+2));
        }
        const auto elapsed=1000.0*double(std::clock()-begin)/CLOCKS_PER_SEC;const auto after=p.stats();
        std::cout<<"{\"mode\":\""<<mode<<"\",\"callbacks\":96,\"cpu_ms\":"<<elapsed
            <<",\"markup_batches\":"<<after.markup_turns-before.markup_turns
            <<",\"vm_peak_bytes\":"<<after.peak_bytes<<"}\n";
    }
}
int main(int argc,char** argv) {
    assert(argc==2 || (argc==3 && std::string_view(argv[2])=="--measure"));
    std::ifstream f(argv[1],std::ios::binary);bootstrap.runtime.assign(std::istreambuf_iterator<char>(f),{});
    assert(!bootstrap.runtime.empty());if(argc==3){measure();return 0;}
    {
        Clock c;auto p=open(R"HTML(<p id=t style="color:red">Before</p><p id=empty></p><script>
        var n=0,t=document.getElementById('t');var timer=setInterval(()=>{
          ++n;t.textContent=n===8?'After 8':'Before';
          document.getElementById('empty').textContent='';document.getElementById('empty').innerHTML='';
          Promise.resolve().then(()=>{t.style.color='red'});
          if(n===8)clearInterval(timer);
        },10);</script>)HTML",c);
        assert(!settle(p).empty());const auto batches=p.stats().markup_turns;
        for(int i=0;i<7;++i){c.us+=20000;assert(settle(p).empty());assert(p.stats().markup_turns==batches);}
        c.us+=20000;assert(settle(p).find("After 8")!=std::string::npos);
        assert(p.stats().markup_turns>batches && p.next_wake_us()==UINT64_MAX);
    }
    {
        Clock c;auto p=open(R"HTML(<button onclick=change()>Change handler</button><p id=t>Target</p><script>
        var phase=0,count=0,t=document.getElementById('t'),many=[];
        function handler(){t.textContent='Clicked '+(++count)}
        function change(){switch(++phase){
          case 1:t.onclick=handler;break;
          case 2:t.onclick=null;break;
          case 3:t.addEventListener('click',handler);break;
          case 4:t.removeEventListener('click',handler);break;
          case 5:
            for(let i=0;i<16;i++){let fn=()=>{t.textContent='Many '+(++count)};many.push(fn);t.addEventListener('click',fn)}
            t.addEventListener('click',many[0]);
            let limited=false;try{t.addEventListener('click',()=>{})}catch(e){limited=e instanceof RangeError}
            if(!limited)throw Error('listener quota');break;
          case 6:t.addEventListener('click',many[0]);break;
          case 7:for(const fn of many)t.removeEventListener('click',fn);break;
        }}</script>)HTML",c);
        assert(!target(settle(p),2));
        click(p,1);assert(target(settle(p),2));click(p,2);assert(settle(p).find("Clicked 1")!=std::string::npos);
        click(p,1);assert(!target(settle(p),2));auto batches=p.stats().markup_turns;
        click(p,2);assert(settle(p).empty() && p.stats().markup_turns==batches);
        click(p,1);assert(target(settle(p),2));click(p,2);assert(settle(p).find("Clicked 2")!=std::string::npos);
        click(p,1);assert(!target(settle(p),2));
        click(p,1);assert(target(settle(p),2));click(p,2);assert(settle(p).find("Many 18")!=std::string::npos);
        batches=p.stats().markup_turns;click(p,1);assert(settle(p).empty() && p.stats().markup_turns==batches);
        click(p,1);assert(!target(settle(p),2));
    }
    {
        Clock c;auto p=open(R"HTML(<button onclick=change()>Toggle</button>
        <p id=t onclick="this.textContent='Inline fired'">Inline target</p><script>
        var t=document.getElementById('t'),phase=0;
        function change(){if(++phase===2)t.setAttribute('onclick',t.getAttribute('onclick'));else t.onclick=undefined}
        </script>)HTML",c);
        assert(target(settle(p),2));click(p,1);assert(!target(settle(p),2));
        click(p,1);assert(target(settle(p),2));click(p,2);assert(settle(p).find("Inline fired")!=std::string::npos);
        click(p,1);assert(!target(settle(p),2));
    }
    {
        // A response callback has no conservative event repaint to rely on.
        Clock c;auto p=open(R"HTML(<p id=t>Loaded</p><script>
        var t=document.getElementById('t'),x=new XMLHttpRequest();
        x.onload=()=>t.addEventListener('click',()=>{t.textContent='XHR handler worked'});
        x.open('GET','/bind');x.send();</script>)HTML",c);
        assert(!target(settle(p),1));const auto request=p.take_request();assert(request);
        p.complete({request->id,200,"ok"});assert(target(settle(p),1));
        click(p,1);assert(settle(p).find("XHR handler worked")!=std::string::npos);
    }
    {
        Clock c;auto p=open(R"HTML(<button onclick=change()>Toggle</button><a id=a href="/next">Next</a><script>
        var a=document.getElementById('a'),phase=0;
        function change(){a.onclick=++phase===1?()=>false:null}
        </script>)HTML",c);
        assert(!settle(p).empty());const auto batches=p.stats().markup_turns;
        click(p,1);assert(settle(p).empty());click(p,2);assert(settle(p).empty() && !p.take_navigation());
        click(p,1);assert(settle(p).empty());click(p,2);assert(settle(p).empty());
        const auto nav=p.take_navigation();assert(nav && nav->url=="https://example.test/next");
        assert(p.stats().markup_turns==batches);
    }
    std::cout<<"event updates PASS: idle callbacks, same values, handler changes, listener quota, XHR and live links\n";
}
