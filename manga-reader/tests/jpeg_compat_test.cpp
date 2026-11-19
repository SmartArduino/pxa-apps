#include "../jpeg_compat.hpp"
#include <cstdio>
#include <cassert>
int main(int argc,char** argv){
    if(argc!=3)return 2;
    auto file=std::fopen(argv[1],"rb");assert(file);
    manga::JpegCompat decoder;
    auto read=[](void* f,std::span<std::byte> bytes)->std::size_t{return std::fread(bytes.data(),1,bytes.size(),static_cast<std::FILE*>(f));};
    assert(decoder.begin(read,file));assert(decoder.width<=768&&decoder.height<=2048&&decoder.width*decoder.height<=524288);
    while(!decoder.done())assert(decoder.next_row());
    // Small scheduling units must preserve exactly the same output pixels.
    std::rewind(file);manga::JpegCompat sliced;assert(sliced.begin(read,file));unsigned steps=0;
    while(!sliced.done()){assert(sliced.step());++steps;}
    assert(steps==unsigned(sliced.info.m_MCUSPerRow)*unsigned(sliced.info.m_MCUSPerCol));
    assert(sliced.pixels==decoder.pixels);
    auto png=decoder.png();assert(!png.empty()&&png.size()<=524288);std::fclose(file);
    file=std::fopen(argv[2],"wb");assert(file);assert(std::fwrite(png.data(),1,png.size(),file)==png.size());std::fclose(file);
    std::printf("JPEG -> truecolor PNG: %u x %u, %zu bytes\n",decoder.width,decoder.height,png.size());
}
