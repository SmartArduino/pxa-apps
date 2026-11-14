// Snapshot continuity under callbacks; --measure also runs against the baseline.
#include "../browser_script.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
using namespace browser::script;
static Bootstrap bootstrap;
static Page open(std::string body,Hooks hooks={}) {
    Page p;Limits l;l.heap_bytes=512*1024;l.turn_us=200000;
    assert(p.open("<html><body>"+body+"</body></html>","https://example.test/",bootstrap,hooks,{},l));return p;
}
static std::string settle(Page& p) {
    std::string result;
    for(unsigned i=0;i<4096 && p.active();++i) {
        p.step();if(auto html=p.take_markup())result=std::move(*html);
        if(p.ready() && p.next_wake_us()!=0){if(p.stats().errors)std::cerr<<p.error()<<'\n';assert(!p.stats().errors);return result;}
    }
    assert(false && "snapshot did not settle");return {};
}
static std::string body(std::string callback) {
    std::string s="<p id=head>Head 0</p>";
    for(int i=0;i<40;++i)s+="<p>Stable paragraph</p>";
    s+="<p id=tail>Tail 0</p><button onclick=start()>Start</button><button onclick=noop()>Noop</button><button onclick=edit()>Edit</button><input id=q value=old oninput='bad )'><script>"
       "let serial=0,count=0,ghost=document.createElement('div');"
       "function generation(){document.getElementById('head').textContent='Head '+(++serial);document.getElementById('tail').textContent='Tail '+serial}"
       "function start(){generation();poll()}function noop(){++count}function edit(){generation()}"
       "function poll(){let x=new XMLHttpRequest();x.open('GET','/poll');x.onload=function(){"+callback+"};x.send()}</script>";
    return s;
}
struct Measurement {std::uint32_t batches;unsigned first;};
static Measurement continuity(const char* mode,bool require_progress) {
    const bool network=std::string_view(mode)=="xhr" || std::string_view(mode)=="detached";
    auto p=open(body(std::string(std::string_view(mode)=="detached"?"ghost.textContent='Staged '+count;ghost.dataset.count=count;":"")+
        "if(++count<48)poll();else location.href='/done/'+count"));
    assert(!settle(p).empty());const auto before=p.stats().markup_turns;
    assert(p.click(1));p.step();assert(!p.take_markup());unsigned first=0;std::string result;
    for(unsigned n=1;n<=48;++n) {
        if(network){auto request=p.take_request();assert(request && !request->script);p.complete({request->id,200,"ok"});}
        else if(std::string_view(mode)=="click")assert(p.click(2));
        else p.change_field(4,"old",false);
        p.step();if(auto html=p.take_markup()){assert(result.empty());result=std::move(*html);first=n;}
    }
    if(auto html=settle(p);!html.empty()){assert(result.empty());result=std::move(html);first=49;}
    assert(result.find("Head 1")!=result.npos && result.find("Tail 1")!=result.npos && result.find("Staged")==result.npos);
    assert(!p.stats().errors && p.active());
    if(network){auto nav=p.take_navigation();assert(nav && nav->url=="https://example.test/done/48");}
    if(require_progress)assert(first>0 && first<48);
    return {p.stats().markup_turns-before,first};
}
int main(int argc,char** argv) {
    assert(argc==2 || argc==3);std::ifstream f(argv[1],std::ios::binary);bootstrap.runtime.assign(std::istreambuf_iterator<char>(f),{});assert(!bootstrap.runtime.empty());
    const bool measure=argc==3;if(measure)assert(std::string_view(argv[2])=="--measure");
    unsigned reference=0;
    for(auto mode:{"xhr","detached","click","field"}) {
        const auto r=continuity(mode,!measure);if(!measure){if(!reference)reference=r.batches;assert(r.batches==reference);}
        std::cout<<"{\"mode\":\""<<mode<<"\",\"callbacks\":48,\"markup_batches\":"<<r.batches<<",\"first_publication_after_callback\":"<<r.first<<"}\n";
    }
    if(measure)return 0;
    {
        // Image preview can defer projection. A genuinely invalidated snapshot
        // must release its rendering priority so due timers keep running.
        std::uint64_t now=0;Hooks hooks;hooks.owner=&now;hooks.now_us=[](void* p){return *static_cast<std::uint64_t*>(p);};
        auto p=open(body("generation()")+"<script>setTimeout(()=>{generation();location.href='/timer-fired'},100)</script>",hooks);
        settle(p);assert(p.click(1));p.step();assert(!p.take_markup());auto request=p.take_request();assert(request);
        p.complete({request->id,200,"ok"});p.step();now=200000;p.step();auto nav=p.take_navigation();
        assert(nav && nav->url=="https://example.test/timer-fired");auto html=settle(p);assert(html.find("Head 3")!=html.npos && html.find("Tail 3")!=html.npos);
    }
    {
        auto p=open(body("generation();location.href='/changed'"));settle(p);assert(p.click(1));p.step();assert(!p.take_markup());
        auto req=p.take_request();assert(req);p.complete({req->id,200,"ok"});p.step();auto html=settle(p);
        assert(html.find("Head 2")!=html.npos && html.find("Tail 2")!=html.npos && html.find("Head 1")==html.npos && html.find("Tail 1")==html.npos);
    }
    {
        // Multiple genuine edits coalesce before the next batch, while the
        // old visible input/button IDs remain usable until projection commits.
        auto s=body("");const auto at=s.find(" oninput='bad )'");assert(at!=s.npos);s.erase(at,std::string(" oninput='bad )'").size());
        auto p=open(s);settle(p);assert(p.click(3));p.step();assert(!p.take_markup());assert(p.click(3));p.step();p.change_field(4,"Edited",false);p.step();
        auto html=settle(p);assert(html.find("Head 2")!=html.npos && html.find("Tail 2")!=html.npos && html.find("Edited")!=html.npos && html.find("Head 1")==html.npos);
    }
    {
        auto s=body("");s+="<script>let script=document.createElement('script');script.src='/async.js';document.body.appendChild(script)</script>";
        auto p=open(s);settle(p);auto request=p.take_request();assert(request && request->script);const auto before=p.stats().markup_turns;
        assert(p.click(3));p.step();assert(!p.take_markup());p.complete({request->id,200,"location.href='/script-loaded'"});auto html=settle(p);
        assert(html.find("Head 1")!=html.npos && html.find("Tail 1")!=html.npos && p.stats().markup_turns-before<=reference+1);
        auto nav=p.take_navigation();assert(nav && nav->url=="https://example.test/script-loaded");
    }
    {
        auto s=body("");s+="<script>fetch('/fetch').then(r=>r.text()).then(t=>{generation();document.getElementById('q').value=t})</script>";
        auto p=open(s);settle(p);auto request=p.take_request();assert(request && !request->script);assert(p.click(3));p.step();assert(!p.take_markup());
        p.complete({request->id,200,"Fetched"});auto html=settle(p);assert(html.find("Head 2")!=html.npos && html.find("Tail 2")!=html.npos && html.find("Fetched")!=html.npos);
    }
    {
        auto s=body("");s+="<script>let script=document.createElement('script');script.src='/async.js';document.body.appendChild(script)</script>";
        auto p=open(s);settle(p);auto request=p.take_request();assert(request && request->script);
        assert(p.click(3));p.step();assert(!p.take_markup());p.complete({request->id,200,"generation()"});auto html=settle(p);
        assert(html.find("Head 2")!=html.npos && html.find("Tail 2")!=html.npos && !p.stats().errors);
    }
    std::cout<<"snapshot PASS: no-op callbacks retain progress, coherent edits, old hit IDs, fields, promises and asynchronous scripts\n";
}
