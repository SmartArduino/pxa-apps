#include "../site_sources.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
using namespace manga;
static std::string read(const std::string& path){std::ifstream f(path);assert(f);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char** argv){
    Book pill{"https://mangapill.com/manga/4907/yotsuba-to","Yotsuba to!","mangapill"};
    Book town{"https://www.mangatown.com/manga/yotsubato/","Yotsubato!","mangatown"};
    assert(valid_book(pill)&&valid_book(town));
    assert(!pill_book_url("https://mangapill.com.evil.test/manga/4907/title"));
    assert(!town_book_url("https://www.mangatown.com/manga/a/../b/"));
    assert(!pill_book_url("https://mangapill.com/manga/4907/%2e%2e"));
    pill.chapter="https://mangapill.com/chapters/4907-10001000/yotsuba-to-chapter-1";
    town.chapter=town.id+"c001/";assert(valid_book(pill)&&valid_book(town));
    assert(!pill_chapter_url("https://mangapill.com/chapters/4908-10001000/other",pill.id));
    assert(!town_chapter_url(town.id+"c001/2.html",town.id));
    assert(town_chapter_url(town.id+"v01/c001/",town.id));
    assert(town_chapter_url(town.id+"v38/c489.5/",town.id));
    assert(!town_chapter_url(town.id+"v/c001/",town.id));
    assert(!town_chapter_url(town.id+"v01/v02/c001/",town.id));
    assert(!town_chapter_url(town.id+"v01/../c001/",town.id));
    assert(!town_page_url(town.chapter+"../2.html",town.chapter));
    assert(!town_page_url("https://evil.test/2.html",town.chapter));
    assert(town_page_url(town.chapter+"2.html",town.chapter));
    std::vector<Book> shelf{pill,town},restored;unsigned lang=0;std::string source;
    assert(restore(save(shelf,1,"mangapill"),restored,lang,source));assert(source=="mangapill"&&restored.size()==2&&restored[1].chapter==town.chapter);
    bool more=false;std::vector<Book> catalog{pill};
    assert(!site_books("<html>Verify you are human</html>",SourceKind::pill,catalog,more));assert(catalog[0].id==pill.id);
    std::vector<std::string> pages{"https://old.test/image.jpg"};
    assert(!pill_pages("<picture><img data-src='javascript:bad'></picture>",pages));assert(pages.size()==1&&pages[0].starts_with("https://old"));
    assert(!town_pages("<div class=\"page_select\"><option value='/manga/yotsubato/c001/2.html'>2</option></div>",town.chapter,pages));
    assert(!town_image("<div id=\"viewer\"><img id='image' src='http://insecure.test/a.jpg'></div>"));
    assert(town_image("<div id=\"viewer\"><img id='image' src='//cdn.test/a.jpg?x=1&amp;y=2'></div>")=="https://cdn.test/a.jpg?x=1&y=2");
    assert(!town_image("<div id=\"viewer\"><img id='image' src='https://a.test/1.jpg'><img id='image' src='https://a.test/2.jpg'></div>"));
    assert(pill_pages("<script>var fake=\"<picture>\";</script><picture><img data-src='https://cdn.test/real.jpg'></picture>",pages));assert(pages.size()==1&&pages[0]=="https://cdn.test/real.jpg");
    std::vector<Chapter> unchanged{{"old","old"}};assert(!site_chapters("<div id=\"chapters\"><a href='/chapters/4908-100/other'>Other</a></div>",pill,unchanged));assert(unchanged[0].id=="old");
    if(argc==2){
        std::string dir=argv[1];std::vector<Chapter> chapters;
        for(auto kind:{SourceKind::pill,SourceKind::town}){
            bool is_pill=kind==SourceKind::pill;std::string prefix=is_pill?"pill":"town";
            auto& book=is_pill?pill:town;
            assert(site_books(read(dir+"/"+(is_pill?"pillall":"town")+".txt"),kind,catalog,more));assert(catalog.size()>=20);
            assert(site_chapters(read(dir+"/"+prefix+"book.txt"),book,chapters));assert(chapters.size()>100);
            assert(chapters.front().id==book.chapter);
            auto html=read(dir+"/"+prefix+"pages.txt");
            assert(is_pill?pill_pages(html,pages):town_pages(html,book.chapter,pages));assert(pages.size()>20);
            if(!is_pill)assert(town_image(html));
            std::cout<<prefix<<": "<<catalog.size()<<" books, "<<chapters.size()<<" chapters, "<<pages.size()<<" pages; next="<<more<<"\n";
        }
    }
    std::cout<<"site adapters: path isolation, transactional parsing, image selection and shelf persistence passed\n";
}
