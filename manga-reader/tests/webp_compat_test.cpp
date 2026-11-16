#include "../webp_compat.hpp"
#include "../jpeg_compat.hpp"
#include <cstdio>
#include <cassert>
#include <iostream>
static std::size_t read(void* f,std::span<std::byte> out){return std::fread(out.data(),1,out.size(),static_cast<FILE*>(f));}
int main(int argc,char** argv){
    assert(argc>=2);
    for(int i=1;i<argc;++i){
        auto f=std::fopen(argv[i],"rb");assert(f);
        {manga::WebpCompat denied;bool ok=denied.begin(read,f,64);while(ok&&!denied.done())ok=denied.next();assert(!ok&&denied.limited);}assert(manga_webp_live()==0);std::rewind(f);
        {manga::WebpCompat image;assert(image.begin(read,f));while(!image.done())assert(image.next());
            assert(image.width<=512&&image.height<=2048&&std::uint64_t(image.width)*image.height<=262144);assert(manga_webp_peak()<=4*1024*1024);
            if(std::string_view(argv[i]).ends_with("/alpha.webp"))for(std::size_t n=0;n<std::size_t(image.width)*image.height*3;++n)assert(image.pixels[n]==255);
            manga::JpegCompat png;assert(png.begin_rgb(image.pixels,image.width,image.height,3));while(!png.done())assert(png.next_row());assert(png.png().size()<=524288);
            std::cout<<argv[i]<<": "<<image.width<<"x"<<image.height<<", decoder peak="<<manga_webp_peak()<<"\n";
        }assert(manga_webp_live()==0);
        std::fseek(f,0,SEEK_END);auto total=std::ftell(f);std::rewind(f);struct Partial {FILE* file;std::size_t left=96;} partial{f};
        auto truncated=[](void* p,std::span<std::byte> out)->std::size_t{auto& state=*static_cast<Partial*>(p);auto n=std::fread(out.data(),1,std::min(out.size(),state.left),state.file);state.left-=n;return n;};
        {manga::WebpCompat broken;bool ok=broken.begin(truncated,&partial);while(ok&&!broken.done())ok=broken.next();if(total>96)assert(!ok);}assert(manga_webp_live()==0);std::fclose(f);
    }
    auto empty=[](void*,std::span<std::byte>)->std::size_t{return 0;};manga::WebpCompat broken;assert(!broken.begin(empty,nullptr));
    std::cout<<"WebP incremental decode, scaling, palette conversion, allocation cap and recovery passed\n";
}
