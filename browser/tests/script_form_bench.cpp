// Paired host-only form collection benchmark; not device/network throughput.
#include "../browser_script.hpp"
#include <cassert>
#include <ctime>
#include <fstream>
#include <iostream>
#include <iterator>
using namespace browser::script;
static void settle(Page& p) {
    for(unsigned i=0;i<4096 && p.active();++i) {
        p.step();p.take_markup();
        if(p.ready() && p.next_wake_us()!=0){assert(!p.stats().errors);return;}
    }
    assert(false && "benchmark did not settle");
}
int main(int argc,char** argv) {
    assert(argc==2);std::ifstream f(argv[1],std::ios::binary);
    Bootstrap b{{std::istreambuf_iterator<char>(f),{}}};assert(!b.runtime.empty());
    std::string html="<html><body><form method=post action=/post>", expected;
    for(int i=0;i<16;++i) {
        const auto name="n"+std::to_string(i);
        html+="<div><p><input type=hidden name="+name+" value=abcdef0123456789></p></div>";
        if(i)expected+='&';expected+=name+"=abcdef0123456789";
    }
    html+="<button>Send</button></form><script>void 0;</script></body></html>";
    Page p;Limits l;l.heap_bytes=512*1024;l.turn_us=200000;
    assert(p.open(std::move(html),"https://example.test/",std::move(b),{},{},l));settle(p);
    const auto before=p.stats().markup_turns;const auto begin=std::clock();
    for(unsigned i=0;i<64;++i) {
        // Hidden inputs keep existing field IDs 1..16; the button owns 17.
        assert(p.click(17));settle(p);const auto nav=p.take_navigation();
        assert(nav && nav->post && nav->url=="https://example.test/post" && nav->body==expected);
    }
    const auto elapsed=1000.0*double(std::clock()-begin)/CLOCKS_PER_SEC;
    std::cout<<"{\"submissions\":64,\"fields\":16,\"cpu_ms\":"<<elapsed
        <<",\"markup_batches\":"<<p.stats().markup_turns-before<<",\"vm_peak_bytes\":"<<p.stats().peak_bytes<<"}\n";
}
