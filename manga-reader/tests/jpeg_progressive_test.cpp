#include "../progressive_jpeg.hpp"
#include "../jpeg_compat.hpp"
#include <cassert>
#include <cstdio>
#include <iostream>
#include <fstream>
static std::size_t read_file(void* f,std::span<std::byte> out){return std::fread(out.data(),1,out.size(),static_cast<FILE*>(f));}
int main(int argc,char** argv){
    if(argc<3)return 2;
    auto f=std::fopen(argv[1],"rb");assert(f);manga::JpegHeader header;
    assert(manga::jpeg_header(read_file,f,header));assert(std::uint64_t(header.width)*header.height<=1048576);std::rewind(f);
    {manga::ProgressiveJpeg decoded;assert(decoded.load(read_file,f));assert(decoded.peak_bytes<=6*1024*1024);
        manga::JpegCompat output;assert(output.begin_rgb(decoded.pixels,decoded.width,decoded.height,decoded.channels));
        while(!output.done())assert(output.next_row());auto png=output.png();assert(!png.empty()&&output.width*output.height<=262144);
        std::ofstream target(argv[2],std::ios::binary);target.write(reinterpret_cast<const char*>(png.data()),png.size());
        std::cout<<"progressive JPEG "<<decoded.width<<"x"<<decoded.height<<", peak decoder allocations="<<decoded.peak_bytes<<", output="<<output.width<<"x"<<output.height<<"\n";
    }std::fclose(f);
    // A malformed input and an allocation denial must leave the next decode usable.
    auto empty=[](void*,std::span<std::byte>)->std::size_t{return 0;};assert(!manga::jpeg_header(empty,nullptr,header));
    if(argc==4){f=std::fopen(argv[3],"rb");assert(f);manga::ProgressiveJpeg limited;assert(!limited.load(read_file,f)&&limited.limited);std::fclose(f);}
    f=std::fopen(argv[1],"rb");assert(f);{manga::ProgressiveJpeg again;assert(again.load(read_file,f));}std::fclose(f);
}
