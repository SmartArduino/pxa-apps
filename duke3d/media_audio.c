#include "media_audio.h"
#include <limits.h>
#include <string.h>

static uint32_t le24(const uint8_t *p) {return p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16);}
static uint32_t le32(const uint8_t *p) {return le24(p)|((uint32_t)p[3]<<24);}
static uint32_t be32(const uint8_t *p) {return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}

bool duke_voc_probe(duke_media_read_fn read,void *context,uint32_t bytes,duke_pcm_t *pcm) {
    uint8_t header[26];
    if(!read || !pcm)return false;
    memset(pcm,0,sizeof(*pcm));
    if(bytes<26 || !read(context,0,header,26) || memcmp(header,"Creative Voice File\x1a",20))return false;
    uint32_t pos=header[20]|((uint32_t)header[21]<<8);
    if(pos<26 || pos>bytes)return false;
    while(pos<bytes) {
        uint8_t block[12],type;
        if(!read(context,pos++,&type,1))return false;
        if(!type)break;
        if(bytes-pos<3 || !read(context,pos,block,3))return false;
        uint32_t count=le24(block);pos+=3;
        if(count>bytes-pos)return false;
        uint32_t skip=0,bits=pcm->bits;
        if(type==1) {
            if(count<2 || !read(context,pos,block,2) || block[1])return false;
            pcm->rate=1000000u/(256u-block[0]);skip=2;bits=8;
        } else if(type==9) {
            if(count<12 || !read(context,pos,block,12) || block[5]!=1 || block[7] ||
               !((block[4]==8 && block[6]==0) || (block[4]==16 && block[6]==4)))return false;
            pcm->rate=le32(block);skip=12;bits=block[4];
        } else if(type==2) {
            if(!pcm->rate)return false;
        } else if(type==5 || type==4) {pos+=count;continue;}
        else return false;
        if(!pcm->rate || pcm->rate>48000 || (pcm->count && bits!=pcm->bits))return false;
        pcm->bits=bits;
        if(count>skip) {
            uint32_t sampleBytes=bits/8;
            if(!sampleBytes || (count-skip)%sampleBytes || pcm->count==DUKE_PCM_SEGMENTS ||
               (count-skip)/sampleBytes>INT_MAX-pcm->frames)return false;
            pcm->segments[pcm->count++]=(duke_pcm_segment_t){pos+skip,count-skip};
            pcm->frames+=(count-skip)/sampleBytes;
        }
        pos+=count;
    }
    return pcm->frames!=0;
}

bool duke_pcm_read(duke_media_read_fn read,void *context,const duke_pcm_t *pcm,
                   uint32_t frame,void *output,uint32_t bytes) {
    if(!read || !pcm || !output || pcm->count>DUKE_PCM_SEGMENTS || (pcm->bits!=8 && pcm->bits!=16))return false;
    uint8_t *out=output;
    for(uint32_t i=0;i<pcm->count && bytes;++i) {
        const duke_pcm_segment_t *segment=&pcm->segments[i];
        uint32_t frames=segment->bytes/(pcm->bits/8);
        if(frame>=frames) {frame-=frames;continue;}
        uint32_t n=frames-frame;if(n>bytes)n=bytes;
        if(pcm->bits==8) {
            if(!read(context,segment->offset+frame,out,n))return false;
        } else {
            uint8_t raw[512];
            for(uint32_t done=0;done<n;) {
                uint32_t chunk=n-done;if(chunk>sizeof(raw)/2)chunk=sizeof(raw)/2;
                if(!read(context,segment->offset+(frame+done)*2,raw,chunk*2))return false;
                for(uint32_t j=0;j<chunk;++j)out[done+j]=raw[j*2+1]^128;
                done+=chunk;
            }
        }
        out+=n;bytes-=n;frame=0;
    }
    memset(out,128,bytes); // Bounded interpolation lookahead past the last frame.
    return true;
}

typedef struct {
    uint32_t pos,end,tick,start,bytes;
    uint8_t running,status,kind,active;
} midi_track_t;

static bool byte_at(duke_media_read_fn read,void *ctx,uint32_t *pos,uint32_t end,uint8_t *v) {
    if(*pos>=end || !read(ctx,*pos,v,1))return false;
    ++*pos;return true;
}
static bool variable(duke_media_read_fn read,void *ctx,uint32_t *pos,uint32_t end,uint32_t *v) {
    *v=0;
    for(unsigned i=0;i<4;++i) {
        uint8_t b;if(!byte_at(read,ctx,pos,end,&b))return false;
        *v=(*v<<7)|(b&127);if(!(b&128))return true;
    }
    return false;
}
static bool next_event(duke_media_read_fn read,void *ctx,midi_track_t *track) {
    uint32_t delta;
    uint8_t status;
    if(!variable(read,ctx,&track->pos,track->end,&delta) || delta>UINT32_MAX-track->tick ||
       !byte_at(read,ctx,&track->pos,track->end,&status))return false;
    track->tick+=delta;
    track->status=status;
    track->kind=0;
    if(status==255 || status==240 || status==247) {
        track->start=track->pos-1;
        uint8_t type=0;
        uint32_t bytes;
        if(status==255 && !byte_at(read,ctx,&track->pos,track->end,&type))return false;
        if(!variable(read,ctx,&track->pos,track->end,&bytes) || bytes>track->end-track->pos)return false;
        track->pos+=bytes;
        track->bytes=track->pos-track->start;
        track->kind=status==255 && type==47?2:1;
        if(track->kind==2 && bytes)return false;
        if(status!=255)track->running=0;
    } else {
        if(status&128)track->running=status;
        else {track->status=track->running;--track->pos;}
        if(track->status<128 || track->status>=240)return false;
        track->bytes=(track->status&0xe0)==0xc0?1:2;
        track->start=track->pos;
        if(track->bytes>track->end-track->pos)return false;
        uint8_t values[2];
        if(!read(ctx,track->pos,values,track->bytes) || values[0]&128 ||
           (track->bytes==2 && values[1]&128))return false;
        track->pos+=track->bytes;
    }
    track->active=1;return true;
}
static bool emit(uint8_t *output,uint32_t capacity,uint32_t *pos,const void *data,uint32_t bytes) {
    if(bytes>capacity-*pos)return false;
    memcpy(output+*pos,data,bytes);*pos+=bytes;return true;
}
static bool emit_variable(uint8_t *output,uint32_t capacity,uint32_t *pos,uint32_t v) {
    uint8_t b[4];unsigned n=0;
    if(v>0x0fffffff)return false;
    b[n++]=v&127;
    while(v>>=7)b[n++]=128|(v&127);
    while(n)if(!emit(output,capacity,pos,&b[--n],1))return false;
    return true;
}
bool duke_midi_merge(duke_media_read_fn read,void *context,uint32_t bytes,
                     uint8_t *output,uint32_t capacity,uint32_t *written) {
    uint8_t header[14];
    if(written)*written=0;
    if(!read || !output || !written || bytes<14 || capacity<26 || !read(context,0,header,14) ||
       memcmp(header,"MThd",4) || be32(header+4)<6 || header[8]!=0 || header[9]!=1 || header[12]&128)return false;
    unsigned count=((unsigned)header[10]<<8)|header[11];
    if(!count || count>64 || (!header[12] && !header[13]))return false;
    midi_track_t tracks[64]={0};
    uint32_t pos=be32(header+4);
    if(pos>bytes-8)return false;
    pos+=8;
    for(unsigned i=0;i<count;++i) {
        uint8_t chunk[8];
        if(bytes-pos<8 || !read(context,pos,chunk,8) || memcmp(chunk,"MTrk",4))return false;
        uint32_t length=be32(chunk+4);pos+=8;
        if(!length || length>bytes-pos)return false;
        tracks[i].pos=pos;tracks[i].end=pos+length;pos+=length;
        if(!next_event(read,context,&tracks[i]))return false;
    }
    uint8_t start[22]={'M','T','h','d',0,0,0,6,0,0,0,1,header[12],header[13],'M','T','r','k',0,0,0,0};
    memcpy(output,start,sizeof(start));
    uint32_t out=22,last=0,final=0;
    uint8_t running=0;
    for(;;) {
        unsigned chosen=count;
        for(unsigned i=0;i<count;++i)
            if(tracks[i].active && (chosen==count || tracks[i].tick<tracks[chosen].tick))chosen=i;
        if(chosen==count)break;
        midi_track_t *track=&tracks[chosen];
        if(track->tick>final)final=track->tick;
        if(track->kind==2) {track->active=0;continue;}
        if(!emit_variable(output,capacity,&out,track->tick-last))return false;
        last=track->tick;
        if(!track->kind) {
            if(running!=track->status && !emit(output,capacity,&out,&track->status,1))return false;
            running=track->status;
        } else running=0;
        if(track->bytes>capacity-out || !read(context,track->start,output+out,track->bytes))return false;
        out+=track->bytes;
        if(!next_event(read,context,track))return false;
    }
    const uint8_t end[]={255,47,0};
    if(!emit_variable(output,capacity,&out,final-last) || !emit(output,capacity,&out,end,3))return false;
    uint32_t length=out-22;
    output[18]=length>>24;output[19]=length>>16;output[20]=length>>8;output[21]=length;
    *written=out;return true;
}
