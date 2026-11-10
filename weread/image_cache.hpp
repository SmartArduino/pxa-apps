#pragma once
#include "book_image.hpp"
#include <pxa/app.hpp>

namespace weread {
// Four-way sets avoid replacing a neighbour on every hash collision. The URL
// is also stored verbatim: hashes select slots, never establish identity.
// Fixed filenames bound disk use, including drafts left by cancellation.
class ImageCache {
public:
    static constexpr unsigned slots=32,ways=4;
    static constexpr std::size_t max_file_bytes=131072;
    static constexpr std::size_t max_encoded_bytes=524288,max_large_file_bytes=max_encoded_bytes+2054;
    void configure(pxa::Context& context){context_=&context;}
    static unsigned first_slot(std::string_view url){
        std::uint32_t hash=2166136261u;
        for(unsigned char c:url){hash^=c;hash*=16777619u;}
        return (hash%(slots/ways))*ways;
    }
    static std::string path(unsigned slot){return "image-cache-v1-"+number(slot);}
    static std::string header(std::string_view url){return "WRC1\n"+std::string(url)+"\n";}
    unsigned victim(std::string_view url,std::size_t bytes)const{
        if(bytes>max_file_bytes)return slots;
        const auto first=first_slot(url);unsigned result=first;
        for(unsigned i=first+1;i<first+ways;++i)if(used_[i]<used_[result])result=i;
        return result;
    }
    pxa::Task<pxa::File> find(std::string_view url){
        const auto prefix=header(url);const auto first=first_slot(url);
        // Buffer is owned only for the duration of a cache lookup/download.
        auto buffer=std::make_unique<std::array<std::byte,1024>>();
        for(unsigned i=0;i<=ways;++i){
            const unsigned slot=i<ways?first+i:slots;
            const auto name=path(slot);
            auto file=co_await context_->fs().open(name);
            if(!file){used_[slot]=0;continue;}
            if(!used_[slot])used_[slot]=1;
            std::size_t at=0;bool match=true;
            while(at<prefix.size()){
                auto n=file->read(std::span{*buffer}.first(std::min(buffer->size(),prefix.size()-at)));
                if(!n&&n.error()==pxa::Error::would_block){
                    if(auto y=co_await context_->clock().yield();!y)co_return std::unexpected(y.error());
                    continue;
                }
                if(!n||!*n||std::memcmp(buffer->data(),prefix.data()+at,*n)){match=false;break;}
                at+=*n;
            }
            if(!match)continue;
            auto info=co_await context_->fs().stat(name);
            if(!info||info->size<=prefix.size()||info->size>(slot==slots?max_large_file_bytes:max_file_bytes)){
                (void)file->close();(void)co_await context_->fs().remove(name);used_[slot]=0;continue;
            }
            used_[slot]=++serial_;found_=slot;
            co_return std::move(*file);
        }
        co_return std::unexpected(pxa::Error::not_found);
    }
    pxa::Task<void> invalidate(){
        used_[found_]=0;co_return co_await context_->fs().remove(path(found_));
    }
    pxa::Task<void> write(pxa::File& file,std::span<const std::byte> bytes){
        while(!bytes.empty()){
            auto n=file.write(bytes.first(std::min<std::size_t>(4064,bytes.size())));
            if(!n&&n.error()==pxa::Error::would_block){
                if(auto y=co_await context_->clock().yield();!y)co_return std::unexpected(y.error());
                continue;
            }
            if(!n||!*n)co_return std::unexpected(n?pxa::Error::io_error:n.error());
            bytes=bytes.subspan(*n);
            if(auto y=co_await context_->clock().yield();!y)co_return std::unexpected(y.error());
        }
        co_return pxa::Result<void>{};
    }
    static constexpr std::string_view draft="image-cache-v1.tmp";
    struct Transfer {bool consumed=false,stream_failed=false;std::size_t bytes=0;};
    pxa::Task<pxa::File> store(pxa::HttpResponse& response,std::string_view url,Transfer& transfer){
        const auto prefix=header(url);
        auto file=co_await context_->fs().open(draft,pxa::OpenMode::write|pxa::OpenMode::create|pxa::OpenMode::truncate);
        if(!file)co_return std::unexpected(file.error());
        if(auto w=co_await write(*file,std::as_bytes(std::span{prefix.data(),prefix.size()}));!w)co_return std::unexpected(w.error());
        auto buffer=std::make_unique<std::array<std::byte,4064>>();std::size_t size=prefix.size();
        for(;;){
            transfer.consumed=true;
            auto n=co_await response.body.read_some(*buffer);
            if(!n){transfer.stream_failed=true;co_return std::unexpected(n.error());}
            if(!*n)break;
            if(*n>max_encoded_bytes-(size-prefix.size()))co_return std::unexpected(pxa::Error::limit_exceeded);
            if(auto w=co_await write(*file,std::span{*buffer}.first(*n));!w)co_return std::unexpected(w.error());
            size+=*n;
        }
        if(size==prefix.size()||(response.body_length&&size-prefix.size()!=*response.body_length)){
            transfer.stream_failed=true;
            co_return std::unexpected(pxa::Error::protocol_error);
        }
        if(auto c=file->close();!c)co_return std::unexpected(c.error());
        auto input=co_await context_->fs().open(draft);
        if(!input)co_return std::unexpected(input.error());
        if(auto s=co_await input->seek(prefix.size());!s)co_return std::unexpected(s.error());
        transfer.bytes=size;
        co_return std::move(*input);
    }
    void committed(unsigned slot){used_[slot]=++serial_;}
private:
    pxa::Context* context_=nullptr;
    std::array<std::uint64_t,slots+1> used_{};
    std::uint64_t serial_=1;
    unsigned found_=0;
};
} // namespace weread
