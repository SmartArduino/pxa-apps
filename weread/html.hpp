#pragma once
#include <optional>
#include <charconv>
#include <cstdint>
#include <string_view>

namespace weread::html {
inline bool space(char c){return c==' '||c=='\n'||c=='\r'||c=='\t'||c=='\f';}
inline char lower(char c){return c>='A'&&c<='Z'?char(c+32):c;}
inline bool equal(std::string_view a,std::string_view b){
    if(a.size()!=b.size())return false;for(std::size_t i=0;i<a.size();++i)if(lower(a[i])!=lower(b[i]))return false;return true;
}
inline bool name_char(char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c==':';}
inline std::optional<std::uint32_t> entity(std::string_view name){
    if(name=="nbsp")return ' ';if(name=="amp")return '&';if(name=="lt")return '<';
    if(name=="gt")return '>';if(name=="quot")return '"';if(name=="apos")return '\'';
    if(!name.starts_with('#'))return {};
    name.remove_prefix(1);int base=10;
    if(name.starts_with('x')||name.starts_with('X')){base=16;name.remove_prefix(1);}
    std::uint32_t cp=0;const auto [end,ec]=std::from_chars(name.data(),name.data()+name.size(),cp,base);
    if(ec!=std::errc{}||end!=name.data()+name.size()||!cp||cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff))return {};
    return cp;
}
struct Tag {
    std::string_view name,attributes;
    std::size_t begin=0,end=0;
    bool closing=false,single=false;
    std::optional<std::string_view> attribute(std::string_view key)const {
        std::size_t at=0;
        while(at<attributes.size()){
            while(at<attributes.size()&&(space(attributes[at])||attributes[at]=='/'))++at;
            const auto begin=at;while(at<attributes.size()&&name_char(attributes[at]))++at;
            if(at==begin){++at;continue;}
            auto name=attributes.substr(begin,at-begin);
            while(at<attributes.size()&&space(attributes[at]))++at;
            std::string_view value;
            if(at<attributes.size()&&attributes[at]=='='){
                ++at;while(at<attributes.size()&&space(attributes[at]))++at;
                const char quote=at<attributes.size()&&(attributes[at]=='\''||attributes[at]=='"')?attributes[at++]:0;
                const auto start=at;
                while(at<attributes.size()&&(quote?attributes[at]!=quote:!space(attributes[at])))++at;
                value=attributes.substr(start,at-start);if(quote&&at<attributes.size())++at;
            }
            if(equal(name,key))return value;
        }
        return {};
    }
};
inline bool next(std::string_view input,std::size_t& at,Tag& tag){
    while(at<input.size()){
        auto begin=input.find('<',at);if(begin==input.npos){at=input.size();return false;}
        at=begin+1;
        if(input.substr(begin).starts_with("<!--")){
            auto end=input.find("-->",at+3);at=end==input.npos?input.size():end+3;continue;
        }
        bool closing=at<input.size()&&input[at]=='/';if(closing)++at;
        const auto name=at;while(at<input.size()&&name_char(input[at]))++at;
        if(name==at)continue;
        const auto attributes=at;char quote=0;
        while(at<input.size()){
            char c=input[at];
            if(quote){if(c==quote)quote=0;}
            else if(c=='"'||c=='\'')quote=c;
            else if(c=='>')break;
            ++at;
        }
        if(at==input.size())return false;
        tag={input.substr(name,attributes-name),input.substr(attributes,at-attributes),begin,at+1,closing,at>attributes&&input[at-1]=='/'};
        ++at;
        for(auto single:{"br","hr","img","meta","link","input","source","wbr","area","base","embed","param","col"})if(equal(tag.name,single))tag.single=true;
        return true;
    }
    return false;
}
inline std::size_t raw_end(std::string_view input,std::size_t at,std::string_view name){
    while(at<input.size()){
        at=input.find("</",at);if(at==input.npos)return input.size();
        const auto boundary=at+2+name.size();
        if(boundary<input.size()&&equal(input.substr(at+2,name.size()),name)&&
           (space(input[boundary])||input[boundary]=='>'||input[boundary]=='/')){
            auto end=input.find('>',at+2+name.size());if(end==input.npos)return input.size();return end+1;}
        at+=2;
    }
    return input.size();
}
// Borrow the server-rendered body. Reader state deliberately omits this text;
// duplicating its HTML in a JSON field would waste bandwidth and memory.
inline std::optional<std::string_view> element(std::string_view input,std::string_view id){
    std::size_t at=0,begin=0;unsigned depth=0;std::string_view name;Tag tag;
    while(next(input,at,tag)){
        if(depth){
            if(equal(tag.name,name)){
                if(tag.closing){if(!--depth)return input.substr(begin,tag.begin-begin);}
                else if(!tag.single)++depth;
            }
        }else if(!tag.closing && tag.attribute("id")==id){
            if(tag.single)return std::string_view{};begin=tag.end;name=tag.name;depth=1;
        }
        if(!tag.closing&&(equal(tag.name,"style")||equal(tag.name,"script")))at=raw_end(input,at,tag.name);
    }
    return {};
}
} // namespace weread::html
