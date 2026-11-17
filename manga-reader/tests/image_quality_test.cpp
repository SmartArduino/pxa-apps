#include "../jpeg_compat.hpp"
#include <cassert>
#include <cstdio>
#include <string>
static void write(const std::string& path,const std::vector<std::uint8_t>& data){auto f=fopen(path.c_str(),"wb");assert(f&&fwrite(data.data(),1,data.size(),f)==data.size());fclose(f);}
int main(int argc,char** argv){
    assert(argc==2);std::vector<std::uint8_t> rgb(320*180*3);
    for(unsigned y=0;y<180;++y)for(unsigned x=0;x<320;++x){auto p=&rgb[(y*320+x)*3];p[0]=x*255/319;p[1]=y*255/179;p[2]=(x+y)*255/498;}
    manga::JpegCompat image;assert(image.begin_rgb(rgb.data(),320,180,3));while(!image.done())assert(image.next_row());assert(image.borrowed&&image.pixels.empty());
    assert(image.begin_png());unsigned steps=0;while(!image.png_done()){assert(image.next_png());++steps;}auto png=image.png_result();assert(!png.empty()&&png.size()<40000&&steps>1);write(std::string(argv[1])+"/truecolor.png",png);write(std::string(argv[1])+"/truecolor.rgb",rgb);
    std::vector<std::uint8_t> large(480*1000*3);for(unsigned y=0;y<1000;++y)for(unsigned x=0;x<480;++x)for(unsigned c=0;c<3;++c)large[(y*480+x)*3+c]=(x+y+c*61)%256;
    manga::JpegCompat full;assert(full.begin_rgb(large.data(),480,1000,3));assert(full.width==480&&full.height==1000);while(!full.done())assert(full.next_row());assert(full.borrowed&&full.pixels.empty());auto full_png=full.png();assert(!full_png.empty()&&full_png.size()<524288);write(std::string(argv[1])+"/long-rgb.png",full_png);
    unsigned random=19;for(unsigned y=0;y<1000;++y)for(unsigned x=0;x<480;++x)for(unsigned c=0;c<3;++c){random=random*1664525u+1013904223u;large[(y*480+x)*3+c]=(((x/16+y/32+c*7)%30)*8)+(random>>29);}
    manga::JpegCompat detailed;assert(detailed.begin_rgb(large.data(),480,1000,3));while(!detailed.done())assert(detailed.next_row());auto detailed_png=detailed.png();assert(!detailed_png.empty()&&detailed.width==480&&detailed.height==1000&&detailed.png_precision==5);write(std::string(argv[1])+"/precision-fallback.png",detailed_png);write(std::string(argv[1])+"/precision-fallback.rgb",large);
    unsigned r=17;for(auto& b:large){r=r*1664525u+1013904223u;b=r>>24;}manga::JpegCompat noisy;assert(noisy.begin_rgb(large.data(),480,1000,3));while(!noisy.done())assert(noisy.next_row());auto reduced=noisy.png();assert(!reduced.empty()&&reduced.size()<=524288&&noisy.width<480);write(std::string(argv[1])+"/noise-bounded.png",reduced);
    printf("Exact RGB samples, cooperative compression, preserved 480x1000 detail, and noisy-image bound passed: %zu / %zu / %zu bytes\n",png.size(),full_png.size(),reduced.size());
}
