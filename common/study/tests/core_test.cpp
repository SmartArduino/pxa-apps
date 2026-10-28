// SPDX-License-Identifier: MIT
#include "../core.hpp"
#include <cassert>
#include <iostream>
#include <set>

int main() {
    using namespace study;
    uint32_t seed=42;
    for(unsigned level=0;level<4;++level)for(unsigned i=0;i<10000;++i) {
        auto q=math_question(level,seed);
        assert(q.correct<4 && q.options[q.correct]==q.item.answer);
        assert(std::set<std::string>(q.options.begin(),q.options.end()).size()==4);
        assert(!q.item.explanation.empty() && q.item.id==identity(q.item));
        int left=0,right=0,total=0,expected=0;
        if(level==0) {
            assert(std::sscanf(q.item.question.c_str(),"%d + %d",&left,&right)==2);
            expected=left+right;
        } else if(level==1) {
            if(q.item.question.find("×")!=std::string::npos) {
                assert(std::sscanf(q.item.question.c_str(),"%d × %d",&left,&right)==2);
                expected=left*right;
            } else {
                assert(std::sscanf(q.item.question.c_str(),"%d - %d",&left,&right)==2);
                expected=left-right;
            }
        } else if(level==2) {
            assert(std::sscanf(q.item.question.c_str(),"%dx + %d = %d",&left,&right,&total)==3);
            assert((total-right)%left==0);expected=(total-right)/left;
        } else if(q.item.question.starts_with("原价")) {
            assert(std::sscanf(q.item.question.c_str(),"原价 %d 元，减价 %d%%",&left,&right)==2);
            expected=left*(100-right)/100;
        } else {
            assert(std::sscanf(q.item.question.c_str(),"%d 元的 %d%%",&left,&right)==2);
            expected=left*right/100;
        }
        assert(std::stoi(q.item.answer)==expected);
        if(level==0)assert(std::stoi(q.item.answer)<=10);
        uint32_t duplicate_seed=seed;auto a=math_question(level,duplicate_seed);
        duplicate_seed=seed;auto b=math_question(level,duplicate_seed);
        assert(a.item.id==b.item.id && a.options==b.options);
    }
    Progress p; p.level=2;
    p.record(100,false,20000);assert(p.find(100)->wrong && p.find(100)->due==20000);
    p.record(100,true,20000);assert(!p.find(100)->wrong && p.find(100)->due==20001);
    p.record(100,true,20001);assert(p.find(100)->due==20004);
    p.record(200,true,0);assert(p.find(200)->due==0);
    p.math_mistakes[2][3]=12345;
    auto bytes=encode(p,Kind::math);Progress loaded;
    assert(decode(loaded,Kind::math,bytes.data(),bytes.size()));
    assert(loaded.stats[2].answered==4 && loaded.stats[2].correct==3);
    assert(loaded.math_mistakes[2][3]==12345 && loaded.find(100)->due==20004);
    assert(!decode(loaded,Kind::words,bytes.data(),bytes.size()));
    for(unsigned i=0;i<bytes.size();++i) {bytes[i]^=1;assert(!decode(loaded,Kind::math,bytes.data(),bytes.size()));bytes[i]^=1;}
    assert(!decode(loaded,Kind::math,bytes.data(),bytes.size()-1));
    for(unsigned i=1;i<200;++i)p.record(1000+i,false,20000);
    // A full review table never discards an unresolved wrong answer.
    assert(p.find(1001)&&p.find(1001)->wrong);
    bytes=encode(p,Kind::math);
    assert(bytes.size()<=960 && decode(loaded,Kind::math,bytes.data(),bytes.size()));
    for(const auto& card:p.cards) {
        const auto* restored=loaded.find(card.id);
        assert(restored && restored->due==card.due && restored->wrong==card.wrong);
    }
    assert(loaded.math_mistakes[2][3]==12345);
    std::vector<Item> bank;
    for(unsigned i=0;i<24;++i) {Item item{std::to_string(i),"answer"+std::to_string(i),"note","",uint8_t(i/6),0};item.id=identity(item);bank.push_back(item);}
    Progress reviews;reviews.level=1;auto list=selection(bank,reviews,false,20000);
    assert(list.size()==6 && std::set<unsigned>(list.begin(),list.end()).size()==6);
    for(unsigned i:list)assert(bank[i].level==1);
    reviews.record(bank[8].id,false,20000);
    auto wrong=selection(bank,reviews,true,20000);assert(wrong.size()==1&&wrong[0]==8);
    reviews.record(bank[8].id,true,20000);
    assert(selection(bank,reviews,true,20000).empty());
    assert(selection(bank,reviews,true,20001).size()==1);
    auto q=choice(bank,8,seed);assert(q.options[q.correct]==bank[8].answer);
    std::string good=R"({"version":1,"cards":[{"level":0,"question":"一加一","answer":"2"},{"level":0,"question":"三加一","answer":"4"},{"level":0,"question":"三加三","answer":"6"},{"level":0,"question":"四加四","answer":"8"}]})";
    std::vector<Item> imported;
    assert(parse_course(good,imported)&&imported.size()==4);
    for(const auto& invalid : {std::string("{}"),good+"garbage",good.substr(0,good.size()-1),std::string(8193,' ')}) {
        auto before=imported[0].id;assert(!parse_course(invalid,imported)&&before==imported[0].id);
    }
    auto replace=[](std::string text,std::string_view from,std::string_view to) {
        const auto at=text.find(from);assert(at!=text.npos);text.replace(at,from.size(),to);return text;
    };
    auto invalid_courses=std::vector<std::string>{
        replace(good,"\"answer\":\"2\"","\"answer\":\"2\",\"explanation\":42"),
        replace(good,"\"answer\":\"2\"","\"answer\":\"2\",\"explanation\":null"),
        replace(good,"\"answer\":\"2\"","\"answer\":\"2\",\"explanation\":\""+std::string(481,'x')+"\""),
        replace(good,"三加一","一加一"),
        replace(good,"一加一"," \\t\\n "),
        replace(good,"\"answer\":\"2\"","\"answer\":\"  \\t\""),
        replace(good,"\"answer\":\"4\"","\"answer\":\" 2 \""),
        replace(good,"\"version\":1","\"version\":1.5"),
        replace(good,"\"level\":0","\"level\":0.5"),
        replace(good,"一加一",std::string("\xf8\x90\x80\x80",4)),
    };
    for(const auto& invalid:invalid_courses) {
        const auto before=imported;
        assert(!parse_course(invalid,imported));
        assert(imported.size()==before.size()&&imported[0].question==before[0].question);
    }
    assert(parse_course(replace(good,"\"answer\":\"2\"","\"answer\":\"2\",\"explanation\":\""+std::string(480,'x')+"\""),imported));
    assert(imported[0].explanation.size()==480);
    assert(parse_course(replace(good,"一加一"," \\t一加一 "),imported)&&imported[0].question=="一加一");
    assert(complete_json("\"\xf4\x8f\xbf\xbf\"")); // U+10FFFF is the last valid codepoint.
    for(unsigned lead=0xf5;lead<=0xff;++lead) {
        const char bad[]={ '"',char(lead),char(0x90),char(0x80),char(0x80),'"' };
        assert(!complete_json(std::string_view(bad,sizeof bad)));
    }
    assert(!complete_json("\"\xed\xa0\x80\"")); // A raw UTF-8 surrogate.
    assert(!complete_json("\"\xc0\xaf\"")); // An overlong sequence.
    std::vector<Stroke> strokes;
    assert(parse_strokes(R"({"medians":[[[100,900],[500,0]],[[500,500],[900,-20]]]})",strokes));
    assert(strokes.size()==2 && strokes[1][1].y== -20);
    for(auto invalid : {"{}","{\"medians\":[[[1,2]]]}" ,"{\"medians\":[[[1,2],[3000,5]]]}"})assert(!parse_strokes(invalid,strokes));
    assert(strokes.size()==2);
    assert(valid_feed_url("https://example.org/cards.json"));
    for(auto invalid : {"http://example.org/cards.json","https://","https://a@b/path","https://x/\nX"})assert(!valid_feed_url(invalid));
    for(unsigned i=0;i<10000;++i)assert(cache_slot(std::to_string(i))<8);
    std::cout<<"Study core: 40,000 generated math questions, save corruption, spaced review, course/stroke limits passed\n";
}
