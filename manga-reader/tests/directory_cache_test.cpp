#include "../directory_cache.hpp"
#include <cassert>
#include <iostream>
using namespace manga;
int main(){
    DirectoryCache c;const std::string payload=R"({"chapters":[{"id":"chapter","title":"第一话"}],"groups":[],"groupIndex":0,"total":1})";
    for(auto key:{"A/zh/asc/0","A/zh/desc/0","A/en/asc/0","B/zh/asc/0"})assert(c.put(key,payload));
    assert(c.get("A/zh/asc/0")==payload);assert(!c.get("A/zh/asc/20"));assert(!c.get("A/zh/group2/0"));
    assert(c.put("C/zh/asc/0",payload));assert(!c.get("A/zh/desc/0"));assert(c.get("A/zh/asc/0"));
    DirectoryCache restored;assert(restored.restore(c.save()));assert(restored.get("B/zh/asc/0")==payload);
    auto stable=restored.save();assert(!restored.restore("[]"));assert(restored.save()==stable);
    assert(!restored.restore(R"({"version":1,"entries":[{"key":"same","payload":{}},{"key":"same","payload":{}}]})"));assert(restored.save()==stable);
    assert(!restored.put("bad","{"));assert(restored.save()==stable);
    assert(!restored.put("too-large",quote(std::string(DirectoryCache::maximum,'x'))));assert(restored.save()==stable);
    assert(restored.put("large",quote(std::string(120000,'x'))));assert(restored.put("new-large",quote(std::string(120000,'y'))));
    assert(restored.bytes()<=DirectoryCache::maximum&&!restored.get("large")&&restored.get("new-large"));
    assert(restored.save().size()<=DirectoryCache::file_maximum);
    std::cout<<"Directory persistence, scoped keys, LRU and bounded atomic restore passed\n";
}
