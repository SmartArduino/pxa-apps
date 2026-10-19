#include "../browser_script.hpp"
#include "quickjs.h"
#include "../browser_script_library.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
using namespace browser::script;
static std::string read(const char* path){std::ifstream f(path,std::ios::binary);assert(f);return {std::istreambuf_iterator<char>(f),{}};}
struct Cache {
    std::string compiled,source="function cachedValue(){return 17};";
    unsigned calls=0,hits=0,compiles=0;
    bool invalid=false;
    std::uint64_t time=1000;
    static CompiledScript lookup(void* pointer,std::uint32_t id,std::string_view source){
        auto& s=*static_cast<Cache*>(pointer);assert(id);++s.calls;
        if(source!=s.source)return {};
        if(s.calls<3)return {CompiledScript::State::loading,{}};
        ++s.hits;return {CompiledScript::State::bytecode,s.invalid?"not bytecode":s.compiled};
    }
    Hooks hooks(){
        Hooks h;h.owner=this;h.compiled_script=lookup;
        h.now_us=[](void* p){return static_cast<Cache*>(p)->time;};
        h.trace=[](void* p,std::string_view phase){if(phase=="COMPILE")++static_cast<Cache*>(p)->compiles;};
        return h;
    }
};
static std::string compile(std::string_view source) {
    auto* runtime=JS_NewRuntime();assert(runtime);
    JS_SetStripInfo(runtime,JS_STRIP_SOURCE);
    auto* context=JS_NewContextRaw(runtime);assert(context);
    assert(JS_AddIntrinsicBaseObjects(context)==0);
    assert(JS_AddIntrinsicEval(context)==0);
    auto program=JS_Eval(context,source.data(),source.size(),"test-library.js",JS_EVAL_TYPE_GLOBAL|JS_EVAL_FLAG_COMPILE_ONLY);
    assert(!JS_IsException(program));
    std::size_t size=0;
    auto* bytes=JS_WriteObject(context,&size,program,JS_WRITE_OBJ_BYTECODE);assert(bytes);
    std::string result="PXAQJS1\n";result.append(reinterpret_cast<const char*>(bytes),size);
    js_free(context,bytes);JS_FreeValue(context,program);
    JS_FreeContext(context);JS_FreeRuntime(runtime);
    return result;
}
static void identities(std::string source) {
    using Pair=std::pair<std::size_t,std::string_view>;
    for(auto [size,digest]:std::array<Pair,14>{{
        {0,"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"},
        {1,"ca978112ca1bbdcafac231b39a23dc4da786eff8147c4e72b9807785afee48bb"},
        {55,"9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318"},
        {56,"b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a"},
        {63,"7d3e74a05d7db15bce4ad9ec0658ea98e3f06eeecf16b4c6fff2da457ddc2f34"},
        {64,"ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb"},
        {65,"635361c48bb9eab14198e76ea8ab7f1a41685d6ad62aa9146d301d4f17eb0ae0"},
        {119,"31eba51c313a5c08226adf18d4a359cfdfd8d2e816b13f4af952f7ea6584dcfb"},
        {120,"2f3d335432c70b580af0e8e1b3674a7c020d683aa5f73aaaedfdc55af904c21c"},
        {127,"c57e9278af78fa3cab38667bef4ce29d783787a2f731d4e12200270f0c32320a"},
        {128,"6836cf13bac400e9105071cd6af47084dfacad4e5e302c94bfed24e013afb73e"},
        {129,"c12cb024a2e5551cca0e08fce8f1c5e314555cc3fef6329ee994a3db752166ae"},
        {25442,"0905c47e61019afdfab1d88278dd34c17dfc7d0f2ce38f0f02c9bb21b6da4e6c"},
        {32768,"b217b65e6f205f41b3fb8ef90cf7c44da93f630ca03965273485bbb21a5cccf5"},
    }})assert(library_path(std::string(size,'a'))=="assets/script-libraries/"+std::string(digest)+".qjb");
    // Digest state owns no source pointer. Its caller can resume using equal
    // immutable bytes at another address, or discard a partial page and reset.
    SourceDigest partial;
    for(unsigned i=0;i<7;++i)assert(!partial.advance(source));
    auto resumed=partial;
    for(;;){std::string relocated(source);if(resumed.advance(relocated))break;}
    const auto expected=resumed.hex();
    assert(matching_library_path(source.size(),expected)==known_library_path(source));
    partial={};while(!partial.advance(source)){}
    assert(partial.hex()==expected&&partial.advance(source)&&partial.hex()==expected);
    partial={};assert(!partial.advance(source));partial={};
    assert(partial.advance("")&&digest_path({partial.hex().data(),64})==library_path(""));
    const auto path=known_library_path(source);
    if(!compiled_libraries.empty())assert(path==library_path(source));
    else assert(path.empty());
    source[source.size()/2]^=1;assert(known_library_path(source).empty());
    assert(known_library_path("ordinary unknown script").empty());
}
int main(int argc,char**argv){
    assert(argc==3);identities(read(argv[2]));Bootstrap bootstrap;bootstrap.runtime=read(argv[1]);
    const auto bytecode=compile(Cache{}.source);
    for(int mode=0;mode<6;++mode){
        Cache cache;cache.compiled=bytecode;cache.invalid=mode==2;
        if(mode==4)cache.compiled="PXAQJS1\nBAD";
        Page p;Limits limits;limits.heap_bytes=512*1024;limits.turn_us=200000;
        if(mode==3)limits.script_bytes=16;
        assert(p.open("<p id=a>Before</p><script src='/library.js'></script><script>document.getElementById('a').textContent='Library '+cachedValue();</script>","https://example.test/",bootstrap,cache.hooks(),{},limits));
        std::optional<Request> request;
        for(unsigned i=0;i<1000&&!request;++i){p.step();(void)p.take_markup();request=p.take_request();}
        assert(request&&request->script);
        p.complete({request->id,200,mode==1?"function cachedValue(){return 18};":mode==5?bytecode:cache.source});
        std::string output;
        for(unsigned i=0;i<2000&&p.active();++i){p.step();if(auto s=p.take_markup())output=std::move(*s);if(p.ready()&&!output.empty())break;}
        if(mode<2){assert(output.find(mode==0?"Library 17":"Library 18")!=output.npos);assert(p.error().empty()&&p.stats().errors==0);assert(cache.hits==(mode==0?1u:0u));assert(cache.compiles==(mode==0?1u:2u));}
        if(mode==2){assert(!p.active()&&p.error()=="预编译脚本资源无效"&&cache.hits==1);}
        if(mode==3){assert(cache.calls==0&&p.stats().errors>0);}
        if(mode==4){assert(cache.hits==1&&p.stats().errors>0);}
        if(mode==5){assert(cache.hits==0&&p.stats().errors>0&&output.find("Library 17")==output.npos);}
    }
    std::cout<<"signed library hit/wait/miss/changed-source/header/payload/limit/order/network-bytecode PASS\n";
}
