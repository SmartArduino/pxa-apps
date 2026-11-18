// SPDX-License-Identifier: MIT
#pragma once
#include "art.hpp"
#include "art_data.hpp"
#include "packed_art.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <vector>

namespace garden {
constexpr uint16_t rgb(unsigned r,unsigned g,unsigned b){return ((r&248)<<8)|((g&252)<<3)|(b>>3);}
struct Rect {int x=0,y=0,w=0,h=0;bool operator==(const Rect&)const=default;bool contains(int xx,int yy)const{return xx>=x&&yy>=y&&xx<x+w&&yy<y+h;}};
// Premultiplied RGB prevents transparent pixels from bleeding black into edges.
inline uint32_t premultiply(uint16_t color,unsigned a){
 unsigned r=(color>>11)*255/31,g=((color>>5)&63)*255/63,b=(color&31)*255/31;
 return a<<24|((r*a+127)/255)<<16|((g*a+127)/255)<<8|(b*a+127)/255;
}
inline uint16_t over(uint32_t source,uint16_t dest){
 unsigned a=source>>24;if(!a)return dest;
 unsigned r=(source>>16)&255,g=(source>>8)&255,b=source&255;
 if(a!=255){unsigned inv=255-a;r+=((dest>>11)*255/31*inv+127)/255;g+=(((dest>>5)&63)*255/63*inv+127)/255;b+=((dest&31)*255/31*inv+127)/255;}
 return rgb(std::min(r,255u),std::min(g,255u),std::min(b,255u));
}
struct SampleAxis {int first,second;unsigned fraction;};
inline SampleAxis sample_axis(int target,int source_size,int target_size){
 int q=int((int64_t(target*2+1)*source_size*128)/target_size)-128;
 q=std::clamp(q,0,(source_size-1)*256);int at=q/256;
 return {at,std::min(at+1,source_size-1),unsigned(q&255)};
}
inline uint32_t filtered_pixel(uint32_t a,uint32_t b,uint32_t c,uint32_t d,unsigned x,unsigned y){
 uint32_t out=0;
 for(unsigned shift=0;shift<32;shift+=8){unsigned top=((a>>shift)&255)*(256-x)+((b>>shift)&255)*x;
  unsigned bottom=((c>>shift)&255)*(256-x)+((d>>shift)&255)*x;
  out|=((top*(256-y)+bottom*y+32768)>>16)<<shift;}
 return out;
}
struct FilteredArt {
 size_t cache_budget=1024*1024;
 struct Entry {unsigned id=~0u;int w=0,h=0;uint64_t used=0;bool shadow=false,pinned=false;uint8_t quarter=0;size_t offset=0,count=0;};
 std::vector<uint8_t> bytes;std::vector<uint32_t> arena;std::array<Entry,512> cache;std::array<std::array<uint16_t,2>,2048> lookup{};size_t cache_bytes=0;uint64_t serial=0,hits=0,misses=0;bool cached_motion=false;
 static constexpr auto expanded=[](){std::array<uint32_t,256> out{};for(unsigned i=0;i<out.size();++i){uint16_t c=garden_art::palette[i];out[i]=((c>>11)*255/31)<<16|(((c>>5)&63)*255/63)<<8|(c&31)*255/31;}return out;}();
 void clear_cache(){for(auto& e:cache)e=Entry{};lookup={};cache_bytes=0;std::vector<uint32_t>().swap(arena);}
 // Static HUD pixels already live in the backdrop. Retaining their large
 // filtered textures would evict walking/attack poses during a crowded battle.
 void release_hud_cache(){for(auto& e:cache)if(e.used&&e.id>=garden_art::frost_shot&&
  e.id!=garden_art::frost_shot&&e.id!=garden_art::ember_shot&&e.id!=garden_art::ui_seed_shot&&e.id!=garden_art::ui_leaf_shot&&e.id!=garden_art::ui_shadow&&e.id!=garden_art::ui_dew&&
  e.id!=garden_art::ui_seed&&e.id!=garden_art::ui_seed_selected&&e.id!=garden_art::ui_seed_disabled&&e.id!=garden_art::ui_page_button&&e.id!=garden_art::ui_trowel&&
  !(e.id>=garden_art::glyph_0&&e.id<=garden_art::glyph_9)){cache_bytes-=e.count*4;e=Entry{};}}
 unsigned coverage(garden_art::SourceRow row,int x)const{int i=x-row.left;if(i<0||i>=row.count)return 0;if(i>=row.opaque_begin&&i<row.opaque_begin+row.opaque_count)return 255;return bytes[row.offset+row.count+i-(i>=row.opaque_begin+row.opaque_count?row.opaque_count:0)];}
 uint32_t texel(unsigned id,int x,int y,bool shadow)const{
  auto row=garden_art::source_rows[garden_art::source_first[id]+y];unsigned a=coverage(row,x);
  if(!a)return 0;if(shadow)return premultiply(rgb(35,53,27),a);auto color=expanded[bytes[row.offset+x-row.left]];if(a==255)return 0xff000000u|color;
  return a<<24|((((color>>16)&255)*a+127)/255)<<16|((((color>>8)&255)*a+127)/255)<<8|((color&255)*a+127)/255;
 }
 uint32_t sample(unsigned id,SampleAxis x,SampleAxis y,bool shadow)const{
  return filtered_pixel(texel(id,x.first,y.first,shadow),texel(id,x.second,y.first,shadow),texel(id,x.first,y.second,shadow),texel(id,x.second,y.second,shadow),x.fraction,y.fraction);
 }
 struct Row {int left=0,count=0;const uint32_t* pixels=nullptr;const uint8_t* colors=nullptr;const uint8_t* alpha=nullptr;
  uint32_t value(int i)const{if(i<0||i>=count)return 0;if(pixels)return pixels[i];return unsigned(colors[i*2])|(unsigned(colors[i*2+1])<<8)|(uint32_t(alpha[i])<<16);}
 };
 static Row cached_row(const uint32_t* block,int y){unsigned at=block[y]&0x7fffffffu;if(!at)return {};auto header=block[at];int count=header&65535;auto data=block+at+1;
  if(block[0]&0x80000000u)return {int(header>>16),count,nullptr,reinterpret_cast<const uint8_t*>(data),reinterpret_cast<const uint8_t*>(data)+count*2};
  return {int(header>>16),count,data};}
 bool covered(unsigned id,SampleAxis x,SampleAxis y)const{
  for(int yy:{y.first,y.second}){auto row=garden_art::source_rows[garden_art::source_first[id]+yy];
   for(int xx:{x.first,x.second})if(coverage(row,xx))return true;}
  return false;
 }
 const uint32_t* prepare(unsigned id,int width,int height,bool shadow,unsigned quarter=0){
  ++serial;if(width<=0||height<=0)return nullptr;if(arena.empty())arena.resize(cache_budget/4);
  unsigned hash=(id*2654435761u+unsigned(width)*97u+unsigned(height)*8191u+unsigned(shadow)*131u+quarter*733u)&2047;
  auto matches=[&](const Entry& e){return e.used&&e.id==id&&e.w==width&&e.h==height&&e.shadow==shadow&&e.quarter==quarter;};
  for(auto at:lookup[hash])if(at&&matches(cache[at-1])){auto& e=cache[at-1];e.used=serial;++hits;return arena.data()+e.offset;}
  auto remember=[&](unsigned index){lookup[hash][1]=lookup[hash][0];lookup[hash][0]=index+1;};
  for(unsigned i=0;i<cache.size();++i)if(matches(cache[i])){auto& e=cache[i];e.used=serial;++hits;remember(i);return arena.data()+e.offset;}
  ++misses;
  struct Pin {Entry* entry=nullptr;~Pin(){if(entry)entry->pinned=false;}} pin;
  if(quarter){auto base=prepare(id,width,height,shadow,0);if(!base)return nullptr;size_t offset=base-arena.data();for(auto& e:cache)if(e.used&&e.offset==offset){pin.entry=&e;e.pinned=true;break;}if(!pin.entry)return nullptr;}
  const garden_art::Prepared* prepared=nullptr;
  if(!quarter&&!shadow)for(const auto& p:garden_art::prepared)if(p.id==id&&p.width==unsigned(width)&&p.height==unsigned(height)){prepared=&p;break;}
  auto f=garden_art::frames[id];std::vector<SampleAxis> columns;std::vector<std::pair<int,int>> bounds;size_t words=height;
  if(prepared)words=prepared->words;
  else if(quarter){bounds.resize(height);auto base=arena.data()+pin.entry->offset;for(int y=0;y<height;++y){auto row=cached_row(base,y);int left=row.left,right=left+row.count+(row.count?1:0);bounds[y]={left,right};if(right>left)words+=1+(size_t(right-left)*3+3)/4;}}
  else {columns.resize(width);bounds.resize(height);for(int x=0;x<width;++x)columns[x]=sample_axis(x,f.w,width);
   for(int y=0;y<height;++y){auto sy=sample_axis(y,f.h,height);int left=0,right=width;while(left<right&&!covered(id,columns[left],sy))++left;while(right>left&&!covered(id,columns[right-1],sy))--right;
    bounds[y]={left,right};if(right>left)words+=1+(size_t(right-left)*3+3)/4;}}
  size_t needed=words*4;if(needed+(pin.entry?pin.entry->count*4:0)>cache_budget)return nullptr;
  auto evict=[&](){auto i=std::min_element(cache.begin(),cache.end(),[](const auto& a,const auto& b){return (a.used&&!a.pinned?a.used:UINT64_MAX)<(b.used&&!b.pinned?b.used:UINT64_MAX);});cache_bytes-=i->count*4;*i=Entry{};};
  while(cache_bytes+needed>cache_budget)evict();
  auto it=std::find_if(cache.begin(),cache.end(),[](const auto& e){return !e.used;});if(it==cache.end()){evict();it=std::find_if(cache.begin(),cache.end(),[](const auto& e){return !e.used;});}
  // Compact live blocks inside one fixed arena. No image allocates a large
  // heap block when a menu opens after many small animation frames.
  std::array<Entry*,512> order{};unsigned count=0;for(auto& item:cache)if(item.used)order[count++]=&item;
  std::sort(order.begin(),order.begin()+count,[](auto a,auto b){return a->offset<b->offset;});size_t end=0;
  for(unsigned i=0;i<count;++i){auto& item=*order[i];if(item.offset-end>=needed/4)break;end=item.offset+item.count;}
  if(end+needed/4>arena.size()) {end=0;for(unsigned i=0;i<count;++i){auto& item=*order[i];if(item.offset!=end)std::memmove(arena.data()+end,arena.data()+item.offset,item.count*4);item.offset=end;end+=item.count;}}
  auto& e=*it;e.id=id;e.w=width;e.h=height;e.shadow=shadow;e.quarter=quarter;e.used=serial;e.offset=end;e.count=words;cache_bytes+=needed;
  remember(unsigned(it-cache.begin()));
  if(prepared){auto p=*prepared;
   auto output=std::span(reinterpret_cast<uint8_t*>(arena.data()+e.offset),e.count*4);
   if(!unpack(std::span(garden_art::packed_prepared).subspan(p.offset,p.size),output))return nullptr;
   return arena.data()+e.offset;}
  auto block=arena.data()+e.offset;size_t cursor=height;
  for(int y=0;y<height;++y){auto [left,right]=bounds[y];block[y]=0;if(right<=left)continue;block[y]=cursor;block[cursor++]=(unsigned(left)<<16)|unsigned(right-left);auto sy=sample_axis(y,f.h,height);
   auto n=right-left;auto colors=reinterpret_cast<uint8_t*>(block+cursor);auto alpha=colors+n*2;
   auto base=quarter?cached_row(arena.data()+pin.entry->offset,y):Row{};
   for(int x=left;x<right;++x){uint16_t color;unsigned a;
    if(quarter){auto aa=base.value(x-base.left-1),bb=base.value(x-base.left);unsigned phase=quarter*64;auto mix=[&](unsigned shift,unsigned mask){return ((((aa>>shift)&mask)*phase+((bb>>shift)&mask)*(256-phase)+128)>>8);};color=(mix(11,31)<<11)|(mix(5,63)<<5)|mix(0,31);a=mix(16,255);}
    else {auto c=sample(id,columns[x],sy,shadow);color=rgb((c>>16)&255,(c>>8)&255,c&255);a=c>>24;}
    colors[(x-left)*2]=color;colors[(x-left)*2+1]=color>>8;alpha[x-left]=a;}
   size_t packed=(size_t(n)*3+3)/4;std::memset(reinterpret_cast<uint8_t*>(block+cursor)+n*3,0,packed*4-n*3);cursor+=packed;}
  block[0]|=0x80000000u;
  return arena.data()+e.offset;
 }
 void composite(uint16_t* dest,int width,int height,int stride,const uint32_t* pixels,Rect r){
  int l=std::max(0,r.x),t=std::max(0,r.y),right=std::min(width,r.x+r.w),bottom=std::min(height,r.y+r.h);
  for(int y=t;y<bottom;++y){auto row=dest+y*stride;auto span=cached_row(pixels,y-r.y);int begin=std::max(l,r.x+span.left),end=std::min(right,r.x+span.left+span.count);
   for(int x=begin;x<end;){int i=x-r.x-span.left;auto value=span.value(i);unsigned a=value>>16;
    if(a==255&&span.colors){int start=x;while(x<end&&span.alpha[x-r.x-span.left]==255)++x;int n=x-start;if(n>=8)std::memcpy(row+start,span.colors+i*2,n*2);else for(int j=0;j<n;++j)row[start+j]=unsigned(span.colors[(i+j)*2])|(unsigned(span.colors[(i+j)*2+1])<<8);continue;}
    if(a==255)row[x]=uint16_t(value);else if(a){auto d=row[x];unsigned inv=255-a,red=std::min(31u,unsigned((value>>11)&31)+((d>>11)*inv+127)/255),green=std::min(63u,unsigned((value>>5)&63)+(((d>>5)&63)*inv+127)/255),blue=std::min(31u,unsigned(value&31)+((d&31)*inv+127)/255);row[x]=(red<<11)|(green<<5)|blue;}++x;}
  }
 }
 void blit(uint16_t* dest,int width,int height,int stride,unsigned id,Rect r,bool shadow=false,bool cached=true){
  int l=std::max(0,r.x),t=std::max(0,r.y),right=std::min(width,r.x+r.w),bottom=std::min(height,r.y+r.h);
  if(l>=right||t>=bottom||bytes.empty())return;
  auto f=garden_art::frames[id];const uint32_t* pixels=cached?prepare(id,r.w,r.h,shadow):nullptr;
  if(pixels){composite(dest,width,height,stride,pixels,r);return;}
  std::vector<SampleAxis> columns(right-l);for(int x=l;x<right;++x)columns[x-l]=sample_axis(x-r.x,f.w,r.w);
  for(int y=t;y<bottom;++y){auto sy=sample_axis(y-r.y,f.h,r.h);auto row=dest+y*stride;for(int x=l;x<right;++x)row[x]=over(sample(id,columns[x-l],sy,shadow),row[x]);}
 }
 // Keep horizontal movement below one display pixel. Interpolate spatially
 // inside this pose's premultiplied cache, never between animation frames.
 void blit_subpixel(uint16_t* dest,int width,int height,int stride,unsigned id,Rect r,float left,bool shadow=false){
  int origin=int(std::floor(left));
  if(cached_motion){unsigned quarter=unsigned(std::lround((left-origin)*4));if(quarter==4){++origin;quarter=0;}
   auto pixels=prepare(id,r.w,r.h,shadow,quarter);if(pixels){r.x=origin;r.w+=quarter?1:0;composite(dest,width,height,stride,pixels,r);return;}}
  origin=int(std::floor(left));unsigned phase=unsigned(std::lround((left-origin)*256));
  if(phase==256){++origin;phase=0;}
  if(!phase){r.x=origin;blit(dest,width,height,stride,id,r,shadow);return;}
  auto pixels=prepare(id,r.w,r.h,shadow);
  if(!pixels){r.x=int(std::lround(left));blit(dest,width,height,stride,id,r,shadow);return;}
  int l=std::max(0,origin),right=std::min(width,origin+r.w+1),t=std::max(0,r.y),bottom=std::min(height,r.y+r.h);
  for(int y=t;y<bottom;++y){auto span=cached_row(pixels,y-r.y);auto row=dest+y*stride;int begin=std::max(l,origin+span.left),end=std::min(right,origin+span.left+span.count+1);
   for(int x=begin;x<end;++x){int i=x-origin-span.left;uint32_t a=span.value(i-1),b=span.value(i);
    if(a==b&&(a>>16)==255){row[x]=uint16_t(a);continue;}
    auto mix=[&](unsigned shift,unsigned mask){return (((a>>shift)&mask)*phase+((b>>shift)&mask)*(256-phase)+128)>>8;};
    unsigned alpha=mix(16,255);if(!alpha)continue;
    if(alpha==255){row[x]=(mix(11,31)<<11)|(mix(5,63)<<5)|mix(0,31);continue;}
    unsigned inv=255-alpha;auto d=row[x];
    unsigned red=std::min(31u,mix(11,31)+((d>>11)*inv+127)/255),green=std::min(63u,mix(5,63)+(((d>>5)&63)*inv+127)/255),blue=std::min(31u,mix(0,31)+((d&31)*inv+127)/255);
    row[x]=(red<<11)|(green<<5)|blue;
   }
  }
 }
};
}
