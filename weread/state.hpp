#pragma once
#include "protocol.hpp"
#include "json_writer.hpp"

namespace weread {
struct Preferences {
    unsigned font=18;
    unsigned leading=150;
    bool night=false,remember=true,volume_keys=false,images=true;
    constexpr bool operator==(const Preferences&)const=default;
};
struct Resume {
    std::string book,param;
    std::uint32_t chapter=0,section=0,offset=0,text_version=0;
    bool operator==(const Resume&)const=default;
    std::string query()const{return "?bc="+percent(param)+"&sect="+number(section);}
};
// A bounded, lazy LRU of positions. Store byte offsets, never pre-rendered
// pages; a font or screen change cannot invalidate the saved position.
struct SavedState {
    static constexpr std::size_t max_books=32,max_bytes=49152;
    Preferences preferences;
    std::vector<Resume> progress;
    std::string account;
    void bind_account(std::string_view vid){
        if(vid.empty()||vid.size()>64||!clean(vid))return;
        if(!account.empty()&&account!=vid)progress.clear();
        account=vid;
    }
    const Resume* find(std::string_view book)const {
        auto i=std::ranges::find(progress,book,&Resume::book);return i==progress.end()?nullptr:&*i;
    }
    void remember(Resume item){
        if(item.book.empty()||item.book.size()>128||item.param.empty()||item.param.size()>1024)return;
        auto i=std::ranges::find(progress,item.book,&Resume::book);
        if(i!=progress.end()){
            if(*i==item)return;
            *i=std::move(item);std::rotate(i,i+1,progress.end());return;
        }
        if(progress.size()==max_books)progress.erase(progress.begin());
        progress.push_back(std::move(item));
    }
    std::string encode()const {
        std::size_t estimate=256+account.size();
        for(const auto& p:progress)estimate+=120+p.book.size()+p.param.size();
        std::string out;out.reserve(estimate);
        JsonObjectWriter root(out);root.number<"version">(1);root.text<"account">(account);root.key<"preferences">();
        JsonObjectWriter pref(out);pref.number<"font">(preferences.font);pref.number<"leading">(preferences.leading);
        pref.boolean<"night">(preferences.night);pref.boolean<"remember">(preferences.remember);
        pref.boolean<"volume">(preferences.volume_keys);pref.boolean<"images">(preferences.images);pref.close();
        root.key<"progress">();out+='[';
        bool first=true;
        for(const auto& p:progress){
            if(!first)out+=',';first=false;
            JsonObjectWriter item(out);item.text<"book">(p.book);item.text<"param">(p.param);item.number<"chapter">(p.chapter);
            item.number<"section">(p.section);item.number<"offset">(p.offset);item.number<"text">(p.text_version);item.close();
        }
        out+=']';root.close();return out;
    }
    bool decode(std::string_view raw){
        Json j(raw);if(raw.size()>max_bytes || !j.valid() || j.get("version").number()!=1)return false;
        SavedState candidate;auto pref=j.get("preferences");
        if(!j.get("account").raw().empty()){
            auto account=j.get("account").string(64);if(!account||!clean(*account))return false;
            candidate.account=std::move(*account);
        }
        auto font=pref.get("font").number(),leading=pref.get("leading").number();
        if(!font||*font<12||*font>28||!leading||*leading<120||*leading>200)return false;
        candidate.preferences.font=*font;candidate.preferences.leading=*leading;
        auto read_bool=[](Json value,bool& out){
            if(value.raw()=="true")out=true;else if(value.raw()=="false")out=false;else return false;return true;
        };
        if(!read_bool(pref.get("night"),candidate.preferences.night)||!read_bool(pref.get("remember"),candidate.preferences.remember)||
           !read_bool(pref.get("volume"),candidate.preferences.volume_keys))return false;
        if(!pref.get("images").raw().empty()&&!read_bool(pref.get("images"),candidate.preferences.images))return false;
        bool valid=true;
        if(!j.get("progress").each([&](Json value){
            auto book=value.get("book").string(128),param=value.get("param").string(1024);
            auto chapter=value.get("chapter").number(),section=value.get("section").number(),offset=value.get("offset").number();
            if(!book||book->empty()||!param||param->empty()||!clean(*book)||!clean(*param)||!chapter||!section||!offset||
                *offset>196608||*section>8192||candidate.find(*book)){valid=false;return false;}
            auto text_version=value.get("text").number().value_or(0);
            if(text_version>1||(!value.get("text").raw().empty()&&!value.get("text").number())){valid=false;return false;}
            candidate.progress.push_back({std::move(*book),std::move(*param),*chapter,*section,*offset,text_version});return true;
        },max_books)||!valid)return false;
        *this=std::move(candidate);return true;
    }
};
} // namespace weread
