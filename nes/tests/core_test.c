#include "core.h"
#include "audio_fifo.h"
#include "scheduler.h"
#include "nes/nes.h"
#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(c) do { if(!(c)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#c);exit(1);} }while(0)
void nes_port_log(const char *format, ...) { if(getenv("NES_VERBOSE")){va_list ap;va_start(ap,format);vfprintf(stderr,format,ap);va_end(ap);} }
static uint64_t now(void) {struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec*1000000ull+t.tv_nsec/1000;}
int main(int argc,char **argv) {
    CHECK(argc>=2);
    nes_audio_fifo_t fifo={0};int16_t stream[5000];
    for(unsigned i=0;i<5000;++i)stream[i]=i;
    CHECK(!nes_audio_fifo_push(&fifo,stream,1000));nes_audio_fifo_consume(&fifo,700);
    CHECK(!nes_audio_fifo_push(&fifo,stream+1000,3500));
    for(unsigned i=700;i<4500;) {
        unsigned count;int16_t *p=nes_audio_fifo_peek(&fifo,127,&count);CHECK(count>0);
        for(unsigned j=0;j<count;++j)CHECK(p[j]==(int16_t)(i+j));
        nes_audio_fifo_consume(&fifo,count);i+=count;
    }
    CHECK(fifo.count==0);
    CHECK(nes_audio_fifo_push(&fifo,stream,4116)==20);
    unsigned count;CHECK(*nes_audio_fifo_peek(&fifo,1,&count)==20 && count==1);
    nes_audio_fifo_clear(&fifo);
    CHECK(!nes_audio_fifo_push(&fifo,stream,4000));
    CHECK(nes_audio_fifo_push(&fifo,stream+4000,200)==104);
    CHECK(*nes_audio_fifo_peek(&fifo,1,&count)==104 && fifo.count==4096);
    nes_audio_fifo_clear(&fifo);
    nes_schedule_t s={0};nes_schedule_tick(&s,1000000,60);
    for(unsigned i=1;i<=125;++i)nes_schedule_tick(&s,1000000+8000*i,60);
    CHECK(s.debt==60000000);
    unsigned steps=0;while(nes_schedule_due(&s)){nes_schedule_step(&s);++steps;}
    CHECK(steps==60 && s.debt==0);
    nes_schedule_tick(&s,2012345,60);CHECK(s.debt==740700);
    nes_schedule_reset(&s);CHECK(!s.previous && !s.debt);
    FILE *f=fopen(argv[1],"rb");CHECK(f);fseek(f,0,SEEK_END);size_t size=ftell(f);rewind(f);
    uint8_t *b=malloc(size);CHECK(b);CHECK(fread(b,1,size,f)==size);fclose(f);
    CHECK(!nes_core_validate(b,size));CHECK(nes_core_validate(b,15));CHECK(nes_core_validate(b,size-1));
    uint8_t h=b[7];b[7]=8;CHECK(nes_core_validate(b,size));b[7]=h;
    CHECK(nes_port_crc32(0,"123456789",9)==0xcbf43926);
    CHECK(nes_port_crc32(nes_port_crc32(0,"1234",4),"56789",5)==0xcbf43926);
    CHECK(!nes_core_load(b,size));
    input_update(0,1);input_write(0x4016,1);
    for(unsigned i=0;i<4;++i)CHECK((input_read(0x4016)&1)==1);
    input_write(0x4016,0);
    CHECK((input_read(0x4016)&1)==1);
    for(unsigned i=0;i<7;++i)CHECK((input_read(0x4016)&1)==0);
    for(unsigned i=0;i<40;++i)CHECK((input_read(0x4016)&1)==1);
    uint64_t start=now();unsigned long samples=0;uint64_t energy=0;unsigned frames=argc>2?atoi(argv[2]):600;
    for(unsigned i=0;i<frames;++i){nes_core_frame(i%3==0,argc>3?0:(i>60?0x81:0),0);size_t n;const int16_t *pcm=nes_core_audio(&n);samples+=n;for(size_t j=0;j<n;++j)energy+=pcm[j]<0?-(int)pcm[j]:pcm[j];}
    uint64_t us=now()-start;unsigned hz=nes_core_rate();
    CHECK(samples==(unsigned long)frames*16000/hz);
    size_t n;uint8_t *ram=nes_core_sram(&n);
    if(argc>3 && !strcmp(argv[3],"blargg")) {
        ram=nes_getptr()->cart->prg_ram;n=nes_getptr()->cart->prg_ram_banks*8192;
        CHECK(n>=8192);CHECK(ram[1]==0xde && ram[2]==0xb0 && ram[3]==0x61);fprintf(stderr,"blargg status=%u %.*s\n",ram[0],4092,(char*)ram+4);
        nes6502_t cpu;nes6502_getcontext(&cpu);if(ram[0])fprintf(stderr,"cpu pc=%04x cycles=%ld jammed=%d\n",cpu.pc_reg,cpu.total_cycles,cpu.jammed);CHECK(ram[0]==0);
    } else {CHECK(n>=8192);CHECK(ram[1]==0x81);CHECK(energy>0);}
    printf("PASS frames=%u samples=%lu elapsed_us=%llu fps=%.2f mapper=%u\n",frames,samples,(unsigned long long)us,frames*1000000.0/us,nes_core_mapper());
    if(b[6]&4)CHECK(!memcmp(nes_getptr()->cart->prg_ram+0x1000,b+16,512));
    nes6502_t cpu;nes6502_getcontext(&cpu);
    uint32_t state[]={cpu.pc_reg,cpu.a_reg,cpu.x_reg,cpu.y_reg,cpu.s_reg,cpu.p_reg,(uint32_t)cpu.total_cycles};
    uint32_t signature=nes_port_crc32(0,state,sizeof(state));
    signature=nes_port_crc32(signature,nes_getptr()->mem->ram,2048);
    signature=nes_port_crc32(signature,nes_getptr()->cart->prg_ram,nes_getptr()->cart->prg_ram_banks*8192u);
    signature=nes_port_crc32(signature,nes_core_video()-8,272*240);
    printf("SIGNATURE %08x\n",signature);
    nes_core_close();free(b);return 0;
}
