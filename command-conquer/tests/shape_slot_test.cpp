#include "pxa_shape_slots.h"
#include <unordered_map>
#include <cassert>
#include <cstdio>
struct Map : std::unordered_map<const void*,uint32_t> {
    unsigned lookups=0;
    uint32_t &operator[](const void *key) {++lookups;return std::unordered_map<const void*,uint32_t>::operator[](key);}
};
int main() {
    Map values;PxaShapeSlotCache cache;unsigned keys[2048]{};
    uint32_t &first=cache.get(values,keys);first=56789;
    for(unsigned i=0;i<10000;++i)assert(cache.get(values,keys)==56789);
    assert(values.lookups==1 && cache.hits==10000);
    // A rehash must not invalidate the cached reference, including updates
    // through the returned reference and changes made through the map.
    for(unsigned i=1;i<2048;++i)values[&keys[i]]=i;
    values[keys]=1234;assert(cache.get(values,keys)==1234);
    cache.get(values,keys)=4321;assert(values.at(keys)==4321);
    unsigned state=17;
    for(unsigned i=0;i<20000;++i) {
        state=state*1664525u+1013904223u;unsigned k=state%2048;
        assert(cache.get(values,&keys[k])==values.at(&keys[k]));
        if(i%19==0) {cache.invalidate();values.erase(&keys[k]);values[&keys[k]]=state;}
        if(i%71==0) {cache.invalidate();values.clear();for(unsigned n=0;n<2048;++n)values[&keys[n]]=n+i;}
    }
    std::puts("PASS: 10000 cache hits avoid map lookups; 20000 rehash/update/collision/erase/reset operations preserve references and values");
}
