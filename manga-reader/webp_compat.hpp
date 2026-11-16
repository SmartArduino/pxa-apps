#pragma once
#include "vendor/libwebp/src/webp/decode.h"
#include <array>
#include <span>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <cstring>
extern "C" {
int manga_webp_budget(std::size_t);
std::size_t manga_webp_peak(void);
std::size_t manga_webp_live(void);
int manga_webp_denied(void);
}
namespace manga {
class WebpCompat {
    using Read=std::size_t(*)(void*,std::span<std::byte>);
    Read read=nullptr;void* context=nullptr;WebPDecoderConfig config{};WebPIDecoder* decoder=nullptr;
    std::array<std::byte,4096> chunk{};std::size_t received=0;bool complete=false,alpha=false,gathering=false;
    uint8_t* framed=nullptr;std::size_t framed_size=0,framed_offset=0,budget=4*1024*1024;
    bool append(const uint8_t* data,std::size_t n){
        if(!n||n>1048576-received){limited=n!=0;return false;}received+=n;
        auto status=WebPIAppend(decoder,data,n);
        if(status!=VP8_STATUS_OK&&status!=VP8_STATUS_SUSPENDED){limited=manga_webp_denied()||status==VP8_STATUS_OUT_OF_MEMORY;return false;}
        complete=status==VP8_STATUS_OK;
        if(complete&&alpha)for(std::size_t i=0;i<std::size_t(width)*height;++i){unsigned a=pixels[i*4+3];for(unsigned c=0;c<3;++c)pixels[i*3+c]=uint8_t((pixels[i*4+c]*a+255*(255-a)+127)/255);}
        return true;
    }
    bool setup(const uint8_t* data,std::size_t n){
        if(WebPGetFeatures(data,n,&config.input)!=VP8_STATUS_OK||config.input.has_animation)return false;
        auto w=unsigned(config.input.width),h=unsigned(config.input.height);if(!w||!h||std::uint64_t(w)*h>8388608){limited=true;return false;}
        double factor=std::min({1.,512./w,2048./h,std::sqrt(262144./(double(w)*h))});
        width=std::max(1u,unsigned(w*factor));height=std::max(1u,unsigned(h*factor));alpha=config.input.has_alpha;
        unsigned channels=alpha?4:3;auto bytes=std::size_t(width)*height*channels;
        pixels=static_cast<uint8_t*>(std::malloc(bytes));if(!pixels){limited=true;return false;}
        config.output.colorspace=alpha?MODE_RGBA:MODE_RGB;config.output.is_external_memory=1;
        config.output.u.RGBA.rgba=pixels;config.output.u.RGBA.stride=width*channels;config.output.u.RGBA.size=bytes;
        config.options.use_scaling=1;config.options.scaled_width=width;config.options.scaled_height=height;config.options.use_threads=0;
        if(!manga_webp_budget(budget)){limited=true;return false;}
        decoder=WebPIDecode(nullptr,0,&config);if(!decoder){limited=manga_webp_denied();return false;}return append(data,n);
    }
    static uint32_t le(const uint8_t* p,unsigned count){uint32_t v=0;for(unsigned i=0;i<count;++i)v|=uint32_t(p[i])<<(i*8);return v;}
    static void put(uint8_t* p,uint32_t v,unsigned count){for(unsigned i=0;i<count;++i)p[i]=uint8_t(v>>(i*8));}
    bool first_frame(){
        // Some still pages use an ANMF container with one full-canvas frame.
        // Decode that frame without linking an animation player or demux library.
        if(framed_size<12||std::memcmp(framed,"RIFF",4)||std::memcmp(framed+8,"WEBP",4)||uint64_t(le(framed+4,4))+8>framed_size)return false;
        auto end=std::size_t(le(framed+4,4))+8;
        for(std::size_t at=12;at+8<=end;){
            auto size=std::size_t(le(framed+at+4,4));if(size>end-at-8)return false;
            if(!std::memcmp(framed+at,"ANMF",4)){
                if(size<24)return false;auto frame=framed+at+8;
                if(le(frame,3)||le(frame+3,3)||le(frame+6,3)+1!=unsigned(config.input.width)||le(frame+9,3)+1!=unsigned(config.input.height))return false;
                auto payload=frame+16;auto bytes=size-16;bool alpha_chunk=!std::memcmp(payload,"ALPH",4);unsigned prefix=alpha_chunk?30:12;
                std::memmove(framed+prefix,payload,bytes);std::memcpy(framed,"RIFF",4);put(framed+4,prefix+bytes-8,4);std::memcpy(framed+8,"WEBP",4);
                if(alpha_chunk){std::memcpy(framed+12,"VP8X",4);put(framed+16,10,4);put(framed+20,0x10,4);put(framed+24,config.input.width-1,3);put(framed+27,config.input.height-1,3);}
                framed_size=prefix+bytes;framed_offset=std::min<std::size_t>(4096,framed_size);return setup(framed,framed_offset);
            }
            at+=8+size+(size&1);
        }return false;
    }
public:
    uint8_t* pixels=nullptr;unsigned width=0,height=0;bool limited=false;
    WebpCompat()=default;WebpCompat(const WebpCompat&)=delete;
    ~WebpCompat(){if(decoder)WebPIDelete(decoder);WebPFreeDecBuffer(&config.output);std::free(pixels);std::free(framed);}
    [[gnu::noinline]] bool begin(Read input,void* user,std::size_t allocation_limit=4*1024*1024){
        budget=allocation_limit;
        read=input;context=user;auto n=read(context,chunk);if(n<12||!WebPInitDecoderConfig(&config))return false;
        auto data=reinterpret_cast<const uint8_t*>(chunk.data());if(WebPGetFeatures(data,n,&config.input)!=VP8_STATUS_OK)return false;
        if(config.input.has_animation){framed=static_cast<uint8_t*>(std::malloc(1048576));if(!framed){limited=true;return false;}std::memcpy(framed,data,n);framed_size=n;gathering=true;return true;}
        return setup(data,n);
    }
    [[gnu::noinline]] bool next(){
        if(complete)return false;
        if(gathering){auto n=read(context,chunk);if(!n){gathering=false;return first_frame();}if(n>1048576-framed_size){limited=true;return false;}std::memcpy(framed+framed_size,chunk.data(),n);framed_size+=n;return true;}
        if(!decoder)return false;
        if(framed){auto n=std::min<std::size_t>(4096,framed_size-framed_offset);auto data=framed+framed_offset;framed_offset+=n;return append(data,n);}
        return append(reinterpret_cast<const uint8_t*>(chunk.data()),read(context,chunk));
    }
    bool done()const{return complete;}
};
}
