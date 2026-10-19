#pragma once
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <utility>

namespace weread {
// Owned response bytes. Unlike string growth, realloc may extend the last
// allocation in place and reports exhaustion without throwing/trapping in a
// Guest built with exceptions disabled. Views expire on append/move/destruction.
class ReaderInput {
    char* bytes=nullptr;
    std::size_t length=0,allocated=0;
public:
    ReaderInput()=default;
    ReaderInput(const ReaderInput&)=delete;
    ReaderInput& operator=(const ReaderInput&)=delete;
    ReaderInput(ReaderInput&& other)noexcept {swap(other);}
    ReaderInput& operator=(ReaderInput&& other)noexcept {
        ReaderInput moved(std::move(other));swap(moved);return *this;
    }
    ~ReaderInput(){std::free(bytes);}
    void swap(ReaderInput& other)noexcept {
        std::swap(bytes,other.bytes);std::swap(length,other.length);std::swap(allocated,other.allocated);
    }
    std::size_t size()const noexcept{return length;}
    std::size_t capacity()const noexcept{return allocated;}
    operator std::string_view()const noexcept{return view();}
    std::string_view view()const noexcept{return bytes?std::string_view(bytes,length):std::string_view{};}
    bool reserve(std::size_t wanted,std::size_t limit)noexcept {
        if(wanted>limit)return false;
        if(wanted<=allocated)return true;
        auto* grown=static_cast<char*>(std::realloc(bytes,wanted));
        if(!grown)return false;
        bytes=grown;allocated=wanted;return true;
    }
    // Input must not alias the owned allocation; the HTTP receive span is
    // separate fixed storage. A failed append preserves all previous bytes.
    bool append(std::string_view input,std::size_t limit)noexcept {
        if(length>limit||input.size()>limit-length)return false;
        const auto needed=length+input.size();
        if(needed>allocated){
            const auto extra=std::min(limit-allocated,std::clamp<std::size_t>(allocated/2,1024,65536));
            if(!reserve(std::max(needed,allocated+extra),limit)&&!reserve(needed,limit))return false;
        }
        if(!input.empty())std::memcpy(bytes+length,input.data(),input.size());
        length=needed;return true;
    }
};
} // namespace weread
