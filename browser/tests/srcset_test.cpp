#include "../browser_comic.hpp"
#include <cassert>
#include <iostream>
#include <random>
using namespace browser;

static Document parse(std::string_view html,std::size_t chunk=4096) {
    Document doc;doc.clear("https://images.test/chapter/page");Parser parser(doc);
    for(std::size_t at=0;at<html.size();at+=chunk)parser.feed(html.substr(at,chunk));
    parser.finish();return doc;
}
static void check(std::string_view html,std::initializer_list<std::string_view> urls) {
    const auto expected=parse(html);
    assert(expected.images.size()==urls.size());std::size_t i=0;
    for(auto url:urls)assert(expected.images[i++].source==url);
    for(auto chunk:{1,2,3,7,31,2048}) {
        const auto doc=parse(html,chunk);
        assert(doc.images.size()==expected.images.size() && doc.text==expected.text);
        for(i=0;i<doc.images.size();++i)assert(doc.images[i].source==expected.images[i].source);
    }
}
int main() {
    check("<img src='/fallback.png' srcset='/bad.webp 1x, /good.jpg 2x'>",{"https://images.test/good.jpg"});
    check("<img src='/fallback.png' srcset='javascript:alert(1) 1x, /good.png 640w'>",{"https://images.test/good.png"});
    for(auto invalid:{"0w","-1w","NaNx","0x","1x 2x","1w 2x","1h","1.x","99999999999999999999w","bad(1,2)"})
        check("<img srcset='/bad.png "+std::string(invalid)+", /good.png 2x'>",{"https://images.test/good.png"});
    check("<img srcset='/first.jpg 1.25x, /second.jpg 2x'>",{"https://images.test/first.jpg"});
    check("<img srcset='/first.jpg .5x, /second.jpg 2x'>",{"https://images.test/first.jpg"});
    check("<img srcset='/cdn,image.png 1x, /second.png 2x'>",{"https://images.test/cdn,image.png"});
    check("<img srcset=',, /first.png, /second.jpg 2x'>",{"https://images.test/first.png"});
    check("<img srcset='data:image/svg+xml,invalid 1x, /good.png 2x'>",{"https://images.test/good.png"});
    check("<img srcset='data:image/png;base64,iVBORw0KGgo= 1x, /good.png 2x'>",{"data:image/png;base64,iVBORw0KGgo="});
    check("<img class=lazy src='/loading.gif' data-src='javascript:bad' data-original='/page.jpg'>",{"https://images.test/page.jpg"});
    check("<img class=lazyload src='/loading.gif' data-src='/bad.svg' data-srcset='/bad.avif 1x, /page.jpg 2x'>",{"https://images.test/page.jpg"});
    check("<img data-lazy-srcset='/page.png 320w'><img src='/real.jpg' data-src='/ignored.jpg' data-srcset='/ignored.png 1x'>",
          {"https://images.test/page.png","https://images.test/real.jpg"});
    check("<img src='javascript:bad' data-src='/good.png'><img class=lazy data-src=' /spaced.jpg '>",
          {"https://images.test/good.png","https://images.test/spaced.jpg"});
    check("<picture><source type='image/webp' srcset='/secret 1x'><source type='IMAGE/JPEG' srcset='/page.jpg 640w'>"
          "<img src='/fallback.svg'></picture><img src='/other.png'>",{"https://images.test/page.jpg","https://images.test/other.png"});
    check("<source srcset='/outside.png 1x'><picture><source type=image/png media=print srcset='/print.png'>"
          "<source media='(max-width: 600px)' srcset='/query.png'><source media=screen srcset='/screen.png'>"
          "<img src='/fallback.jpg'></picture>",{"https://images.test/screen.png"});
    check("<picture><source srcset='/first.png'><source srcset='/second.png'><img src='/fallback.jpg'>"
          "<source srcset='/late.png'><img src='/other.jpg'></picture><picture><img src='/last.jpg'></picture>",
          {"https://images.test/first.png","https://images.test/other.jpg","https://images.test/last.jpg"});
    check("<picture><source hidden srcset='/hidden.png'><img src='/real.jpg'></picture>"
          "<picture hidden><source srcset='/hidden.png'><img src='/hidden.jpg'></picture><img src='/last.jpg'>",
          {"https://images.test/real.jpg","https://images.test/last.jpg"});
    check("<picture><source srcset='/first.png'><img hidden src='/hidden.jpg'><img src='/second.jpg'></picture>",
          {"https://images.test/second.jpg"});
    std::string many;
    for(unsigned i=0;i<srcset::max_candidates;++i)many+="/bad.webp 1x, ";
    check("<img src='/fallback.png' srcset='"+many+"/late.jpg 1x'>",{"https://images.test/fallback.png"});
    auto bounded=parse("<img src='/fallback.png' srcset='"+std::string(4100,'x')+"'>");
    assert(bounded.images.size()==1 && bounded.images[0].source=="https://images.test/fallback.png");
    std::mt19937 random(0x505841);
    constexpr std::string_view alphabet="abc123 ,\t()wx/.:-&;";
    for(unsigned turn=0;turn<500;++turn) {
        std::string list;
        for(unsigned j=0,n=random()%200;j<n;++j)list+=alphabet[random()%alphabet.size()];
        const auto a=parse("<img src='/fallback.png' srcset='"+list+"'>",1);
        const auto b=parse("<img src='/fallback.png' srcset='"+list+"'>",4096);
        assert(a.images.size()==b.images.size());
        if(!a.images.empty())assert(a.images[0].source==b.images[0].source);
    }
    std::cout<<"responsive images PASS: srcset, URL commas, descriptors, lazy fallbacks, picture formats, streaming and budgets\n";
}
