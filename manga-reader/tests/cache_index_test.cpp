#include "../cache_model.hpp"
#include <cassert>
#include <iostream>
using namespace manga;
int main(){
    const std::string legacy=R"({"version":1,"enabled":1,"mode":0,"verified":1,"pages":[{"slot":7,"key":"manga:source:book:chapter:9:rgb3","bytes":4096,"used":4294967300}]})";
    PageCache c;assert(c.restore(legacy));assert(c.indexed[7]&&c.find("manga:source:book:chapter:9:rgb3")==7);
    PageCache migrated;assert(migrated.restore(c.save()));assert(migrated.pages[7].touched==4294967300ull&&migrated.enabled&&migrated.storage_verified);
    // Failed/abandoned publication can still commit in the Host: preserve the
    // conservative set until a confirmed invalidation removes the old entry.
    c.pages[8]={"https://cdn.test/path/1.png",1024,++c.clock};assert(!c.indexed[8]);auto before=c.prepare_index();assert(c.indexed[8]);
    c.pages[8]={};assert(c.indexed[8]);auto invalidate=c.prepare_index();c.persisted_index(invalidate);assert(!c.indexed[8]);assert(before[8]&&!invalidate[8]);
    auto stable=c.save();
    for(auto rows:{"[[7,1,\"a\",1,1]]","[[7,0,\"a\",1,1],[7,0,\"b\",1,2]]","[[7,0,\"a\",1,1],[8,0,\"a\",1,2]]","[[7,0,\"a\",1,18446744073709551615]]","[[7,0,\"a\",1,1,9]]","[[7,0,\"a\",0,1]]"}){
        assert(!c.restore(std::string("{\"version\":2,\"mode\":0,\"prefixes\":[\"https://cdn.test/\"],\"pages\":")+rows+"}"));assert(c.save()==stable);
    }
    PageCache big;std::string old="{\"version\":1,\"mode\":0,\"pages\":[";
    for(unsigned i=0;i<106;++i){auto key="manga:copymanga:longbook:longchapter:"+std::to_string(i)+":rgb3";big.pages[i]={key,49152,i+1};if(i)old+=',';old+="{\"slot\":"+std::to_string(i)+",\"key\":"+quote(key)+",\"bytes\":49152,\"used\":"+std::to_string(i+1)+"}";}
    old+="]}";PageCache v1,v2;assert(v1.restore(old)&&v2.restore(big.save()));assert(v2.count()==106&&big.save().size()<old.size()/2);
    for(unsigned i=0;i<106;++i)assert(v1.pages[i].key==v2.pages[i].key&&v1.pages[i].bytes==v2.pages[i].bytes&&v1.pages[i].touched==v2.pages[i].touched);
    std::cout<<"Legacy cache migration, compact exact-key round trip, cancelled publications and malformed-index rejection passed\n";
}
