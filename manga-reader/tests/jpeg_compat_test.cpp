#include "../jpeg_compat.hpp"
#include <cstdio>
#include <cassert>
int main(int argc,char** argv){
    if(argc!=3)return 2;
    auto file=std::fopen(argv[1],"rb");assert(file);
    manga::JpegCompat decoder;
    auto read=[](void* f,std::span<std::byte> bytes)->std::size_t{return std::fread(bytes.data(),1,bytes.size(),static_cast<std::FILE*>(f));};
    assert(decoder.begin(read,file));assert(decoder.width<=512&&decoder.height<=2048&&decoder.width*decoder.height<=262144);
    while(!decoder.done())assert(decoder.next_row());
    auto png=decoder.png();assert(!png.empty()&&png.size()<=524288);std::fclose(file);
    file=std::fopen(argv[2],"wb");assert(file);assert(std::fwrite(png.data(),1,png.size(),file)==png.size());std::fclose(file);
    std::printf("JPEG -> palette PNG: %u x %u, %zu bytes\n",decoder.width,decoder.height,png.size());
}
