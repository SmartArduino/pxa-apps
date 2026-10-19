#include "../bookmarks.hpp"
#include <cassert>
#include <iostream>
using namespace weread;
int main(){
    LocalBookmarks marks{"account","book",{}};
    assert(marks.valid_identity()&&marks.items.empty());
    const auto path=marks.path();
    assert(path.starts_with("bookmarks-")&&path.ends_with(".json")&&path.size()<=31);
    assert(path!=LocalBookmarks({"account2","book",{}}).path());
    assert(path!=LocalBookmarks({"account","book2",{}}).path());
    assert(path!=LocalBookmarks({"acc","ountbook",{}}).path());
    LocalBookmark first{"chapter-param",bookmark_label("章节𠳐","正文\n第二段"),7,1,9,0xffffffffu};
    assert(marks.add(first));
    auto encoded=marks.encode();assert(encoded&&encoded->size()<LocalBookmarks::max_bytes);
    LocalBookmarks restored{"account","book",{}};assert(restored.decode(*encoded));
    assert(restored.items==marks.items&&restored.path()==path);
    assert(restored.items[0].resume("book")==Resume({"book","chapter-param",7,1,9,1}));
    first.label="更新标签";assert(marks.add(first)&&marks.items.size()==1&&marks.items[0].label==first.label);
    auto second=first;second.section=2;assert(marks.add(second)&&marks.items.size()==2);
    auto before=restored.encode();
    for(auto invalid:{
        std::string("{}"),std::string("[]"),std::string("not-json"),std::string(LocalBookmarks::max_bytes+1,' '),
        std::string(R"({"version":1,"account":"other","book":"book","items":[]})"),
        std::string(R"({"version":1,"account":"account","book":"other","items":[]})"),
        std::string(R"({"version":2,"account":"account","book":"book","items":[]})"),
        std::string(R"({"version":1,"account":"account","book":"book","items":{}})")}){
        assert(!restored.decode(invalid));assert(restored.encode()==before);
    }
    for(std::string_view field:{"chapter","section","offset","hash"}){
        auto raw=*encoded;const auto pos=raw.find(':'+number(field=="hash"?0xffffffffu:field=="chapter"?7:field=="section"?1:9),raw.find(quote(field)));
        assert(pos!=raw.npos);raw.insert(pos+1,"-");assert(!restored.decode(raw));assert(restored.encode()==before);
    }
    auto duplicate=*encoded;const auto start=duplicate.find("[{"),end=duplicate.find("}]",start);
    duplicate.insert(end+1,","+duplicate.substr(start+1,end-start));
    assert(!restored.decode(duplicate));assert(restored.encode()==before);
    for(auto label:{std::string("bad\nlabel"),std::string("bad\0label",9),std::string("\xff"),std::string(193,'x')}){
        auto invalid=first;invalid.label=label;assert(!marks.add(invalid));
    }
    for(auto param:{std::string(""),std::string("bad\nparam"),std::string(1025,'p')}){
        auto invalid=first;invalid.param=param;assert(!marks.add(invalid));
    }
    auto invalid=first;invalid.offset=196609;assert(!marks.add(invalid));invalid=first;invalid.section=8193;assert(!marks.add(invalid));
    marks.items.clear();
    for(unsigned i=0;i<LocalBookmarks::max_entries;++i){auto mark=first;mark.offset=i;assert(marks.add(mark));}
    auto extra=first;extra.offset=1000;assert(!marks.add(extra));
    encoded=marks.encode();assert(encoded&&restored.decode(*encoded)&&restored.items.size()==LocalBookmarks::max_entries);
    for(auto& mark:marks.items)mark.param=std::string(1024,'"');assert(!marks.encode());
    auto label=bookmark_label(std::string(200,'A'),std::string(200,'B'));
    assert(label.size()<=192&&label.find('B')!=label.npos&&valid_utf8(label));
    label=bookmark_label("𠳐章节",std::string(191,'A')+"𠳐");assert(label.size()<=192&&valid_utf8(label));
    assert(bookmark_text_hash("中文𠳐")!=bookmark_text_hash("中文𠳐x"));
    assert(bookmark_text_hash("")==2166136261u);
    std::cout<<"Local bookmarks roundtrip, identity, limits, duplicates and transactional decode passed\n";
}
