// SPDX-License-Identifier: MIT
#pragma once
#include "pixel_bank.hpp"
#include "pxa_assets.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#ifdef CARROT_RASTER_TIMING
#include <time.h>
#endif

namespace carrot {
extern "C" bool carrot_inflate(const unsigned char*,size_t,unsigned char*,size_t);
// This renderer interprets this application's small Canvas vocabulary. Art is
// decoded independently of the Host PNG cache; the game/UI/input stay unchanged.
class PixelRenderer {
public:
  struct Rect { int x,y,w,h; };
  uint64_t hits=0, misses=0;
  uint64_t sourceMisses=0;
  uint64_t backdropRevision=0;
  uint32_t density=65536;
  int width=480,height=480,stride=480;
  size_t staticBytes=0;
private:
#ifdef CARROT_RASTER_TIMING
  struct RasterTimes{uint64_t image=0,blit=0,source=0,shape=0,line=0,restore=0,hud=0;} raster;
  struct RasterTimer{
    uint64_t& total;uint64_t began;
    static uint64_t now(){timespec t{};clock_gettime(CLOCK_MONOTONIC,&t);return uint64_t(t.tv_sec)*1000000+t.tv_nsec/1000;}
    RasterTimer(uint64_t& sum):total(sum),began(now()){}
    ~RasterTimer(){total+=now()-began;}
  };
#endif
  friend struct PixelRendererTest;
  struct Block { uint64_t key=0,used=0; size_t at=0,size=0; };
  template<size_t N> struct Arena {
    std::vector<uint8_t> bytes;
    std::array<Block,N> blocks{};
    std::array<uint16_t,2048> index{};
    std::vector<uint16_t> allocationOrder;
    uint64_t clock=0;
    uint64_t evictions=0,movedBytes=0;
    void reset(size_t capacity) { bytes.resize(capacity); blocks={};index={};clock=evictions=movedBytes=0;allocationOrder.clear();allocationOrder.reserve(N); }
    static unsigned bucket(uint64_t key){return (key^(key>>32))&2047;}
    void remember(unsigned id){
      unsigned slot=bucket(blocks[id].key);
      while(index[slot])slot=(slot+1)&2047;
      index[slot]=id+1;
    }
    void forget(unsigned id){
      unsigned slot=bucket(blocks[id].key);
      while(index[slot]&&index[slot]!=id+1)slot=(slot+1)&2047;
      if(!index[slot])return;
      index[slot]=0;
      for(slot=(slot+1)&2047;index[slot];slot=(slot+1)&2047){unsigned moved=index[slot]-1;index[slot]=0;remember(moved);}
    }
    uint8_t* find(uint64_t key) {
      for(unsigned slot=bucket(key);index[slot];slot=(slot+1)&2047){
        auto& b=blocks[index[slot]-1];
        if(b.key==key){b.used=++clock;return bytes.data()+b.at;}
      }
      return nullptr;
    }
    uint8_t* insert(uint64_t key,size_t count) {
      if(count>bytes.size())return nullptr;
      for(;;) {
        size_t live=0;
        Block* free=nullptr;Block* oldest=nullptr;
        for(auto& b:blocks) if(b.used) {
          live+=b.size;
          if(!oldest||b.used<oldest->used)oldest=&b;
        } else free=&b;
        if(free && live+count<=bytes.size()) {
          size_t end=0;
          // Reuse a hole or the unused tail before moving any live pixels.
          // Compacting a megabyte on every new pose defeats sprite caching.
          for(size_t i=0;i<=allocationOrder.size();++i) {
            size_t next=i<allocationOrder.size()?blocks[allocationOrder[i]].at:bytes.size();
            if(next-end>=count){*free={key,++clock,end,count};unsigned id=unsigned(free-blocks.data());remember(id);allocationOrder.insert(allocationOrder.begin()+i,id);return bytes.data()+end;}
            if(i<allocationOrder.size()){const auto& b=blocks[allocationOrder[i]];end=b.at+b.size;}
          }
          end=0;
          for(unsigned id:allocationOrder) {
            auto& b=blocks[id];
            if(b.at!=end){std::memmove(bytes.data()+end,bytes.data()+b.at,b.size);movedBytes+=b.size;}
            b.at=end;end+=b.size;
          }
          *free={key,++clock,end,count};unsigned id=unsigned(free-blocks.data());remember(id);allocationOrder.push_back(id);return bytes.data()+end;
        }
        if(!oldest)return nullptr;
        unsigned id=unsigned(oldest-blocks.data());forget(id);allocationOrder.erase(std::find(allocationOrder.begin(),allocationOrder.end(),id));*oldest={};++evictions;
      }
    }
  };
  Arena<128> sources;
  Arena<768> prepared;
  std::vector<uint16_t> backdrop;
  uint64_t backdropHash=0;
  static constexpr uint64_t ReadToken=7320;
  int loading=-1;
  std::vector<uint8_t> compressed;
  uint32_t loaded=0,requestedBytes=0;
  struct Read {uint32_t offset=0,bytes=0;bool active=false;};
  std::array<Read,1> reads{};
  bool incomplete=false,loadFailed=false;
  void requestRead() {
    if(loading<0||loadFailed)return;
    char path[96];std::snprintf(path,sizeof(path),"%s",pixels::sources[loading].path);
    std::memcpy(path+std::strlen(path)-4,".bin",5);
    for(unsigned i=0;i<reads.size();++i){auto& r=reads[i];if(r.active)continue;
      if(!r.bytes){if(requestedBytes==compressed.size())continue;r.offset=requestedBytes;r.bytes=std::min(uint32_t(PXA_ASSETS_READ_MAX_BYTES),uint32_t(compressed.size()-requestedBytes));requestedBytes+=r.bytes;}
      int status=pxa_assets_read(ReadToken+i,path,r.offset,r.bytes);r.active=status==PXA_STATUS_OK;
      if(!r.active&&status!=PXA_STATUS_BUSY&&status!=PXA_STATUS_WOULD_BLOCK){loadFailed=true;char message[180];std::snprintf(message,sizeof(message),"carrot background read unavailable status=%d path=%s offset=%u bytes=%u",status,path,r.offset,r.bytes);pxa_log_write(3,message);return;}
    }
  }
  uint16_t* target=nullptr;
  bool layerTarget=false;
  int layerTop=0,layerBottom=0;
  uint64_t layerKey=0;
  std::vector<uint16_t> hudColors;
  std::vector<uint8_t> hudAlpha;
  struct HudRun {unsigned at,count,alpha;};
  std::array<std::vector<HudRun>,2> hudRuns;
  std::array<uint64_t,2> hudKeys{};
  struct ShapeRows {
    int w=0,h=0,border=0,radius=0,type=0;
    uint64_t used=0;
    std::vector<std::array<int16_t,4>> rows;
  };
  std::array<std::array<ShapeRows,2>,32> shapeRows{};
  uint64_t shapeClock=0;
  struct BufferDamage {
    uint16_t* address=nullptr;
    int stride=0;
    uint64_t revision=0;
    std::vector<uint8_t> rows;
  };
  std::array<BufferDamage,2> bufferDamage;
  uint8_t* dirtyRows=nullptr;
  uint16_t* rowPixels(int y){
    if(!layerTarget){if(dirtyRows)dirtyRows[y]=1;return target+y*stride;}
    if(y>=layerTop&&y<layerBottom)return nullptr;
    return target+(y<layerTop?y:y-layerBottom+layerTop)*stride;
  }
  uint8_t* rowAlpha(int y){return layerTarget?hudAlpha.data()+(y<layerTop?y:y-layerBottom+layerTop)*stride:nullptr;}
  Rect clip{};
  static uint16_t u16(const uint8_t* p){return unsigned(p[0])|unsigned(p[1])<<8;}
  static uint32_t u32(const uint8_t* p){return u16(p)|uint32_t(u16(p+2))<<16;}
  static int s32(const uint8_t* p){return int32_t(u32(p));}
  static void put16(std::vector<uint8_t>& b,unsigned value){b.push_back(value);b.push_back(value>>8);}
  static void put32(std::vector<uint8_t>& b,unsigned value){put16(b,value);put16(b,value>>16);}
  static Rect intersect(Rect a,Rect b) {
    int x=std::max(a.x,b.x),y=std::max(a.y,b.y);
    return {x,y,std::max(0,std::min(a.x+a.w,b.x+b.w)-x),std::max(0,std::min(a.y+a.h,b.y+b.h)-y)};
  }
  static uint64_t hash(const uint8_t* p,size_t n,uint64_t seed=1469598103934665603ULL) {
    for(size_t i=0;i<n;++i)seed=(seed^p[i])*1099511628211ULL;
    return seed;
  }
  static uint16_t color565(uint32_t premultiplied) {
    return ((premultiplied>>8)&0xf800)|((premultiplied>>5)&0x7e0)|((premultiplied>>3)&31);
  }
  static uint16_t over(uint16_t source,unsigned alpha,uint16_t dest) {
    if(alpha==255)return source;
    unsigned inv=255-alpha;
    unsigned r=(source>>11)+(((dest>>11)*inv+127)/255);
    unsigned g=((source>>5)&63)+((((dest>>5)&63)*inv+127)/255);
    unsigned b=(source&31)+(((dest&31)*inv+127)/255);
    return (std::min(r,31u)<<11)|(std::min(g,63u)<<5)|std::min(b,31u);
  }
  struct Blend {
    uint32_t pixel=0;bool valid=false;uint32_t used=0;
    uint16_t red[32]{},green[64]{},blue[32]{};
    uint16_t apply(uint16_t dest)const{return red[dest>>11]|green[(dest>>5)&63]|blue[dest&31];}
  };
  std::array<std::array<Blend,2>,8> blends{};
  uint32_t blendClock=0;
  const Blend& blender(uint32_t pixel){
    auto& bucket=blends[(pixel^(pixel>>8)^(pixel>>16)^(pixel>>24))&7];
    for(auto& b:bucket)if(b.valid&&b.pixel==pixel){b.used=++blendClock;return b;}
    auto& b=!bucket[0].valid?bucket[0]:!bucket[1].valid?bucket[1]:bucket[0].used<bucket[1].used?bucket[0]:bucket[1];
    b.pixel=pixel;b.valid=true;b.used=++blendClock;
    unsigned inverse=255-(pixel>>24);uint16_t source=color565(pixel);
    for(unsigned i=0;i<64;++i){
      b.green[i]=std::min(63u,unsigned((source>>5)&63)+(i*inverse+127)/255)<<5;
      if(i<32){b.red[i]=std::min(31u,unsigned(source>>11)+(i*inverse+127)/255)<<11;b.blue[i]=std::min(31u,unsigned(source&31)+(i*inverse+127)/255);}
    }
    return b;
  }
  struct OpacityBlend {
    unsigned alpha=0;bool valid=false;uint32_t used=0;
    uint8_t coverage[256]{};
    uint16_t sourceRed[32]{},sourceGreen[64]{},sourceBlue[32]{};
    uint16_t destRed[32]{},destGreen[64]{},destBlue[32]{};
    uint16_t scale(uint16_t color)const{return sourceRed[color>>11]|sourceGreen[(color>>5)&63]|sourceBlue[color&31];}
    uint16_t opaque(uint16_t source,uint16_t dest)const{
      // For opaque source pixels the complementary rounded contributions
      // never exceed their channel maxima, so packed addition is exact.
      return scale(source)+(destRed[dest>>11]|destGreen[(dest>>5)&63]|destBlue[dest&31]);
    }
  };
  std::array<OpacityBlend,2> opacityBlends{};
  uint32_t opacityClock=0;
  const OpacityBlend& opacityBlend(unsigned alpha){
    for(auto& b:opacityBlends)if(b.valid&&b.alpha==alpha){b.used=++opacityClock;return b;}
    auto& b=!opacityBlends[0].valid?opacityBlends[0]:!opacityBlends[1].valid?opacityBlends[1]:opacityBlends[0].used<opacityBlends[1].used?opacityBlends[0]:opacityBlends[1];
    b.alpha=alpha;b.valid=true;b.used=++opacityClock;
    for(unsigned i=0;i<256;++i)b.coverage[i]=(i*alpha+127)/255;
    for(unsigned i=0;i<64;++i){
      unsigned source=(i*alpha+127)/255,dest=(i*(255-alpha)+127)/255;
      b.sourceGreen[i]=source<<5;b.destGreen[i]=dest<<5;
      if(i<32){b.sourceRed[i]=source<<11;b.sourceBlue[i]=source;b.destRed[i]=dest<<11;b.destBlue[i]=dest;}
    }
    return b;
  }
  static uint16_t interpolate565(uint16_t a,uint16_t b,unsigned fraction){
    if(!fraction||a==b)return a;
    unsigned inv=256-fraction;
    unsigned r=(((a>>11)*inv+(b>>11)*fraction+128)>>8);
    unsigned g=(((a>>5)&63)*inv+((b>>5)&63)*fraction+128)>>8;
    unsigned blue=(((a&31)*inv+(b&31)*fraction+128)>>8);
    return (r<<11)|(g<<5)|blue;
  }
  static uint32_t opacity(uint32_t p,unsigned a) {
    if(a==255)return p;
    uint32_t out=0;
    for(unsigned s=0;s<32;s+=8)out|=((((p>>s)&255)*a+127)/255)<<s;
    return out;
  }
  static uint32_t rgba(uint32_t color) {
    unsigned a=color&255;
    return a<<24|(((color>>24)*a+127)/255)<<16|
      ((((color>>16)&255)*a+127)/255)<<8|(((color>>8)&255)*a+127)/255;
  }
  int physical(int p)const {return int((int64_t(p)*density+32768)>>16);}
  Rect rectangle(const uint8_t* p)const {
    int x=physical(s32(p)),y=physical(s32(p+4));
    return {x,y,std::max(0,physical(s32(p+8))),std::max(0,physical(s32(p+12)))};
  }
  const uint8_t* source(unsigned id) {
#ifdef CARROT_RASTER_TIMING
    RasterTimer timer(raster.source);
#endif
    if(auto p=sources.find(id+1))return p;
    ++sourceMisses;
    const auto& a=pixels::sources[id];
    if(a.external) {
      incomplete=true;
      if(loading<0){loading=id;loaded=requestedBytes=0;reads={};compressed.resize(a.size);loadFailed=false;}
      requestRead();return nullptr;
    }
    auto p=sources.insert(id+1,a.decoded);
    if(!p||!carrot_inflate(pixels::bank+a.offset,a.size,p,a.decoded))return nullptr;
    return p;
  }
  static uint32_t texel(const uint8_t* data,int h,int x,int y) {
    auto row=data+1024+y*8;unsigned left=u16(row+4),n=u16(row+6);
    if(x<int(left)||x>=int(left+n))return 0;
    auto index=data[1024+h*8+u32(row)+x-left];return u32(data+index*4);
  }
  struct Axis {int first,second;unsigned mix;};
  static Axis axis(int x,int sourceSize,int targetSize,bool nearest) {
    int q=int((int64_t(2*x+1)*sourceSize*128)/targetSize)-128;
    q=std::clamp(q,0,(sourceSize-1)*256);
    if(nearest){int at=std::min(sourceSize-1,(q+128)/256);return {at,at,0};}
    return {q/256,std::min(q/256+1,sourceSize-1),unsigned(q&255)};
  }
  static uint32_t sample(const uint8_t* data,int h,Axis x,Axis y,int sx=0,int sy=0) {
    x.first+=sx;x.second+=sx;y.first+=sy;y.second+=sy;
    uint32_t a=texel(data,h,x.first,y.first);
    if(!x.mix&&!y.mix)return a;
    uint32_t b=texel(data,h,x.second,y.first),c=texel(data,h,x.first,y.second),d=texel(data,h,x.second,y.second),out=0;
    for(unsigned shift=0;shift<32;shift+=8) {
      unsigned top=((a>>shift)&255)*(256-x.mix)+((b>>shift)&255)*x.mix;
      unsigned bottom=((c>>shift)&255)*(256-x.mix)+((d>>shift)&255)*x.mix;
      out|=((top*(256-y.mix)+bottom*y.mix+32768)>>16)<<shift;
    }
    return out;
  }
  void plot(int x,int y,uint32_t pixel) {
    unsigned a=pixel>>24;if(!a)return;
    auto row=rowPixels(y);if(!row)return;
    auto& dest=row[x];dest=over(color565(pixel),a,dest);
    if(auto coverage=rowAlpha(y))coverage[x]=a+(unsigned(coverage[x])*(255-a)+127)/255;
  }
  void solidSpan(int y,int left,int right,uint32_t pixel){
    unsigned alpha=pixel>>24;if(!alpha||left>=right)return;
    auto dest=rowPixels(y);if(!dest)return;
    auto coverage=rowAlpha(y);uint16_t color=color565(pixel);
    if(alpha==255){std::fill(dest+left,dest+right,color);if(coverage)std::memset(coverage+left,255,right-left);}
    else{
      const auto& blend=blender(pixel);
      for(int x=left;x<right;++x)dest[x]=blend.apply(dest[x]);
      if(coverage)for(int x=left;x<right;++x)coverage[x]=alpha+(unsigned(coverage[x])*(255-alpha)+127)/255;
    }
  }
  void blit(const uint8_t* data,Rect dst,Rect visible,unsigned alpha) {
#ifdef CARROT_RASTER_TIMING
    RasterTimer timer(raster.blit);
#endif
    const auto* fade=alpha==255?nullptr:&opacityBlend(alpha);
    for(int y=visible.y-dst.y;y<visible.y+visible.h-dst.y;++y) {
      unsigned at=u32(data+y*4);if(!at)continue;
      auto row=data+at;int left=u16(row),n=u16(row+2);
      unsigned opaque=u16(row+4),opaqueCount=u16(row+6);
      auto colors=row+8;auto coverage=colors+n*2;
      auto destRow=rowPixels(dst.y+y);if(!destRow)continue;
      auto coverageRow=rowAlpha(dst.y+y);
      int begin=std::max(0,visible.x-dst.x-left),end=std::min(n,visible.x+visible.w-dst.x-left);
      if(begin>=end)continue;
      int opaqueEnd=std::min(end,int(opaque+opaqueCount));
      int opaqueBegin=std::max(begin,int(opaque));
      for(int x=begin;x<end;) {
        if(alpha==255 && x==opaqueBegin && opaqueEnd>opaqueBegin) {
          std::memcpy(destRow+dst.x+left+x,colors+x*2,(opaqueEnd-x)*2);
          if(coverageRow)std::memset(coverageRow+dst.x+left+x,255,opaqueEnd-x);
          x=opaqueEnd;
        } else {
          bool solid=opaqueCount&&unsigned(x)>=opaque&&unsigned(x)<opaque+opaqueCount;
          uint16_t c=u16(colors+x*2);
          if(fade&&solid){
            auto& dest=destRow[dst.x+left+x];dest=fade->opaque(c,dest);
            if(coverageRow){auto& destA=coverageRow[dst.x+left+x];destA=alpha+(unsigned(destA)*(255-alpha)+127)/255;}
            ++x;continue;
          }
          unsigned a=solid?255:coverage[x-(unsigned(x)>=opaque?opaqueCount:0)];
          if(fade){a=fade->coverage[a];c=fade->scale(c);}
          if(a){auto& dest=destRow[dst.x+left+x];dest=over(c,a,dest);if(coverageRow){auto& destA=coverageRow[dst.x+left+x];destA=a+(unsigned(destA)*(255-a)+127)/255;}}++x;
        }
      }
    }
  }
  void backgroundPixels(const uint8_t* raw,const pixels::Source& source,Rect dest,Rect visible){
    std::array<uint16_t,256> palette{};
    for(unsigned i=0;i<256;++i)palette[i]=color565(u32(raw+i*4));
    std::vector<Axis> columns(visible.w);
    for(int x=0;x<visible.w;++x)columns[x]=axis(x+visible.x-dest.x,source.w,dest.w,false);
    std::array<std::vector<uint16_t>,2> rows;
    rows[0].resize(visible.w);rows[1].resize(visible.w);
    int cached[2]={-1,-1};
    auto prepare=[&](int y){
      int slot=y&1;if(cached[slot]==y)return slot;
      const auto indices=raw+1024+source.h*8+u32(raw+1024+y*8);
      for(int x=0;x<visible.w;++x){auto a=columns[x];rows[slot][x]=interpolate565(palette[indices[a.first]],palette[indices[a.second]],a.mix);}
      cached[slot]=y;return slot;
    };
    // Reuse the two horizontally filtered rows across adjacent output rows.
    // The compiler guarantees opaque full-width scanlines for backgrounds.
    for(int y=0;y<visible.h;++y){
      auto sy=axis(y+visible.y-dest.y,source.h,dest.h,false);
      int first=prepare(sy.first),second=prepare(sy.second);auto out=rowPixels(y+visible.y)+visible.x;
      if(!sy.mix)std::memcpy(out,rows[first].data(),visible.w*2);
      else for(int x=0;x<visible.w;++x)out[x]=interpolate565(rows[first][x],rows[second][x],sy.mix);
    }
  }
  void image(const uint8_t* p,unsigned len,bool nearest,bool region,bool byId,bool flipped,bool sharedEdges) {
#ifdef CARROT_RASTER_TIMING
    RasterTimer timer(raster.image);
#endif
    unsigned pathStart=region?26:18;
    if(len<=pathStart)return;
    auto first=std::begin(pixels::sources),last=std::end(pixels::sources);
    auto item=first;
    if(byId){if(len!=pathStart+2||u16(p+pathStart)>=unsigned(last-first))return;item+=u16(p+pathStart);}
    else {
      item=std::lower_bound(first,last,p+pathStart,[len,pathStart](const auto& a,const uint8_t* name){
        size_t n=std::strlen(a.path),m=len-pathStart;int c=std::memcmp(a.path,name,std::min(n,m));return c<0||(c==0&&n<m);
      });
      if(item==last||std::strlen(item->path)!=len-pathStart||std::memcmp(item->path,p+pathStart,len-pathStart))return;
    }
    unsigned id=item-first;
    int sx=region?u16(p+18):0,sy=region?u16(p+20):0,sw=region?u16(p+22):item->w,sh=region?u16(p+24):item->h;
    if(!sw||!sh||sx+sw>item->w||sy+sh>item->h)return;
    Rect dest=rectangle(p);
    if(sharedEdges){dest.w=physical(s32(p)+s32(p+8))-dest.x;dest.h=physical(s32(p+4)+s32(p+12))-dest.y;}
    if(item->background)dest={0,0,width,height};
    if(!dest.w||!dest.h)return;
    if(p[17]!=1) {
      bool byWidth=(int64_t(dest.w)*sh<=int64_t(dest.h)*sw)^(p[17]==2);
      int w=byWidth?dest.w:int((int64_t(dest.h)*sw+sh/2)/sh);
      int h=byWidth?int((int64_t(dest.w)*sh+sw/2)/sw):dest.h;
      dest.x+=(dest.w-w)/2;dest.y+=(dest.h-h)/2;dest.w=w;dest.h=h;
    }
    Rect visible=intersect(dest,clip);if(!visible.w||!visible.h)return;
    if(item->background&&!flipped&&p[16]==255&&!layerTarget){
      auto raw=source(id);if(raw)backgroundPixels(raw,*item,dest,visible);return;
    }
    // A moving sprite keeps one cached pose when it crosses a clip boundary.
    // Large stretched panel strips still cache only the visible part.
    bool whole=dest.w<=width&&dest.h<=height&&size_t(dest.w)*dest.h<=16000;
    Rect cached=whole?dest:visible;
    int dx=cached.x-dest.x,dy=cached.y-dest.y;
    int keyParts[]={int(id),dest.w,dest.h,dx,dy,cached.w,cached.h,int(nearest)|int(flipped)*2,sx,sy,sw,sh};
    uint64_t key=hash(reinterpret_cast<const uint8_t*>(keyParts),sizeof(keyParts));
    if(auto pixels=prepared.find(key)){++hits;blit(pixels,cached,visible,p[16]);return;}
    ++misses;auto raw=source(id);if(!raw)return;
    std::vector<Axis> columns(cached.w);
    for(int x=0;x<cached.w;++x){columns[x]=axis(x+dx,sw,dest.w,nearest);if(flipped){columns[x].first=sw-1-columns[x].first;columns[x].second=sw-1-columns[x].second;}}
    // Landscapes are already baked in the static backdrop. Caching their
    // resampled pixels too would evict hundreds of small animation poses.
    bool cache=size_t(cached.w)*cached.h<=60000;
    if(!cache) {
      for(int y=0;y<visible.h;++y) {
        auto row=axis(y+dy,sh,dest.h,nearest);
        for(int x=0;x<visible.w;++x)plot(visible.x+x,visible.y+y,opacity(sample(raw,item->h,columns[x],row,sx,sy),p[16]));
      }
      return;
    }
    std::vector<uint8_t> result(cached.h*4),coverage(cached.w);
    std::vector<uint16_t> colors(cached.w);
    for(int y=0;y<cached.h;++y) {
      auto row=axis(y+dy,sh,dest.h,nearest);int left=cached.w,right=0,best=0,begin=0,run=0;
      for(int x=0;x<cached.w;++x) {
        uint32_t pixel=sample(raw,item->h,columns[x],row,sx,sy);coverage[x]=pixel>>24;colors[x]=color565(pixel);
        if(coverage[x]){left=std::min(left,x);right=x+1;}
        if(coverage[x]==255){++run;if(run>best){best=run;begin=x-run+1;}}else run=0;
      }
      if(right<=left)continue;
      uint32_t offset=result.size();for(int k=0;k<4;++k)result[y*4+k]=offset>>(k*8);
      put16(result,left);put16(result,right-left);put16(result,begin-left);put16(result,best);
      for(int x=left;x<right;++x)put16(result,colors[x]);
      // The longest opaque run already has implicit alpha 255. Store alpha
      // only for its two outer spans; retain every original edge value.
      if(best){result.insert(result.end(),coverage.begin()+left,coverage.begin()+begin);result.insert(result.end(),coverage.begin()+begin+best,coverage.begin()+right);}
      else result.insert(result.end(),coverage.begin()+left,coverage.begin()+right);
    }
    auto saved=prepared.insert(key,result.size());
    if(saved){std::memcpy(saved,result.data(),result.size());blit(saved,cached,visible,p[16]);}
    else blit(result.data(),cached,visible,p[16]);
  }
  void shape(const uint8_t* p,unsigned type) {
#ifdef CARROT_RASTER_TIMING
    RasterTimer timer(raster.shape);
#endif
    Rect r=rectangle(p),visible=intersect(r,clip);if(!visible.w||!visible.h)return;
    unsigned border=physical(u16(p+(type==1?22:20)));
    unsigned radius=type==1?physical(u16(p+20)):0;
    uint32_t fill=rgba(u32(p+16)),edge=rgba(u32(p+(type==1?24:22)));
    auto inside=[&](float x,float y,float inset) {
      float w=r.w-2*inset,h=r.h-2*inset;if(w<=0||h<=0)return false;
      x-=inset;y-=inset;
      if(type==2){float a=(x-w/2)/(w/2),b=(y-h/2)/(h/2);return a*a+b*b<=1;}
      if(x<0||y<0||x>=w||y>=h)return false;
      float rad=std::min({std::max(0.f,float(radius)-inset),w/2,h/2});
      float a=std::max({rad-x,0.f,x-(w-rad)}),b=std::max({rad-y,0.f,y-(h-rad)});
      return a*a+b*b<=rad*rad;
    };
    auto bounds=[&](int y,float inset){
        int mid=r.w/2;
        if(!inside(mid+.5f,y+.5f,inset))return std::pair<int,int>{0,0};
        int lo=0,hi=mid;
        while(lo<hi){int at=lo+(hi-lo)/2;if(inside(at+.5f,y+.5f,inset))hi=at;else lo=at+1;}
        int left=lo;lo=mid;hi=r.w;
        while(lo<hi){int at=lo+(hi-lo)/2;if(inside(at+.5f,y+.5f,inset))lo=at+1;else hi=at;}
        return std::pair<int,int>{left,lo};
    };
    // Shape boundaries depend on geometry, not position or colour. Reuse
    // exact pixel-centre spans for shadows, projectiles and expanding rings.
    ShapeRows* cached=nullptr;
    if(r.w<=4096&&r.h<=160){
      unsigned key=unsigned(r.w)*31+unsigned(r.h)*131+border*17+radius*7+type;
      auto& bucket=shapeRows[key&31];
      for(auto& entry:bucket)if(entry.used&&entry.w==r.w&&entry.h==r.h&&entry.border==int(border)&&entry.radius==int(radius)&&entry.type==int(type))cached=&entry;
      if(!cached){
        cached=bucket[0].used<=bucket[1].used?&bucket[0]:&bucket[1];
        cached->w=r.w;cached->h=r.h;cached->border=border;cached->radius=radius;cached->type=type;cached->rows.resize(r.h);
        for(int y=0;y<r.h;++y){auto a=bounds(y,0),b=border?bounds(y,border):a;cached->rows[y]={int16_t(a.first),int16_t(a.second),int16_t(b.first),int16_t(b.second)};}
      }
      cached->used=++shapeClock;
    }
    for(int y=visible.y;y<visible.y+visible.h;++y){
      auto span=[&](bool inner){
        auto b=cached?std::pair<int,int>{cached->rows[y-r.y][inner?2:0],cached->rows[y-r.y][inner?3:1]}:bounds(y-r.y,inner?border:0);
        return std::pair<int,int>{std::max(r.x+b.first,visible.x),std::min(r.x+b.second,visible.x+visible.w)};
      };
      auto outer=span(0);if(outer.first>=outer.second)continue;
      if(!border){solidSpan(y,outer.first,outer.second,fill);continue;}
      auto inner=span(true);
      if(inner.first>=inner.second){solidSpan(y,outer.first,outer.second,edge);continue;}
      solidSpan(y,outer.first,inner.first,edge);
      solidSpan(y,inner.first,inner.second,fill);
      solidSpan(y,inner.second,outer.second,edge);
    }
  }
  void line(int x1,int y1,int x2,int y2,int thickness,uint32_t color) {
#ifdef CARROT_RASTER_TIMING
    RasterTimer timer(raster.line);
#endif
    int pad=(thickness+1)/2;Rect r=intersect({std::min(x1,x2)-pad,std::min(y1,y2)-pad,std::abs(x2-x1)+2*pad+1,std::abs(y2-y1)+2*pad+1},clip);
    float vx=x2-x1,vy=y2-y1,dd=vx*vx+vy*vy,radius=thickness*.5f;
    float inv=dd?1/dd:0,outer=(radius+.5f)*(radius+.5f),inner=std::max(0.f,radius-.5f);inner*=inner;
    float half=vy?(radius+1)*std::sqrt(dd)/std::abs(vy):0;
    const auto& blend=blender(color);
    for(int y=r.y;y<r.y+r.h;++y) {
      int left=r.x,right=r.x+r.w;
      if(vy) {
        float ty=float(y-y1)/vy,cx=x1+vx*std::clamp(ty,0.f,1.f),pad=ty<0||ty>1?radius+1:half;
        left=std::max(left,int(std::floor(cx-pad)));right=std::min(right,int(std::ceil(cx+pad))+1);
      }
      for(int x=left;x<right;++x) {
        float t=std::clamp(((x-x1)*vx+(y-y1)*vy)*inv,0.f,1.f),dx=x-x1-t*vx,dy=y-y1-t*vy,d=dx*dx+dy*dy;
        if(d>=outer)continue;
        if(d<=inner){auto row=rowPixels(y);if(row){row[x]=blend.apply(row[x]);if(auto coverage=rowAlpha(y)){unsigned a=color>>24;coverage[x]=a+(unsigned(coverage[x])*(255-a)+127)/255;}}}
        else plot(x,y,opacity(color,unsigned(std::clamp(radius+.5f-std::sqrt(d),0.f,1.f)*255+.5f)));
      }
    }
  }
  bool execute(const uint8_t* data,size_t bytes,Rect initial) {
    std::array<Rect,16> stack{};unsigned depth=0;clip=initial;size_t at=0;
    while(at+4<=bytes) {
      unsigned type=data[at],flags=data[at+1],n=u16(data+at+2);at+=4;
      if(n>bytes-at)return false;const uint8_t* p=data+at;at+=n;
      if(type==7 && n==16) {if(depth==stack.size())return false;stack[depth++]=clip;clip=intersect(clip,rectangle(p));}
      else if(type==8 && !n){if(!depth)return false;clip=stack[--depth];}
      else if(type==6||type==12||type==13||type==14)image(p,n,flags&1,type==12||type==14,type>=13,flags&2,flags&4);
      else if((type==1&&n==28)||(type==2&&n==26))shape(p,type);
      else if(type==3&&n==22)line(physical(s32(p)),physical(s32(p+4)),physical(s32(p+8)),physical(s32(p+12)),std::max(1,physical(u16(p+20))),rgba(u32(p+16)));
      else if(type==4&&n==22) {
        float radius=physical(s32(p+8));int x=physical(s32(p)),y=physical(s32(p+4));float start=u16(p+12)*.01745329252f,end=u16(p+14)*.01745329252f;
        for(float a=start;a<end;a+=.12f){float b=std::min(a+.12f,end);line(x+int(std::cos(a)*radius),y+int(std::sin(a)*radius),x+int(std::cos(b)*radius),y+int(std::sin(b)*radius),std::max(1,physical(u16(p+20))),rgba(u32(p+16)));}
      } else return false;
    }
    return at==bytes&&!depth;
  }
  bool execute(const uint8_t* data,size_t bytes){return execute(data,bytes,{0,0,width,height});}
  bool compositeHud(const uint8_t* commands,size_t bytes,uint64_t revision,int top,int bottom,size_t split){
#ifdef CARROT_RASTER_TIMING
    RasterTimer timer(raster.hud);
#endif
    bool resized=layerTop!=top||layerBottom!=bottom||hudColors.empty();
    if(layerKey!=revision||resized){
      layerTop=top;layerBottom=bottom;
      size_t count=size_t(width)*(top+height-bottom);
      if(resized){hudColors.assign(count,0);hudAlpha.assign(count,0);hudKeys={};}
      auto saved=target;int savedStride=stride;target=hudColors.data();stride=width;layerTarget=true;
      bool ok=true;
      for(unsigned band=0;band<2&&ok;++band){
        size_t first=band?split:0,length=split?(band?bytes-split:split):bytes;
        if(!split)first=0;
        uint64_t key=hash(commands+first,length);
        if(!resized&&hudKeys[band]==key)continue;
        unsigned begin=band?unsigned(top*width):0,end=band?unsigned(count):unsigned(top*width);
        std::fill(hudColors.begin()+begin,hudColors.begin()+end,0);
        std::fill(hudAlpha.begin()+begin,hudAlpha.begin()+end,0);
        ok=execute(commands+first,length,{0,band?bottom:0,width,band?height-bottom:top});
        if(!ok)break;
        auto& runs=hudRuns[band];runs.clear();
        for(unsigned at=begin;at<end;){
          unsigned a=hudAlpha[at],start=at++,rowEnd=std::min(end,(start/width+1)*unsigned(width));
          while(at<rowEnd&&hudAlpha[at]==a)++at;
          if(a)runs.push_back({start,at-start,a});
        }
        hudKeys[band]=key;
      }
      layerTarget=false;target=saved;stride=savedStride;
      if(!ok)return false;layerKey=revision;
    }
    // Preserve transparent corners over the current battlefield. Interior
    // opaque spans copy directly; only the antialiased rim needs blending.
    for(unsigned band=0;band<2;++band)for(const auto& run:hudRuns[band]){
      unsigned y=run.at/width,x=run.at%width;
      unsigned screenY=y+(band?bottom-top:0);
      if(dirtyRows)dirtyRows[screenY]=1;
      auto dest=target+screenY*stride+x;
      auto colors=hudColors.data()+run.at;
      if(run.alpha==255)std::memcpy(dest,colors,run.count*2);
      else for(unsigned i=0;i<run.count;++i)dest[i]=over(colors[i],run.alpha,dest[i]);
    }
    return true;
  }
public:
#ifdef CARROT_RASTER_TIMING
  void reportRaster(unsigned frames){
    unsigned n=std::max(1u,frames);char message[256];
    std::snprintf(message,sizeof(message),"carrot raster image_us=%llu blit_us=%llu source_us=%llu shape_us=%llu line_us=%llu restore_us=%llu hud_us=%llu",(unsigned long long)(raster.image/n),(unsigned long long)(raster.blit/n),(unsigned long long)(raster.source/n),(unsigned long long)(raster.shape/n),(unsigned long long)(raster.line/n),(unsigned long long)(raster.restore/n),(unsigned long long)(raster.hud/n));
    pxa_log_write(2,message);raster={};
  }
#endif
  uint64_t cacheEvictions()const{return prepared.evictions;}
  uint64_t cacheMovedBytes()const{return prepared.movedBytes;}
  bool event(const pxa_event_t& e) {
    if(e.token<ReadToken||e.token>=ReadToken+reads.size())return false;
    pxa_asset_read_result_t r{};
    if(!pxa_assets_parse_read(&e,e.token,&r))return false;
    if(loading<0)return true;
    auto& pending=reads[e.token-ReadToken];
    if(!pending.active||r.status!=PXA_STATUS_OK||r.offset!=pending.offset||r.total_bytes!=compressed.size()||r.bytes!=pending.bytes) {
      loadFailed=true;pxa_log_write(3,"carrot background read failed");return true;
    }
    std::memcpy(compressed.data()+r.offset,r.data,r.bytes);loaded+=r.bytes;pending={};
    if(loaded==compressed.size()) {
      auto& a=pixels::sources[loading];auto p=sources.insert(loading+1,a.decoded);
      if(!p||!carrot_inflate(compressed.data(),compressed.size(),p,a.decoded))pxa_log_write(3,"carrot background inflate failed");
      loading=-1;std::vector<uint8_t>().swap(compressed);backdropHash=0;
    } else requestRead();
    return true;
  }
  bool pending()const{return incomplete;}
  bool loadingBackground()const{return loading>=0;}
  void pump(){requestRead();}
  bool stageReady(const uint8_t* data,size_t bytes){
    uint64_t key=hash(data,bytes);key^=uint64_t(density)<<32|unsigned(width);
    if(backdropHash&&key==backdropHash)return true;
    if(bytes>=24&&data[0]==13&&u16(data+2)==20){unsigned id=u16(data+22);if(id<std::size(pixels::sources)&&pixels::sources[id].background)return source(id)!=nullptr;}
    return true;
  }
  void resize(int w,int h,uint32_t d) {
    width=w;height=h;density=d;stride=w;backdrop.resize(size_t(w)*h);
    sources.reset(320*1024);prepared.reset(1024*1024);backdropHash=0;layerKey=0;
    std::vector<uint16_t>().swap(hudColors);std::vector<uint8_t>().swap(hudAlpha);
    dirtyRows=nullptr;bufferDamage={};
  }
  void invalidate(){backdropHash=0;}
  bool paint(uint16_t* dest,int rowStride,const uint8_t* data,size_t bytes,size_t prefix,
             size_t hudBegin=0,size_t hudEnd=0,uint64_t hudRevision=0,int hudTop=0,int hudBottom=0,size_t hudSplit=0,int persistentBuffer=-1) {
    if(prefix>bytes)return false;
    incomplete=false;dirtyRows=nullptr;
    uint64_t key=hash(data,prefix);key^=uint64_t(density)<<32|unsigned(width);
    if(!backdropHash||key!=backdropHash) {
      std::fill(backdrop.begin(),backdrop.end(),uint16_t(0));target=backdrop.data();stride=width;
      if(!execute(data,prefix))return false;if(!incomplete){backdropHash=key;++backdropRevision;}
    }
    auto damage=persistentBuffer>=0&&persistentBuffer<2?&bufferDamage[persistentBuffer]:nullptr;
    bool retained=damage&&damage->address==dest&&damage->stride==rowStride&&damage->revision==backdropRevision&&backdropHash&&!incomplete;
    {
#ifdef CARROT_RASTER_TIMING
      RasterTimer timer(raster.restore);
#endif
      // Each mapped buffer retains its own previous frame. Restore only rows
      // it actually touched; untouched rows already contain this backdrop.
      // Merge adjacent dirty rows into one copy when the surface is packed.
      for(int y=0;y<height;){
        if(retained&&!damage->rows[y]){++y;continue;}
        int end=y+1;
        if(rowStride==width){while(end<height&&(!retained||damage->rows[end]))++end;}
        std::memcpy(dest+y*rowStride,backdrop.data()+y*width,size_t(end-y)*width*2);y=end;
      }
    }
    if(damage){damage->address=dest;damage->stride=rowStride;damage->rows.assign(height,0);dirtyRows=damage->rows.data();}
    target=dest;stride=rowStride;
    bool ok;
    if(hudBegin>=prefix&&hudEnd>hudBegin&&hudEnd<=bytes&&hudTop>0&&hudBottom>=hudTop&&hudBottom<=height){
      if(hudSplit>hudEnd-hudBegin){dirtyRows=nullptr;if(damage)damage->revision=0;return false;}
      ok=execute(data+prefix,hudBegin-prefix)&&compositeHud(data+hudBegin,hudEnd-hudBegin,hudRevision,hudTop,hudBottom,hudSplit)&&execute(data+hudEnd,bytes-hudEnd);
    }else ok=execute(data+prefix,bytes-prefix);
    dirtyRows=nullptr;if(damage)damage->revision=ok&&!incomplete?backdropRevision:0;
    return ok&&!incomplete;
  }
};
} // namespace carrot
