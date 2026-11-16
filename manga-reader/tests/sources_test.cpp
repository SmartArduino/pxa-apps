#include "../sources.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
using namespace manga;

static std::string encrypted_page_list(std::string plain) {
    const std::string key="0123456789abcdef",iv="abcdef0123456789";
    auto padding=16-plain.size()%16;plain.append(padding,char(padding));
    AES_ctx context{};AES_init_ctx_iv(&context,reinterpret_cast<const uint8_t*>(key.data()),reinterpret_cast<const uint8_t*>(iv.data()));
    AES_CBC_encrypt_buffer(&context,reinterpret_cast<uint8_t*>(plain.data()),plain.size());
    std::string hex;for(unsigned char c:plain){hex+="0123456789abcdef"[c>>4];hex+="0123456789abcdef"[c&15];}
    return "<script>var cct = '"+key+"'; var contentKey = '"+iv+hex+"';</script>";
}

int main(int argc,char** argv){
    std::vector<Book> books;
    assert(copy_books(Json(R"({"code":200,"results":{"list":[{"name":"测试漫画","path_word":"example"}]}})"),books));
    assert(books.size()==1&&books[0].source=="copymanga");
    assert(!copy_books(Json(R"({"code":200,"results":{"list":[{"name":"bad","path_word":"../other"}]}})"),books));
    assert(books.size()==1&&books[0].id=="example");
    std::vector<Chapter> chapters;
    assert(copy_chapters(Json(R"({"code":200,"results":{"list":[{"uuid":"11111111-1111-1111-1111-111111111111","name":"序章"}]}})"),chapters,"example"));
    assert(chapters[0].id==copy_web("example")+"/chapter/11111111-1111-1111-1111-111111111111");
    assert(!copy_chapters(Json(R"({"code":200,"results":{"list":[{"uuid":"not-a-uuid","name":"bad"}]}})"),chapters,"example"));
    unsigned groups=0;assert(Json(R"({"default":{"name":"默认"},"extra":{"name":"番外"}})").members([&](const std::string& key,Json value){assert(value.get("name").string());++groups;return true;}));assert(groups==2);
    std::vector<std::string> urls{"https://example.com/old.png"};
    auto html=encrypted_page_list(R"([{"url":"https://example.com/one.jpg"},{"url":"https://example.com/two.jpg"}])");
    assert(copy_pages(html,urls));assert(urls.size()==2&&urls[1]=="https://example.com/two.jpg");
    assert(!copy_pages(encrypted_page_list(R"([{"url":"javascript:bad"}])"),urls));assert(urls.size()==2);
    assert(!copy_pages(encrypted_page_list("{bad-json}"),urls));
    auto broken=html;broken[broken.rfind("';")-1]='z';assert(!copy_pages(broken,urls));
    assert(!copy_pages("var cct='short'; var contentKey='x';",urls));
    assert(!js_literal("var contentKey='abc\\def';","var contentKey",32));
    assert(!js_literal("var contentKey='overlong';","var contentKey",4));
    std::vector<Book> restored;unsigned lang=0;std::string source;
    books[0].chapter=chapters[0].id;books[0].pages=66;books[0].page=3;
    assert(restore(save(books,0,"copymanga"),restored,lang,source));assert(restored[0].page==3);
    assert(ellipsize("中文测试漫画",40,14)=="中…");
    assert(html_text(" A &amp; B &#x4e2d;&#25991; ") == "A & B 中文");
    assert(html_text("&#65;&#65;&#65;",2).empty());
    assert(html_text("<b>Title</b>")=="Title");
    assert(html_attribute("<img data-x='src=bad' src=\"real.jpg\">","src")=="real.jpg");
    assert(!html_attribute("<img src='unterminated>","src"));
    assert(image_uri("javascript:bad").empty());
    std::vector<Book> catalog;bool more=false;
    assert(katana_books("<div id=\"book_list\"><h3 class=\"title\"><a href=\"https://mangakatana.com/manga/sample.42\">A &amp; B</a></h3></div>",catalog,more));
    assert(catalog.size()==1&&catalog[0].title=="A & B");
    assert(!katana_books("<div id=\"book_list\"><h3 class=\"title\"><a href=\"https://evil.example/manga/a\">A</a></h3></div>",catalog,more));
    assert(katana_pages("var pics=['https://i1.mangakatana.com/one.jpg','https://i1.mangakatana.com/two.jpg']; node.attr('data-src', pics[i]);",urls));assert(urls.size()==2);
    assert(!katana_pages("var pics=['javascript:bad']; node.attr('data-src', pics[i]);",urls));assert(urls.size()==2);
    if(argc==4){
        auto read=[](const char* path){std::ifstream f(path);return std::string((std::istreambuf_iterator<char>(f)),{});};
        auto catalog_html=read(argv[1]),book_html=read(argv[2]),page_html=read(argv[3]);
        assert(katana_books(catalog_html,catalog,more));assert(catalog.size()>=3);
        assert(katana_chapters(book_html,"https://mangakatana.com/manga/yotsubato.2098",chapters));assert(chapters.size()>100&&chapters[0].id.ends_with("/c1"));
        assert(katana_pages(page_html,urls));assert(urls.size()==48);
        std::cout<<"real HTML: "<<catalog.size()<<" books, "<<chapters.size()<<" chapters, "<<urls.size()<<" images passed\n";
    }
    if(argc==2){std::ifstream f(argv[1]);std::string real((std::istreambuf_iterator<char>(f)),{});assert(copy_pages(real,urls));assert(urls.size()==66);std::cout<<"public chapter: "<<urls.size()<<" image URLs decoded\n";}
    std::cout<<"sources: catalogs, chapters, public AES list, invalid input and saved-state migration passed\n";
}
