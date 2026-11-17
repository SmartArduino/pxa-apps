#pragma once
#include "model.hpp"
#include <array>
namespace manga {
struct CachedPage {std::string key;unsigned bytes=0;std::uint64_t touched=0;};
struct PageCache {
    static constexpr unsigned slots=24,maximum=2*1024*1024;
    std::array<CachedPage,slots> pages{};
    bool enabled=true,storage_verified=false;unsigned mode=0,limit=0;std::uint64_t clock=0;
    unsigned used()const{unsigned n=0;for(auto& p:pages)n+=p.bytes;return n;}
    unsigned count()const{unsigned n=0;for(auto& p:pages)n+=!p.key.empty();return n;}
    int find(std::string_view key)const{for(unsigned i=0;i<slots;++i)if(pages[i].key==key&&!key.empty())return i;return -1;}
    int oldest()const{int oldest=-1;for(unsigned i=0;i<slots;++i)if(!pages[i].key.empty()&&(oldest<0||pages[i].touched<pages[oldest].touched))oldest=i;return oldest;}
    int vacant()const{for(unsigned i=0;i<slots;++i)if(pages[i].key.empty())return i;return -1;}
    void touch(unsigned slot){pages[slot].touched=++clock;}
    void budget(std::uint64_t free,bool known){limit=known&&free>=8*1024*1024?unsigned(std::min<std::uint64_t>(maximum,free/16)):0;}
    std::string save()const{
        std::string s="{\"version\":1,\"enabled\":"+std::to_string(enabled)+",\"mode\":"+std::to_string(mode)+",\"verified\":"+std::to_string(storage_verified)+",\"pages\":[";bool first=true;
        for(unsigned i=0;i<slots;++i)if(!pages[i].key.empty()){if(!first)s+=',';first=false;s+="{\"slot\":"+std::to_string(i)+",\"key\":"+quote(pages[i].key)+",\"bytes\":"+std::to_string(pages[i].bytes)+",\"used\":"+std::to_string(pages[i].touched)+"}";}
        return s+"]}";
    }
    bool restore(std::string_view text){
        if(text.size()>49152)return false;Json root(text);if(!root.valid()||root.get("version").number()!=1||root.get("mode").number()>2)return false;
        PageCache next;next.enabled=root.get("enabled").number()!=0;next.mode=root.get("mode").number();next.storage_verified=root.get("verified").number()==1;bool good=true;
        bool parsed=root.get("pages").each([&](Json j){auto slot=j.get("slot").number(),bytes=j.get("bytes").number();auto key=j.get("key").string().value_or("");auto used=j.get("used").number();
            if(slot>=slots||bytes==0||bytes>524288||key.size()>1024||!web(key)||!next.pages[slot].key.empty()||next.find(key)>=0){good=false;return false;}
            next.pages[slot]={std::move(key),unsigned(bytes),used};next.clock=std::max(next.clock,std::uint64_t(used));return true;});
        if(!parsed||!good||next.used()>maximum)return false;*this=std::move(next);return true;
    }
};
}
