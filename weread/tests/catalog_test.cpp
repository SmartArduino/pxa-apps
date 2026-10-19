#include "../reading.hpp"
#include <cassert>
#include <iostream>
#include <fstream>

int main(int argc,char** argv){
    using namespace weread;
    {
        Catalog target;
        assert(target.initial(Json(R"({"chapterIndexes":[{"chapterUid":1,"title":"Original"}],"readerUrlParams":[]})")));
        const auto overflowing=R"({"chapterIndexes":[{"chapterUid":2}],"readerUrlParams":[],"catalogWindow":{"offset":4294967295,"length":4294967295}})";
        assert(!target.initial(Json(overflowing)));
        assert(target.items.size()==1&&target.items[0].title=="Original");
        std::string rows="[";
        for(unsigned i=0;i<301;++i){if(i)rows+=',';rows+="{\"chapterUid\":"+number(i)+"}";}
        rows+=']';
        assert(!target.initial(Json("{\"chapterIndexes\":"+rows+",\"readerUrlParams\":[],\"curChapter\":{\"chapterUid\":300},\"catalogWindow\":{\"offset\":4294967295,\"length\":4294967295}}")));
        assert(target.items.size()==1&&target.items[0].title=="Original");
    }
    {
        const auto [a,b,c,d]=Json(R"({"ignored":[{"deep":[1,2,{"x":"\\\"}"}]}],"a":null,"a":9,"\u0062":"value","c":{"a":3}})")
            .select({"a","b","c","missing"});
        assert(a.raw()=="null"&&b.string()=="value"&&c.get("a").number()==3&&d.raw().empty());
        const auto [x,y]=Json(R"({"x":1,"y":2})").select({"x","x"});
        assert(x.number()==1&&y.number()==1);
        const auto [bad]=Json(R"([{"x":1}])").select({"x"});assert(bad.raw().empty());
    }
    // 635 headings, only 34 actual chapters. All headings must survive bounded
    // projection; a directory row number must never become a wire chapter ID.
    std::string rows="[",params="[";
    for(unsigned i=0;i<635;++i){
        if(i){rows+=',';params+=',';}
        const auto chapter=i*34/635+1;
        rows+="{title:'Heading "+number(i)+"',chapterUid:"+number(chapter)+",chapterIdx:"+number(chapter)+"}";
        params+="{cUid:"+number(chapter)+",param:'authorized-"+number(chapter)+"'}";
    }
    rows+=']';params+=']';
    const auto html="window.__NUXT__=(function(a){return {state:{reader:{bookInfo:{bookId:'test',title:'Synthetic',lastChapterIdx:34},curChapter:{chapterUid:1},chapterInfoCount:635,chapterIndexes:"+rows+",readerUrlParams:"+params+",curCatalogRange:{}}}}}(0));";
    assert(cover_image_url("http://wfqqreader-1252317822.image.myqcloud.com/cover/a?x=a&amp;b=c")=="https://wfqqreader-1252317822.image.myqcloud.com/cover/a?x=a&amp;b=c");
    assert(cover_image_url("//cdn.weread.qq.com/cover/a")=="https://cdn.weread.qq.com/cover/a");
    for(auto url:{"http://cdn.weread.qq.com/cover/a","https://wfqqreader-1252317822.image.myqcloud.com.evil/a","https://wfqqreader-1252317822.image.myqcloud.com@evil/a"})assert(cover_image_url(url).empty());
    unsigned seen=0;
    for(auto offset:{0u,300u,600u}){
        Hydration h;assert(h.prepare(html));auto job=h.reader_projection(html.size(),offset);
        while(!job.done())assert(job.step(7));
        auto json=std::move(job).result();assert(json);
        Reading reader;assert(reader.parse(Json(*json)));const auto& c=reader.catalog;
        assert(c.begin==offset&&c.source_rows==635&&c.total==635&&c.items.size()<=300);
        assert(c.wire_begin==offset*34/635+1);
        for(const auto& row:c.items){assert(row.title=="Heading "+number(seen++));assert(!row.param.empty());}
        assert(bool(c.local_window(-1))==(offset!=0));
        assert(bool(c.local_window(1))==(offset!=600));
        if(offset==0)assert(c.local_window(1)==300&&c.wire_end==17);
        if(offset==600)assert(!c.can_load(1)&&c.wire_end==34);
    }
    assert(seen==635);
    Catalog remote;
    assert(remote.initial(Json(R"({"bookInfo":{"lastChapterIdx":34},"chapterInfoCount":635,"chapterIndexes":[{"chapterUid":16,"chapterIdx":16}],"readerUrlParams":[]})")));
    assert(remote.can_load(1)&&remote.wire_end==16);
    assert(remote.page(Json(R"({"succ":1,"chapterInfos":[{"chapterUid":34,"chapterIdx":34}],"readerUrlParmas":[]})"),1));
    assert(!remote.can_load(1)&&remote.can_load(-1));
    assert(!remote.page(Json(R"({"succ":1,"chapterInfos":[{"chapterUid":33,"chapterIdx":33}],"readerUrlParmas":[]})"),1));
    assert(remote.wire_end==34);
    assert(remote.page(Json(R"({"succ":1,"chapterInfos":[],"readerUrlParmas":[]})"),-1));
    assert(!remote.can_load(-1)&&remote.items.size()==1);
    for(auto error:{"null","false","0","\"\"","{}","{\"code\":0}"}){
        Reading r;assert(r.parse(Json("{\"bookInfo\":{\"title\":\"Synthetic\"},\"errorInfo\":"+std::string(error)+"}")));assert(!r.unavailable);
    }
    for(auto [rights,grant]:{std::pair{"\"paid\":1",ReadingGrant::purchased},
        {"\"paid\":0,\"isChapterPaid\":1",ReadingGrant::chapter_purchased},
        {"\"paid\":0,\"free\":1,\"maxFreeChapter\":8",ReadingGrant::free},
        {"\"paid\":0,\"payType\":32,\"maxFreeChapter\":8",ReadingGrant::free},
        {"\"paid\":0,\"payType\":64,\"maxFreeChapter\":8",ReadingGrant::limited_free},
        {"\"paid\":0,\"free\":0,\"maxFreeChapter\":8",ReadingGrant::preview},
        {"\"paid\":0",ReadingGrant::unknown}}){
        Reading r;assert(r.parse(Json("{\"bookInfo\":{\"title\":\"Synthetic\","+std::string(rights)+"},\"isCurChapterNeedToPay\":false}")));
        assert(r.grant==grant&&!r.requires_purchase&&!r.unavailable);
    }
    { // Skip exact adjacent duplicates, preserving changed and interleaved
      // last assignments for the same chapter, including escaped values.
      Catalog c;assert(c.initial(Json(R"({"chapterIndexes":[{"chapterUid":1,"chapterIdx":1}],"readerUrlParams":[{"cUid":1,"param":"a"},{"cUid":1,"param":"a"},{"cUid":2,"param":"x"},{"cUid":1,"param":"b"},{"cUid":1,"param":"b"},{"cUid":1,"param":"\u0063"}]})")));
      assert(c.items[0].param=="c");
    }
    Reading denied;
    assert(denied.parse(Json(R"({"bookInfo":{"title":"Synthetic","paid":1},"isCurChapterNeedToPay":true,"errorInfo":{"code":-505}})")));
    assert(denied.grant==ReadingGrant::purchased&&denied.requires_purchase&&denied.content_error);
    if(argc>1){
        std::ifstream f(argv[1]);std::string source{std::istreambuf_iterator<char>(f),{}};
        for(auto offset:{0u,300u,600u}){
            Hydration h;assert(h.prepare(source));auto job=h.reader_projection(source.size(),offset);
            while(!job.done())assert(job.step(7));auto projected=std::move(job).result();assert(projected);
            Reading r;assert(r.parse(Json(*projected)));
            assert(r.catalog.source_rows==635&&r.catalog.begin==offset&&r.catalog.items.size()<=300);
            assert(r.catalog.wire_end<=34&&r.catalog.wire_known);
            assert(std::ranges::all_of(r.catalog.items,[](const auto& row){return !row.param.empty();}));
        }
    }
    std::cout<<"635/34 directory windows, chapter cursors, atomic failures, free/paid/preview grants passed\n";
}
