#pragma once
#include "state.hpp"

namespace weread {
// Per-book private data, loaded only while its bookmark page is open. These
// positions are local UTF-8 offsets, not WeRead cloud/DOM coordinates.
struct LocalBookmark {
    std::string param,label;
    std::uint32_t chapter=0,section=0,offset=0,body_hash=0;
    bool operator==(const LocalBookmark&)const=default;
    bool same_position(const LocalBookmark& other)const{
        return chapter==other.chapter&&section==other.section&&offset==other.offset;
    }
    Resume resume(std::string_view book)const{
        return {std::string(book),param,chapter,section,offset,1};
    }
};

inline std::uint32_t bookmark_text_hash(std::string_view body){
    // Change detection only, not an authentication digest.
    std::uint32_t value=2166136261u;
    for(unsigned char c:body)value=(value^c)*16777619u;
    return value;
}
inline std::string bookmark_label(std::string_view title,std::string_view body){
    std::string label;
    auto append=[&](std::string_view text,std::size_t limit){
        const auto end=std::min<std::size_t>(192,label.size()+limit);
        for(std::size_t i=0;i<text.size()&&label.size()<end;){
            const auto c=static_cast<unsigned char>(text[i]);
            const auto n=c<128?1u:c<224?2u:c<240?3u:4u;
            if(n>text.size()-i||n>end-label.size())break;
            if(c>=32&&c!=127)label.append(text.substr(i,n));
            else if(!label.empty()&&label.back()!=' ')label+=' ';
            i+=n;
        }
    };
    append(title,96);
    if(!label.empty()&&!body.empty())label+=" · ";
    append(body,192);
    return label;
}

struct LocalBookmarks {
    static constexpr std::size_t max_entries=64,max_bytes=65536;
    std::string account,book;
    std::vector<LocalBookmark> items;
    bool valid_identity()const{
        return !account.empty()&&account.size()<=64&&clean(account)&&valid_utf8(account)&&
            !book.empty()&&book.size()<=128&&clean(book)&&valid_utf8(book);
    }
    std::string path()const{
        // Fixed-length safe filename. Collisions cannot cross accounts/books:
        // decode validates the full identity before allowing edits or writes.
        std::uint64_t hash=14695981039346656037ull;
        for(auto value:{std::string_view(account),std::string_view(book)}){
            for(unsigned char c:value)hash=(hash^c)*1099511628211ull;
            hash=(hash^0xffu)*1099511628211ull;
        }
        std::array<char,16> hex;
        const auto end=std::to_chars(hex.data(),hex.data()+hex.size(),hash,16).ptr;
        std::string out="bookmarks-";out.append(hex.data(),end);out+=".json";return out;
    }
    static bool valid(const LocalBookmark& mark){
        return !mark.param.empty()&&mark.param.size()<=1024&&clean(mark.param)&&valid_utf8(mark.param)&&
            !mark.label.empty()&&mark.label.size()<=192&&clean(mark.label)&&valid_utf8(mark.label)&&
            mark.section<=8192&&mark.offset<=196608;
    }
    std::optional<std::size_t> add(LocalBookmark mark){
        if(!valid_identity()||!valid(mark))return {};
        auto old=std::ranges::find_if(items,[&](const auto& item){return item.same_position(mark);});
        if(old!=items.end()){
            const auto index=std::size_t(old-items.begin());*old=std::move(mark);return index;
        }
        if(items.size()==max_entries)return {};
        items.push_back(std::move(mark));return items.size()-1;
    }
    std::optional<std::string> encode()const{
        if(!valid_identity()||items.size()>max_entries)return {};
        std::size_t estimate=256+account.size()+book.size();
        for(const auto& mark:items)estimate+=128+mark.param.size()+mark.label.size();
        std::string out;out.reserve(std::min(estimate,max_bytes));
        JsonObjectWriter root(out);root.number<"version">(1);root.text<"account">(account);root.text<"book">(book);
        root.key<"items">();out+='[';
        bool first=true;
        for(const auto& mark:items){
            if(!valid(mark))return {};
            if(!first)out+=',';first=false;
            JsonObjectWriter row(out);row.text<"param">(mark.param);row.text<"label">(mark.label);
            row.number<"chapter">(mark.chapter);row.number<"section">(mark.section);row.number<"offset">(mark.offset);
            row.number<"hash">(mark.body_hash);row.close();
            if(out.size()>max_bytes-2)return {};
        }
        out+=']';root.close();return out;
    }
    [[gnu::noinline]] bool decode(std::string_view raw){
        if(!valid_identity()||raw.size()>max_bytes)return false;
        Json root(raw);
        const auto [version,owner,id,rows]=root.select({"version","account","book","items"});
        if(!root.valid()||version.number()!=1||owner.string(64)!=account||id.string(128)!=book)return false;
        LocalBookmarks next{account,book,{}};bool valid_rows=true;
        if(!rows.each([&](Json row){
            const auto [param,label,chapter,section,offset,hash]=row.select({"param","label","chapter","section","offset","hash"});
            auto p=param.string(1024),l=label.string(192);
            auto c=chapter.number(),s=section.number(),o=offset.number(),h=hash.number();
            if(!p||!l||!c||!s||!o||!h){valid_rows=false;return false;}
            LocalBookmark mark{std::move(*p),std::move(*l),*c,*s,*o,*h};
            if(!valid(mark)||std::ranges::any_of(next.items,[&](const auto& old){return old.same_position(mark);})){valid_rows=false;return false;}
            next.items.push_back(std::move(mark));return true;
        },max_entries)||!valid_rows)return false;
        *this=std::move(next);return true;
    }
};
} // namespace weread
