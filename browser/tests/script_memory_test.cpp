#include "../browser_script.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
using namespace browser::script;
static Bootstrap bootstrap;
static void settle(Page& page) {
    for(unsigned n=0;n<4096 && page.active();++n) {
        page.step();(void)page.take_markup();
        if(page.ready() && page.next_wake_us()!=0)break;
    }
}
int main(int argc,char** argv) {
    assert(argc==2 || argc==3);std::ifstream f(argv[1],std::ios::binary);
    bootstrap.runtime.assign(std::istreambuf_iterator<char>(f),{});assert(!bootstrap.runtime.empty());
    const bool measure=argc==3;
    for(unsigned count:{1000u,5000u,10000u}) {
        Limits limits;limits.heap_bytes=512*1024;limits.turn_us=200000;
        Page p;assert(p.open("<script>var a=[];for(var i=0;i<"+std::to_string(count)+";i++)a.push(i);if(a.length!=="+std::to_string(count)+"||a[0]!==0||a[a.length-1]!==a.length-1)throw Error('array contents');</script>","https://memory.test/",bootstrap,{},{},limits));
        assert(p.stats().current_bytes==0);settle(p);const auto s=p.stats();
        std::cout<<"{\"array_elements\":"<<count<<",\"heap_limit\":"<<limits.heap_bytes<<",\"active\":"<<p.active()<<",\"errors\":"<<s.errors<<",\"used\":"<<s.current_bytes<<",\"peak_live_bytes\":"<<s.peak_bytes<<"}\n";
        if(!measure)assert(p.ready() && p.active() && !s.errors && s.current_bytes<=limits.heap_bytes);
    }
    if(measure)return 0;
    Limits limits;limits.heap_bytes=16*1024*1024;limits.turn_us=200000;
    Page p;assert(p.open("<script>var b='x'.repeat(6*1024*1024);if(b.length!==6*1024*1024||b.charAt(0)!=='x'||b.charAt(b.length-1)!=='x')throw Error('string contents');</script>","https://memory.test/",bootstrap,{},{},limits));
    assert(p.stats().current_bytes==0);settle(p);
    if(!p.ready() || !p.active() || p.stats().errors)std::cerr<<"large string: "<<p.error()<<" current="<<p.stats().current_bytes<<" errors="<<p.stats().errors<<"\n";
    assert(p.ready() && p.active() && !p.stats().errors && p.stats().current_bytes>6*1024*1024);
    const auto before=p.stats().current_bytes;assert(p.set_heap_limit(8*1024*1024));
    assert(p.set_heap_limit(16*1024*1024) && p.stats().current_bytes==before);
    assert(!p.set_heap_limit(16*1024*1024+1) && p.active());
    p={};assert(p.stats().current_bytes==0);
    limits.heap_bytes=16*1024*1024+1;
    assert(!p.open("<p>invalid limit</p>","https://memory.test/",bootstrap,{},{},limits));
    std::cout<<"script memory PASS: lazy allocation, growing arrays, 6 MiB string, quota changes without reservation and page teardown\n";
}
