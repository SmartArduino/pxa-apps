#include "../webp_compat.hpp"
#include "../jpeg_compat.hpp"
#include <cstdio>
#include <cassert>
#include <iostream>
static std::size_t read(void* f,std::span<std::byte> out){return std::fread(out.data(),1,out.size(),static_cast<FILE*>(f));}
// Wrap a still bitstream as a full-canvas animation frame. Large frames must
// feed their prefix exactly once when the incremental decoder resumes.
static void animated_copy(const char* path,const manga::WebpCompat& original){
    auto file=std::fopen(path,"rb");assert(file);std::vector<unsigned char> input;std::array<unsigned char,4096> chunk{};
    for(std::size_t n;(n=std::fread(chunk.data(),1,chunk.size(),file));)input.insert(input.end(),chunk.begin(),chunk.begin()+n);std::fclose(file);
    WebPBitstreamFeatures features{};assert(WebPGetFeatures(input.data(),input.size(),&features)==VP8_STATUS_OK);if(features.has_animation)return;
    auto put=[](std::vector<unsigned char>& v,std::size_t at,unsigned value,unsigned count){for(unsigned i=0;i<count;++i)v[at+i]=value>>(i*8);};
    auto number=[](const unsigned char* p){return unsigned(p[0])|(unsigned(p[1])<<8)|(unsigned(p[2])<<16)|(unsigned(p[3])<<24);};
    std::vector<unsigned char> frame(16);put(frame,6,features.width-1,3);put(frame,9,features.height-1,3);put(frame,12,100,3);
    for(std::size_t at=12;at+8<=input.size();){auto n=std::size_t(number(input.data()+at+4));assert(n<=input.size()-at-8);auto end=at+8+n+(n&1);assert(end<=input.size());
        if(!std::memcmp(input.data()+at,"VP8 ",4)||!std::memcmp(input.data()+at,"VP8L",4)||!std::memcmp(input.data()+at,"ALPH",4))frame.insert(frame.end(),input.begin()+at,input.begin()+end);at=end;}
    std::vector<unsigned char> bytes(52);std::memcpy(bytes.data(),"RIFF",4);std::memcpy(bytes.data()+8,"WEBPVP8X",8);put(bytes,16,10,4);bytes[20]=2|(features.has_alpha?16:0);put(bytes,24,features.width-1,3);put(bytes,27,features.height-1,3);std::memcpy(bytes.data()+30,"ANIM",4);put(bytes,34,6,4);std::memcpy(bytes.data()+44,"ANMF",4);put(bytes,48,frame.size(),4);
    bytes.insert(bytes.end(),frame.begin(),frame.end());if(frame.size()&1)bytes.push_back(0);put(bytes,4,bytes.size()-8,4);
    struct Memory {std::span<const unsigned char> bytes;std::size_t cursor=0;} memory{bytes};
    auto read_memory=[](void* p,std::span<std::byte> out){auto& m=*static_cast<Memory*>(p);auto n=std::min(out.size(),m.bytes.size()-m.cursor);std::memcpy(out.data(),m.bytes.data()+m.cursor,n);m.cursor+=n;return n;};
    manga::WebpCompat image;assert(image.begin(read_memory,&memory));while(!image.done())assert(image.next());assert(image.width==original.width&&image.height==original.height);assert(!std::memcmp(image.pixels,original.pixels,std::size_t(image.width)*image.height*3));
}
int main(int argc,char** argv){
    assert(argc>=2);
    for(int i=1;i<argc;++i){
        auto f=std::fopen(argv[i],"rb");assert(f);
        {manga::WebpCompat denied;bool ok=denied.begin(read,f,64);while(ok&&!denied.done())ok=denied.next();assert(!ok&&denied.limited);}assert(manga_webp_live()==0);std::rewind(f);
        {manga::WebpCompat image;assert(image.begin(read,f));while(!image.done())assert(image.next());
            assert(image.width<=768&&image.height<=2048&&std::uint64_t(image.width)*image.height<=524288);assert(manga_webp_peak()<=4*1024*1024);
            if(std::string_view(argv[i]).ends_with("/alpha.webp"))for(std::size_t n=0;n<std::size_t(image.width)*image.height*3;++n)assert(image.pixels[n]==255);
            image.release_decoder();animated_copy(argv[i],image);
            manga::JpegCompat png;assert(png.begin_rgb(image.pixels,image.width,image.height,3));while(!png.done())assert(png.next_row());assert(png.png().size()<=524288);
            std::cout<<argv[i]<<": "<<image.width<<"x"<<image.height<<", decoder peak="<<manga_webp_peak()<<"\n";
        }assert(manga_webp_live()==0);
        std::fseek(f,0,SEEK_END);auto total=std::ftell(f);std::rewind(f);struct Partial {FILE* file;std::size_t left=96;} partial{f};
        auto truncated=[](void* p,std::span<std::byte> out)->std::size_t{auto& state=*static_cast<Partial*>(p);auto n=std::fread(out.data(),1,std::min(out.size(),state.left),state.file);state.left-=n;return n;};
        {manga::WebpCompat broken;bool ok=broken.begin(truncated,&partial);while(ok&&!broken.done())ok=broken.next();if(total>96)assert(!ok);}assert(manga_webp_live()==0);std::fclose(f);
    }
    auto empty=[](void*,std::span<std::byte>)->std::size_t{return 0;};manga::WebpCompat broken;assert(!broken.begin(empty,nullptr));
    std::cout<<"WebP incremental decode, scaling, truecolor PNG conversion, allocation cap and recovery passed\n";
}
