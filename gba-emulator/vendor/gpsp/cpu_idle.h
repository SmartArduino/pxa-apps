/* GPL-2.0-or-later. Coalesce only proven, side-effect-free polling loops.
 * State is local to one hardware-event slice: DMA, IRQs, timers, input, cheats,
 * writable code and savestate loads can never reuse a previous decision. */
#ifndef PXA_GPSP_CPU_IDLE_H
#define PXA_GPSP_CPU_IDLE_H

#ifdef PXA_GBA_IDLE_TEST
extern "C" { bool gba_cpu_idle_enabled = true; }
#endif

struct cpu_idle_state {
  u32 branch = ~0u, target = 0, flags = 0, regs[15];
  s32 remaining = 0;
  int eligible = 0;
  unsigned attempts = 0;
};

static bool idle_read_safe(u32 address, unsigned bytes)
{
  if (address & (bytes - 1)) return false;
  unsigned region = address >> 24;
  if (region == 2 || region == 3) return true;
  /* DISPSTAT/VCOUNT change only in update_gba(), which bounds this slice.
   * Other MMIO, ROM GPIO, EEPROM and open-bus reads are deliberately excluded. */
  return address >= 0x04000004 && address <= 0x04000008 - bytes;
}

static bool idle_loop_safe(u32 first, u32 last, bool thumb)
{
  if (first > last || last - first > (thumb ? 16u : 32u) ||
      (first >> 15) != (last >> 15) || first >= 0x10000000 ||
      (cheat_master_hook >= first && cheat_master_hook <= last)) return false;
  u8 *page = memory_map_read[first >> 15];
  if (!page) return false;
  u32 written = 0, bases = 0;
  unsigned reads = 0;
  for (u32 pc = first; pc < last; pc += thumb ? 2 : 4) {
    u32 op = thumb ? cpu_readmap16(page, pc & 0x7fff) : cpu_readmap32(page, pc & 0x7fff);
    unsigned rd, rn, bytes, offset;
    if (thumb) {
      if ((op & 0xf800) == 0x6800 || (op & 0xf800) == 0x7800 || (op & 0xf800) == 0x8800) {
        bytes = (op & 0xf800) == 0x6800 ? 4 : (op & 0xf800) == 0x8800 ? 2 : 1;
        rn = (op >> 3) & 7; rd = op & 7; offset = ((op >> 6) & 31) * bytes;
      } else {
        if (op < 0x2000) written |= 1u << (op & 7); /* shifts, ADD/SUB */
        else if (op < 0x4000) {
          if ((op & 0xf800) != 0x2800) written |= 1u << ((op >> 8) & 7);
        } else if ((op & 0xfc00) == 0x4000) {
          unsigned alu = (op >> 6) & 15;
          if (alu != 8 && alu != 10 && alu != 11) written |= 1u << (op & 7);
        } else return false; /* no stores, nested branches, PSR or mode changes */
        continue;
      }
    } else {
      if ((op >> 28) != 14) return false;
      if ((op & 0x0f300000) == 0x05100000) { /* LDR/B imm, pre-index, no writeback */
        bytes = (op & 0x00400000) ? 1 : 4;
        offset = op & 0xfff;
      } else if ((op & 0x0f7000f0) == 0x015000b0) { /* LDRH imm, no writeback */
        bytes = 2; offset = ((op >> 4) & 0xf0) | (op & 15);
      } else {
        if ((op & 0x0c000000) || (!(op & 0x02000000) && (op & 0x90) == 0x90)) return false;
        unsigned alu = (op >> 21) & 15;
        if (alu >= 8 && alu <= 11) { if (!(op & 0x00100000)) return false; }
        else { rd = (op >> 12) & 15; if (rd == 15) return false; written |= 1u << rd; }
        continue;
      }
      rn = (op >> 16) & 15; rd = (op >> 12) & 15;
      if (!(op & 0x00800000)) offset = 0u - offset;
    }
    if (rn == 15 || rd == 15 || !idle_read_safe(reg[rn] + offset, bytes)) return false;
    bases |= 1u << rn; written |= 1u << rd; ++reads;
  }
  /* Effective addresses must be invariant throughout every iteration, not
   * merely restored at the back edge. Otherwise a load could touch MMIO. */
  return reads && !(written & bases);
}

/* LDRH rd,[rb,#imm]; CMP rd,#imm; a taken back edge is idempotent after
 * one iteration when rb != rd and the read has no side effects. Prove this
 * directly instead of executing two more iterations and comparing 15 regs.
 * Revalidate each hardware-event slice; no address or code cache survives. */
static unsigned idle_thumb_poll_period(u32 first,u32 last,u32 flags)
{
  if(last-first!=4 || first>=0x10000000 || (first>>15)!=(last>>15) ||
      (cheat_master_hook>=first && cheat_master_hook<=last) ||
      (idle_loop_target_pc>=first && idle_loop_target_pc<=last)) return 0;
  u8 *page=memory_map_read[first>>15];
  if(!page)return 0;
  u32 load=cpu_readmap16(page,first&0x7fff),cmp=cpu_readmap16(page,(first+2)&0x7fff);
  unsigned rd=load&7,rb=(load>>3)&7;
  if((load&0xf800)!=0x8800 || (cmp&0xff00)!=(0x2800|(rd<<8)) || rd==rb)return 0;
  u32 address=reg[rb]+((load>>6)&31)*2;
  if(!idle_read_safe(address,2))return 0;
  /* A hardware event may have interrupted the preceding load/CMP. Confirm
   * the current read value and all flags, including an IRQ return or a jump
   * into the loop body, before treating this back edge as a fixed point. */
  page=memory_map_read[address>>15];
  if(!page || reg[rd]!=cpu_readmap16(page,address&0x7fff))return 0;
  u32 value=reg[rd],imm=cmp&255,diff=value-imm;
  u32 expected=((diff>>31)<<3)|((diff==0)<<2)|((value>=imm)<<1);
  if(flags!=expected)return 0;
  return 3*ws_cyc_seq[first>>24][0]+ws_cyc_nseq[address>>24][0]+ws_cyc_nseq[first>>24][0];
}

static void idle_poll_backedge(cpu_idle_state &state, u32 branch, bool thumb,
                               u32 flags, s32 &remaining)
{
#ifndef PXA_GBA_DISABLE_IDLE
#ifdef PXA_GBA_IDLE_TEST
  if (!gba_cpu_idle_enabled) return;
#endif
  u32 target = reg[REG_PC];
  if (remaining <= 0 || target >= branch || branch - target > (thumb ? 16u : 32u)) {
    if (state.branch == branch) state.branch = ~0u;
    return;
  }
  if (state.branch != branch || state.target != target) {
    state.branch = branch; state.target = target;
    unsigned period=thumb?idle_thumb_poll_period(target,branch,flags):0;
    if(period){
      if(remaining>(s32)period)remaining-=((remaining-1)/(s32)period)*(s32)period;
      state.eligible=-1;
      return;
    }
    state.eligible = idle_loop_safe(target, branch, thumb) ? 1 : -1;
    state.attempts = 0;
  }
  if (state.eligible < 0) return;
  /* A failed loop branch invalidates this state (including ARM conditions
   * handled by arm_next_instruction). Accepted bodies have no other exits:
   * these snapshots therefore describe consecutive complete iterations. */
  if (state.attempts && state.flags == flags && !memcmp(state.regs, reg, sizeof(state.regs))) {
    s32 period = state.remaining - remaining;
    /* Keep the final partial iteration on the interpreter. This preserves
     * its exact event overshoot, PC, NZCV and read values at update_gba(). */
    if (period > 0 && remaining > period) remaining -= ((remaining - 1) / period) * period;
    state.eligible = -1;
    return;
  }
  if (++state.attempts >= 4) {state.eligible = -1; return;}
  state.flags = flags; state.remaining = remaining;
  memcpy(state.regs, reg, sizeof(state.regs));
#endif
}
#endif
