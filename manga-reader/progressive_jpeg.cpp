#include "progressive_jpeg.hpp"
#include <algorithm>
#include <cstdlib>
#include <limits>
namespace {
constexpr std::size_t budget=6*1024*1024;
struct alignas(std::max_align_t) Block {std::size_t bytes;};
std::size_t live=0,peak=0;
bool limited=false;
void* bounded_alloc(std::size_t n){
    if(n>budget-live||n>std::numeric_limits<std::size_t>::max()-sizeof(Block)){limited=true;return nullptr;}
    auto b=static_cast<Block*>(std::malloc(sizeof(Block)+n));if(!b){limited=true;return nullptr;}
    b->bytes=n;live+=n;peak=std::max(peak,live);return b+1;
}
void bounded_free(void* p){if(p){auto b=static_cast<Block*>(p)-1;live-=b->bytes;std::free(b);}}
void* bounded_realloc(void* p,std::size_t n){
    if(!p)return bounded_alloc(n);auto old=static_cast<Block*>(p)-1;
    if(n>budget-(live-old->bytes)||n>std::numeric_limits<std::size_t>::max()-sizeof(Block)){limited=true;return nullptr;}
    auto bytes=old->bytes;auto b=static_cast<Block*>(std::realloc(old,sizeof(Block)+n));if(!b){limited=true;return nullptr;}
    b->bytes=n;live=live-bytes+n;peak=std::max(peak,live);return b+1;
}
struct Stream {manga::JpegRead read;void* context;bool ended=false;};
int stream_read(void* p,char* out,int n){auto& s=*static_cast<Stream*>(p);auto got=s.read(s.context,{reinterpret_cast<std::byte*>(out),std::size_t(std::max(0,n))});if(!got)s.ended=true;return int(got);}
void stream_skip(void* p,int n){auto& s=*static_cast<Stream*>(p);std::byte scratch[1024];while(n>0){auto got=s.read(s.context,std::span{scratch}.first(std::min(n,1024)));if(!got){s.ended=true;return;}n-=int(got);}}
int stream_eof(void* p){return static_cast<Stream*>(p)->ended;}
}
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_SIMD
#define STBI_MAX_DIMENSIONS 4096
#define STBI_MALLOC(n) bounded_alloc(n)
#define STBI_REALLOC(p,n) bounded_realloc(p,n)
#define STBI_FREE(p) bounded_free(p)
#define STB_IMAGE_IMPLEMENTATION
#include "vendor/stb/stb_image.h"
namespace manga {
static const stbi_io_callbacks callbacks{stream_read,stream_skip,stream_eof};
bool jpeg_header(JpegRead read,void* context,JpegHeader& header){
    Stream s{read,context};int w=0,h=0,c=0;
    if(!stbi_info_from_callbacks(&callbacks,&s,&w,&h,&c)||w<1||h<1||(c!=1&&c!=3))return false;
    header={unsigned(w),unsigned(h)};return true;
}
ProgressiveJpeg::~ProgressiveJpeg(){bounded_free(pixels);}
bool ProgressiveJpeg::load(JpegRead read,void* context){
    if(pixels||live)return false;limited=false;::limited=false;peak=0;
    Stream s{read,context};int w=0,h=0,c=0;
    pixels=stbi_load_from_callbacks(&callbacks,&s,&w,&h,&c,0);
    this->limited=::limited;peak_bytes=peak;
    if(!pixels)return false;
    width=unsigned(w);height=unsigned(h);channels=unsigned(c);return true;
}
}
