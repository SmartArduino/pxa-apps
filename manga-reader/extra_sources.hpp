#pragma once
#include "chinese_sources.hpp"

namespace manga {
// Public formats referenced by Keiyoushi's Manhuashe and MH1234 extensions.
// No script evaluation: only bounded tag views and one literal reader redirect.
inline bool extra_source(SourceKind k){return k==SourceKind::she||k==SourceKind::mh1234;}
inline std::string_view extra_base(SourceKind k){return k==SourceKind::she?"https://www.311s.com":"https://m.wmh1234.com";}
inline std::string extra_catalog(SourceKind k,std::string_view query,unsigned page){
    std::string path=query.empty()?"/category/order/hits":"/search/"+encoded(query);
    if(k==SourceKind::she)path+=(query.empty()?"/page/":"/")+std::to_string(page);
    else if(page>1)path+="/page/"+std::to_string(page);
    return std::string(extra_base(k))+path;
}
inline std::optional<std::string> extra_reader_redirect(std::string_view html){
    html=html.substr(0,10240);auto at=html.find("replace(");if(at==html.npos)return {};at+=8;
    while(at<html.size()&&html_space(html[at]))++at;if(at==html.size()||(html[at]!='\''&&html[at]!='"'))return {};
    auto quote=html[at++];auto end=html.find(quote,at);if(end==html.npos)return {};auto url=html.substr(at,end-at);
    constexpr std::string_view base="https://reader.hqread.cc/r/";
    if(!url.starts_with(base)||url.size()<=base.size()||!web(url)||url.find_first_of("\\<>\"'")!=url.npos)return {};return std::string(url);
}
[[gnu::noinline]] inline bool extra_books(std::string_view html,SourceKind k,std::vector<Book>& out,bool& more){
    if(!extra_source(k)||html.find(k==SourceKind::she?"comic-list":"漫画1234")==html.npos)return false;
    std::vector<Book> next;bool good=true;std::string next_url,current_url;
    bool parsed=site_tags(html,"a",[&](auto tag,auto body){auto raw=html_attribute(tag,"href");if(!raw)return true;
        auto url=resolve(extra_base(k),*raw);auto label=html_text(body);
        if(k==SourceKind::she&&html_class(tag,"on"))current_url=url;
        if(label=="下一页"&&web(url))next_url=url;
        if(!(k==SourceKind::she?she_book_url(url):mh1234_book_url(url)))return true;
        std::string title;
        if(k==SourceKind::she)title=html_text(html_attribute(tag,"title").value_or(""));
        else if(html_class(tag,"comic-card__link"))html_tags(body,"h3",[&](auto t,auto b){if(html_class(t,"comic-card__title"))title=html_text(b);return true;},true);
        if(title.empty())return true;
        if(std::any_of(next.begin(),next.end(),[&](auto& b){return b.id==url;}))return true;
        if(next.size()==64){good=false;return false;}next.push_back({std::move(url),std::move(title),k==SourceKind::she?"manhuashe":"mh1234"});return true;
    },true);
    if(!parsed||!good)return false;out=std::move(next);more=!next_url.empty()&&next_url!=current_url;return true;
}
[[gnu::noinline]] inline bool extra_chapters(std::string_view html,const Book& book,std::vector<Chapter>& out){
    auto k=source_kind(book.source);if(!extra_source(k))return false;
    auto list=html_div(html,k==SourceKind::she?"id=\"chapter-list\"":"class=\"mint-chapter-grid\"");if(!list)return false;
    std::vector<Chapter> next;bool good=true;
    bool parsed=site_tags(*list,"a",[&](auto tag,auto body){auto raw=html_attribute(tag,"href");if(!raw)return true;auto title=html_text(body);
        if(title.find("APP")!=title.npos)return true;auto url=resolve(extra_base(k),*raw);
        if(!extra_chapter_url(url,book.id,k)||title.empty()||next.size()==4096){good=false;return false;}
        if(std::any_of(next.begin(),next.end(),[&](auto& c){return c.id==url;}))return true;
        next.push_back({std::move(url),std::move(title)});return true;
    },true);
    if(!parsed||!good||next.empty())return false;
    // MH1234 lists newest first, including unnumbered extras at the bottom.
    if(k==SourceKind::mh1234)std::reverse(next.begin(),next.end());else chinese_reading_order(next);
    out=std::move(next);return true;
}
[[gnu::noinline]] inline bool extra_pages(std::string_view html,SourceKind k,std::vector<std::string>& out){
    if(!extra_source(k))return false;std::string_view body=html;
    if(k==SourceKind::she){auto container=html_div(html,"class=\"comic-content\"");if(!container)return false;body=*container;}
    std::vector<std::string> next;bool good=true;
    bool parsed=site_tags(body,"img",[&](auto tag,auto){
        if(k==SourceKind::mh1234&&!html_class(tag,"reader-image"))return true;
        auto raw=html_attribute(tag,k==SourceKind::she?"src":"data-src");if(!raw)return true;
        auto value=raw->starts_with("//")?"https:"+std::string(*raw):std::string(*raw);auto url=image_uri(value);
        if(!url.starts_with("https://")||next.size()==512){good=false;return false;}next.push_back(std::move(url));return true;
    });
    if(!parsed||!good||next.empty())return false;out=std::move(next);return true;
}
}
