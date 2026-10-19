// Optional standalone WASI/WAMR diagnostic, not part of the application.
// Runs captured original HTML/four scripts and a synthetic QR JSON response.
// File/cache inputs are deliberately retained: these pages are not the live
// PXA component's memory. Compare variants within this exact probe only.
// Arguments: bootstrap.qjb captured-original-directory es5-shim.qjb fixture.json.
// The harness must verify the captured es5-shim source against the compiled
// library manifest first; this diagnostic never accepts network bytecode.
#include "../browser_script.hpp"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
using namespace browser::script;
static std::string read(const std::string& path) {
    std::ifstream f(path,std::ios::binary);assert(f);return {std::istreambuf_iterator<char>(f),{}};
}
struct Cache {
    std::string source,bytecode; unsigned hits=0;
    static CompiledScript lookup(void* p,std::uint32_t,std::string_view source) {
        auto& cache=*static_cast<Cache*>(p);
        if(source!=cache.source)return {};
        ++cache.hits;return {CompiledScript::State::bytecode,cache.bytecode};
    }
};
int main(int argc,char**argv) {
    assert(argc==5);
    Bootstrap bootstrap;bootstrap.runtime=read(argv[1]);
    const std::string official=argv[2];
    Cache cache{read(official+"/es5-shim.js"),read(argv[3])};
    auto fixture=read(argv[4]);
    const auto image_at=fixture.find("data:image/png;base64,");assert(image_at!=fixture.npos);
    const auto image=fixture.substr(image_at,fixture.find('"',image_at)-image_at);
    Page p;Hooks hooks;hooks.owner=&cache;hooks.compiled_script=Cache::lookup;
    Limits limits;limits.heap_bytes=512*1024;
    assert(p.open(read(official+"/login.html"),"https://weread.qq.com/wrwebsimplenjlogic/login",bootstrap,hooks,{},limits));
    const auto initial=__builtin_wasm_memory_size(0);
    bool rendered=false;unsigned scripts=0,requests=0,polls=0;
    for(;polls<4096 && p.active() && !rendered;++polls) {
        p.step();
        if(auto markup=p.take_markup())rendered=markup->find(image)!=markup->npos;
        if(auto request=p.take_request()) {
            ++requests;std::string body;
            if(request->script) {
                bool found=false;
                for(auto name:{"es5-shim","global","cookie","login"})
                    if(request->url.find(name)!=request->url.npos) {body=read(official+"/"+name+".js");found=true;break;}
                assert(found);++scripts;
            } else if(request->url.find("/getuid")!=request->url.npos)
                body="{\"uid\":\"fixture0-0000-0000-0000-000000000000\"}";
            else if(request->url.find("/qrcode")!=request->url.npos)body=fixture;
            else if(request->url.find("/getlogininfo")!=request->url.npos)continue;
            else body="{\"succ\":1}";
            p.complete({.id=request->id,.status=200,.body=std::move(body)});
        }
    }
    auto stats=p.stats();
    std::printf("{\"complete\":%s,\"scripts\":%u,\"requests\":%u,\"cache_hits\":%u,\"polls\":%u,\"js_current\":%zu,\"js_peak\":%zu,\"linear_initial_pages\":%zu,\"linear_final_peak_pages\":%zu,\"markup_max_us\":%llu,\"script_max_us\":%llu}\n",
        rendered&&p.ready()&&stats.errors==0?"true":"false",scripts,requests,cache.hits,polls,stats.current_bytes,stats.peak_bytes,
        initial,__builtin_wasm_memory_size(0),(unsigned long long)stats.max_markup_us,(unsigned long long)stats.max_turn_us);
    if(!rendered||!p.ready()||stats.errors||scripts!=4||cache.hits!=1) {
        std::fprintf(stderr,"Page failure: %.*s errors=%u\n",int(p.error().size()),p.error().data(),stats.errors);return 1;
    }
}
