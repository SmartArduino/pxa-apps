/* GPL-2.0-or-later. Compare actual CPU state at every scanline budget. */
#include "core.h"
#include "vendor/pce-go/pce.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern void h6280_test_batch_enabled(int enabled);
static int source_read(void *ctx,uint64_t offset,void *out,size_t n){memcpy(out,(uint8_t*)ctx+offset,n);return 0;}
int main(int argc,char **argv){
    assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);fseek(f,0,SEEK_END);size_t size=ftell(f);rewind(f);
    uint8_t *rom=calloc(1,EMU_ROM_CAPACITY(size));assert(rom && fread(rom,1,size,f)==size);fclose(f);assert(!emu_core_load(rom,size));
    const uint8_t code[]={0xee,0x43,0x27,0xd0,3,0xee,0x44,0x27,0xcd,0x41,0x22,0xf0,0xf3,0x80,0xfe};
    h6280_batch_cartridge(NULL,0);assert(h6280_batch_available());
    uint8_t empty[13]={0};h6280_batch_cartridge(empty,sizeof(empty));assert(!h6280_batch_available());
    for(unsigned n=0;n<13;++n){h6280_batch_cartridge(code,n);assert(!h6280_batch_available());}
    h6280_batch_cartridge(code,13);assert(h6280_batch_available());
    memcpy(PageR[7]+0xe200,code,sizeof(code));
    h6280_batch_cartridge(rom,size);assert(h6280_batch_available());
    uint8_t *vector=PageR[7]+0xfff8;vector[0]=0;vector[1]=0xe2;
    const unsigned entries[]={0,3,5,8,11};
    uint8_t initial[8192],reference[8192];unsigned checks=0;
    for(unsigned paged=0;paged<2;++paged){
    if(paged){rom_source_t source={rom,source_read,size,emu_port_crc32(0,rom,size)};assert(!emu_core_load_source(&source,65536));}
    for(unsigned scenario=0;scenario<4;++scenario)for(unsigned mismatch=0;mismatch<2;++mismatch)for(unsigned low=0;low<256;low+=17)
    for(unsigned flag=0;flag<256;flag+=17)for(unsigned entry=0;entry<5;++entry)
    for(unsigned budget=1;budget<=160;budget+=3){
        uint8_t program[sizeof(code)];memcpy(program,code,sizeof(code));
        if(scenario==1){program[9]=0x43;program[10]=0x27;}
        if(scenario==2){program[6]=0x43;program[7]=0x27;}
        memcpy(PageR[7]+0xe200,program,sizeof(program));
        memset(initial,0x5a,sizeof(initial));
        if(scenario==3)memcpy(initial+0x100,program,sizeof(program));initial[0x743]=low;initial[0x744]=255;initial[0x241]=0x42+mismatch;
        h6280_t start={.PC=(scenario==3?0x2100:0xe200)+entries[entry],.A=0x42,.X=13,.Y=27,.P=flag,.S=0xff,.irq_lines=(flag&1)?INT_IRQ1:0,.irq_mask=(low&32)?INT_IRQ1:0};
        CPU=start;memcpy(PCE.RAM,initial,sizeof(initial));PCE.Cycles=0;h6280_test_batch_enabled(0);h6280_run(budget);
        h6280_t expected=CPU;int cycles=PCE.Cycles;memcpy(reference,PCE.RAM,sizeof(reference));
        CPU=start;memcpy(PCE.RAM,initial,sizeof(initial));PCE.Cycles=0;h6280_test_batch_enabled(1);h6280_run_batched(budget);
        assert(!memcmp(&CPU,&expected,sizeof(CPU)) && PCE.Cycles==cycles && !memcmp(PCE.RAM,reference,sizeof(reference)));++checks;
    }
    }
    printf("PASS: %u exact CPU/flags/RAM/cycle comparisons, partial entries, overflow, loop exit, aliases, RAM code fallback, resident and paged ROM, pending/masked IRQ\n",checks);
    emu_core_close();free(rom);
}
