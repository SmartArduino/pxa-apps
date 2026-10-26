#pragma once
#include <stdint.h>

// Map element references survive unordered_map rehashes. Any erase must
// invalidate this borrowed lookup cache before it removes those elements.
class PxaShapeSlotCache {
    struct Entry {const void *key;uint32_t *value;};
    Entry entries[64]{};
public:
    unsigned hits=0,misses=0;
    template<class Map> uint32_t &get(Map &map,const void *key) {
        uintptr_t address=reinterpret_cast<uintptr_t>(key);
        Entry &entry=entries[((address>>4)^(address>>10)^(address>>16))&63];
        if(entry.key==key && entry.value) {++hits;return *entry.value;}
        ++misses;entry.key=key;entry.value=&map[key];return *entry.value;
    }
    void invalidate() {for(auto &entry:entries)entry={nullptr,nullptr};}
};
