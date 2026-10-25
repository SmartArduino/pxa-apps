/* GPL-2.0-or-later. Exact ARM instruction fusion, independent of ROM identity.
 * No HLE: preserve registers, flags, fetch cycles and event boundaries. The
 * instruction bytes are checked before reuse; stores and hardware events
 * invalidate cached IWRAM validation. */
#ifndef PXA_GPSP_CPU_FUSION_H
#define PXA_GPSP_CPU_FUSION_H

#ifdef PXA_GBA_IDLE_TEST
extern "C" { bool gba_cpu_fusion_enabled = true; uint64_t gba_cpu_fusion_hits[3]; }
#define FUSION_HIT(i) (++gba_cpu_fusion_hits[i])
#else
#define FUSION_HIT(i) ((void)0)
#endif

/* Only IWRAM code can reuse validation. CPU stores invalidate it; hardware
 * events, DMA, cheats and ARM/Thumb transitions reset it before execution. */
static struct {u32 pc, words;} fusion_code_cache={~0u,0};
static inline void fusion_write_barrier() {fusion_code_cache.pc=~0u;}

/* Continue through sample advancement when every memory access is already
 * mapped ordinary memory and the sample-end branch will not be taken. */
static inline bool fusion_byte_pointer(u32 address, u8 **out)
{
  unsigned region=address>>24;
  if (!(region==2 || region==3 || region==6 || (region>=8 && region<=12))) return false;
  u8 *page=memory_map_read[address>>15];
  if (!page) return false;
  *out=page+(address&0x7fff);return true;
}
static void arm_fuse_sample_tail(u32 pc,u8 *page,u32 off,unsigned fetch,
                                unsigned tmp,unsigned phase,s32 &remaining,
                                u32 &n,u32 &z,u32 &c,u32 &v)
{
  if (off>=0x8000-84 ||
      (cheat_master_hook>=pc+40 && cheat_master_hook<pc+84) ||
      (idle_loop_target_pc>=pc+40 && idle_loop_target_pc<pc+84)) return;
  u32 step=reg[tmp];
  if (step && (s32)reg[2]<=(s32)step) return;
  if (!(fusion_code_cache.pc==pc && fusion_code_cache.words==21) &&
     (cpu_readmap32(page,off+40)!=0x0a000007 ||
      cpu_readmap32(page,off+44)!=(0xe3c005feu|phase<<16|phase<<12) ||
      cpu_readmap32(page,off+48)!=(0xe0522000u|tmp) ||
      (cpu_readmap32(page,off+52)&0xff000000)!=0xda000000 ||
      cpu_readmap32(page,off+56)!=(0xe2500001u|tmp<<16|tmp<<12) ||
      cpu_readmap32(page,off+60)!=0x00800001 ||
      cpu_readmap32(page,off+64)!=(0x11b300d0u|tmp) ||
      cpu_readmap32(page,off+68)!=0xe1f310d1 ||
      cpu_readmap32(page,off+72)!=0xe0411000 ||
      cpu_readmap32(page,off+76)!=0xe2955101 ||
      cpu_readmap32(page,off+80)!=0x3affffea)) return;
  if ((pc>>24)==3) {fusion_code_cache.pc=pc;fusion_code_cache.words=21;}
  u32 old=reg[5],next=old+0x40000000u;
  unsigned branch_cost=ws_cyc_nseq[pc>>24][1];
  unsigned cost=(step?11:3)*fetch + (step?0:branch_cost) + (next>=old?branch_cost:0);
  u32 address=reg[3];u8 *first=NULL,*second=NULL;
  if(step) {
    if(step>1) {
      address+=step-1;
      if(!fusion_byte_pointer(address,&first))return;
      cost+=ws_cyc_nseq[address>>24][0];
    }
    ++address;
    if(!fusion_byte_pointer(address,&second))return;
    cost+=ws_cyc_nseq[address>>24][0];
  }
  /* One fetch remains pending in the ordinary dispatch epilogue. */
  if (remaining<(s32)(cost+fetch))return;
  if(step) {
    reg[phase]&=~0x3f800000u;
    reg[2]-=step;reg[tmp]=step-1;
    reg[0]=step==1 ? reg[0]+reg[1] : (u32)(s32)(s8)*first;
    reg[1]=(u32)(s32)(s8)*second-reg[0];reg[3]=address;
  }
  FUSION_HIT(1);
  reg[5]=next;n=next>>31;z=next==0;c=next<old;
  v=((~(old^0x40000000u))&(old^next))>>31;
  reg[REG_PC]=c?pc+84:pc;
  remaining-=cost;
}

static bool arm_fuse_multiply(u32 opcode, u8 *page, s32 &remaining,
                             u32 &n, u32 &z, u32 &c, u32 &v)
{
#if defined(PXA_GBA_PROFILE) || defined(TRACE_INSTRUCTIONS)
  return false;
#else
#ifdef PXA_GBA_IDLE_TEST
  if (!gba_cpu_fusion_enabled) return false;
#endif
  /* Two register allocations of a common stereo interpolation block.
   * All ten instructions are ALU operations; only the final MOVS sets NZC.
   * Matching the complete sequence avoids any dependency on game addresses. */
  unsigned tmp, phase;
  if (opcode == 0xe009019e) { tmp=9; phase=14; }
  else if (opcode == 0xe00e0199) { tmp=14; phase=9; }
  else return false;
  u32 pc=reg[REG_PC], off=pc & 0x7fff;
  unsigned fetch=ws_cyc_seq[(pc >> 24) & 15][1];
  if (off >= 0x8000-40 || remaining < (s32)(10*fetch) ||
      (cheat_master_hook >= pc && cheat_master_hook < pc+40) ||
      (idle_loop_target_pc >= pc && idle_loop_target_pc < pc+40)) return false;
  if (fusion_code_cache.pc!=pc &&
     (cpu_readmap32(page,off+4) != (0xe0800bc0u | tmp << 12 | tmp) ||
      cpu_readmap32(page,off+8) != (0xe00c009au | tmp << 8) ||
      cpu_readmap32(page,off+12) != 0xe3ccc8ff ||
      cpu_readmap32(page,off+16) != 0xe08c6466 ||
      cpu_readmap32(page,off+20) != (0xe00c009bu | tmp << 8) ||
      cpu_readmap32(page,off+24) != 0xe3ccc8ff ||
      cpu_readmap32(page,off+28) != 0xe08c7467 ||
      cpu_readmap32(page,off+32) != (0xe0800004u | phase << 16 | phase << 12) ||
      cpu_readmap32(page,off+36) != (0xe1b00ba0u | tmp << 12 | phase))) return false;
  if ((pc>>24)==3 && fusion_code_cache.pc!=pc) {fusion_code_cache.pc=pc;fusion_code_cache.words=10;}
again:
  FUSION_HIT(0);
  u32 sample=reg[0]+(u32)((s32)(reg[phase]*reg[1]) >> 23);
  u32 left=(reg[10]*sample) & ~0x00ff0000u;
  u32 right=(reg[11]*sample) & ~0x00ff0000u;
  reg[6]=left+((reg[6] >> 8) | (reg[6] << 24));
  reg[7]=right+((reg[7] >> 8) | (reg[7] << 24));
  reg[12]=right;
  u32 position=reg[phase]+reg[4];
  reg[phase]=position; reg[tmp]=position >> 23;
  n=0; z=(position >> 23)==0; c=(position >> 22) & 1;
  reg[REG_PC]=pc+40;
  /* The ordinary dispatch epilogue charges the final instruction fetch. */
  remaining-=9*fetch;
  arm_fuse_sample_tail(pc,page,off,fetch,tmp,phase,remaining,n,z,c,v);
  if (reg[REG_PC]==pc && remaining>=(s32)(11*fetch)) {
    remaining-=fetch; /* Complete this iteration before starting another. */
    goto again;
  }
  return true;
#endif
}

/* Checked word-copy iterations, including palette conversion and OAM dirty
 * tracking. Only complete iterations fit before the next hardware event. */
static bool arm_fuse_copy(u32 opcode, u8 *page, s32 &remaining,
                         u32 &n, u32 &z, u32 &c, u32 &v)
{
#if defined(PXA_GBA_PROFILE) || defined(TRACE_INSTRUCTIONS)
  return false;
#else
#ifdef PXA_GBA_IDLE_TEST
  if (!gba_cpu_fusion_enabled) return false;
#endif
  if (opcode != 0xe153000c) return false;
  u32 pc=reg[REG_PC],off=pc&0x7fff;
  if (pc>=0x10000000 || off >= 0x8000-28 ||
      (cheat_master_hook>=pc && cheat_master_hook<pc+28) ||
      (idle_loop_target_pc>=pc && idle_loop_target_pc<pc+28)) return false;
  unsigned cr=pc>>24,fetch=ws_cyc_seq[cr][1];
  bool copied=false;
  for (;;) {
    u32 src=reg[3],dst=reg[1]+src,next=src+4;
    unsigned sr=src>>24,dr=dst>>24;
    if (((src|dst)&3) || src>reg[12] ||
        !(sr==2 || sr==3 || sr==6 || (sr>=8 && sr<=12)) ||
        !(dr==2 || dr==3 || dr==5 || dr==6 || dr==7)) break;
    u8 *source=memory_map_read[src>>15];
    if (!source) break;
    unsigned cost=7*fetch+ws_cyc_nseq[sr][1]+ws_cyc_nseq[dr][1];
    if (next!=reg[0]) cost+=ws_cyc_nseq[cr][1];
    if (remaining<(s32)cost) break;
    /* No accepted write can alter these instructions, even through a mirror.
     * Rechecking once per call suffices across its consecutive iterations. */
    if (!copied && (cpu_readmap32(page,off+4)!=0x95932000 ||
        cpu_readmap32(page,off+8)!=0x859f201c ||
        cpu_readmap32(page,off+12)!=0xe7812003 ||
        cpu_readmap32(page,off+16)!=0xe2833004 ||
        cpu_readmap32(page,off+20)!=0xe1530000 ||
        cpu_readmap32(page,off+24)!=0x1afffff8)) break;
    u8 *target;
    if (dr==2) target=ewram+(dst&0x3ffff);
    else if (dr==3) target=iwram+0x8000+(dst&0x7fff);
    else if (dr==5) target=(u8*)palette_ram+(dst&0x3ff);
    else if (dr==7) target=(u8*)oam_ram+(dst&0x3ff);
    else {u32 at=dst&0x1ffff;if(at>=0x18000)at-=0x8000;target=vram+at;}
    uintptr_t out=(uintptr_t)target, code=(uintptr_t)(page+off);
    if (out>=code && out<code+28) break;
    fusion_write_barrier();FUSION_HIT(2);
    u32 value=cpu_readmap32(source,src&0x7fff);
    address32(target,0)=eswap32(value);
    if (dr==7) reg[OAM_UPDATED]=1;
    if (dr==5) {
      u32 low=value&0xffff,high=value>>16;
      address16(palette_ram_converted,dst&0x3ff)=convert_palette(low);
      address16(palette_ram_converted,(dst&0x3ff)+2)=convert_palette(high);
    }
    reg[2]=value;reg[3]=next;
    u32 diff=next-reg[0];
    n=diff>>31;z=diff==0;c=next>=reg[0];
    v=((next^reg[0])&(next^diff))>>31;
    reg[REG_PC]=next==reg[0]?pc+28:pc;
    remaining-=cost;copied=true;
    if (next==reg[0]) break;
  }
  if (copied) remaining+=fetch; /* Dispatch epilogue charges the last fetch. */
  return copied;
#endif
}
#endif
