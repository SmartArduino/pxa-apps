#pragma once
#define Z_SOLO
#include "vendor/zlib/zlib.h"
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <span>
#include <vector>
namespace manga {
// One filtered scanline and a small deflate window; callers yield between steps.
// The 8-bit path retains exact RGB samples. An explicit lower-precision
// fallback keeps spatial detail when the encoded Host limit is exceeded.
class RgbPng {
    z_stream stream{};bool active=false,finished=false;
    const std::uint8_t* pixels=nullptr;unsigned width=0,height=0,row=0,precision=8;
    std::vector<std::uint8_t> scanline,output;std::array<std::uint8_t,4096> block{};std::size_t idat=0;
    static void* alloc(void*,unsigned count,unsigned bytes){if(bytes&&count>262144/bytes)return nullptr;return std::calloc(count,bytes);}
    static void release(void*,void* p){std::free(p);}
    static void put(std::uint8_t* p,std::uint32_t n){p[0]=n>>24;p[1]=n>>16;p[2]=n>>8;p[3]=n;}
    void number(std::uint32_t n){auto at=output.size();output.resize(at+4);put(output.data()+at,n);}
    static unsigned crc(std::span<const std::uint8_t> bytes){
        static constexpr auto table=[] {std::array<unsigned,256> t{};for(unsigned i=0;i<256;++i){unsigned n=i;for(int j=0;j<8;++j)n=(n>>1)^((n&1)?0xedb88320u:0);t[i]=n;}return t;}();
        unsigned n=~0u;for(auto b:bytes)n=table[(n^b)&255]^(n>>8);return ~n;
    }
    void chunk(const char* type,std::span<const std::uint8_t> bytes){number(bytes.size());auto at=output.size();output.insert(output.end(),type,type+4);output.insert(output.end(),bytes.begin(),bytes.end());number(crc(std::span{output}.subspan(at)));}
    bool compress(int flush){
        int result;
        do{stream.next_out=block.data();stream.avail_out=block.size();result=deflate(&stream,flush);if(result!=Z_OK&&result!=Z_STREAM_END)return false;
            auto n=block.size()-stream.avail_out;if(output.size()+n+24>524288){limited=true;return false;}output.insert(output.end(),block.begin(),block.begin()+n);
        }while(stream.avail_in||!stream.avail_out||(flush==Z_FINISH&&result!=Z_STREAM_END));return true;
    }
public:
    bool limited=false;
    ~RgbPng(){if(active)deflateEnd(&stream);}
    bool begin(const std::uint8_t* data,unsigned w,unsigned h,unsigned bits=8){
        if(active||!data||!w||!h||w>2048||h>2048)return false;pixels=data;width=w;height=h;precision=bits;scanline.resize(w*3+1);output.reserve(65536);
        stream.zalloc=alloc;stream.zfree=release;
        if(deflateInit2(&stream,3,Z_DEFLATED,15,5,Z_DEFAULT_STRATEGY)!=Z_OK)return false;active=true;
        output.insert(output.end(),{137,80,78,71,13,10,26,10});std::array<std::uint8_t,13> header{};put(header.data(),w);put(header.data()+4,h);header[8]=8;header[9]=2;chunk("IHDR",header);
        idat=output.size();number(0);output.insert(output.end(),{'I','D','A','T'});return true;
    }
    bool done()const{return finished;}
    bool step(){
        if(!active||finished)return false;
        for(unsigned batch=0;batch<8&&row<height;++batch,++row){auto line=pixels+std::size_t(row)*width*3;unsigned up=0,sub=0;auto sample=[&](unsigned b){return precision==8?b:std::min(255u,((b+4)>>3)<<3);};
            for(unsigned x=0;x<width*3;++x){up+=std::abs(int(std::int8_t(sample(line[x])-(row?sample(pixels[(std::size_t(row)-1)*width*3+x]):0))));sub+=std::abs(int(std::int8_t(sample(line[x])-(x>=3?sample(line[x-3]):0))));}
            scanline[0]=sub<up?1:2;
            for(unsigned x=0;x<width*3;++x)scanline[x+1]=sample(line[x])-(sub<up?(x>=3?sample(line[x-3]):0):(row?sample(pixels[(std::size_t(row)-1)*width*3+x]):0));
            stream.next_in=scanline.data();stream.avail_in=scanline.size();if(!compress(Z_NO_FLUSH)){finished=true;return false;}}
        if(row==height){stream.next_in=nullptr;stream.avail_in=0;if(!compress(Z_FINISH)){finished=true;return false;}deflateEnd(&stream);active=false;put(output.data()+idat,unsigned(output.size()-idat-8));number(crc(std::span{output}.subspan(idat+4)));chunk("IEND",{});finished=true;}
        return true;
    }
    std::vector<std::uint8_t> take(){if(!finished||active)return {};return std::move(output);}
};
}
