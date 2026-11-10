#include "../shelf_snapshot.hpp"
#include <cassert>
#include <iostream>

int main(){
    using namespace weread;
    Shelf shelf;assert(shelf.parse(R"({"books":[{"bookId":"1","title":"A \"中文\"","author":"作者","cover":"https://cdn.weread.qq.com/a.jpg?x=a&b=c"}],"bookReaderUrls":[{"bId":"1","param":"opaque?x=1&y=2"}],"totalCount":82,"kkIdx":-1})"));
    const auto raw=ShelfSnapshot::encode(shelf,"42");assert(raw);
    auto result=ShelfSnapshot::decode(*raw,"42");assert(result&&result->kk== -1&&result->total==82);
    assert(result->books[0].param==shelf.books[0].param&&result->books[0].title==shelf.books[0].title);
    assert(ShelfSnapshot::same_books(*result,shelf));
    assert(!ShelfSnapshot::decode(*raw,"43")&&!ShelfSnapshot::decode(*raw,""));
    assert(!ShelfSnapshot::decode(raw->substr(0,raw->size()-1),"42"));
    assert(!ShelfSnapshot::decode(std::string(ShelfSnapshot::max_bytes+1,'x'),"42"));
    auto changed=*result;changed.books[0].cover+="&version=2";assert(!ShelfSnapshot::same_books(changed,shelf));
    changed=*result;changed.books[0].title="new title";assert(ShelfSnapshot::same_books(changed,shelf));
    for(auto input:{R"({"version":2,"account":"42","data":{"books":[]}})",
        R"({"version":1,"account":"42","data":{"books":[{"bookId":"1","title":"x"},{"bookId":"1","title":"y"}]}})",
        R"({"version":1,"account":"42","data":{"books":[{"bookId":"1","title":"x"}],"totalCount":0}})",
        R"({"version":1,"account":"42","data":{"books":[],"kkIdx":-2}})"})assert(!ShelfSnapshot::decode(input,"42"));
    changed=shelf;changed.books[0].param="bad\nparam";assert(!ShelfSnapshot::encode(changed,"42"));
    assert(!ShelfSnapshot::encode(shelf,"bad\naccount"));
    Shelf empty;auto empty_raw=ShelfSnapshot::encode(empty,"42");assert(empty_raw&&ShelfSnapshot::decode(*empty_raw,"42"));
    std::cout<<"Shelf snapshot roundtrip, account isolation, bounds and corruption passed\n";
}
