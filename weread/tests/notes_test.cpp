#include "../notes.hpp"
#include <cassert>
#include <fstream>
#include <iostream>

int main(int argc,char** argv){
    using namespace weread;using namespace weread::agent;
    assert(number(INT64_MIN)=="-9223372036854775808");
    assert(number(true)=="1"&&number(false)=="0");
    auto body=payload(Api::thoughts,"a\"b",9007199254740993LL);
    Json request(body);assert(request.valid());
    assert(request.get("bookid").string()=="a\"b"&&request.get("bookId").raw().empty());
    assert(request.get("api_name").string()=="/review/list/mine"&&request.get("count").number()==10);
    assert(request.get("synckey").raw()=="9007199254740993");
    assert(payload(Api::thoughts,"1",-1).empty());
    assert(payload(static_cast<Api>(100),"1").empty());
    assert(Json(payload(Api::highlights,"1",10)).get("synckey").raw().empty());
    for(auto invalid:{"","-1","1.0","1e4","\"123\"","9223372036854775808"})assert(!note_cursor(Json(invalid)));
    assert(note_cursor(Json("9223372036854775807"))==INT64_MAX);
    assert(note_text(Json(R"("A\r\nB\rC\n𠳐")"),64)=="A\nB\nC\n𠳐");
    for(auto invalid:{R"("A\u0000B")",R"("A\u001bB")",R"("A\u007fB")","\"\xff\""})assert(!note_text(Json(invalid),64));
    std::string marks="[";
    for(unsigned i=0;i<23;++i){
        if(i)marks+=',';
        marks+="{\"bookId\":\"1\",\"type\":1,\"chapterUid\":1,\"markText\":\"Mark "+number(i)+"\"}";
    }
    marks+=']';
    const auto document="{\"updated\":"+marks+",\"chapters\":[{\"chapterUid\":1,\"title\":\"章节标题\"}]}";
    auto first=note_page(document,"1",NoteKind::highlights);
    assert(first&&first->count==10&&first->next==10&&first->more);
    assert(first->body.contains("章节标题\nMark 0")&&first->body.contains("Mark 9")&&!first->body.contains("Mark 10"));
    auto second=note_page(document,"1",NoteKind::highlights,10);
    assert(second&&second->count==10&&second->next==20&&second->more);
    auto last=note_page(document,"1",NoteKind::highlights,20);
    assert(last&&last->count==3&&last->next==23&&!last->more&&last->body.contains("Mark 22"));
    assert(!note_page(document,"1",NoteKind::highlights,24));
    assert(!note_page(document,"2",NoteKind::highlights));
    assert(!note_page(document,"1",NoteKind::highlights,INT64_MAX));
    assert(!note_page(document,"1",static_cast<NoteKind>(10)));
    for(auto invalid:{"{}",R"({"updated":{}})",R"({"updated":[{"bookId":"1","type":0,"markText":"Bookmark"}]})",
        R"({"updated":[{"bookId":"1","type":1,"markText":3}]})",R"({"errcode":401,"updated":[]})",
        R"({"updated":[{"bookId":"1","type":1,"markText":"A"}],"chapters":{}})"})assert(!note_page(invalid,"1",NoteKind::highlights));
    assert(note_page(R"({"updated":[]})","1",NoteKind::highlights)->body=="暂无划线");
    const auto thoughts=R"({"reviews":[{"review":{"bookId":"1","chapterTitle":"第一章","abstract":"原文\r\n第二行","content":"个人想法"}},{"review":{"bookId":"1","chapterName":"整本书","content":"个人点评"}}],"hasMore":1,"synckey":9007199254740993})";
    auto page=note_page(thoughts,"1",NoteKind::thoughts);
    assert(page&&page->count==2&&page->more&&page->next==9007199254740993LL);
    assert(page->body.contains("第一章\n原文：原文\n第二行\n想法：个人想法")&&page->body.contains("整本书\n想法：个人点评"));
    assert(!note_page(thoughts,"1",NoteKind::thoughts,9007199254740993LL));
    for(auto invalid:{R"({"reviews":[],"hasMore":1,"synckey":0})",R"({"reviews":[],"hasMore":1,"synckey":-1})",
        R"({"reviews":[],"hasMore":"0"})",R"({"reviews":[]})",R"({"reviews":[{"review":{"bookId":"2","content":"Other"}}],"hasMore":0})",
        R"({"reviews":[{"review":{"bookId":"1","content":"Good"}},{"review":{"bookId":"1","content":null}}],"hasMore":0})"})assert(!note_page(invalid,"1",NoteKind::thoughts));
    assert(note_page(R"({"reviews":[],"hasMore":false})","1",NoteKind::thoughts)->body=="本页暂无个人想法");
    auto empty_more=note_page(R"({"reviews":[],"hasMore":true,"synckey":123})","1",NoteKind::thoughts);
    assert(empty_more&&empty_more->more&&empty_more->next==123);
    assert(!note_page(std::string(note_response_limit+1,' '),"1",NoteKind::highlights));
    assert(!note_page("{\"updated\":[{\"bookId\":\"1\",\"type\":1,\"markText\":\""+std::string(8193,'x')+"\"}]}","1",NoteKind::highlights));
    std::string large="{\"reviews\":[";
    for(int i=0;i<5;++i){if(i)large+=',';large+="{\"review\":{\"bookId\":\"1\",\"content\":\""+std::string(8192,'x')+"\"}}";}
    large+="],\"hasMore\":0}";assert(!note_page(large,"1",NoteKind::thoughts));
    if(argc==4){
        const auto read=[](const char* path){std::ifstream f(path);return std::string(std::istreambuf_iterator<char>(f),{});};
        assert(note_page(read(argv[1]),argv[3],NoteKind::highlights));
        assert(note_page(read(argv[2]),argv[3],NoteKind::thoughts));
    }
    std::cout<<"Notes windows, 64-bit cursors, Unicode, wrong-book, malformed/limit and optional real response checks passed\n";
}
