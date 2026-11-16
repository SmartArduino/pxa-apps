#pragma once
#include "vendor/picojpeg/picojpeg.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

namespace manga {
// Baseline originals can exceed the Host's 512 KiB encoded-image limit.
// Decode one MCU row at a time; retain only a bounded, downsampled palette image.
// The caller yields between rows and owns the file-backed compressed stream.
class JpegCompat {
public:
    using Read=std::size_t(*)(void*,std::span<std::byte>);
    pjpeg_image_info_t info{};
    unsigned width=0,height=0,row=0;
    std::vector<std::uint8_t> pixels;
    bool begin(Read read,void* context) {
        read_=read;context_=context;
        if(pjpeg_decode_init(&info,fill,this,0)||info.m_width<=0||info.m_height<=0||
           info.m_width>4096||info.m_height>4096||(info.m_comps!=1&&info.m_comps!=3))return false;
        double scale=std::min({1.,512./info.m_width,2048./info.m_height,
            std::sqrt(262144./(double(info.m_width)*info.m_height))});
        width=std::max(1u,unsigned(info.m_width*scale));height=std::max(1u,unsigned(info.m_height*scale));
        if(width*height>262144)return false;
        pixels.resize(width*height);return true;
    }
    bool done()const{return row>=unsigned(info.m_MCUSPerCol);}
    bool begin_rgb(const std::uint8_t* data,unsigned w,unsigned h,unsigned channels){
        if(!data||!w||!h||w>4096||h>4096||(channels!=1&&channels!=3))return false;
        rgb_=data;rgb_width_=w;rgb_height_=h;rgb_channels_=channels;
        double scale=std::min({1.,512./w,2048./h,std::sqrt(262144./(double(w)*h))});
        width=std::max(1u,unsigned(w*scale));height=std::max(1u,unsigned(h*scale));
        pixels.resize(width*height);row=0;info.m_MCUSPerCol=h;return true;
    }
    bool next_row() {
        if(done())return false;
        if(rgb_){
            auto end=std::min(rgb_height_,row+16);
            auto first=(row*height+rgb_height_-1)/rgb_height_,last=(end*height+rgb_height_-1)/rgb_height_;
            for(unsigned y=first;y<last;++y)for(unsigned x=0;x<width;++x){
                auto p=rgb_+((y*rgb_height_/height)*rgb_width_+x*rgb_width_/width)*rgb_channels_;
                unsigned r=p[0],g=rgb_channels_==1?r:p[1],b=rgb_channels_==1?r:p[2];pixels[y*width+x]=std::max({r,g,b})-std::min({r,g,b})<12?
                    216+((r+g+b)/3)*39/255:((r*5+127)/255)*36+((g*5+127)/255)*6+(b*5+127)/255;
            }row=end;return true;
        }
        const unsigned sy=row*info.m_MCUHeight;
        auto ceil_ratio=[](unsigned x,unsigned n,unsigned d){return (x*n+d-1)/d;};
        unsigned ya=ceil_ratio(sy,height,info.m_height),yb=std::min(height,ceil_ratio(sy+info.m_MCUHeight,height,info.m_height));
        for(unsigned col=0;col<unsigned(info.m_MCUSPerRow);++col){
            if(pjpeg_decode_mcu())return false;
            unsigned sx=col*info.m_MCUWidth;
            unsigned xa=ceil_ratio(sx,width,info.m_width),xb=std::min(width,ceil_ratio(sx+info.m_MCUWidth,width,info.m_width));
            for(unsigned y=ya;y<yb;++y)for(unsigned x=xa;x<xb;++x){
                unsigned rx=x*info.m_width/width-sx,ry=y*info.m_height/height-sy;
                unsigned i=(ry/8)*128+(rx/8)*64+(ry%8)*8+rx%8;
                unsigned r=info.m_pMCUBufR[i],g=info.m_comps==1?r:info.m_pMCUBufG[i],b=info.m_comps==1?r:info.m_pMCUBufB[i];
                pixels[y*width+x]=std::max({r,g,b})-std::min({r,g,b})<12?
                    216+((r+g+b)/3)*39/255:((r*5+127)/255)*36+((g*5+127)/255)*6+(b*5+127)/255;
            }
        }++row;return true;
    }
    std::vector<std::uint8_t> png() {
        if(!done()||pixels.empty())return {};
        pixels.resize((width+1)*height);
        for(unsigned y=height;y-- >0;){std::memmove(pixels.data()+y*(width+1)+1,pixels.data()+y*width,width);pixels[y*(width+1)]=0;}
        std::vector<std::uint8_t> out;out.reserve(pixels.size()+1024);
        out.insert(out.end(),{137,80,78,71,13,10,26,10});
        std::array<std::uint8_t,13> ihdr{};put(ihdr.data(),width);put(ihdr.data()+4,height);ihdr[8]=8;ihdr[9]=3;chunk(out,"IHDR",ihdr);
        std::array<std::uint8_t,768> palette{};
        for(unsigned i=0;i<256;++i){palette[i*3]=i<216?(i/36)*51:(i-216)*255/39;palette[i*3+1]=i<216?((i/6)%6)*51:palette[i*3];palette[i*3+2]=i<216?(i%6)*51:palette[i*3];}
        chunk(out,"PLTE",palette);
        const auto header=out.size();out.resize(out.size()+8);std::memcpy(out.data()+header+4,"IDAT",4);out.push_back(0x78);out.push_back(1);
        unsigned a=1,b=0;
        for(std::size_t at=0;at<pixels.size();){unsigned n=std::min<std::size_t>(65535,pixels.size()-at);out.push_back(at+n==pixels.size()?1:0);
            out.push_back(n);out.push_back(n>>8);out.push_back(~n);out.push_back((~n)>>8);
            out.insert(out.end(),pixels.begin()+at,pixels.begin()+at+n);
            for(unsigned k=0;k<n;++k){a=(a+pixels[at+k])%65521;b=(b+a)%65521;}at+=n;
        }
        append32(out,(b<<16)|a);put(out.data()+header,unsigned(out.size()-header-8));
        append32(out,crc(std::span{out}.subspan(header+4)));chunk(out,"IEND",{});
        std::vector<std::uint8_t>{}.swap(pixels);return out;
    }
private:
    const std::uint8_t* rgb_=nullptr;unsigned rgb_width_=0,rgb_height_=0,rgb_channels_=3;
    Read read_=nullptr;void* context_=nullptr;
    std::array<std::byte,4096> input_{};std::size_t begin_=0,end_=0;
    static unsigned char fill(unsigned char* out,unsigned char size,unsigned char* count,void* context){
        auto& s=*static_cast<JpegCompat*>(context);*count=0;
        while(*count<size){if(s.begin_==s.end_){s.end_=s.read_(s.context_,s.input_);s.begin_=0;if(!s.end_)break;}
            auto n=std::min<std::size_t>(size-*count,s.end_-s.begin_);std::memcpy(out+*count,s.input_.data()+s.begin_,n);s.begin_+=n;*count+=n;}
        return 0;
    }
    static void put(std::uint8_t* p,unsigned n){p[0]=n>>24;p[1]=n>>16;p[2]=n>>8;p[3]=n;}
    static void append32(std::vector<std::uint8_t>& out,unsigned n){auto p=out.size();out.resize(p+4);put(out.data()+p,n);}
    static unsigned crc(std::span<const std::uint8_t> bytes){
        static constexpr auto table=[] {std::array<unsigned,256> t{};for(unsigned i=0;i<256;++i){auto n=i;for(int j=0;j<8;++j)n=(n>>1)^((n&1)?0xedb88320u:0);t[i]=n;}return t;}();
        unsigned n=~0u;for(auto c:bytes)n=table[(n^c)&255]^(n>>8);return ~n;
    }
    static void chunk(std::vector<std::uint8_t>& out,const char* name,std::span<const std::uint8_t> bytes){
        append32(out,bytes.size());auto at=out.size();out.insert(out.end(),name,name+4);out.insert(out.end(),bytes.begin(),bytes.end());append32(out,crc(std::span{out}.subspan(at)));
    }
};
}
