#include "../chinese_sources.hpp"
#include <cassert>
#include <fstream>
#include <iterator>
#include <iostream>
using namespace manga;
static std::string read(const std::string& p){std::ifstream f(p);assert(f);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char** argv){
    Book guazi{"https://www.guazimanhua.com/comic.php?id=14109","从姑获鸟开始","guazimanhua"};
    Book woi{"https://www.woimh.com/book/mh11747","我要终结这场附身","woaimanhua"};
    guazi.chapter="https://www.guazimanhua.com/chapter.php?id=1633768";woi.chapter="https://www.woimh.com/chapter/406291-4219537";
    assert(valid_book(guazi)&&valid_book(woi));
    assert(!guazi_book_url("https://www.guazimanhua.com/comic.php?id=1&other=2"));assert(!woi_chapter_url("https://www.woimh.com.evil.test/chapter/1"));
    std::vector<Book> saved;unsigned language=0;std::string selected;
    assert(restore(save({guazi,woi},0,"guazimanhua"),saved,language,selected));assert(saved.size()==2&&selected=="guazimanhua");
    std::vector<Book> catalog;bool has_next=false;
    assert(chinese_books("<article class=\"card\"><a class='cover-wrap' href='/comic.php?id=1'>8.8连载</a><h3><a href='/comic.php?id=1'>真实书名</a></h3></article>",SourceKind::guazi,catalog,has_next));assert(catalog.size()==1&&catalog[0].title=="真实书名");
    std::vector<std::string> pages;
    assert(chinese_pages("<div class=\"comicpage\"><div style='display: none;'><img src='http://ad.test/ad.jpg'></div><div><img src='/grey.gif' data-echo='http://cdn.test/1.jpg'></div></div>",SourceKind::woi,pages));assert(pages.size()==1&&pages[0]=="https://cdn.test/1.jpg");
    assert(!chinese_pages("<section class=\"reader-images\"><img src='javascript:bad'></section>",SourceKind::guazi,pages));assert(pages.size()==1);
    std::vector<Chapter> chapters;
    assert(chinese_chapters("<ul id=\"detail-list-select\"><li><a href='/chapter/10'>第01话</a></li><li><a href='/chapter/11'>第02话</a><img class='detail-list-3-info-right'></li></ul>",woi,chapters));assert(chapters.size()==1&&chapters[0].id.ends_with("/10"));
    if(argc==2){std::string dir=argv[1];std::vector<Book> books;bool more=false;
        for(bool g:{true,false}){auto kind=g?SourceKind::guazi:SourceKind::woi;std::string prefix=g?"guazi":"woi";auto& book=g?guazi:woi;
            assert(chinese_books(read(dir+"/"+prefix+"cat.txt"),kind,books,more));assert(books.size()>=20&&more);assert(books[0].title==book.title);
            assert(chinese_chapters(read(dir+"/"+prefix+"book.txt"),book,chapters));assert(chapters.size()>=(g?300:2));
            assert(chinese_pages(read(dir+"/"+prefix+"pages.txt"),kind,pages));assert(pages.size()>1);
            std::cout<<prefix<<": "<<books.size()<<" books, "<<chapters.size()<<" chapters, "<<pages.size()<<" pages; first chapter="<<chapters[0].title<<"\n";
        }
    }
    std::cout<<"Chinese sources: bounded catalog/chapter parsing, lazy pages, hidden images, locked chapters and persistence passed\n";
}
