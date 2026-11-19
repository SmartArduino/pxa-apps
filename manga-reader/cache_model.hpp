#pragma once
#include "model.hpp"
#include <array>
namespace manga {
inline std::string page_cache_key(const Book& book,unsigned page){auto s="manga:"+encoded(book.source)+":"+encoded(book.id)+":"+encoded(book.chapter)+":"+std::to_string(page);return s.size()<=1024?s:std::string{};}
inline bool valid_cache_key(std::string_view key){return web(key)||(key.starts_with("manga:")&&key.size()<=1024&&key.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.%~:")==key.npos);}
struct CachedPage {std::string key;unsigned bytes=0;std::uint64_t touched=0;};
struct PageCache {
    static constexpr unsigned slots=128,maximum=64*1024*1024,index_max=196608;
    std::array<CachedPage,slots> pages{};
    using IndexSlots=std::array<bool,slots>;IndexSlots indexed{};
    IndexSlots prepare_index(){IndexSlots snapshot{};for(unsigned i=0;i<slots;++i){snapshot[i]=!pages[i].key.empty();indexed[i]=indexed[i]||snapshot[i];}return snapshot;}
    void persisted_index(const IndexSlots& snapshot){indexed=snapshot;}
    bool enabled=true,storage_verified=false;unsigned mode=0,limit=0;std::uint64_t clock=0;
    unsigned used()const{unsigned n=0;for(auto& p:pages)n+=p.bytes;return n;}
    unsigned count()const{unsigned n=0;for(auto& p:pages)n+=!p.key.empty();return n;}
    int find(std::string_view key)const{for(unsigned i=0;i<slots;++i)if(pages[i].key==key&&!key.empty())return i;return -1;}
    int oldest()const{int oldest=-1;for(unsigned i=0;i<slots;++i)if(!pages[i].key.empty()&&(oldest<0||pages[i].touched<pages[oldest].touched))oldest=i;return oldest;}
    int vacant()const{for(unsigned i=0;i<slots;++i)if(pages[i].key.empty())return i;return -1;}
    void touch(unsigned slot){pages[slot].touched=++clock;}
    void budget(std::uint64_t free,bool known){constexpr unsigned reserve=2*1024*1024;limit=known&&free>reserve?unsigned(std::min<std::uint64_t>(maximum,used()+(free-reserve)/2)):0;}
    std::string save()const{
        // Share the long chapter/directory prefix; keep exact keys, not hashes.
        std::vector<std::string_view> prefixes;std::string rows;bool first=true;
        for(unsigned i=0;i<slots;++i)if(!pages[i].key.empty()){
            std::string_view key=pages[i].key;auto split=key.rfind('/');
            if(key.starts_with("manga:")){split=key.rfind(':');if(key.ends_with(":rgb3")&&split)split=key.rfind(':',split-1);}
            split=split==key.npos?0:split+1;auto prefix=key.substr(0,split),tail=key.substr(split);
            auto found=std::find(prefixes.begin(),prefixes.end(),prefix);unsigned group=found-prefixes.begin();if(found==prefixes.end())prefixes.push_back(prefix);
            if(!first)rows+=',';first=false;rows+='['+std::to_string(i)+','+std::to_string(group)+','+quote(tail)+','+std::to_string(pages[i].bytes)+','+std::to_string(pages[i].touched)+']';
        }
        std::string text="{\"version\":2,\"enabled\":"+std::to_string(enabled)+",\"mode\":"+std::to_string(mode)+",\"verified\":"+std::to_string(storage_verified)+",\"prefixes\":[";
        first=true;for(auto prefix:prefixes){if(!first)text+=',';first=false;text+=quote(prefix);}return text+"],\"pages\":["+rows+"]}";
    }
    bool restore(std::string_view text){
        if(text.size()>index_max)return false;Json root(text);auto version=root.get("version").number();if(!root.valid()||(version!=1&&version!=2)||root.get("mode").number()>2)return false;
        PageCache next;next.enabled=root.get("enabled").number()!=0;next.mode=root.get("mode").number();next.storage_verified=root.get("verified").number()==1;bool good=true;
        std::vector<std::string> prefixes;
        if(version==2){bool parsed=root.get("prefixes").each([&](Json j){auto p=j.string();if(!p){good=false;return false;}prefixes.push_back(std::move(*p));return true;},slots);if(!parsed||!good)return false;}
        bool parsed=root.get("pages").each([&](Json j){unsigned slot=slots,bytes=0;std::uint64_t used=0;std::string key;
            if(version==1){slot=j.get("slot").number(slots);bytes=j.get("bytes").number();key=j.get("key").string().value_or("");auto v=j.get("used").view();auto [p,e]=std::from_chars(v.data(),v.data()+v.size(),used);if(e!=std::errc{}||p!=v.data()+v.size()){good=false;return false;}}
            else {std::array<Json,5> values;unsigned n=0;bool shape=j.each([&](Json v){if(n==values.size()){good=false;return false;}values[n++]=v;return true;},6);
                if(!shape||!good||n!=5){good=false;return false;}slot=values[0].number(slots);auto group=values[1].number(slots);auto tail=values[2].string();bytes=values[3].number();
                auto v=values[4].view();auto [p,e]=std::from_chars(v.data(),v.data()+v.size(),used);if(!tail||group>=prefixes.size()||e!=std::errc{}||p!=v.data()+v.size()){good=false;return false;}key=prefixes[group]+*tail;
            }
            if(slot>=slots||bytes==0||bytes>524288||key.size()>1024||!valid_cache_key(key)||!next.pages[slot].key.empty()||next.find(key)>=0||used==UINT64_MAX){good=false;return false;}
            next.pages[slot]={std::move(key),bytes,used};next.indexed[slot]=true;next.clock=std::max(next.clock,used);return true;
        },slots);
        if(!parsed||!good||next.used()>maximum)return false;*this=std::move(next);return true;
    }
};
}
