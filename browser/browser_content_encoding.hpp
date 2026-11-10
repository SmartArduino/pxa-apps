// SPDX-License-Identifier: MIT
#pragma once
#include "vendor/zlib/zlib.h"
#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <span>
#include <string_view>

namespace browser {
enum class ContentEncoding { identity, gzip, deflate, unsupported };
inline ContentEncoding content_encoding(std::string_view value) {
    while (!value.empty() && (value.front()==' ' || value.front()=='\t')) value.remove_prefix(1);
    while (!value.empty() && (value.back()==' ' || value.back()=='\t')) value.remove_suffix(1);
    auto equal=[&](std::string_view name) {
        if (value.size()!=name.size()) return false;
        for (std::size_t n=0;n<value.size();++n) {
            char c=value[n]; if(c>='A'&&c<='Z') c=char(c-'A'+'a');
            if(c!=name[n])return false;
        }
        return true;
    };
    if(value.empty()||equal("identity"))return ContentEncoding::identity;
    if(equal("gzip")||equal("x-gzip"))return ContentEncoding::gzip;
    if(equal("deflate"))return ContentEncoding::deflate;
    return ContentEncoding::unsupported; // Do not guess a stacked/unknown coding.
}

// HTTP content decoding only. No compressor, file I/O, whole-body buffer or
// window on identity responses. CRC/Adler and gzip member trailers are checked.
class ContentDecoder {
public:
    enum class Error { none, unsupported, memory, format, limit, truncated };
    struct Step { std::size_t consumed=0, produced=0; Error error=Error::none; };
    static constexpr std::size_t heap_limit=48*1024;
    ContentDecoder(ContentEncoding encoding,std::size_t limit):encoding_(encoding),limit_(limit){}
    ContentDecoder(const ContentDecoder&)=delete;
    ContentDecoder& operator=(const ContentDecoder&)=delete;
    ~ContentDecoder(){release();}
    void release() noexcept {
        if(state_&&initialized_)inflateEnd(state_.get());
        state_.reset();initialized_=false;
    }
    bool compressed()const{return encoding_==ContentEncoding::gzip||encoding_==ContentEncoding::deflate;}
    bool supported()const{return encoding_!=ContentEncoding::unsupported;}
    std::size_t size()const{return decoded_;}
    std::size_t peak_heap()const{return peak_;}
    bool pending()const{return drain_;}
    Error finish()const{return error_!=Error::none?error_:compressed()&&!ended_?Error::truncated:Error::none;}
    Step step(std::span<const std::byte> input,std::span<std::byte> output) {
        if(error_!=Error::none)return {0,0,error_};
        if(!supported())return fail(Error::unsupported);
        if(output.empty())return fail(Error::format);
        if(!compressed()) {
            auto n=std::min(input.size(),output.size());
            if(n>limit_-decoded_)return fail(Error::limit);
            if(output.data()!=input.data())std::memcpy(output.data(),input.data(),n);
            decoded_+=n;return {n,n};
        }
        if(!initialized_) {
            state_.reset(new(std::nothrow) z_stream{});
            if(!state_)return fail(Error::memory);
            state_->opaque=this;state_->zalloc=allocate;state_->zfree=deallocate;
            const auto status=inflateInit2(state_.get(),encoding_==ContentEncoding::gzip?31:15);
            if(status!=Z_OK){state_.reset();return fail(status==Z_MEM_ERROR?Error::memory:Error::format);}
            initialized_=true;
        }
        if(ended_) {
            if(input.empty())return {};
            if(encoding_!=ContentEncoding::gzip||members_>=16||inflateReset(state_.get())!=Z_OK)
                return fail(Error::format);
            ended_=false;
        }
        const auto in=std::min<std::size_t>(input.size(),std::numeric_limits<uInt>::max());
        const auto out=std::min<std::size_t>(output.size(),std::numeric_limits<uInt>::max());
        state_->next_in=reinterpret_cast<Bytef*>(const_cast<std::byte*>(input.data()));state_->avail_in=uInt(in);
        state_->next_out=reinterpret_cast<Bytef*>(output.data());state_->avail_out=uInt(out);
        const int status=inflate(state_.get(),Z_NO_FLUSH);
        Step result{in-state_->avail_in,out-state_->avail_out};drain_=state_->avail_out==0;
        if(result.produced>limit_-decoded_)return fail(Error::limit);
        decoded_+=result.produced;
        if(status==Z_STREAM_END){ended_=true;drain_=false;++members_;}
        else if(status==Z_MEM_ERROR)return fail(Error::memory);
        else if(status!=Z_OK&&status!=Z_BUF_ERROR)return fail(Error::format);
        if(!result.consumed&&!result.produced){drain_=false;if(!input.empty())return fail(Error::format);}
        return result;
    }
private:
    struct alignas(std::max_align_t) Allocation {std::size_t bytes;};
    static voidpf allocate(voidpf opaque,uInt count,uInt bytes) {
        auto& self=*static_cast<ContentDecoder*>(opaque);
        if(!bytes||count>(heap_limit-sizeof(Allocation))/bytes)return nullptr;
        const auto size=std::size_t(count)*bytes+sizeof(Allocation);
        if(size>heap_limit-self.live_)return nullptr;
        auto* p=static_cast<Allocation*>(std::calloc(1,size));if(!p)return nullptr;
        p->bytes=size;self.live_+=size;self.peak_=std::max(self.peak_,self.live_);return p+1;
    }
    static void deallocate(voidpf opaque,voidpf pointer) {
        if(!pointer)return;
        auto& self=*static_cast<ContentDecoder*>(opaque);auto* p=static_cast<Allocation*>(pointer)-1;
        self.live_-=p->bytes;std::free(p);
    }
    Step fail(Error error){error_=error;return {0,0,error};}
    ContentEncoding encoding_;
    std::size_t limit_,decoded_=0,live_=0,peak_=0;
    std::unique_ptr<z_stream> state_;
    unsigned members_=0;
    Error error_=Error::none;
    bool initialized_=false,ended_=false,drain_=false;
};
}
