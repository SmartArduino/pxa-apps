#pragma once
#include "reader_input.hpp"
#include "vendor/zlib/zlib.h"
#include <array>
#include <limits>

namespace weread {
// HTTP gzip only. zlib validates each header, CRC and decoded size. No file
// APIs or compression code are linked. A step emits at most one wire-sized
// block, allowing the caller to yield/cancel even for highly compressed input.
class ReaderGzip {
public:
    enum class Error {none,format,limit,memory};
    ReaderGzip(){
        stream.zalloc=[](voidpf,unsigned count,unsigned bytes)->voidpf{
            if(!bytes||count>65536/bytes)return nullptr;
            return std::calloc(count,bytes);
        };
        stream.zfree=[](voidpf,voidpf data){std::free(data);};
        initialized=inflateInit2(&stream,31)==Z_OK;
    }
    ReaderGzip(const ReaderGzip&)=delete;
    ReaderGzip& operator=(const ReaderGzip&)=delete;
    ~ReaderGzip(){if(initialized)inflateEnd(&stream);}
    bool complete()const{return ended;}
    bool pending()const{return drain;}
    std::size_t size()const{return decoded;}
    Error step(std::string_view& input,ReaderInput& result,std::size_t limit){return step(input,&result,limit);}
    // A null destination checks/counts without retaining decoded bytes.
    Error step(std::string_view& input,ReaderInput* result,std::size_t limit){
        if(!initialized)return Error::memory;
        if(ended){
            if(input.empty())return Error::none;
            if(members>=16||inflateReset2(&stream,31)!=Z_OK)return Error::format;
            ended=false;
        }
        const auto count=std::min<std::size_t>(input.size(),std::numeric_limits<uInt>::max());
        stream.next_in=reinterpret_cast<Bytef*>(const_cast<char*>(input.data()));stream.avail_in=uInt(count);
        stream.next_out=reinterpret_cast<Bytef*>(output.data());stream.avail_out=output.size();
        const auto code=inflate(&stream,Z_NO_FLUSH);
        const auto used=count-stream.avail_in,produced=output.size()-stream.avail_out;
        input.remove_prefix(used);drain=stream.avail_out==0;
        if(decoded>limit||produced>limit-decoded)return Error::limit;
        if(result&&!result->append({output.data(),produced},limit))return Error::memory;
        decoded+=produced;
        if(code==Z_STREAM_END){ended=true;drain=false;++members;return Error::none;}
        if(code==Z_MEM_ERROR)return Error::memory;
        if(code!=Z_OK&&code!=Z_BUF_ERROR)return Error::format;
        if(!used&&!produced){drain=false;if(!input.empty())return Error::format;}
        return Error::none;
    }
private:
    z_stream stream{};
    std::array<char,4064> output{};
    unsigned members=0;
    std::size_t decoded=0;
    bool initialized=false,ended=false,drain=false;
};
}
