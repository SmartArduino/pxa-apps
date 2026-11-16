#pragma once
#include "model.hpp"
#include "html.hpp"
#include <array>
extern "C" {
#include "vendor/tiny-aes/aes.h"
}
namespace manga {
inline constexpr std::string_view copy_api="https://api.copy4000.com/api/v3";
inline std::string referrer(const Book& b){switch(source_kind(b.source)){
    case SourceKind::mangadex:return "https://mangadex.org/";
    case SourceKind::copy:return "https://www.mangacopy.com/";
    case SourceKind::katana:return "https://mangakatana.com/";
    default:return {};}}
inline bool copy_books(Json j,std::vector<Book>& out){if(j.get("code").number()!=200)return false;std::vector<Book> next;bool ok=true;
    bool valid=j.get("results").get("list").each([&](Json row){auto id=row.get("path_word").string(192),t=row.get("name").string(192);if(!id||!t||!slug(*id)){ok=false;return false;}next.push_back({*id,*t,"copymanga"});return true;},32);
    if(!valid||!ok)return false;out=std::move(next);return true;}
inline bool copy_chapters(Json j,std::vector<Chapter>& out,std::string_view id){if(j.get("code").number()!=200)return false;std::vector<Chapter> next;bool ok=true;
    bool valid=j.get("results").get("list").each([&](Json row){auto uuid_value=row.get("uuid").string(64),t=row.get("name").string(192);if(!uuid_value||!t||!uuid(*uuid_value)){ok=false;return false;}next.push_back({copy_web(id)+"/chapter/"+*uuid_value,*t});return true;},64);
    if(!valid||!ok)return false;out=std::move(next);return true;}
inline constexpr std::string_view katana_base="https://mangakatana.com";
inline bool katana_books(std::string_view html,std::vector<Book>& out,bool& more){
    auto container=html_div(html,"id=\"book_list\"");
    if(!container){
        std::string id,title;
        html_tags(html,"link",[&](auto tag,auto){if(html_attribute(tag,"rel").value_or("")=="canonical")id=std::string(html_attribute(tag,"href").value_or(""));return true;});
        html_tags(html,"h1",[&](auto,auto body){if(title.empty())title=html_text(body);return true;},true);
        if(!katana_book_url(id)||title.empty())return false;out={{std::move(id),std::move(title),"mangakatana"}};more=false;return true;
    }
    auto list=*container;
    std::vector<Book> next;bool ok=true;
    bool valid=html_tags(list,"h3",[&](auto tag,auto body){
        if(html_attribute(tag,"class").value_or("").find("title")==std::string_view::npos)return true;
        return html_tags(body,"a",[&](auto anchor,auto label){auto raw=html_attribute(anchor,"href");if(!raw)return true;
            auto id=resolve(katana_base,*raw);auto title=html_text(label);
            if(!katana_book_url(id)||title.empty()){ok=false;return false;}if(next.size()==64){ok=false;return false;}
            next.push_back({std::move(id),std::move(title),"mangakatana"});return true;},true);
    },true);
    if(!valid||!ok)return false;more=html.find("next page-numbers")!=html.npos;out=std::move(next);return true;
}
inline bool katana_chapters(std::string_view html,std::string_view book,std::vector<Chapter>& out){
    auto start=html.find("class=\"chapters\"");if(start==html.npos)return false;
    auto end=html.find("</table>",start);if(end==html.npos)return false;
    std::vector<Chapter> next;bool ok=true;
    bool valid=html_tags(html.substr(start,end-start),"a",[&](auto tag,auto body){auto href=html_attribute(tag,"href");if(!href)return true;
        auto id=resolve(katana_base,*href);auto title=html_text(body);
        if(!id.starts_with(std::string(book)+"/c"))return true;
        if(!web(id)||title.empty()||next.size()==2048){ok=false;return false;}
        next.push_back({std::move(id),std::move(title)});return true;},true);
    if(!valid||!ok||next.empty())return false;
    // The public directory is newest first; the app starts with reading order.
    std::reverse(next.begin(),next.end());out=std::move(next);return true;
}
inline bool katana_pages(std::string_view html,std::vector<std::string>& out){
    auto binding=html.find("data-src',");if(binding==html.npos)binding=html.find("data-src\",");if(binding==html.npos)return false;
    auto p=binding+10;while(p<html.size()&&html_space(html[p]))++p;auto start=p;
    while(p<html.size()&&((html[p]>='a'&&html[p]<='z')||(html[p]>='A'&&html[p]<='Z')||(html[p]>='0'&&html[p]<='9')||html[p]=='_'))++p;
    if(p==start||p-start>32)return false;auto variable=html.substr(start,p-start);
    auto assignment=html.find("var "+std::string(variable)+"=",0);if(assignment==html.npos)return false;
    p=assignment+4+variable.size()+1;while(p<html.size()&&html_space(html[p]))++p;if(p==html.size()||html[p++]!='[')return false;
    std::vector<std::string> next;
    for(;;){while(p<html.size()&&html_space(html[p]))++p;if(p==html.size())return false;if(html[p]==']')break;
        char quote=html[p++];if(quote!='\''&&quote!='\"')return false;auto last=html.find(quote,p);if(last==html.npos)return false;
        auto raw=html.substr(p,last-p);if(raw.find('\\')!=raw.npos)return false;auto url=image_uri(raw);if(url.empty()||!url.starts_with("https://")||next.size()==512)return false;
        next.push_back(std::move(url));p=last+1;while(p<html.size()&&html_space(html[p]))++p;
        if(p==html.size()||html[p]==']')break;if(html[p++]!=',')return false;
    }
    if(next.empty())return false;out=std::move(next);return true;
}
inline std::optional<std::string> js_literal(std::string_view html,std::string_view name,size_t limit){
    size_t at=0;while((at=html.find(name,at))!=html.npos){auto p=at+name.size();++at;
        auto space=[&]{while(p<html.size()&&(html[p]==' '||html[p]=='\t'||html[p]=='\r'||html[p]=='\n'))++p;};
        space();if(p==html.size()||html[p++]!='=')continue;space();if(p==html.size()||(html[p]!='\''&&html[p]!='"'))continue;
        char quote=html[p++];auto end=html.find(quote,p);if(end==html.npos||end-p>limit)return {};auto value=html.substr(p,end-p);if(value.find('\\')!=value.npos)return {};return std::string(value);
    }return {};
}
// Read only two public string assignments and the AES-wrapped image list.
// No JavaScript execution, login or privileged chapter API is involved.
inline bool copy_pages(std::string_view html,std::vector<std::string>& out){
    auto key=js_literal(html,"var cct",16),content=js_literal(html,"var contentKey",160*1024);
    if(!key||key->size()!=16||!content||content->size()<48)return false;
    auto hex=std::string_view(*content).substr(16);if(hex.size()%32||hex.size()>160*1024)return false;
    auto digit=[](char c)->int{return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;};
    std::string bytes;bytes.reserve(hex.size()/2);for(size_t i=0;i<hex.size();i+=2){int a=digit(hex[i]),b=digit(hex[i+1]);if(a<0||b<0)return false;bytes+=char(a*16+b);}
    AES_ctx context{};AES_init_ctx_iv(&context,reinterpret_cast<const uint8_t*>(key->data()),reinterpret_cast<const uint8_t*>(content->data()));
    AES_CBC_decrypt_buffer(&context,reinterpret_cast<uint8_t*>(bytes.data()),bytes.size());
    unsigned padding=static_cast<unsigned char>(bytes.back());if(!padding||padding>16||padding>bytes.size())return false;
    for(size_t i=bytes.size()-padding;i<bytes.size();++i)if(static_cast<unsigned char>(bytes[i])!=padding)return false;
    bytes.resize(bytes.size()-padding);Json j(bytes);if(!j.valid())return false;std::vector<std::string> next;bool ok=true;
    bool valid=j.each([&](Json row){auto u=row.get("url").string();if(!u||!web(*u)){ok=false;return false;}next.push_back(*u);return true;},512);
    if(!valid||!ok||next.empty())return false;out=std::move(next);return true;
}
}
