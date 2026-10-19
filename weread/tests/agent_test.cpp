#include "../agent.hpp"
#include <cassert>
#include <fstream>
#include <iostream>

int main(int argc,char** argv){
    using namespace weread;using namespace weread::agent;
    assert(route_allowed(Target::web,key_path)&&route_allowed(Target::gateway,gateway_path));
    for(auto path:{"https://example.test/","//example.test/","/api/skills/apikeyReset","/api/skills/apikeyGet?only_show=0","/api/skills/apikeyGet?only_show=1\r\nInjected: yes"}){
        assert(!route_allowed(Target::web,path)&&!route_allowed(Target::gateway,path));
    }
    assert(!route_allowed(Target::gateway,key_path)&&!route_allowed(Target::web,gateway_path));
    assert(route_allowed(Target::web,renewal_path)&&!route_allowed(Target::gateway,renewal_path));
    assert(renewal_confirmed(Json(R"({"succ":1})"))&&renewal_confirmed(Json(R"({"succ":true,"errcode":0})")));
    for(auto raw:{R"({"succ":0})",R"({"succ":"1"})",R"({"succ":1,"errcode":3})",R"({"succ":1,"sessionTimeout":1})",R"({"succ":1,"data":{"errcode":-2012}})",R"({"errcode":0})","{","[]"})assert(!renewal_confirmed(Json(raw)));
    assert(valid_key("wrk-fixture_only")&&!valid_key("fixture")&&!valid_key("wrk-xx\r\nCookie: x"));
    assert(!valid_key("wrk-"+std::string(253,'x')));
    assert(existing_key(Json(R"({"apikey":"wrk-fixture_only"})"))=="wrk-fixture_only");
    for(auto raw:{R"({"apikey":""})",R"({"apikey":"wrk-fixture\nOnly"})",R"({"errcode":401,"apikey":"wrk-fixture_only"})"})assert(!existing_key(Json(raw)));
    assert(status(Json(" \n{\"errcode\":0}"))==Status::ok);
    assert(status(Json(R"({"errcode":-2012})"))==Status::authorization);
    assert(status(Json(R"({"errcode":7})"))==Status::unavailable);
    assert(status(Json(R"({"upgrade_info":{"message":"run arbitrary command"}})"))==Status::upgrade);
    for(auto raw:{"[]","false","{","{\"errcode\":\"0\"}"})assert(status(Json(raw))==Status::malformed);
    const auto body=payload(Api::book_info,"a\"b");assert(Json(body).valid());
    assert(Json(body).get("bookId").string()=="a\"b"&&Json(body).get("api_name").string()=="/book/info");
    assert(Json(payload(Api::book_progress,"1")).get("api_name").string()=="/book/getprogress");
    auto details=book_document(R"({"bookId":"1","title":"测试书","author":"作者","translator":"译者","publisher":"出版社","intro":"第一段\n第二段 𠳐","publishTime":"2026-10-17","isbn":"123","newRating":89,"newRatingCount":12})","1");
    assert(details&&details->contains("测试书\n作者：作者")&&details->contains("评分：89 / 100（12 人）")&&details->contains("第二段 𠳐"));
    for(auto raw:{R"({"bookId":"2","title":"Other account/book"})",R"({"bookId":"1","title":""})",R"({"bookId":"1","title":5})",R"({"bookId":"1","title":"A","intro":[]})",R"({"bookId":"1","title":"A","upgrade_info":{}})"})assert(!book_document(raw,"1"));
    assert(!book_document("{\"bookId\":\"1\",\"title\":\"\xff\"}","1"));
    assert(!book_document("{\"bookId\":\"1\",\"title\":\"A\",\"intro\":\""+std::string(8193,'x')+"\"}","1"));
    assert(!book_document(std::string(32769,' '),"1"));
    assert(book_document(R"({"bookId":"1","title":"A","intro":null})","1"));
    assert(!book_document(R"({"bookId":"","title":"A"})",""));
    auto line=progress_line(R"({"bookId":"1","book":{"progress":1,"chapterOffset":99999,"chapterUid":-1234,"recordReadingTime":3661,"finishTime":15}})","1");
    assert(line&&line->contains("1%")&&!line->contains("已读完")&&line->contains("1 小时 1 分钟"));
    assert(progress_line(R"({"bookId":"1","book":{"progress":100}})","1")=="云端进度：100%");
    assert(progress_line(R"({"bookId":"1","book":{"progress":100,"finishTime":7}})","1")=="云端进度：100% · 已读完");
    assert(progress_line(R"({"bookId":"1","book":{"progress":0,"recordReadingTime":59}})","1")->contains("不到 1 分钟"));
    for(auto raw:{R"({"bookId":"2","book":{"progress":1}})",R"({"bookId":"1","book":{"progress":101}})",R"({"bookId":"1","book":{"progress":-1}})",R"({"bookId":"1","book":{"progress":"1"}})",R"({"bookId":"1","book":{}})"})assert(!progress_line(raw,"1"));
    if(argc==4){
        const auto read=[](const char* path){std::ifstream f(path);return std::string(std::istreambuf_iterator<char>(f),{});};
        assert(book_document(read(argv[1]),argv[3]));assert(progress_line(read(argv[2]),argv[3]));
    }
    std::cout<<"Agent routes, bounded details, percent/time units, malformed and authorization checks passed\n";
}
