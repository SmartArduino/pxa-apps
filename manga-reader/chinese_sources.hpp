#pragma once
#include "site_sources.hpp"
namespace manga {
inline constexpr std::string_view guazi_base="https://www.guazimanhua.com",woi_base="https://www.woimh.com";
inline bool chinese_source(SourceKind k){return k==SourceKind::guazi||k==SourceKind::woi;}
[[gnu::noinline]] inline bool chinese_books(std::string_view html,SourceKind kind,std::vector<Book>& out,bool& more){
    bool guazi=kind==SourceKind::guazi;if(!guazi&&kind!=SourceKind::woi)return false;
    if(guazi?html.find("class=\"card\"")==html.npos:(html.find("mh-item")==html.npos&&html.find("book-list")==html.npos))return false;
    auto base=guazi?guazi_base:woi_base;std::vector<Book> next;bool ok=true,has_next=false;
    bool valid=site_tags(html,"a",[&](auto tag,auto body){auto raw=html_attribute(tag,"href");if(!raw)return true;
        auto url=resolve(base,*raw);auto label=html_text(body,192);
        if((label==">"||html_attribute(tag,"title").value_or("")=="下一页")&&url.starts_with(std::string(base)+(guazi?"/category.php?":"/booklist?")))has_next=true;
        if(!(guazi?guazi_book_url(url):woi_book_url(url))||(guazi&&html_class(tag,"cover-wrap")))return true;
        auto title=html_text(html_attribute(tag,"title").value_or(""),192);if(title.empty())title=std::move(label);if(title.empty())return true;
        if(std::any_of(next.begin(),next.end(),[&](auto& b){return b.id==url;}))return true;
        if(next.size()==64){ok=false;return false;}next.push_back({std::move(url),std::move(title),guazi?"guazimanhua":"woaimanhua"});return true;
    },true);
    if(!valid||!ok)return false;out=std::move(next);more=has_next;return true;
}
inline void chinese_reading_order(std::vector<Chapter>& list){
    std::optional<unsigned> first;
    for(auto& c:list){auto at=c.title.find_first_of("0123456789");if(at==c.title.npos)continue;
        auto end=c.title.find_first_not_of("0123456789",at);auto n=Json(std::string_view(c.title).substr(at,end==c.title.npos?end:end-at)).number();
        if(!first){first=n;continue;}if(n==*first)continue;if(n<*first)std::reverse(list.begin(),list.end());break;
    }
}
[[gnu::noinline]] inline bool chinese_chapters(std::string_view html,const Book& book,std::vector<Chapter>& out){
    bool guazi=source_kind(book.source)==SourceKind::guazi;auto marker=guazi?"class=\"mobile-chapter-grid":"id=\"detail-list-select\"";
    auto at=html.find(marker);if(at==html.npos)return false;auto end=html.find(guazi?"</section>":"</ul>",at);if(end==html.npos)return false;
    std::vector<Chapter> next;bool ok=true;
    auto add=[&](auto tag,auto body){auto raw=html_attribute(tag,"href");if(!raw)return true;
        auto id=resolve(guazi?guazi_base:woi_base,*raw);auto title=html_text(body,192);
        if(!(guazi?guazi_chapter_url(id):woi_chapter_url(id))||title.empty()||next.size()==4096){ok=false;return false;}
        if(std::any_of(next.begin(),next.end(),[&](auto& c){return c.id==id;}))return true;next.push_back({std::move(id),std::move(title)});return true;
    };
    auto list=html.substr(at,end-at);
    bool valid=guazi?site_tags(list,"a",add,true):site_tags(list,"li",[&](auto,auto body){
        if(body.find("detail-list-3-info-right")!=body.npos)return true;return site_tags(body,"a",add,true);
    },true);
    if(!valid||!ok||next.empty())return false;chinese_reading_order(next);out=std::move(next);return true;
}
[[gnu::noinline]] inline bool chinese_pages(std::string_view html,SourceKind kind,std::vector<std::string>& out){
    bool guazi=kind==SourceKind::guazi;std::string_view fragment;
    if(guazi){auto at=html.find("class=\"reader-images");if(at==html.npos)return false;auto end=html.find("</section>",at);if(end==html.npos)return false;fragment=html.substr(at,end-at);}
    else {auto container=html_div(html,"class=\"comicpage\"");if(!container)return false;fragment=*container;}
    std::vector<std::string> next;bool ok=true;
    auto add=[&](auto tag,auto){auto raw=html_attribute(tag,"data-echo");if(!raw)raw=html_attribute(tag,"src");if(!raw)return true;
        std::string value(*raw);if(value.starts_with("//"))value="https:"+value;else if(value.starts_with("http://"))value.replace(0,7,"https://");
        auto url=image_uri(value);if(!url.starts_with("https://")||next.size()==512){ok=false;return false;}next.push_back(std::move(url));return true;
    };
    bool valid=guazi?site_tags(fragment,"img",add):site_tags(fragment,"div",[&](auto tag,auto body){
        auto raw=html_attribute(tag,"style").value_or("");std::string style;for(char c:raw)if(!html_space(c))style+=c;
        if(style.find("display:none")!=style.npos)return true;return site_tags(body,"img",add);
    },true);
    if(!valid||!ok||next.empty())return false;out=std::move(next);return true;
}
}
