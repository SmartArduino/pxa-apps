#include "../extra_sources.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
using namespace manga;
static std::string read(std::string p){std::ifstream f(p);assert(f);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char** argv){
    Book she{"https://www.311s.com/comic_13871.html","一人之下","manhuashe"};
    Book mh{"https://m.wmh1234.com/comic/12188.html","斗破苍穹(旧)","mh1234"};
    she.chapter="https://www.311s.com/chapter_13871_4992.html";mh.chapter="https://m.wmh1234.com/comic/12188/269807.html";
    assert(valid_book(she)&&valid_book(mh));
    assert(!extra_chapter_url("https://www.311s.com/chapter_13872_4992.html",she.id,SourceKind::she));
    assert(!extra_chapter_url("https://m.wmh1234.com/comic/12189/269807.html",mh.id,SourceKind::mh1234));
    assert(!mh1234_book_url("https://m.wmh1234.com.evil.test/comic/1.html"));
    assert(extra_catalog(SourceKind::she,"斗破 苍穹",2)=="https://www.311s.com/search/%E6%96%97%E7%A0%B4%20%E8%8B%8D%E7%A9%B9/2");
    assert(extra_catalog(SourceKind::mh1234,{},1)=="https://m.wmh1234.com/category/order/hits");
    assert(extra_catalog(SourceKind::mh1234,"斗破",2).ends_with("/page/2"));
    std::vector<Book> shelf;unsigned language=0;std::string selected;
    assert(restore(save({she,mh},0,"manhuashe"),shelf,language,selected));assert(shelf.size()==2&&selected=="manhuashe");
    assert(extra_reader_redirect("<script>location.replace(\"https://reader.hqread.cc/r/abc\");</script>")=="https://reader.hqread.cc/r/abc");
    assert(!extra_reader_redirect("replace('javascript:bad')"));
    assert(!extra_reader_redirect("replace('https://other.test/r/a')"));
    assert(!extra_reader_redirect(std::string(10240,' ')+"replace('https://reader.hqread.cc/r/a')"));
    std::vector<std::string> pages;
    std::vector<Book> empty;bool more=true;
    assert(extra_books("<div class='comic-list'></div><a class='on' href='/category/order/hits/page/2'>2</a><a class='next' href='/category/order/hits/page/2'>下一页</a>",SourceKind::she,empty,more)&&empty.empty()&&!more);
    assert(extra_pages("<div class=\"comic-content\"><img src='https://cdn.test/1.webp?e=2&amp;s=3'></div>",SourceKind::she,pages));
    assert(pages[0]=="https://cdn.test/1.webp?e=2&s=3");auto stable=pages;
    assert(!extra_pages("<img class='reader-image' data-src='javascript:bad'>",SourceKind::mh1234,pages)&&pages==stable);
    assert(!extra_pages("<script>const x=\"<img class='reader-image' data-src='https://cdn.test/ad.webp'>\";</script>",SourceKind::mh1234,pages));
    if(argc==2){std::string p=argv[1];
        for(auto k:{SourceKind::she,SourceKind::mh1234}){auto n=k==SourceKind::she?"she":"1234";auto& b=k==SourceKind::she?she:mh;std::vector<Book> books;std::vector<Chapter> chapters;bool more=false;
            assert(extra_books(read(p+"/"+n+"-list.html"),k,books,more));assert(books.size()>=20&&more);
            assert(extra_chapters(read(p+"/"+n+"-book.html"),b,chapters));assert(chapters.size()>500);
            assert(extra_pages(read(p+"/"+n+(k==SourceKind::she?"-chapter.html":"-reader.html")),k,pages));assert(pages.size()>10);
            for(auto& c:chapters)assert(extra_chapter_url(c.id,b.id,k));for(auto& book:books)assert(valid_book(book));
            if(k==SourceKind::she)assert(chapters.front().title.starts_with("第1话"));
            else assert(chapters.back().title.starts_with("第923话"));
            std::vector<Book> search;assert(extra_books(read(p+"/"+n+"-search.html"),k,search,more));assert(search.size()>=10);
            std::cout<<n<<": "<<books.size()<<" books, "<<chapters.size()<<" chapters, "<<pages.size()<<" images\n";
        }
    }
    std::cout<<"New Chinese source catalog, chapter order, reader redirect, images and persistence passed\n";
}
