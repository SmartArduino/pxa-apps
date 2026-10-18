#include "pcx_stream.h"
#include <string.h>
static unsigned little16(const uint8_t *p){return p[0]|(unsigned)p[1]<<8;}
static int read_at(const port_pcx_source_t *s,uint32_t at,void *out,uint32_t n){
  return s && s->read && at<=s->bytes && n<=s->bytes-at && s->read(s->context,at,out,n);
}
int port_pcx_header(const port_pcx_source_t *s,port_pcx_info_t *info){
  uint8_t h[128],marker;
  if(!info || !s || s->bytes<897 || !read_at(s,0,h,128) ||
     !read_at(s,s->bytes-769,&marker,1) || marker!=12 ||
     h[0]!=10 || h[1]!=5 || h[2]!=1 || h[3]!=8 || h[65]!=1 ||
     little16(h+4) || little16(h+6))return 0;
  unsigned w=little16(h+8)+1,hgt=little16(h+10)+1,stride=little16(h+66);
  if(w>640 || hgt>480 || stride<w || stride>640)return 0;
  *info=(port_pcx_info_t){w,hgt,stride,s->bytes-769};return 1;
}
int port_pcx_palette(const port_pcx_source_t *s,uint8_t p[768]){
  return p && s && s->bytes>=769 && read_at(s,s->bytes-768,p,768);
}
typedef struct {
  const port_pcx_source_t *source;
  uint32_t offset,end,used,count;
  uint8_t buffer[4096];
} Reader;
static int next(Reader *r){
  if(r->used==r->count){
    uint32_t n=r->end-r->offset;if(n>sizeof(r->buffer))n=sizeof(r->buffer);
    if(!n || !read_at(r->source,r->offset,r->buffer,n))return -1;
    r->offset+=n;r->used=0;r->count=n;
  }
  return r->buffer[r->used++];
}
int port_pcx_pixels(const port_pcx_source_t *s,const port_pcx_info_t *info,uint8_t *out,size_t capacity){
  if(!s || !info || !out || !info->width || !info->height ||
     info->width>640 || info->height>480 || info->stride<info->width || info->stride>640 ||
     info->data_end<128 || info->data_end>s->bytes ||
     (size_t)info->width*info->height>capacity)return 0;
  Reader r={.source=s,.offset=128,.end=info->data_end};
  uint32_t position=0,total=(uint32_t)info->stride*info->height;
  while(position<total){
    int value=next(&r);if(value<0)return 0;
    unsigned count=1;
    if((value&192)==192){count=value&63;value=next(&r);if(!count || value<0)return 0;}
    if(count>total-position)return 0;
    while(count){
      unsigned x=position%info->stride,n=info->stride-x;
      if(n>count)n=count;
      if(x<info->width){unsigned visible=info->width-x;if(visible>n)visible=n;
        memset(out+(position/info->stride)*info->width+x,value,visible);}
      position+=n;count-=n;
    }
  }
  return 1;
}
