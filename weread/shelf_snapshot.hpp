#pragma once
#include "protocol.hpp"

namespace weread {
// A small, account-bound first server batch. Never contains authentication or
// chapter bodies; opening a book still goes through the authorized service.
struct ShelfSnapshot {
    static constexpr std::size_t max_bytes=98304;
    static bool valid(const Shelf& shelf){
        if(shelf.books.size()>100||shelf.total<shelf.books.size()||shelf.kk< -1)return false;
        for(const auto& book:shelf.books)
            if(book.id.empty()||book.id.size()>128||!clean(book.id)||book.title.empty()||book.title.size()>384||
               book.author.size()>256||book.cover.size()>1024||book.param.size()>1024||!clean(book.param))return false;
        return true;
    }
    static std::optional<std::string> encode(const Shelf& shelf,std::string_view account){
        if(account.empty()||account.size()>64||!clean(account)||!valid(shelf))return {};
        std::string out;out.reserve(512+shelf.books.size()*256);
        JsonObjectWriter root(out);root.number<"version">(1);root.text<"account">(account);root.key<"data">();
        JsonObjectWriter data(out);data.number<"totalCount">(shelf.total);
        data.key<"kkIdx">();out+=number(shelf.kk);data.key<"books">();out+='[';
        bool first=true;
        for(const auto& book:shelf.books){
            if(!first)out+=',';first=false;
            JsonObjectWriter row(out);row.text<"bookId">(book.id);row.text<"title">(book.title);
            row.text<"author">(book.author);row.text<"cover">(book.cover);row.close();
            if(out.size()>max_bytes)return {};
        }
        out+=']';data.key<"bookReaderUrls">();out+='[';first=true;
        for(const auto& book:shelf.books){
            if(!first)out+=',';first=false;
            JsonObjectWriter row(out);row.text<"bId">(book.id);row.text<"param">(book.param);row.close();
            if(out.size()>max_bytes)return {};
        }
        out+=']';data.close();root.close();
        if(out.size()>max_bytes)return {};
        return out;
    }
    static std::optional<Shelf> decode(std::string_view raw,std::string_view account){
        if(raw.size()>max_bytes||account.empty())return {};
        Json root(raw);
        if(!root.valid()||root.get("version").number()!=1||root.get("account").string(64)!=account)return {};
        Shelf candidate;
        if(!candidate.parse(root.get("data").raw())||!valid(candidate))return {};
        return candidate;
    }
    static bool same_books(const Shelf& a,const Shelf& b){
        if(a.books.size()!=b.books.size())return false;
        for(std::size_t i=0;i<a.books.size();++i){
            const auto& x=a.books[i];const auto& y=b.books[i];
            if(x.id!=y.id||x.cover!=y.cover)return false;
        }
        return true;
    }
};
} // namespace weread
