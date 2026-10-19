#include "../hydration.hpp"
#include "../inline_image.hpp"
#include "../reading.hpp"
#include "../state.hpp"
#include "../search.hpp"
#include <cassert>
#include <fstream>
#include <iostream>

int main(int argc,char** argv){
    using namespace weread;
    assert(Json(R"({"text":"\ud840\udc00","n":42})").valid());
    assert(Json(R"({"text":"\ud840\udc00"})").get("text").string()=="𠀀");
    assert(!Json(R"("\ud800")").string());
    assert(!Json(R"({"a":[1,]})").valid());
    assert(!Json(std::string(30,'[')+"0"+std::string(30,']')).valid());
    {
        const Json object(R"({"":false,"\u0061":null,"a":9,"q\"\\":"escaped","surrogate\ud840\udc00":"value","empty":"","nested":{"rows":[1,2]}})");
        const auto [a,again,quoted,astral,empty,missing]=object.select({"a","a","q\"\\","surrogate𠀀","empty","absent"});
        assert(a.raw()=="null"&&again.raw()=="null"&&quoted.string()=="escaped");
        assert(astral.string()=="value"&&empty.string()==""&&missing.raw().empty());
        assert(object.get("").raw()=="false"&&object.get("a").raw()=="null");
        for(std::string_view key:{"a","q\"\\","surrogate𠀀","empty","absent","nested"}){
            const auto [selected]=object.select({key});assert(object.get(key).raw()==selected.raw());
        }
        assert(Json(R"({"a":1,"broken":[})").get("a").number()==1);
        assert(Json(R"({"broken":[},"a":1})").get("a").raw().empty());
    }
    Session session;session.receive("wr_vid=1; Path=/; HttpOnly; Secure");
    session.receive("wr_skey=synthetic; Path=/");session.vid="1";
    Session restored;assert(restored.decode(session.encode()));
    assert(restored.header()=="wr_vid=1; wr_skey=synthetic");
    const auto before=restored.header();
    assert(!restored.decode(R"({"version":1,"vid":"2","cookies":["wr_vid=2; injected=x"]})"));
    assert(before==restored.header());
    restored.receive("other=must-not-be-sent");assert(restored.header()==before);
    restored.receive("wr_skey=; Max-Age=0");assert(restored.cookies.size()==1);
    Shelf shelf;
    assert(shelf.parse(R"({"books":[{"bookId":"10","title":"测试书","author":"作者"}],"bookReaderUrls":[{"bId":"10","param":"opaque"}],"totalCount":21,"kkIdx":-1})"));
    assert(shelf.total==21&&shelf.books.size()==1&&shelf.books[0].param=="opaque");
    assert(!shelf.parse(R"({"sessionTimeout":1})"));assert(shelf.books.size()==1);
    assert(login_expired(Json(R"({"sessionTimeout":1})")));
    assert(login_expired(Json(R"({"succ":0,"errCode":-1,"data":{"errcode":-2012}})")));
    for(auto value:{R"({"sessionTimeout":0})",R"({"data":{"errcode":-1}})",R"({"scan":0})",R"({})"})assert(!login_expired(Json(value)));
    for(auto malformed:{R"({"books":[{"bookId":"10"}]})",
        R"({"books":[],"bookReaderUrls":{}})",
        R"({"books":[{"bookId":"10","title":"A"},{"bookId":"10","title":"B"}]})"}){
        assert(!shelf.parse(malformed));assert(shelf.books.size()==1&&shelf.total==21);
    }
    Search search;
    const auto sample_search=R"({"sid":"sample","results":[{"type":21,"authors":[]},{"type":1,"scope":17,"scopeCount":50,"currentCount":1,"books":[{"bookInfo":{"bookId":"1","title":"检索结果","author":"作者"}}]}]})";
    assert(search.parse(sample_search)&&search.books.size()==1&&search.total==50&&search.cursor==1);
    assert(!search.parse(R"({"sid":"sample","results":[{"type":1,"scopeCount":1,"currentCount":1,"books":[{}]}]})"));
    assert(search.books.size()==1&&search.total==50);
    assert(search.parse(R"({"sid":"empty","results":[]})")&&search.books.empty());
    Hydration hydration;
    auto result=hydration.decode(R"(<script>window.__NUXT__=(function(a,b,c){return {state:{user:b,text:'it\'s "safe"',number:c,empty:a,flags:[!0,!1]}}}("",null,42));</script>)");
    assert(result);assert(Json(*result).get("state").get("text").string()=="it's \"safe\"");
    assert(Json(*result).get("state").get("number").number()==42);
    auto shared=hydration.decode("window.__NUXT__=(function(a,b,c){a.title=b;a.value=c;return {first:a,second:a,empty:Array(0)}}({},\"章节\",42));");
    assert(shared && Json(*shared).get("first").get("title").string()=="章节");
    assert(Json(*shared).get("second").get("value").number()==42);
    assert(Json(*shared).get("empty").raw()=="[]");
    std::string large="window.__NUXT__=(function(";
    for(unsigned i=0;i<873;++i){if(i)large+=',';large+="a"+number(i);}
    large+="){a0.title=a872;return {chapter:a0}}({}";
    for(unsigned i=1;i<873;++i)large+=",\"synthetic\"";
    large+="));";
    auto expanded=hydration.decode(large);assert(expanded);
    assert(Json(*expanded).get("chapter").get("title").string()=="synthetic");
    for(auto invalid:{
        "window.__NUXT__=(function(a){return {x:alert(1)}}(0));",
        "window.__NUXT__=(function(a){return {x:a.foo}}(0));",
        "window.__NUXT__=(function(a){return {x:a}}(alert(1)));",
        "window.__NUXT__=(function(a){return {x:unknown}}(0));",
        "window.__NUXT__=(function(a){return {x:a}}(0));alert(1);",
        "window.__NUXT__=(function(a){a.x=alert(1);return a}({}));",
        "window.__NUXT__=(function(a){global.x=1;return a}({}));",
        "window.__NUXT__=(function(a){a.x=a;return a}({}));",
        "window.__NUXT__=(function(a){a.x=2;return a}({x:1}));",
        "window.__NUXT__=(function(a){return Array(999999)}(0));"})assert(!hydration.decode(invalid));
    assert(!inline_image::payload("data:image/png;base64,AB=="));
    assert(!inline_image::payload("data:text/html;base64,AAAA"));
    assert(inline_image::payload("data:image/png;base64,AA=="));
    Login login;login.key="7";
    assert(login.confirmation(Json(R"({"vid":1,"skey":"synthetic","code":"synthetic","isAutoLogout":0})")));
    assert(Json(*login.confirmation(Json(R"({"vid":1,"skey":"synthetic","code":"synthetic"})"))).get("vid").number()==1);
    assert(!login.confirmation(Json(R"({"scan":0})")));
    assert(reader_text("<p>第一段</p><p>A &amp; B</p><script>hidden()</script>")=="第一段\nA & B");
    auto element=html::element("<script>var x='<div id=\"body\">bad</div>';</script><div id='body' title='a>b'><div>inside</div><!-- </div> --><p>end</p></div>","body");
    assert(element && element->starts_with("<div>inside</div>"));
    assert(!html::element("<div id='body'><div>unclosed</div>","body"));
    assert(reader_text("<P title='a>b'>A</P><SCRIPT>hidden</scripture>still hidden</SCRIPT><p>B &lt; C</p>")=="A\nB < C");
    assert(reader_text("x < 3<!-- hidden > --> y")=="x < 3 y");
    const std::string prose="第一行中文测试。\n第二段内容。\nEnd.";
    std::size_t at=0,walks=0;
    while(at<prose.size()){
        auto page=paginate(prose,at,60,2,12);assert(page.end>at);assert(page.end<=prose.size());
        for(std::size_t i=0;i<page.count;++i){auto l=page.lines[i];assert(l.start>=at&&l.end>=l.start&&l.end<=page.end);assert(valid_utf8(std::string_view(prose).substr(l.start,l.end-l.start)));}
        at=page.end;assert(++walks<20);
    }
    assert(paginate(prose,0,8,2,12).count==0);
    Catalog catalog;
    std::string chapters="[",params="[";
    for(unsigned i=0;i<900;++i){
        if(i){chapters+=',';params+=',';}
        chapters+="{\"chapterUid\":"+number(i)+",\"chapterIdx\":"+number(i)+",\"title\":\"Chapter\"}";
        params+="{\"cUid\":"+number(i)+",\"param\":\"opaque\"}";
    }
    chapters+=']';params+=']';
    assert(catalog.initial(Json("{\"chapterIndexes\":"+chapters+",\"readerUrlParams\":"+params+",\"chapterInfoCount\":900,\"curChapter\":{\"chapterUid\":700}}")));
    assert(catalog.items.size()==300&&catalog.begin==550&&catalog.end==850&&catalog.total==900);
    assert(std::ranges::all_of(catalog.items,[](auto& item){return item.param=="opaque";}));
    const auto old_begin=catalog.begin;
    assert(!catalog.page(Json(R"({"succ":1,"chapterInfos":[],"readerUrlParmas":{}})"),1)&&catalog.begin==old_begin);
    assert(catalog.page(Json(R"({"succ":1,"chapterInfos":[{"chapterUid":850,"chapterIdx":850,"title":"Next"}],"readerUrlParmas":[{"cUid":850,"param":"next"}]})"),1));
    assert(catalog.wire_begin==850&&catalog.wire_end==850&&catalog.items[0].param=="next");
    assert(catalog.page(Json(R"({"succ":1,"chapterInfos":[{"chapterUid":849,"chapterIdx":849,"title":"Previous"}],"readerUrlParmas":[{"cUid":849,"param":"prev"}]})"),-1));
    assert(catalog.wire_begin==849&&catalog.wire_end==849);
    assert(catalog.initial(Json(R"({"chapterIndexes":[{"chapterUid":1,"chapterIdx":1,"title":"Heading"},{"chapterUid":1,"chapterIdx":1,"title":"Subheading"}],"readerUrlParams":[{"cUid":1,"param":"shared"}],"chapterInfoCount":2,"curChapter":{"chapterUid":1}})")));
    assert(catalog.items[0].param=="shared"&&catalog.items[1].param=="shared");
    SavedState saved;
    saved.preferences={24,170,true,false,true};
    saved.remember({"book-a","opaque",7,1,96});
    SavedState loaded;
    assert(loaded.decode(saved.encode()));
    assert(loaded.preferences==saved.preferences);
    assert(loaded.find("book-a") && loaded.find("book-a")->offset==96);
    loaded.bind_account("first");assert(loaded.find("book-a"));
    loaded.bind_account("first");assert(loaded.find("book-a"));
    SavedState same_account;assert(same_account.decode(loaded.encode())&&same_account.account=="first");
    same_account.bind_account("second");assert(same_account.progress.empty()&&same_account.account=="second");
    const auto saved_before=loaded.encode();
    assert(!loaded.decode(R"({"version":1,"preferences":{"font":0},"progress":[]})"));
    assert(loaded.encode()==saved_before);
    for(unsigned i=0;i<40;++i)saved.remember({number(i),"opaque",1,0,i});
    assert(saved.progress.size()==SavedState::max_books && !saved.find("book-a"));
    saved.remember({"8","opaque-new",2,0,2});
    saved.remember({"40","opaque",1,0,1});
    assert(saved.find("8") && !saved.find("9"));
    assert(saved.encode().size()<SavedState::max_bytes);
    if(argc>1){std::ifstream f(argv[1]);std::string raw((std::istreambuf_iterator<char>(f)),{});
        auto page=hydration.decode(raw);if(!page)std::cerr<<"public fixture stage="<<hydration.stage<<" position="<<hydration.position()<<'\n';
        assert(page);assert(!Json(*page).get("state").get("basePath").raw().empty());}
    if(argc>2){std::ifstream f(argv[2]);std::string raw((std::istreambuf_iterator<char>(f)),{});
        Search actual;assert(actual.parse(raw));assert(!actual.books.empty());}
    std::cout<<"WeRead bounded protocol/session/data-only hydration tests passed\n";
}
