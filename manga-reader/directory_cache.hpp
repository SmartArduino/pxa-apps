#pragma once
#include "model.hpp"

namespace manga {
// Small LRU of parsed directory responses, shared by memory and private storage.
struct DirectoryCache {
    struct Entry {std::string key,payload;};
    static constexpr unsigned capacity=4,maximum=196608,file_maximum=262144;
    std::vector<Entry> entries;
    std::size_t bytes()const {std::size_t n=0;for(auto& e:entries)n+=e.key.size()+e.payload.size();return n;}
    std::optional<std::string> get(std::string_view key){
        auto p=std::find_if(entries.begin(),entries.end(),[&](auto& e){return e.key==key;});
        if(p==entries.end())return {};auto entry=std::move(*p);entries.erase(p);entries.insert(entries.begin(),std::move(entry));return entries.front().payload;
    }
    bool put(std::string key,std::string payload){
        if(key.empty()||key.size()>4096||key.size()+payload.size()>maximum||!Json(payload).valid())return false;
        std::erase_if(entries,[&](auto& e){return e.key==key;});entries.insert(entries.begin(),{std::move(key),std::move(payload)});
        while(entries.size()>capacity||bytes()>maximum)entries.pop_back();return true;
    }
    std::string save()const {
        std::string out="{\"version\":1,\"entries\":[";bool first=true;
        for(auto& e:entries){if(!first)out+=',';first=false;out+="{\"key\":"+quote(e.key)+",\"payload\":"+e.payload+'}';}return out+"]}";
    }
    bool restore(std::string_view text){
        Json root(text);if(text.size()>file_maximum||!root.valid()||root.get("version").number()!=1)return false;
        DirectoryCache next;bool good=true;
        bool valid=root.get("entries").each([&](Json row){auto key=row.get("key").string(4096);auto payload=row.get("payload").view();
            if(!key||key->empty()||payload.empty()||std::any_of(next.entries.begin(),next.entries.end(),[&](auto& e){return e.key==*key;})||!Json(payload).valid())return good=false;
            next.entries.push_back({std::move(*key),std::string(payload)});return true;
        },capacity);
        if(!valid||!good||next.bytes()>maximum)return false;entries=std::move(next.entries);return true;
    }
};
}
