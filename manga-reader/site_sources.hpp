#pragma once
#include "html.hpp"

namespace manga {
// Native adapters for the public pages used by Keiyoushi's MangaPill and
// MangaTown extensions. Borrowed views only: no DOM, script engine or covers.
inline constexpr std::string_view pill_base="https://mangapill.com",town_base="https://www.mangatown.com";
inline bool html_class(std::string_view tag,std::string_view token){
    auto value=html_attribute(tag,"class").value_or("");
    while(!value.empty()){while(!value.empty()&&html_space(value.front()))value.remove_prefix(1);auto end=value.find_first_of(" \t\r\n");if(value.substr(0,end)==token)return true;if(end==value.npos)break;value.remove_prefix(end+1);}return false;
}
// Ignore script/style/template strings and comments when scanning actual markup.
// The reader never evaluates site JavaScript or treats its HTML templates as pages.
template<class F> bool site_tags(std::string_view html,std::string_view name,F f,bool paired=false){
    while(!html.empty()){
        auto script=html.find("<script"),style=html.find("<style"),comment=html.find("<!--");auto at=std::min({script,style,comment});
        if(!html_tags(html.substr(0,at),name,f,paired))return false;if(at==html.npos)return true;
        auto close=at==comment?std::string_view("-->"):at==script?std::string_view("</script>"):std::string_view("</style>");
        auto end=html.find(close,at);if(end==html.npos)return false;html.remove_prefix(end+close.size());
    }return true;
}
[[gnu::noinline]] inline bool site_books(std::string_view html,SourceKind kind,std::vector<Book>& out,bool& more){
    bool pill=kind==SourceKind::pill;if(!pill&&kind!=SourceKind::town)return false;
    // A challenge/error page must not be accepted as an empty catalog.
    if(html.find(pill?"Search Manga":"manga_list")==html.npos)return false;
    auto base=pill?pill_base:town_base;std::vector<Book> next;bool ok=true,has_next=false;
    auto fragment=html;
    if(pill){auto grid=html_div(html,"class=\"my-3 grid");if(!grid){out.clear();more=false;return true;}fragment=*grid;has_next=html.find("class=\"btn btn-sm\"")!=html.npos;}
    bool valid=site_tags(fragment,"a",[&](auto tag,auto body){
        auto raw=html_attribute(tag,"href");if(!raw)return true;
        auto href=resolve(base,*raw);
        if((pill&&html_class(tag,"btn")&&html_text(body)=="Next")||(!pill&&html_class(tag,"next")&&web(href)))has_next=true;
        if(pill?!pill_book_url(href):!town_book_url(href)||!html_class(tag,"manga_cover"))return true;
        auto title=pill?html_text(body,192):html_text(html_attribute(tag,"title").value_or(""),192);
        if(title.empty())return true; // Cover-only anchor; the title anchor follows.
        if(std::any_of(next.begin(),next.end(),[&](auto& b){return b.id==href;}))return true;
        if(next.size()==64){ok=false;return false;}
        next.push_back({std::move(href),std::move(title),pill?"mangapill":"mangatown"});return true;
    },true);
    if(!valid||!ok)return false;out=std::move(next);more=has_next;return true;
}
[[gnu::noinline]] inline bool site_chapters(std::string_view html,const Book& book,std::vector<Chapter>& out){
    bool pill=source_kind(book.source)==SourceKind::pill;std::string_view list;
    if(pill){auto body=html_div(html,"id=\"chapters\"");if(!body)return false;list=*body;}
    else {auto at=html.find("class=\"chapter_list\"");if(at==html.npos)return false;auto end=html.find("</ul>",at);if(end==html.npos)return false;list=html.substr(at,end-at);}
    std::vector<Chapter> next;bool ok=true;
    bool valid=site_tags(list,"a",[&](auto tag,auto body){auto raw=html_attribute(tag,"href");if(!raw)return true;
        auto id=resolve(pill?pill_base:town_base,*raw);auto title=html_text(body,192);
        if(!(pill?pill_chapter_url(id,book.id):town_chapter_url(id,book.id))||title.empty()||next.size()==4096){ok=false;return false;}
        if(std::any_of(next.begin(),next.end(),[&](auto& c){return c.id==id;}))return true;
        next.push_back({std::move(id),std::move(title)});return true;
    },true);
    if(!valid||!ok||next.empty())return false;
    std::reverse(next.begin(),next.end());out=std::move(next);return true;
}
[[gnu::noinline]] inline bool pill_pages(std::string_view html,std::vector<std::string>& out){
    std::vector<std::string> next;bool ok=true;
    bool valid=site_tags(html,"picture",[&](auto,auto body){return html_tags(body,"img",[&](auto tag,auto){
        auto raw=html_attribute(tag,"data-src");if(!raw)return true;auto url=image_uri(*raw);
        if(!url.starts_with("https://")||next.size()==512){ok=false;return false;}next.push_back(std::move(url));return true;
    });},true);
    if(!valid||!ok||next.empty())return false;out=std::move(next);return true;
}
inline bool town_page_url(std::string_view s,std::string_view chapter){
    if(!s.starts_with(chapter))return false;auto tail=s.substr(chapter.size());return tail.empty()||(tail.ends_with(".html")&&digits(tail.substr(0,tail.size()-5)));
}
[[gnu::noinline]] inline bool town_pages(std::string_view html,std::string_view chapter,std::vector<std::string>& out){
    auto list=html_div(html,"class=\"page_select\"");if(!list)return false;
    std::vector<std::string> next;bool ok=true;
    bool valid=site_tags(*list,"option",[&](auto tag,auto body){
        auto raw=html_attribute(tag,"value");if(!raw)return true;
        // The source's optional 'featured' page is not part of the chapter.
        if(raw->ends_with("/featured.html"))return true;
        auto id=resolve(town_base,*raw);if(!town_page_url(id,chapter)||next.size()==512){ok=false;return false;}
        if(next.empty()&&id!=chapter){ok=false;return false;}
        if(std::find(next.begin(),next.end(),id)==next.end())next.push_back(std::move(id));return true;
    },true);
    if(!valid||!ok||next.empty())return false;out=std::move(next);return true;
}
[[gnu::noinline]] inline std::optional<std::string> town_image(std::string_view html){
    auto viewer=html_div(html,"id=\"viewer\"");if(!viewer)return {};
    std::optional<std::string> result;
    bool valid=site_tags(*viewer,"img",[&](auto tag,auto){
        if(html_attribute(tag,"id").value_or("")!="image")return true;
        auto raw=html_attribute(tag,"src");if(!raw||result)return false;
        auto url=raw->starts_with("//")?image_uri("https:"+std::string(*raw)):image_uri(*raw);
        if(!url.starts_with("https://"))return false;result=std::move(url);return true;
    });return valid?result:std::nullopt;
}
}
