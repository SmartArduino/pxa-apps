#include "../browser_gmanhua.hpp"
#include "../browser_script.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <random>

static std::string quote(std::string_view value) {
    std::string result = "'";
    for (char c : value) {
        if (c == '\\' || c == '\'') result += '\\';
        result += c;
    }
    return result + "'";
}
static std::string pack(std::string_view value) {
    return "eval(function(p){return p;}(" + quote(value) + ",36,1,''.split('|'),0,{}))";
}
static std::string read(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    assert(file); return {std::istreambuf_iterator<char>(file),{}};
}
int main(int argc, char** argv) {
    using namespace browser::gmanhua;
    assert(site("https://gmanhua.com/") && site("https://WWW.GMANHUA.COM:443/ch1-42/"));
    for (auto url : {"https://gmanhua.com.evil/", "https://evil/?gmanhua.com",
                     "https://gmanhua.com@evil/", "ftp://gmanhua.com/"}) assert(!site(url));
    for (auto url : {"https://gmanhua.com/ch1-42/", "https://gmanhua.com/ch1-42/?x=1",
                     "https://gmanhua.com/ch1-42-pl10/"}) assert(chapter(url)->cid == 42);
    auto page = chapter("https://gmanhua.com/ch1-42-p3-pl10/");
    assert(page && page->page == 3 && page->per_page == 10);
    for (auto url : {"https://gmanhua.com/manhua42/", "https://gmanhua.com/ch1-0/",
                     "https://gmanhua.com/ch1-4294967296/", "https://gmanhua.com/ch1-42-p0/",
                     "https://gmanhua.com/ch1-42/extra"}) assert(!chapter(url));
    assert(unpack("eval(function(p){return p;}('0 1=2;',13,3,'var|answer|42'.split('|'),0,{}))") ==
           std::optional<std::string>{"var answer=42;"});
    assert(unpack(pack("var name='a\\b';")) == std::optional<std::string>{"var name='a\\b';"});
    const auto html = "<script>var DM5_CID=42;var DM5_IMAGE_COUNT=15;</script><script>" +
        pack("var guidkey=''+'ab'+'12'+'cd'+'34';") + "</script>";
    auto meta = metadata("https://gmanhua.com/ch1-42/",html);
    assert(meta && meta->images == 15 && meta->key == "ab12cd34");
    for (auto size : {1u,37u,4096u}) {
        browser::script::Capture capture;
        for (std::size_t at=0;at<html.size();at+=size) capture.append(std::string_view(html).substr(at,size));
        auto captured = metadata("https://gmanhua.com/ch1-42/",capture.take());
        assert(captured && captured->key == meta->key);
    }
    assert(!metadata("https://gmanhua.com/ch1-43/",html));
    assert(!metadata("https://gmanhua.com/ch1-42/",std::string(packet_limit+1,'x')));
    const std::string image_script =
        "function image(){var pix='http://comic.cdndm5.com/1/42/123';"
        "var pvalue=['/a.jpg','/b.png'];for(var i=0;i<pvalue.length;i++)"
        "pvalue[i]=pix+pvalue[i]+'?cid=42&key=001122aabbcc';return pvalue}var d=image();";
    auto urls = images(pack(image_script),42);
    assert(urls && urls->size() == 2 && (*urls)[0] ==
           "http://comic.cdndm5.com/1/42/123/a.jpg?cid=42&key=001122aabbcc");
    assert(!images(pack(image_script),43));
    auto bad_host = image_script;
    bad_host.replace(bad_host.find("comic.cdndm5.com"),16,"cdndm5.com.evil");
    assert(!images(pack(bad_host),42));
    assert(!unpack("eval(function(p){return p;}('x',1,1,''.split('|'),0,{}))"));
    assert(!unpack("eval(function(p){return p;}('x',36,257,''.split('|'),0,{}))"));
    assert(!unpack(std::string(packet_limit+1,'x')));
    std::mt19937 random(42);
    for (unsigned i=0;i<3000;++i) {
        std::string value(random()%500,' ');
        for (auto& c : value) c=char(random()%128);
        (void)unpack(value); (void)metadata("https://gmanhua.com/ch1-42/",value);
        (void)images(value,42);
    }
    if (argc == 2) {
        const std::string directory=argv[1];
        auto actual = metadata("https://gmanhua.com/ch757-1035958/",
                               read(directory+"/gmanhua-real-chapter.body"));
        assert(actual && actual->images == 15 && actual->key.size() == 16);
        browser::script::Capture capture;
        const auto original = read(directory+"/gmanhua-real-chapter.body");
        for (std::size_t at=0;at<original.size();at+=37) capture.append(std::string_view(original).substr(at,37));
        auto captured = metadata("https://gmanhua.com/ch757-1035958/",capture.take());
        assert(captured && captured->images == 15 && captured->key == actual->key);
        auto actual_images = images(read(directory+"/gmanhua-image-api.body"),1035958);
        assert(actual_images && actual_images->size() == 2);
        auto latest = images(read(directory+"/gmanhua-image-api-latest.body"),2291674);
        assert(latest && latest->size() == 2);
        auto preloaded = images(read(directory+"/gmanhua-image-api-ten.body"),2291674);
        assert(preloaded && preloaded->size() == 16);
    }
    std::cout << "Gmanhua metadata and packed image-list parsing PASS\n";
}
