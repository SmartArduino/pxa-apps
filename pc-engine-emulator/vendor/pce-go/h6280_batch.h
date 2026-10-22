/* GPL-2.0-or-later. Exact batching of a RAM counter/poll loop.
 * No ROM address or checksum patches: recognize instructions and prove that
 * the code is ROM and all data are non-aliasing ordinary work RAM. IRQ/video
 * events are delivered between h6280_run calls. Leave the final partial
 * iteration to the original handlers, preserving instruction overshoot. */
#ifndef PXA_PCE_LOOP_BATCH
#define PXA_PCE_LOOP_BATCH 1
#endif
static int h6280_batch_possible=1;
/* Classify immutable resident code once, off the frame path. A paged source
 * has no contiguous image and keeps the guarded scanline detector enabled. */
void h6280_batch_cartridge(const uint8_t *rom,size_t size){
    h6280_batch_possible=rom==NULL;
    if(!rom || size<13)return;
    const uint8_t *at=rom,*end=rom+size-12;
    while(at<end && (at=memchr(at,0xee,(size_t)(end-at)))){
        if(at[3]==0xd0 && at[4]==3 && at[5]==0xee && at[8]==0xcd && at[11]==0xf0 && at[12]==0xf3){h6280_batch_possible=1;return;}
        ++at;
    }
}
#ifndef PXA_GUEST
static int h6280_batch_enabled=1;
void h6280_test_batch_enabled(int enabled){h6280_batch_enabled=enabled;}
#endif
static inline uint8_t *h6280_work_ram(uint16_t addr){
    uint8_t *read=PageR[addr>>13],*write=PageW[addr>>13];
    if(read!=write || read==PCE.IOAREA)return NULL;
    uintptr_t p=(uintptr_t)read+addr,ram=(uintptr_t)PCE.RAM;
    return p>=ram && p-ram<8192?(uint8_t*)p:NULL;
}
static void h6280_batch(int max_cycles){
#if PXA_PCE_LOOP_BATCH
#ifndef PXA_GUEST
    if(!h6280_batch_enabled)return;
#endif
    /* INC low; BNE +3; INC high; CMP value; BEQ -13. Most scanline
     * entries cannot belong to this block: reject after one opcode load. */
    unsigned first,attempts=1;
    switch(imm_operand(CPU.PC)){
    case 0xee:first=0;attempts=2;break;
    case 0xd0:first=3;break;
    case 0xcd:first=8;break;
    case 0xf0:first=11;break;
    default:return;
    }
    for(unsigned k=0;k<attempts;++k){
        unsigned pc=(uint16_t)(CPU.PC-(k?5:first)),bank=PCE.MMR[pc>>13];
        if((pc&8191)>8192-13 || bank>=128 || pxa_pce_rom_banks[bank]<0)continue;
        const uint8_t *p=PageR[pc>>13]+pc;
        if(p[0]!=0xee || p[3]!=0xd0 || p[4]!=3 || p[5]!=0xee ||
           p[8]!=0xcd || p[11]!=0xf0 || p[12]!=0xf3)continue;
        uint8_t *low=h6280_work_ram((uint16_t)(p[1]|p[2]<<8));
        uint8_t *high=h6280_work_ram((uint16_t)(p[6]|p[7]<<8));
        uint8_t *value=h6280_work_ram((uint16_t)(p[9]|p[10]<<8));
        if(!low || !high || !value || low==high || low==value || high==value)return;
        while(Cycles<max_cycles){
            switch((unsigned)(CPU.PC-pc)){
            case 0:{
                /* A full iteration costs 20 cycles, plus 5 on low-byte
                 * overflow. Conservative bound keeps every fused iteration
                 * strictly inside the caller's event boundary. */
                unsigned n=(unsigned)(max_cycles-Cycles-1)/25;
                if(n && *value==CPU.A){
                    unsigned sum=*low+n,carry=sum>>8;
                    *low=(uint8_t)sum;*high=(uint8_t)(*high+carry);
                    CPU.P=(CPU.P&~(FL_N|FL_T|FL_Z|FL_C))|FL_Z|FL_C;
                    Cycles+=20*n+5*carry;
                }else inc_abs();
                break;
            }
            case 3:bne();break;
            case 5:inc_abs();break;
            case 8:cmp_abs();break;
            case 11:beq();break;
            default:return;
            }
        }
        return;
    }
#else
    (void)max_cycles;
#endif
}
