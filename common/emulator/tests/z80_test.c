/* Compare batched HALT against single-instruction execution at IRQ boundaries. */
#include "core.h"
#include "engines/smsplus/shared.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static int irq(int line){(void)line;return 0xff;}
int main(int argc,char **argv){
 assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);fseek(f,0,SEEK_END);size_t n=ftell(f);rewind(f);
 uint8_t *rom=calloc(1,EMU_ROM_CAPACITY(n));assert(rom&&fread(rom,1,n,f)==n);fclose(f);
 rom[0x100]=rom[0x38]=rom[0x66]=0x76;assert(!emu_core_load(rom,n));
 Z80_Regs initial=Z80;initial.pc.d=0x100;initial.sp.d=0xdff0;initial.irq_callback=irq;
 const unsigned cycles[]={1,4,5,13,227,228,229,60000};
 for(unsigned mode=0;mode<6;++mode)for(unsigned i=0;i<sizeof(cycles)/sizeof(*cycles);++i){
  Z80_Regs before=initial;before.r=0xf7;before.r2=0x80;before.halt=mode!=0;
  before.iff1=before.iff2=mode>=2;before.im=1;
  before.irq_state=(mode==3 || mode==4)?ASSERT_LINE:CLEAR_LINE;
  before.after_ei=mode==4;before.nmi_pending=mode==5;
  z80_set_context(&before);z80_reset_cycle_count();int actual_cycles=z80_execute(cycles[i]);Z80_Regs actual=Z80;
  z80_set_context(&before);z80_reset_cycle_count();int reference_cycles=0;
  while(reference_cycles<(int)cycles[i])reference_cycles+=z80_execute(1);
  assert(reference_cycles==actual_cycles);assert(!memcmp(&actual,&Z80,sizeof(actual)));
 }
 emu_core_close();free(rom);puts("PASS: HALT refresh, rounding, IRQ, EI shadow and NMI versus instruction stepping");
}
