#pragma once
#include "vendor/picojpeg/picojpeg.h"
#include "png_rgb.hpp"
#include <memory>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

namespace manga {
// Baseline originals can exceed the Host's 512 KiB encoded-image limit.
// Decode one MCU row at a time; retain only a bounded, downsampled truecolor image.
// The caller yields between rows and owns the file-backed compressed stream.
class JpegCompat {
public:
    using Read=std::size_t(*)(void*,std::span<std::byte>);
    pjpeg_image_info_t info{};
    unsigned width=0,height=0,row=0;
    std::vector<std::uint8_t> pixels;
    unsigned max_width=768,max_pixels=524288;
    std::unique_ptr<RgbPng> encoder;bool borrowed=false;unsigned png_precision=8;
    bool begin(Read read,void* context) {
        read_=read;context_=context;
        if(pjpeg_decode_init(&info,fill,this,0)||info.m_width<=0||info.m_height<=0||
           info.m_width>4096||info.m_height>4096||(info.m_comps!=1&&info.m_comps!=3))return false;
        double scale=std::min({1.,double(max_width)/info.m_width,2048./info.m_height,
            std::sqrt(double(max_pixels)/(double(info.m_width)*info.m_height))});
        width=std::max(1u,unsigned(info.m_width*scale));height=std::max(1u,unsigned(info.m_height*scale));
        if(width*height>max_pixels)return false;
        pixels.resize(std::size_t(width)*height*3);return true;
    }
    bool done()const{return row>=unsigned(info.m_MCUSPerCol);}
    bool begin_rgb(const std::uint8_t* data,unsigned w,unsigned h,unsigned channels){
        if(!data||!w||!h||w>4096||h>4096||(channels!=1&&channels!=3))return false;
        rgb_=data;rgb_width_=w;rgb_height_=h;rgb_channels_=channels;
        double scale=std::min({1.,double(max_width)/w,2048./h,std::sqrt(double(max_pixels)/(double(w)*h))});
        width=std::max(1u,unsigned(w*scale));height=std::max(1u,unsigned(h*scale));
        row=0;info.m_MCUSPerCol=h;borrowed=width==w&&height==h&&channels==3;if(borrowed){row=h;return true;}pixels.resize(std::size_t(width)*height*3);return true;
    }
    bool next_row() {
        if(done())return false;
        if(rgb_){
            auto end=std::min(rgb_height_,row+16);
            auto first=(row*height+rgb_height_-1)/rgb_height_,last=(end*height+rgb_height_-1)/rgb_height_;
            for(unsigned y=first;y<last;++y)for(unsigned x=0;x<width;++x){
                auto p=rgb_+((y*rgb_height_/height)*rgb_width_+x*rgb_width_/width)*rgb_channels_;
                auto target=pixels.data()+(std::size_t(y)*width+x)*3;target[0]=p[0];target[1]=rgb_channels_==1?p[0]:p[1];target[2]=rgb_channels_==1?p[0]:p[2];
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
                auto target=pixels.data()+(std::size_t(y)*width+x)*3;target[0]=r;target[1]=g;target[2]=b;
            }
        }++row;return true;
    }
    const std::uint8_t* rgb_data()const{return borrowed?rgb_:pixels.data();}
    bool begin_png(){encoder=std::make_unique<RgbPng>();return done()&&(borrowed||!pixels.empty())&&encoder->begin(borrowed?rgb_:pixels.data(),width,height,png_precision);}
    bool png_done()const{return encoder&&encoder->done();}
    bool next_png(){return encoder&&encoder->step();}
    std::vector<std::uint8_t> png_result(){return encoder?encoder->take():std::vector<std::uint8_t>{};}
    bool shrink(){
        encoder.reset();if(png_precision==8){png_precision=5;return true;}if(width<96||height<96)return false;unsigned w=width*4/5,h=height*4/5;
        if(borrowed){pixels.resize(std::size_t(w)*h*3);for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x)std::memcpy(pixels.data()+(std::size_t(y)*w+x)*3,rgb_+(std::size_t(y*height/h)*width+x*width/w)*3,3);borrowed=false;}
        else for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x)for(unsigned c=0;c<3;++c)pixels[(std::size_t(y)*w+x)*3+c]=pixels[(std::size_t(y*height/h)*width+x*width/w)*3+c];
        width=w;height=h;pixels.resize(std::size_t(w)*h*3);return true;
    }
    std::vector<std::uint8_t> png(){
        for(unsigned attempt=0;attempt<5;++attempt){if(!begin_png())return {};while(!png_done())if(!next_png())break;auto out=png_result();if(!out.empty())return out;if(!encoder->limited||!shrink())break;}return {};
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
};
}
