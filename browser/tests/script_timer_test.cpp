#include "../browser_script.hpp"
#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
using namespace browser::script;
static Bootstrap bootstrap;
static std::uint64_t clock_us=1000000;
static Page open(std::string source) {
    clock_us=1000000;Hooks hooks;hooks.owner=&clock_us;
    hooks.now_us=[](void* p){return *static_cast<std::uint64_t*>(p);};
    hooks.cookie_set=[](void* p,std::string_view,std::string_view){*static_cast<std::uint64_t*>(p)+=100000;};
    Limits limits;limits.heap_bytes=512*1024;limits.turn_us=200000;
    Page p;assert(p.open("<p id=proof>Before</p><script>function ok(v,m){if(!v)throw Error(m)};"+source+"</script>","https://timer.test/",bootstrap,hooks,{},limits));return p;
}
static std::string settle(Page& p) {
    std::string html;
    for(unsigned i=0;i<4096 && p.active();++i) {
        p.step();if(auto next=p.take_markup())html=std::move(*next);
        if(p.ready() && p.next_wake_us()>clock_us)break;
    }
    if(p.stats().errors)std::cerr<<p.error()<<'\n';
    assert(p.active() && p.ready() && !p.stats().errors);return html;
}
int main(int argc,char** argv) {
    assert(argc==2 || argc==3);std::ifstream f(argv[1],std::ios::binary);bootstrap.runtime.assign(std::istreambuf_iterator<char>(f),{});assert(!bootstrap.runtime.empty());
    const std::string selected=argc==3?argv[2]:"all";
    auto test=[&](std::string_view name){if(selected!="all" && selected!=name)return false;std::cout<<"timer case "<<name<<'\n'<<std::flush;return true;};
    if(test("order")) {
        auto p=open("const seen=[];function mark(x){seen.push(x);document.getElementById('proof').textContent=seen.join(',')}setTimeout(mark,80,'late');setTimeout(mark,10,'early-a');setTimeout(mark,10,'early-b');");
        settle(p);clock_us+=100000;assert(settle(p).find("early-a,early-b,late")!=std::string::npos);
    }
    if(test("receiver")) {
        auto p=open("setTimeout(function(a,b){'use strict';ok(this===window && a===17 && b==='arg','timer receiver/arguments');document.getElementById('proof').textContent='Receiver PASS'},0,17,'arg');");
        assert(settle(p).find("Receiver PASS")!=std::string::npos);
    }
    if(test("coercion")) {
        auto p=open("let calls=0;const delay={valueOf(){++calls;return 25}};const id=setInterval(()=>{},delay);ok(calls===1,'delay converted twice');clearTimeout(id);document.getElementById('proof').textContent='Coercion PASS';");
        assert(settle(p).find("Coercion PASS")!=std::string::npos && p.next_wake_us()==UINT64_MAX);
    }
    if(test("cancel")) {
        auto p=open("let calls=0;const a=setTimeout(()=>++calls,0),b=setInterval(()=>++calls,0);clearInterval(String(a));clearTimeout(String(b));setTimeout(()=>{ok(calls===0,'cancel numeric-string id');document.getElementById('proof').textContent='Cancel PASS'},0);");
        assert(settle(p).find("Cancel PASS")!=std::string::npos && p.next_wake_us()==UINT64_MAX);
    }
    if(test("delay")) {
        auto p=open("let count=0;for(const delay of [Infinity,NaN,2147483648,-10])setTimeout(()=>{if(++count===4)document.getElementById('proof').textContent='Delay PASS'},delay);");
        assert(settle(p).find("Delay PASS")!=std::string::npos && p.next_wake_us()==UINT64_MAX);
    }
    if(test("quota-coercion")) {
        auto p=open("let limited=false;const delay={valueOf(){for(let i=0;i<8;++i)setTimeout(()=>{},100);return 0}};try{setTimeout(()=>{},delay)}catch(e){limited=e instanceof RangeError}ok(limited,'reentrant timer quota');document.getElementById('proof').textContent='Quota PASS';");
        assert(settle(p).find("Quota PASS")!=std::string::npos);
    }
    if(test("interval")) {
        auto p=open("let count=0;const id=setInterval(()=>{document.cookie='advance';if(++count===2)clearInterval(id)},20);");
        settle(p);clock_us+=20000;settle(p);assert(clock_us==1120000 && p.next_wake_us()==1140000);
        clock_us+=20000;settle(p);assert(clock_us==1240000 && p.next_wake_us()==UINT64_MAX);
    }
    if(test("quota-running")) {
        auto p=open("const id=setInterval(()=>{const ids=[];for(let i=0;i<7;++i)ids.push(setTimeout(()=>{},100));let limited=false;try{setTimeout(()=>{},100)}catch(e){limited=e instanceof RangeError}ok(limited,'running interval occupies quota');clearInterval(id);const extra=setTimeout(()=>{},100);for(const x of ids)clearTimeout(x);clearTimeout(extra);document.getElementById('proof').textContent='Running quota PASS'},0);");
        assert(settle(p).find("Running quota PASS")!=std::string::npos && p.next_wake_us()==UINT64_MAX);
    }
    if(test("memory")) {
        auto p=open("let rounds=0;const id=setInterval(()=>{for(let i=0;i<200;++i){const data=new Array(128).fill(i);const t=setTimeout(()=>data[0],1);clearTimeout(t)}if(++rounds===129)clearInterval(id)},20);");
        settle(p);clock_us+=20000;settle(p);const auto before=p.stats().current_bytes;
        const auto started=std::chrono::steady_clock::now();
        for(unsigned i=0;i<128;++i){clock_us+=20000;settle(p);}
        const auto elapsed=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-started).count();
        const auto after=p.stats();assert(after.current_bytes<=before+1024 && p.next_wake_us()==UINT64_MAX);
        std::cout<<"{\"timer_cycles\":25600,\"before\":"<<before<<",\"after\":"<<after.current_bytes<<",\"peak\":"<<after.peak_bytes<<",\"elapsed_us\":"<<elapsed<<"}\n";
    }
    std::cout<<"timer lifecycle PASS\n";
}
