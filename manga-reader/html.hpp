#pragma once
#include "model.hpp"
namespace manga {
// Small borrowed tag views for public directory fragments; no DOM or scripts.
inline bool html_space(char c){return c==' '||c=='\n'||c=='\r'||c=='\t';}
inline std::optional<std::string_view> html_attribute(std::string_view tag,std::string_view key){
    size_t p=tag.find_first_of(" \n\r\t");if(p==tag.npos)return {};
    while(p<tag.size()){
        while(p<tag.size()&&html_space(tag[p]))++p;if(p==tag.size()||tag[p]=='/'||tag[p]=='>')break;
        size_t start=p;while(p<tag.size()&&!html_space(tag[p])&&tag[p]!='='&&tag[p]!='>')++p;auto name=tag.substr(start,p-start);
        while(p<tag.size()&&html_space(tag[p]))++p;if(p==tag.size()||tag[p]!='=')continue;++p;
        while(p<tag.size()&&html_space(tag[p]))++p;if(p==tag.size())return {};
        char quote=tag[p]=='\''||tag[p]=='"'?tag[p++]:0;start=p;
        if(quote){p=tag.find(quote,p);if(p==tag.npos)return {};}
        else while(p<tag.size()&&!html_space(tag[p])&&tag[p]!='>')++p;
        auto value=tag.substr(start,p-start);if(name==key)return value;if(quote)++p;
    }return {};
}
inline void html_utf8(std::string& out,unsigned c){
    if(c<128)out+=char(c);else if(c<2048){out+=char(0xc0|(c>>6));out+=char(0x80|(c&63));}
    else if(c<65536){out+=char(0xe0|(c>>12));out+=char(0x80|((c>>6)&63));out+=char(0x80|(c&63));}
    else{out+=char(0xf0|(c>>18));out+=char(0x80|((c>>12)&63));out+=char(0x80|((c>>6)&63));out+=char(0x80|(c&63));}
}
inline std::string html_text(std::string_view raw,size_t limit=192){
    std::string out;bool space=false;
    for(size_t i=0;i<raw.size();){
        char c=raw[i++];if(c=='<'){auto end=raw.find('>',i);if(end==raw.npos)return {};i=end+1;continue;}
        if(html_space(c)){space=!out.empty();continue;}if(space){out+=' ';space=false;}
        if(c=='&'){
            auto end=raw.find(';',i);if(end!=raw.npos&&end-i<=12){auto entity=raw.substr(i,end-i);unsigned cp=0;
                if(entity=="amp")cp='&';else if(entity=="quot")cp='"';else if(entity=="apos"||entity=="#39")cp='\'';
                else if(entity=="lt")cp='<';else if(entity=="gt")cp='>';else if(entity=="nbsp")cp=' ';
                else if(entity.starts_with("#")){entity.remove_prefix(1);int base=10;if(entity.starts_with("x")||entity.starts_with("X")){base=16;entity.remove_prefix(1);}
                    auto [last,ec]=std::from_chars(entity.data(),entity.data()+entity.size(),cp,base);if(ec!=std::errc{}||last!=entity.data()+entity.size())cp=0;}
                if(cp>=32&&cp<=0x10ffff&&!(cp>=0xd800&&cp<=0xdfff)){html_utf8(out,cp);if(out.size()>limit)return {};i=end+1;continue;}
            }
        }
        if(static_cast<unsigned char>(c)<32)return {};out+=c;if(out.size()>limit)return {};
    }return out.size()<=limit?out:std::string{};
}
inline std::string image_uri(std::string_view raw){
    auto decoded=html_text(raw,1024);std::string out;constexpr char hex[]="0123456789ABCDEF";
    for(unsigned char c:decoded){if(c<32)return {};if(c>=128||c==' '){out+='%';out+=hex[c>>4];out+=hex[c&15];}else out+=char(c);}
    return web(out)?out:std::string{};
}
template<class F> bool html_tags(std::string_view html,std::string_view name,F f,bool paired=false){
    std::string prefix="<"+std::string(name),close="</"+std::string(name)+">";size_t p=0;unsigned count=0;
    while((p=html.find(prefix,p))!=html.npos){auto start=p;p+=prefix.size();if(p==html.size())return false;
        if(!html_space(html[p])&&html[p]!='>'&&html[p]!='/')continue;
        auto end=html.find('>',p);if(end==html.npos||++count>8192)return false;auto tag=html.substr(start,end-start+1);p=end+1;
        std::string_view body;if(paired){auto last=html.find(close,p);if(last==html.npos)return false;body=html.substr(p,last-p);p=last+close.size();}
        if(!f(tag,body))return false;
    }return true;
}
inline std::optional<std::string_view> html_div(std::string_view html,std::string_view marker){
    auto at=html.find(marker);if(at==html.npos)return {};
    at=html.find('>',at);if(at==html.npos)return {};auto first=++at;unsigned depth=1,count=0;
    while(at<html.size()&&++count<8192){
        auto open=html.find("<div",at),close=html.find("</div",at);
        if(close==html.npos)return {};
        if(open<close){at=open+4;if(at<html.size()&&(html_space(html[at])||html[at]=='>'))++depth;}
        else {if(!--depth)return html.substr(first,close-first);at=close+5;}
    }return {};
}
}
