// SPDX-License-Identifier: GPL-3.0-or-later
#include "music.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t bank[8+175*36];
static uint8_t midi[256];
static size_t track_at, length;
static void header(unsigned tracks) {
    static const uint8_t h[]={ 'M','T','h','d',0,0,0,6,0,1,0,0,0,96 };
    memcpy(midi,h,sizeof(h));midi[11]=(uint8_t)tracks;length=14;
}
static void begin_track(void) {
    memcpy(midi+length,"MTrk\0\0\0\0",8);track_at=length+4;length+=8;
}
static void bytes(const uint8_t *b,size_t n) {memcpy(midi+length,b,n);length+=n;}
static void end_track(void) {
    unsigned n=(unsigned)(length-track_at-4);midi[track_at]=(uint8_t)(n>>24);
    midi[track_at+1]=(uint8_t)(n>>16);midi[track_at+2]=(uint8_t)(n>>8);midi[track_at+3]=(uint8_t)n;
}
static unsigned render(unsigned frames,unsigned volume,int paused) {
    unsigned nonzero=0;
    while(frames) {
        int16_t pcm[512*2]={0};unsigned n=frames>512?512:frames;
        fd_music_mix(pcm,n,volume,paused);
        for(unsigned i=0;i<2*n;++i)nonzero+=pcm[i]!=0;
        frames-=n;
    }
    return nonzero;
}
static void initialize(void) {
    memcpy(bank,"#OPL_II#",8);
    for(unsigned i=0;i<175;++i) {
        uint8_t *p=bank+8+36*i+4;
        p[0]=p[7]=1;p[1]=p[8]=0xf2;p[2]=p[9]=0x74;p[5]=16;
    }
    assert(fd_music_init(bank,sizeof(bank)));
}
static void timing(void) {
    // Two tracks, tempo changes at half a quarter note; duration = .25 + .5 sec.
    header(2);begin_track();
    const uint8_t tempo[]={0,255,81,3,7,161,32,48,255,81,3,15,66,64,48,255,47,0};
    bytes(tempo,sizeof(tempo));end_track();begin_track();
    const uint8_t notes[]={0,192,0,0,144,60,100,96,128,60,0,0,255,47,0};
    bytes(notes,sizeof(notes));end_track();
    assert(fd_music_start(midi,length,1));
    assert(render(8268,8,0)>100);assert(fd_music_loops()==0);
    render(2,8,0);assert(fd_music_loops()==1);
    uint64_t pos=fd_music_position();
    assert(render(1000,8,1)==0 && fd_music_position()==pos);
    assert(render(1000,0,0)==0 && fd_music_position()==pos+1000);
    assert(!fd_music_faults());
}
static void malformed(void) {
    assert(!fd_music_start(NULL,0,1));
    for(size_t n=0;n<length;++n) {
        if(fd_music_start(midi,n,0))render(10000,8,0);
    }
    header(1);begin_track();
    const uint8_t truncated[]={0,144,60};bytes(truncated,sizeof(truncated));end_track();
    assert(fd_music_start(midi,length,1));render(100,8,0);assert(fd_music_faults()==1);
    header(1);begin_track();
    const uint8_t vlq[]={128,128,128,128,0};bytes(vlq,sizeof(vlq));end_track();
    assert(!fd_music_start(midi,length,1));
    header(1);begin_track();
    const uint8_t empty[]={0,255,47,0};bytes(empty,sizeof(empty));end_track();
    assert(fd_music_start(midi,length,1));render(1000,8,0);assert(!fd_music_loops());
}
static void controllers(void) {
    header(1);begin_track();
    const uint8_t notes[]={0,192,0,0,176,64,127,0,144,60,127,0,64,127,
        1,128,60,0,0,64,0,1,176,64,0,0,224,127,127,1,176,121,0,
        0,176,120,0,1,255,47,0};
    bytes(notes,sizeof(notes));end_track();
    assert(fd_music_start(midi,length,0));render(11025,15,0);assert(!fd_music_faults());
    int16_t pcm[1024];for(unsigned i=0;i<1024;++i)pcm[i]=30000;
    fd_music_stop();fd_music_mix(pcm,512,0,0);
    for(unsigned i=0;i<1024;++i)assert(pcm[i]==30000);
}
static uint8_t *read_file(const char *path,size_t *size) {
    FILE *f=fopen(path,"rb");assert(f);
    assert(!fseek(f,0,SEEK_END));long n=ftell(f);assert(n>0 && n<=256*1024);
    rewind(f);uint8_t *data=malloc((size_t)n);assert(data);
    assert(fread(data,1,(size_t)n,f)==(size_t)n);fclose(f);*size=(size_t)n;return data;
}
int main(int argc,char **argv) {
    initialize();timing();malformed();controllers();
    puts("MIDI tempo/multitrack/loop/pause/mute/controllers/bounds pass");
    if(argc>2) {
        size_t size;uint8_t *real_bank=read_file(argv[1],&size);
        assert(fd_music_init(real_bank,size));
        for(int i=2;i<argc;++i) {
            uint8_t *song=read_file(argv[i],&size);assert(fd_music_start(song,size,1));
            unsigned n=0,limit=FD_MUSIC_RATE*60*15;
            while(n<limit && !fd_music_loops()) {
                render(512,8,0);n+=512;assert(!fd_music_faults());
            }
            assert(n<limit && fd_music_peak()>0);
            printf("PASS %s seconds=%.3f peak=%u loops=%u\n",argv[i],n/(double)FD_MUSIC_RATE,fd_music_peak(),fd_music_loops());
            fd_music_stop();free(song);
        }
        free(real_bank);
    }
}
