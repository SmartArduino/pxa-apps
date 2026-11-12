// SPDX-License-Identifier: GPL-3.0-or-later
// Bounded SMF 0/1 sequencer and GENMIDI -> OPL2 adapter. Application code only.
#include "music.h"
#include "opl2.h"
#include <limits.h>
#include <math.h>
#include <string.h>

#define TRACKS 32
#define VOICES 9
#define CHANNELS 16
#define EVENT_BUDGET 4096
#define MAX_MIDI_BYTES (256*1024)
#define MAX_TICKS 0x0fffffffU

typedef struct {
    const uint8_t *begin, *cursor, *end;
    uint32_t tick;
    uint8_t running, ended;
} Track;
typedef struct {uint8_t program, volume, expression, pan, sustain; int bend;} Channel;
typedef struct {
    const uint8_t *patch;
    uint32_t age;
    int note, offset, detune;
    uint8_t channel, velocity, down, sustained, occupied, key;
} Voice;
static Track tracks[TRACKS];
static Channel channels[CHANNELS];
static Voice voices[VOICES];
static const uint8_t *bank;
static unsigned track_count, division, tempo, looping, playing, loops, faults, peak;
static uint32_t tick, age;
static uint64_t position, deadline, fraction;
static uint32_t pitch[128];
static float bend_ratio[385];
static const uint8_t operators[9] = {0,1,2,8,9,10,16,17,18};

static uint16_t be16(const uint8_t *p) {return (uint16_t)((p[0]<<8)|p[1]);}
static uint32_t be32(const uint8_t *p) {return (uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3];}
static int le16s(const uint8_t *p) {unsigned v=p[0]|p[1]<<8;return v>=32768?(int)v-65536:(int)v;}
static int vlq(Track *t, uint32_t *out) {
    uint32_t value=0;
    for(unsigned i=0;i<4;++i) {
        if(t->cursor==t->end)return 0;
        unsigned b=*t->cursor++;
        value=(value<<7)|(b&127);
        if(!(b&128)) {*out=value;return 1;}
    }
    return 0;
}
static int next_delta(Track *t) {
    uint32_t delta;
    if(!vlq(t,&delta) || delta>MAX_TICKS-t->tick)return 0;
    t->tick+=delta;
    return 1;
}
static void key_off(unsigned i) {
    OPL2_WriteRegister(0xb0+i, voices[i].key & ~32);
    voices[i].down=voices[i].sustained=0;
}
static void all_off(unsigned c, int immediate) {
    for(unsigned i=0;i<VOICES;++i)if(voices[i].occupied && voices[i].channel==c) {
        key_off(i);
        if(immediate) {
            OPL2_WriteRegister(0x40+operators[i],63);
            OPL2_WriteRegister(0x43+operators[i],63);
            voices[i].occupied=0;
        }
    }
}
static void reset_channels(void) {
    memset(channels,0,sizeof(channels));
    for(unsigned c=0;c<CHANNELS;++c) {
        channels[c].volume=100;channels[c].expression=127;
        channels[c].pan=64;channels[c].bend=8192;
    }
}
static void reset_chip(void) {
    OPL2_Reset(FD_MUSIC_RATE);OPL2_InitRegisters(0);
    memset(voices,0,sizeof(voices));age=0;
    reset_channels();
}
static void volume_voice(unsigned i) {
    Voice *v=&voices[i];Channel *c=&channels[v->channel];
    unsigned amplitude=(unsigned)v->velocity*c->volume*c->expression;
    // Approximate MIDI's logarithmic loudness in 0.75 dB OPL attenuation steps.
    unsigned level=amplitude/(127*127),attenuation=63;
    if(level) {
        attenuation=0;
        while(level<64 && attenuation<56) {level*=2;attenuation+=8;}
        attenuation+=(127-level)*8/64;
        if(attenuation>63)attenuation=63;
    }
    unsigned carrier=(v->patch[12]&63)+attenuation;
    unsigned modulator=(v->patch[5]&63)+((v->patch[6]&1)?attenuation:0);
    OPL2_WriteRegister(0x43+operators[i],(v->patch[11]&192)|(carrier>63?63:carrier));
    OPL2_WriteRegister(0x40+operators[i],(v->patch[4]&192)|(modulator>63?63:modulator));
}
static void pitch_voice(unsigned i) {
    Voice *v=&voices[i];int note=v->note+v->offset;
    if(note<0)note=0;
    if(note>127)note=127;
    // +/- two semitones for GM pitch bend, plus GENMIDI second-voice detune.
    int bend=channels[v->channel].bend-8192+v->detune*32;
    unsigned at=(unsigned)(bend+12288),index=at>>6,part=at&63;
    float ratio=bend_ratio[index]+(bend_ratio[index+1]-bend_ratio[index])*(part/64.0f);
    unsigned f=(unsigned)(pitch[note]*ratio+0.5f),block=0;
    while(f>1023 && block<7) {f=(f+1)/2;++block;}
    if(f>1023)f=1023;
    OPL2_WriteRegister(0xa0+i,f&255);
    v->key=(uint8_t)((f>>8)|(block<<2)|((v->down||v->sustained)?32:0));
    OPL2_WriteRegister(0xb0+i,v->key);
}
static void update_channel(unsigned c, int frequency) {
    for(unsigned i=0;i<VOICES;++i)if(voices[i].occupied && voices[i].channel==c) {
        if(frequency)pitch_voice(i);else volume_voice(i);
    }
}
static void release_sustain(unsigned c) {
    for(unsigned i=0;i<VOICES;++i)if(voices[i].occupied && voices[i].channel==c && voices[i].sustained)
        key_off(i);
}
static unsigned allocate_voice(void) {
    unsigned candidate=0;
    for(unsigned i=0;i<VOICES;++i) {
        if(!voices[i].occupied)return i;
        if((!voices[i].down && voices[candidate].down) ||
           (voices[i].down==voices[candidate].down && voices[i].age<voices[candidate].age))candidate=i;
    }
    key_off(candidate);return candidate;
}
static void note_on(unsigned c, unsigned note, unsigned velocity) {
    unsigned instrument=channels[c].program;
    if(c==9) {
        if(note<35 || note>81)return;
        instrument=128+note-35;
    }
    const uint8_t *entry=bank+8+36*instrument;
    unsigned flags=entry[0]|entry[1]<<8;
    for(unsigned part=0;part<((flags&4)?2u:1u);++part) {
        unsigned i=allocate_voice();Voice *v=&voices[i];
        key_off(i);
        memset(v,0,sizeof(*v));
        v->patch=entry+4+16*part;v->channel=(uint8_t)c;
        v->velocity=(uint8_t)velocity;v->note=(int)note;
        v->offset=le16s(v->patch+14);
        if((flags&1) || c==9)v->offset+=(int)entry[3]-(int)note;
        v->detune=part?(int)entry[2]-128:0;
        v->age=++age;v->occupied=v->down=1;
        unsigned op=operators[i];
        OPL2_WriteRegister(0x20+op,v->patch[0]);OPL2_WriteRegister(0x23+op,v->patch[7]);
        OPL2_WriteRegister(0x60+op,v->patch[1]);OPL2_WriteRegister(0x63+op,v->patch[8]);
        OPL2_WriteRegister(0x80+op,v->patch[2]);OPL2_WriteRegister(0x83+op,v->patch[9]);
        OPL2_WriteRegister(0xe0+op,v->patch[3]);OPL2_WriteRegister(0xe3+op,v->patch[10]);
        OPL2_WriteRegister(0xc0+i,v->patch[6]);
        volume_voice(i);pitch_voice(i);
    }
}
static void note_off(unsigned c,unsigned note) {
    for(unsigned i=0;i<VOICES;++i)if(voices[i].occupied && voices[i].channel==c && voices[i].note==(int)note && voices[i].down) {
        voices[i].down=0;
        if(channels[c].sustain)voices[i].sustained=1;
        else key_off(i);
    }
}
static void control(unsigned c,unsigned key,unsigned value) {
    Channel *ch=&channels[c];
    switch(key) {
    case 7:ch->volume=(uint8_t)value;update_channel(c,0);break;
    case 10:ch->pan=(uint8_t)value;break; // OPL2 output is mono.
    case 11:ch->expression=(uint8_t)value;update_channel(c,0);break;
    case 64:ch->sustain=value>=64;if(!ch->sustain)release_sustain(c);break;
    case 120:all_off(c,1);break;
    case 121:ch->sustain=0;ch->expression=127;ch->bend=8192;release_sustain(c);update_channel(c,0);update_channel(c,1);break;
    case 123:case 126:case 127:
        for(unsigned i=0;i<VOICES;++i)if(voices[i].occupied && voices[i].channel==c)note_off(c,(unsigned)voices[i].note);
        break;
    default:break;
    }
}
static int event(Track *t) {
    if(t->cursor==t->end)return 0;
    unsigned status=*t->cursor++;
    if(status<128) {
        if(t->running<128 || t->running>=240)return 0;
        --t->cursor;status=t->running;
    } else if(status<240)t->running=(uint8_t)status;
    unsigned type=status&240,c=status&15;
    if(status==255 || status==240 || status==247) {
        unsigned meta=0;uint32_t length;
        if(status==255) {if(t->cursor==t->end)return 0;meta=*t->cursor++;}
        else t->running=0;
        if(!vlq(t,&length) || (size_t)(t->end-t->cursor)<length)return 0;
        if(status==255 && meta==81) {
            if(length!=3)return 0;
            unsigned value=(unsigned)t->cursor[0]<<16|t->cursor[1]<<8|t->cursor[2];
            if(!value)return 0;
            tempo=value;
        }
        if(status==255 && meta==47) {if(length)return 0;t->ended=1;}
        t->cursor+=length;
    } else {
        if(type<128 || type>224)return 0;
        unsigned n=(type==192||type==208)?1:2;
        if((size_t)(t->end-t->cursor)<n)return 0;
        unsigned a=*t->cursor++,b=0;
        if(n==2)b=*t->cursor++;
        if(a>127||b>127)return 0;
        switch(type) {
        case 128:note_off(c,a);break;
        case 144:if(b)note_on(c,a,b);else note_off(c,a);break;
        case 176:control(c,a,b);break;
        case 192:channels[c].program=(uint8_t)a;break;
        case 224:channels[c].bend=(int)(a|(b<<7));update_channel(c,1);break;
        default:break;
        }
    }
    return t->ended || next_delta(t);
}
static unsigned next_track(void) {
    unsigned best=TRACKS;
    for(unsigned i=0;i<track_count;++i)if(!tracks[i].ended && (best==TRACKS || tracks[i].tick<tracks[best].tick))best=i;
    return best;
}
static int rewind_song(void) {
    for(unsigned i=0;i<track_count;++i) {
        Track *t=&tracks[i];t->cursor=t->begin;t->tick=t->running=t->ended=0;
        if(!next_delta(t))return 0;
    }
    reset_chip();tempo=500000;tick=0;fraction=0;
    return 1;
}
int fd_music_init(const void *genmidi,size_t bytes) {
    bank=NULL;fd_music_stop();
    if(!genmidi || bytes<8+175*36 || memcmp(genmidi,"#OPL_II#",8))return 0;
    bank=genmidi;
    // OPL frequency at block 0. Clamp upper MIDI notes to chip range.
    for(unsigned i=0;i<128;++i) {
        double hz=440.0*pow(2.0,((int)i-69)/12.0);
        double f=hz*1048576.0/49716.0;
        pitch[i]=(uint32_t)(f+0.5);
    }
    for(unsigned i=0;i<385;++i)
        bend_ratio[i]=powf(2.0f,((int)i*64-12288)/(8192.0f*6.0f));
    return 1;
}
void fd_music_stop(void) {
    playing=0;position=deadline=fraction=0;loops=peak=0;
    reset_chip();
}
int fd_music_start(const void *midi,size_t bytes,int repeat) {
    fd_music_stop();
    const uint8_t *p=midi;
    if(!bank||!p||bytes<14||bytes>MAX_MIDI_BYTES||memcmp(p,"MThd",4))return 0;
    uint32_t length=be32(p+4);
    if(length<6||length>bytes-8)return 0;
    unsigned format=be16(p+8);track_count=be16(p+10);division=be16(p+12);
    if(format>1||!track_count||track_count>TRACKS||(format==0&&track_count!=1)||!division||(division&32768))return 0;
    const uint8_t *end=p+bytes;p+=8+length;
    memset(tracks,0,sizeof(tracks));
    for(unsigned i=0;i<track_count;++i) {
        if((size_t)(end-p)<8||memcmp(p,"MTrk",4))return 0;
        length=be32(p+4);p+=8;
        if(length>(size_t)(end-p))return 0;
        tracks[i].begin=p;tracks[i].end=p+length;p+=length;
    }
    if(!rewind_song())return 0;
    faults=0;looping=repeat!=0;playing=1;return 1;
}
static int schedule(void) {
    // Bounded per audio call, including malformed zero-time loops.
    unsigned budget=EVENT_BUDGET;
    while(playing && deadline<=position) {
        unsigned i=next_track();
        if(i==TRACKS) {
            if(!looping||tick==0) {
                playing=0;
                for(unsigned c=0;c<CHANNELS;++c)all_off(c,0);
                break;
            }
            deadline=position;++loops;
            if(!rewind_song())goto invalid;
            continue;
        }
        uint32_t delta=tracks[i].tick-tick;
        if(delta) {
            // Preserve fractional samples across events and tempo changes.
            uint64_t denom=(uint64_t)division*1000000;
            // Avoid overflow on hostile event times (official songs are short).
            if(delta>1000000 || (tempo>0 && (uint64_t)delta>UINT64_MAX/tempo/FD_MUSIC_RATE))goto invalid;
            uint64_t value=(uint64_t)delta*tempo*FD_MUSIC_RATE+fraction;
            deadline+=value/denom;fraction=value%denom;tick=tracks[i].tick;
            if(deadline>position)break;
        }
        if(!budget--||!event(&tracks[i]))goto invalid;
    }
    return 1;
invalid:
    ++faults;playing=0;reset_chip();return 0;
}
void fd_music_mix(int16_t *stereo,unsigned frames,unsigned volume,int paused) {
    if(!stereo||paused)return;
    if(volume>15)volume=15;
    while(frames) {
        unsigned n=frames>FD_MUSIC_BLOCK?FD_MUSIC_BLOCK:frames;
        if(playing) {
            if(!schedule())return;
            if(playing && deadline>position && deadline-position<n)n=(unsigned)(deadline-position);
        }
        int32_t mono[FD_MUSIC_BLOCK];
        OPL2_GenerateStream(mono,n);
        for(unsigned i=0;i<n;++i) {
            int sample=mono[i]*(int)volume/30;
            unsigned magnitude=(unsigned)(sample<0?-sample:sample);
            if(magnitude>peak)peak=magnitude;
            for(unsigned c=0;c<2;++c) {
                int mixed=stereo[2*i+c]+sample;
                stereo[2*i+c]=(int16_t)(mixed>32767?32767:mixed< -32768?-32768:mixed);
            }
        }
        position+=n;stereo+=2*n;frames-=n;
    }
}
uint64_t fd_music_position(void) {return position;}
unsigned fd_music_loops(void) {return loops;}
unsigned fd_music_faults(void) {return faults;}
unsigned fd_music_peak(void) {return peak;}
