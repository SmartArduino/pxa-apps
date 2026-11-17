#include "../model.hpp"
#include <cassert>
#include <iostream>
using namespace manga;
int main(){
    assert(Json(R"({"a":[1,true,null,"\u4e2d\u6587\ud83d\ude00"]})").valid());
    assert(Json(R"("\u4e2d\u6587\ud83d\ude00")").string()=="中文😀");
    assert(!Json(R"("\ud800")").string());
    assert(!Json(R"({"a":[1,]})").valid());
    assert(!Json("01").valid());assert(!Json("1e").valid());
    assert(!Json(R"({"a":1}junk)").valid());
    assert(encoded("中文 a&b")=="%E4%B8%AD%E6%96%87%20a%26b");
    assert(resolve("https://example.com/dir/catalog.json","toc.json")=="https://example.com/dir/toc.json");
    assert(resolve("https://example.com/dir/catalog.json","/toc.json")=="https://example.com/toc.json");
    assert(resolve("https://example.com/dir/catalog.json","javascript:bad").empty());
    std::vector<Book> books;
    const auto id="11111111-1111-1111-1111-111111111111";
    assert(manga::books(Json(R"({"data":[{"id":"11111111-1111-1111-1111-111111111111","attributes":{"title":{"zh":"漫画"}}}]})"),books,""));
    assert(books.size()==1&&books[0].id==id);
    assert(!books[0].long_strip);
    assert(manga::books(Json(R"({"data":[{"id":"11111111-1111-1111-1111-111111111111","attributes":{"title":{"zh":"漫画"},"tags":[{"attributes":{"name":{"en":"Long Strip"}}}]}}]})"),books,""));
    assert(books[0].long_strip);
    std::vector<std::string> p;
    assert(pages(Json(R"({"baseUrl":"https://uploads.mangadex.org","chapter":{"hash":"abc123","dataSaver":["one.jpg","two.jpg"]}})"),p,true,""));
    assert(p[0]=="https://uploads.mangadex.org/data-saver/abc123/one.jpg");
    books[0].chapter="22222222-2222-2222-2222-222222222222";books[0].chapter_title="第1话";books[0].page=7;books[0].language=2;
    auto state=save(books,2,"");std::vector<Book> restored;unsigned language=0;std::string source;
    assert(restore(state,restored,language,source));assert(restored[0].page==7&&language==2&&restored[0].long_strip);
    assert(!restore("{}",restored,language,source));assert(restored[0].page==7);
    books[0].page=512;assert(!restore(save(books,2,source),restored,language,source));
    auto old=R"({"version":2,"language":0,"source":"peppercarrot","shelf":[{"id":"peppercarrot","title":"旧来源","source":"peppercarrot"},{"id":"11111111-1111-1111-1111-111111111111","title":"保留的漫画","source":"","page":7,"language":0}]})";
    assert(restore(old,restored,language,source));assert(source=="copymanga"&&restored.size()==1&&restored[0].page==7);
    assert(!valid_source("xkcd")&&!valid_source("commitstrip")&&!valid_source("https://example.com/catalog.json"));
    assert(valid_source("mangakatana"));
    std::string deep(30,'[');deep+='0';deep+=std::string(30,']');assert(!Json(deep).valid());
    std::cout<<"model: Unicode, bounded parsing, source adapters and atomic restore passed\n";
}
