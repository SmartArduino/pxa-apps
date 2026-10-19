#pragma once
#include "protocol.hpp"

namespace weread {
struct Search {
    std::vector<Book> books;
    std::string sid;
    std::uint32_t total=0,cursor=0,scope=17;
    bool parse(std::string_view raw){
        Json root(raw);if(raw.size()>131072||!root.valid())return false;
        Search candidate;candidate.sid=scalar(root.get("sid"),256);bool found=false,valid=true;
        if(!root.get("results").each([&](Json section){
            if(section.get("type").number()!=1)return true;
            if(found){valid=false;return false;}found=true;
            candidate.scope=section.get("scope").number().value_or(17);
            candidate.total=section.get("scopeCount").number().value_or(0);
            candidate.cursor=section.get("currentCount").number().value_or(0);
            valid=section.get("books").each([&](Json item){
                auto info=item.get("bookInfo");
                Book book{scalar(info.get("bookId"),128),scalar(info.get("title"),384),scalar(info.get("author"),256),scalar(info.get("cover"),1024),{}};
                if(book.id.empty()||book.title.empty()||!clean(book.id)){valid=false;return false;}
                candidate.books.push_back(std::move(book));return true;
            },40)&&valid;
            return valid;
        },32)||!valid)return false;
        if(!found){candidate.cursor=0;candidate.total=0;}
        if(candidate.cursor>candidate.total||candidate.books.size()>candidate.cursor||candidate.sid.empty())return false;
        *this=std::move(candidate);return true;
    }
};
} // namespace weread
