#include "../notes.hpp"
#include <cassert>
#include <iostream>
int main(){
    using namespace weread;
    const std::string_view valid[]={"ASCII", "\xc2\x80", "\xdf\xbf", "\xe0\xa0\x80", "\xed\x9f\xbf", "\xee\x80\x80", "\xef\xbf\xbf", "\xf0\x90\x80\x80", "\xf4\x8f\xbf\xbf", "中𠳐文"};
    for(auto text:valid){
        Utf8Validator bytes;
        for(unsigned char c:text)assert(bytes.append(c));
        assert(bytes.complete());
        for(std::size_t split=0;split<=text.size();++split){
            Utf8Validator chunks;assert(chunks.append(text.substr(0,split)));
            assert(chunks.append(text.substr(split))&&chunks.complete());
        }
        assert(agent::note_text(Json(quote(text)),128)==text);
    }
    const std::string_view invalid[]={"\x80", "\xc0\x80", "\xc1\xbf", "\xe0\x80\x80", "\xed\xa0\x80", "\xf0\x80\x80\x80", "\xf4\x90\x80\x80", "\xf5\x80\x80\x80", "\xf8", "\xc2", "\xe0\xa0", "\xf0\x90\x80", "\xc2\r\n", "\xe0\x80"};
    for(auto text:invalid){
        Utf8Validator bytes;
        for(unsigned char c:text)(void)bytes.append(c);
        assert(!bytes.complete());
        assert(!bytes.append(static_cast<unsigned char>('A')));
        assert(!bytes.append("A")&&!bytes.complete());
        assert(!agent::note_text(Json(quote(text)),128));
    }
    std::cout<<"UTF-8 scalar boundaries, split streams, sticky errors and note normalization passed\n";
}
