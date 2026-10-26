/* gpSP instruction semantics extracted from cpu.cc; GPL-2.0-or-later. */
       /* Execute ARM instruction */
       using_instruction(arm);
       GBA_PROFILE_INSTRUCTION(0, reg[REG_PC]);
       check_pc_region();
       reg[REG_PC] &= ~0x03;
       opcode = cpu_readmap32(pc_address_block, (reg[REG_PC] & 0x7FFF));
       condition = opcode >> 28;

       switch(condition)
       {
          case 0x0:
             /* EQ */
             if(!z_flag)
                arm_next_instruction();
             break;
          case 0x1:
             /* NE      */
             if(z_flag)
                arm_next_instruction();
             break;
          case 0x2:
             /* CS       */
             if(!c_flag)
                arm_next_instruction();
             break;
          case 0x3:
             /* CC       */
             if(c_flag)
                arm_next_instruction();
             break;
          case 0x4:
             /* MI       */
             if(!n_flag)
                arm_next_instruction();
             break;

          case 0x5:
             /* PL       */
             if(n_flag)
                arm_next_instruction();
             break;

          case 0x6:
             /* VS       */
             if(!v_flag)
                arm_next_instruction();
             break;

          case 0x7:
             /* VC       */
             if(v_flag)
                arm_next_instruction();
             break;

          case 0x8:
             /* HI       */
             if((c_flag == 0) | z_flag)
                arm_next_instruction();
             break;

          case 0x9:
             /* LS       */
             if(c_flag & (z_flag ^ 1))
                arm_next_instruction();
             break;

          case 0xA:
             /* GE       */
             if(n_flag != v_flag)
                arm_next_instruction();
             break;

          case 0xB:
             /* LT       */
             if(n_flag == v_flag)
                arm_next_instruction();
             break;

          case 0xC:
             /* GT       */
             if(z_flag | (n_flag != v_flag))
                arm_next_instruction();
             break;

          case 0xD:
             /* LE       */
             if((z_flag == 0) & (n_flag == v_flag))
                arm_next_instruction();
             break;

          case 0xE:
             /* AL       */
             break;

          case 0xF:
             /* Reserved - treat as "never" */
             arm_next_instruction();
             break;
       }

       #ifdef TRACE_INSTRUCTIONS
       interp_trace_instruction(reg[REG_PC], 1);
       #endif

       switch((opcode >> 20) & 0xFF)
       {
          case 0x00:
             if((opcode & 0x90) == 0x90)
             {
                if(opcode & 0x20)
                {
                   /* STRH rd, [rn], -rm */
                   arm_access_memory(store, no_op, half_reg, u16, yes, - reg[rm]);
                }
                else
                {
                   /* MUL rd, rm, rs */
                   if (!arm_fuse_multiply(opcode, pc_address_block, cycles_remaining,
                                          n_flag, z_flag, c_flag, v_flag))
                     arm_multiply(no_op, no);
                }
             }
             else
             {
                /* AND rd, rn, reg_op */
                arm_data_proc(reg[rn] & reg_sh, reg);
             }
             break;

          case 0x01:
             if((opcode & 0x90) == 0x90)
             {
                switch((opcode >> 5) & 0x03)
                {
                   case 0:
                      /* MULS rd, rm, rs */
                      arm_multiply(no_op, yes);
                      break;

                   case 1:
                      /* LDRH rd, [rn], -rm */
                      arm_access_memory(load, no_op, half_reg, u16, yes, - reg[rm]);
                      break;

                   case 2:
                      /* LDRSB rd, [rn], -rm */
                      arm_access_memory(load, no_op, half_reg, s8, yes, - reg[rm]);
                      break;

                   case 3:
                      /* LDRSH rd, [rn], -rm */
                      arm_access_memory(load, no_op, half_reg, s16, yes, - reg[rm]);
                      break;
                }
             }
             else
             {
                /* ANDS rd, rn, reg_op */
                arm_data_proc_logic_flags(reg[rn] & reg_sh, reg);
             }
             break;

          case 0x02:
             if((opcode & 0x90) == 0x90)
             {
                if(opcode & 0x20)
                {
                   /* STRH rd, [rn], -rm */
                   arm_access_memory(store, no_op, half_reg, u16, yes, - reg[rm]);
                }
                else
                {
                   /* MLA rd, rm, rs, rn */
                   arm_multiply(+ reg[rn], no);
                }
             }
             else
             {
                /* EOR rd, rn, reg_op */
                arm_data_proc(reg[rn] ^ reg_sh, reg);
             }
             break;

          case 0x03:
             if((opcode & 0x90) == 0x90)
             {
                switch((opcode >> 5) & 0x03)
                {
                   case 0:
                      /* MLAS rd, rm, rs, rn */
                      arm_multiply(+ reg[rn], yes);
                      break;

                   case 1:
                      /* LDRH rd, [rn], -rm */
                      arm_access_memory(load, no_op, half_reg, u16, yes, - reg[rm]);
                      break;

                   case 2:
                      /* LDRSB rd, [rn], -rm */
                      arm_access_memory(load, no_op, half_reg, s8, yes, - reg[rm]);
                      break;

                   case 3:
                      /* LDRSH rd, [rn], -rm */
                      arm_access_memory(load, no_op, half_reg, s16, yes, - reg[rm]);
                      break;
                }
             }
             else
             {
                /* EORS rd, rn, reg_op */
                arm_data_proc_logic_flags(reg[rn] ^ reg_sh, reg);
             }
             break;

          case 0x04:
             if((opcode & 0x90) == 0x90)
             {
                /* STRH rd, [rn], -imm */
                arm_access_memory(store, no_op, half_imm, u16, yes, - offset);
             }
             else
             {
                /* SUB rd, rn, reg_op */
                arm_data_proc(reg[rn] - reg_sh, reg);
             }
             break;

          case 0x05:
             if((opcode & 0x90) == 0x90)
             {
                switch((opcode >> 5) & 0x03)
                {
                   case 1:
                      /* LDRH rd, [rn], -imm */
                      arm_access_memory(load, no_op, half_imm, u16, yes, - offset);
                      break;

                   case 2:
                      /* LDRSB rd, [rn], -imm */
                      arm_access_memory(load, no_op, half_imm, s8, yes, - offset);
                      break;

                   case 3:
                      /* LDRSH rd, [rn], -imm */
                      arm_access_memory(load, no_op, half_imm, s16, yes, - offset);
                      break;
                }
             }
             else
             {
                /* SUBS rd, rn, reg_op */
                arm_data_proc_sub_flags(reg[rn], reg_sh, 1, reg);
             }
             break;

          case 0x06:
             if((opcode & 0x90) == 0x90)
             {
                /* STRH rd, [rn], -imm */
                arm_access_memory(store, no_op, half_imm, u16, yes, - offset);
             }
             else
             {
                /* RSB rd, rn, reg_op */
                arm_data_proc(reg_sh - reg[rn], reg);
             }
             break;

          case 0x07:
             if((opcode & 0x90) == 0x90)
             {
                switch((opcode >> 5) & 0x03)
                {
                   case 1:
                      /* LDRH rd, [rn], -imm */
                      arm_access_memory(load, no_op, half_imm, u16, yes, - offset);
                      break;

                   case 2:
                      /* LDRSB rd, [rn], -imm */
                      arm_access_memory(load, no_op, half_imm, s8, yes, - offset);
                      break;

                   case 3:
                      /* LDRSH rd, [rn], -imm */
                      arm_access_memory(load, no_op, half_imm, s16, yes, - offset);
                      break;
                }
             }
             else
             {
                /* RSBS rd, rn, reg_op */
                arm_data_proc_sub_flags(reg_sh, reg[rn], 1, reg);
             }
             break;

          case 0x08:
             if((opcode & 0x90) == 0x90)
             {
                if(opcode & 0x20)
                {
                   /* STRH rd, [rn], +rm */
                   arm_access_memory(store, no_op, half_reg, u16, yes, + reg[rm]);
                }
                else
                {
                   /* UMULL rd, rm, rs */
                   arm_multiply_long(no_op, no, u);
                }
             }
             else
             {
                /* ADD rd, rn, reg_op */
                arm_data_proc(reg[rn] + reg_sh, reg);
             }
             break;

          case 0x09:
             if((opcode & 0x90) == 0x90)
             {
                switch((opcode >> 5) & 0x03)
                {
                   case 0:
                      /* UMULLS rdlo, rdhi, rm, rs */
                      arm_multiply_long(no_op, yes, u);
                      break;

                   case 1:
                      /* LDRH rd, [rn], +rm */
                      arm_access_memory(load, no_op, half_reg, u16, yes, + reg[rm]);
                      break;

                   case 2:
                      /* LDRSB rd, [rn], +rm */
                      arm_access_memory(load, no_op, half_reg, s8, yes, + reg[rm]);
                      break;

                   case 3:
                      /* LDRSH rd, [rn], +rm */
                      arm_access_memory(load, no_op, half_reg, s16, yes, + reg[rm]);
                      break;
                }
             }
             else
             {
                /* ADDS rd, rn, reg_op */
                arm_data_proc_add_flags(reg[rn], reg_sh, 0, reg);
             }
             break;

          case 0x0A:
             if((opcode & 0x90) == 0x90)
             {
                if(opcode & 0x20)
                {
                   /* STRH rd, [rn], +rm */
                   arm_access_memory(store, no_op, half_reg, u16, yes, + reg[rm]);
                }
                else
                {
                   /* UMLAL rd, rm, rs */
                   arm_multiply_long(arm_multiply_long_addop(u), no, u);
                }
             }
             else
             {
                /* ADC rd, rn, reg_op */
                arm_data_proc(reg[rn] + reg_sh + c_flag, reg);
             }
             break;

          case 0x0B:
             if((opcode & 0x90) == 0x90)
             {
                switch((opcode >> 5) & 0x03)
                {
                   case 0:
                      /* UMLALS rdlo, rdhi, rm, rs */
                      arm_multiply_long(arm_multiply_long_addop(u), yes, u);
                      break;

                   case 1:
                      /* LDRH rd, [rn], +rm */
                      arm_access_memory(load, no_op, half_reg, u16, yes, + reg[rm]);
                      break;

                   case 2:
                      /* LDRSB rd, [rn], +rm */
                      arm_access_memory(load, no_op, half_reg, s8, yes, + reg[rm]);
                      break;

                   case 3:
                      /* LDRSH rd, [rn], +rm */
                      arm_access_memory(load, no_op, half_reg, s16, yes, + reg[rm]);
                      break;
                }
             }
             else
             {
                /* ADCS rd, rn, reg_op */
                arm_data_proc_add_flags(reg[rn], reg_sh, c_flag, reg);
             }
             break;

          case 0x0C:
             if((opcode & 0x90) == 0x90)
             {
                if(opcode & 0x20)
                {
                   /* STRH rd, [rn], +imm */
                   arm_access_memory(store, no_op, half_imm, u16, yes, + offset);
                }
                else
                {
                   /* SMULL rd, rm, rs */
                   arm_multiply_long(no_op, no, s);
                }
             }
             else
             {
                /* SBC rd, rn, reg_op */
                arm_data_proc(reg[rn] - (reg_sh + (c_flag ^ 1)), reg);
             }
             break;

          case 0x0D:
             if((opcode & 0x90) == 0x90)
             {
                switch((opcode >> 5) & 0x03)
                {
                   case 0:
                      /* SMULLS rdlo, rdhi, rm, rs */
                      arm_multiply_long(no_op, yes, s);
                      break;

                   case 1:
                      /* LDRH rd, [rn], +imm */
                      arm_access_memory(load, no_op, half_imm, u16, yes, + offset);
                      break;

                   case 2:
                      /* LDRSB rd, [rn], +imm */
                      if (!arm_fuse_direct_sample(opcode, pc_address_block, cycles_remaining,
                                                   n_flag, z_flag, c_flag, v_flag))
                        arm_access_memory(load, no_op, half_imm, s8, yes, + offset);
                      break;

                   case 3:
                      /* LDRSH rd, [rn], +imm */
                      arm_access_memory(load, no_op, half_imm, s16, yes, + offset);
                      break;
                }
             }
             else
             {
                /* SBCS rd, rn, reg_op */
                arm_data_proc_sub_flags(reg[rn], reg_sh, c_flag, reg);
             }
             break;

          case 0x0E:
             if((opcode & 0x90) == 0x90)
             {
                if(opcode & 0x20)
                {
                   /* STRH rd, [rn], +imm */
                   arm_access_memory(store, no_op, half_imm, u16, yes, + offset);
                }
                else
                {
                   /* SMLAL rd, rm, rs */
                   arm_multiply_long(arm_multiply_long_addop(s), no, s);
                }
             }
             else
             {
                /* RSC rd, rn, reg_op */
                arm_data_proc(reg_sh - reg[rn] + c_flag - 1, reg);
             }
             break;

          case 0x0F:
             if((opcode & 0x90) == 0x90)
             {
                switch((opcode >> 5) & 0x03)
                {
                   case 0:
                      /* SMLALS rdlo, rdhi, rm, rs */
                      arm_multiply_long(arm_multiply_long_addop(s), yes, s);
                      break;

                   case 1:
                      /* LDRH rd, [rn], +imm */
                      arm_access_memory(load, no_op, half_imm, u16, yes, + offset);
                      break;

                   case 2:
                      /* LDRSB rd, [rn], +imm */
                      arm_access_memory(load, no_op, half_imm, s8, yes, + offset);
                      break;

                   case 3:
                      /* LDRSH rd, [rn], +imm */
                      arm_access_memory(load, no_op, half_imm, s16, yes, + offset);
                      break;
                }
             }
             else
             {
                /* RSCS rd, rn, reg_op */
                arm_data_proc_sub_flags(reg_sh, reg[rn], c_flag, reg);
             }
             break;

          case 0x10:
             if((opcode & 0x90) == 0x90)
             {
                if(opcode & 0x20)
                {
                   /* STRH rd, [rn - rm] */
                   arm_access_memory(store, - reg[rm], half_reg, u16, no, no_op);
                }
                else
                {
                   /* SWP rd, rm, [rn] */
                   arm_swap(u32);
                }
             }
             else
             {
                /* MRS rd, cpsr */
                arm_psr(reg, read, reg[REG_CPSR]);
             }
             break;

          case 0x11:
             if((opcode & 0x90) == 0x90)
             {
                switch((opcode >> 5) & 0x03)
                {
                   case 1:
                      /* LDRH rd, [rn - rm] */
                      arm_access_memory(load, - reg[rm], half_reg, u16, no, no_op);
                      break;

                   case 2:
                      /* LDRSB rd, [rn - rm] */
                      arm_access_memory(load, - reg[rm], half_reg, s8, no, no_op);
                      break;

                   case 3:
                      /* LDRSH rd, [rn - rm] */
                      arm_access_memory(load, - reg[rm], half_reg, s16, no, no_op);
                      break;
                }
             }
             else
             {
                /* TST rd, rn, reg_op */
                arm_data_proc_test_logic(reg[rn] & reg_sh, reg);
             }
             break;

          case 0x12:
             if((opcode & 0x90) == 0x90)
             {
                /* STRH rd, [rn - rm]! */
                arm_access_memory(store, - reg[rm], half_reg, u16, yes, no_op);
             }
             else
             {
                if(opcode & 0x10)
                {
                   /* BX rn */
                   arm_decode_branchx(opcode);
                   arm_pc_offset(8);
                   u32 src = reg[rn];
                   if(src & 0x01)
                   {
                      reg[REG_PC] = src - 1;
                      reg[REG_CPSR] |= 0x20;
                      goto thumb_loop;
                   }
                   else
                   {
                      reg[REG_PC] = src;
                   }
                   cycles_remaining -= ws_cyc_nseq[reg[REG_PC] >> 24][1];
                }
                else
                {
                   /* MSR cpsr, rm */
                   arm_psr(reg, store, cpsr);
                }
             }
             break;

          case 0x13:
             if((opcode & 0x90) == 0x90)
             {
                switch((opcode >> 5) & 0x03)
                {
                   case 1:
                      /* LDRH rd, [rn - rm]! */
                      arm_access_memory(load, - reg[rm], half_reg, u16, yes, no_op);
                      break;

                   case 2:
                      /* LDRSB rd, [rn - rm]! */
                      arm_access_memory(load, - reg[rm], half_reg, s8, yes, no_op);
                      break;

                   case 3:
                      /* LDRSH rd, [rn - rm]! */
                      arm_access_memory(load, - reg[rm], half_reg, s16, yes, no_op);
                      break;
                }
             }
             else
             {
                /* TEQ rd, rn, reg_op */
                arm_data_proc_test_logic(reg[rn] ^ reg_sh, reg);
             }
             break;

          case 0x14:
             if((opcode & 0x90) == 0x90)
             {
                if(opcode & 0x20)
                {
                   /* STRH rd, [rn - imm] */
                   arm_access_memory(store, - offset, half_imm, u16, no, no_op);
                }
                else
                {
                   /* SWPB rd, rm, [rn] */
                   arm_swap(u8);
                }
             }
             else
             {
                /* MRS rd, spsr */
                arm_psr(reg, read, REG_SPSR(reg[CPU_MODE]));
             }
             break;

          case 0x15:
             if((opcode & 0x90) == 0x90)
             {
                switch((opcode >> 5) & 0x03)
                {
                   case 1:
                      /* LDRH rd, [rn - imm] */
                      arm_access_memory(load, - offset, half_imm, u16, no, no_op);
                      break;

                   case 2:
                      /* LDRSB rd, [rn - imm] */
                      arm_access_memory(load, - offset, half_imm, s8, no, no_op);
                      break;

                   case 3:
                      /* LDRSH rd, [rn - imm] */
                      arm_access_memory(load, - offset, half_imm, s16, no, no_op);
                      break;
                }
             }
             else
             {
                /* CMP rn, reg_op */
                if (!arm_fuse_copy(opcode, pc_address_block, cycles_remaining,
                                   n_flag, z_flag, c_flag, v_flag))
                  arm_data_proc_test_sub(reg[rn], reg_sh, reg);
             }
             break;

          case 0x16:
             if((opcode & 0x90) == 0x90)
             {
                /* STRH rd, [rn - imm]! */
                arm_access_memory(store, - offset, half_imm, u16, yes, no_op);
             }
             else
             {
                /* MSR spsr, rm */
                arm_psr(reg, store, spsr);
             }
             break;

          case 0x17:
             if((opcode & 0x90) == 0x90)
             {
                switch((opcode >> 5) & 0x03)
                {
                   case 1:
                      /* LDRH rd, [rn - imm]! */
                      arm_access_memory(load, - offset, half_imm, u16, yes, no_op);
                      break;

                   case 2:
                      /* LDRSB rd, [rn - imm]! */
                      arm_access_memory(load, - offset, half_imm, s8, yes, no_op);
                      break;

                   case 3:
                      /* LDRSH rd, [rn - imm]! */
                      arm_access_memory(load, - offset, half_imm, s16, yes, no_op);
                      break;
                }
             }
             else
             {
                /* CMN rd, rn, reg_op */
                arm_data_proc_test_add(reg[rn], reg_sh, reg);
             }
             break;

          case 0x18:
             if((opcode & 0x90) == 0x90)
             {
                /* STRH rd, [rn + rm] */
                arm_access_memory(store, + reg[rm], half_reg, u16, no, no_op);
             }
             else
             {
                /* ORR rd, rn, reg_op */
                arm_data_proc(reg[rn] | reg_sh, reg);
             }
             break;

          case 0x19:
             if((opcode & 0x90) == 0x90)
             {
                switch((opcode >> 5) & 0x03)
                {
                   case 1:
                      /* LDRH rd, [rn + rm] */
                      arm_access_memory(load, + reg[rm], half_reg, u16, no, no_op);
                      break;

                   case 2:
                      /* LDRSB rd, [rn + rm] */
                      arm_access_memory(load, + reg[rm], half_reg, s8, no, no_op);
                      break;

                   case 3:
                      /* LDRSH rd, [rn + rm] */
                      arm_access_memory(load, + reg[rm], half_reg, s16, no, no_op);
                      break;
                }
             }
             else
             {
                /* ORRS rd, rn, reg_op */
                arm_data_proc_logic_flags(reg[rn] | reg_sh, reg);
             }
             break;

          case 0x1A:
             if((opcode & 0x90) == 0x90)
             {
                /* STRH rd, [rn + rm]! */
                arm_access_memory(store, + reg[rm], half_reg, u16, yes, no_op);
             }
             else
             {
                /* MOV rd, reg_op */
                arm_data_proc(reg_sh, reg);
             }
             break;

          case 0x1B:
             if((opcode & 0x90) == 0x90)
             {
                switch((opcode >> 5) & 0x03)
                {
                   case 1:
                      /* LDRH rd, [rn + rm]! */
                      arm_access_memory(load, + reg[rm], half_reg, u16, yes, no_op);
                      break;

                   case 2:
                      /* LDRSB rd, [rn + rm]! */
                      arm_access_memory(load, + reg[rm], half_reg, s8, yes, no_op);
                      break;

                   case 3:
                      /* LDRSH rd, [rn + rm]! */
                      arm_access_memory(load, + reg[rm], half_reg, s16, yes, no_op);
                      break;
                }
             }
             else
             {
                /* MOVS rd, reg_op */
                arm_data_proc_logic_flags(reg_sh, reg);
             }
             break;

          case 0x1C:
             if((opcode & 0x90) == 0x90)
             {
                /* STRH rd, [rn + imm] */
                arm_access_memory(store, + offset, half_imm, u16, no, no_op);
             }
             else
             {
                /* BIC rd, rn, reg_op */
                arm_data_proc(reg[rn] & (~reg_sh), reg);
             }
             break;

          case 0x1D:
             if((opcode & 0x90) == 0x90)
             {
                switch((opcode >> 5) & 0x03)
                {
                   case 1:
                      /* LDRH rd, [rn + imm] */
                      arm_access_memory(load, + offset, half_imm, u16, no, no_op);
                      break;

                   case 2:
                      /* LDRSB rd, [rn + imm] */
                      arm_access_memory(load, + offset, half_imm, s8, no, no_op);
                      break;

                   case 3:
                      /* LDRSH rd, [rn + imm] */
                      arm_access_memory(load, + offset, half_imm, s16, no, no_op);
                      break;
                }
             }
             else
             {
                /* BICS rd, rn, reg_op */
                arm_data_proc_logic_flags(reg[rn] & (~reg_sh), reg);
             }
             break;

          case 0x1E:
             if((opcode & 0x90) == 0x90)
             {
                /* STRH rd, [rn + imm]! */
                arm_access_memory(store, + offset, half_imm, u16, yes, no_op);
             }
             else
             {
                /* MVN rd, reg_op */
                arm_data_proc(~reg_sh, reg);
             }
             break;

          case 0x1F:
             if((opcode & 0x90) == 0x90)
             {
                switch((opcode >> 5) & 0x03)
                {
                   case 1:
                      /* LDRH rd, [rn + imm]! */
                      arm_access_memory(load, + offset, half_imm, u16, yes, no_op);
                      break;

                   case 2:
                      /* LDRSB rd, [rn + imm]! */
                      arm_access_memory(load, + offset, half_imm, s8, yes, no_op);
                      break;

                   case 3:
                      /* LDRSH rd, [rn + imm]! */
                      arm_access_memory(load, + offset, half_imm, s16, yes, no_op);
                      break;
                }
             }
             else
             {
                /* MVNS rd, rn, reg_op */
                arm_data_proc_logic_flags(~reg_sh, reg);
             }
             break;

          case 0x20:
             /* AND rd, rn, imm */
             arm_data_proc(reg[rn] & imm, imm);
             break;

          case 0x21:
             /* ANDS rd, rn, imm */
             arm_data_proc_logic_flags(reg[rn] & imm, imm);
             break;

          case 0x22:
             /* EOR rd, rn, imm */
             arm_data_proc(reg[rn] ^ imm, imm);
             break;

          case 0x23:
             /* EORS rd, rn, imm */
             arm_data_proc_logic_flags(reg[rn] ^ imm, imm);
             break;

          case 0x24:
             /* SUB rd, rn, imm */
             arm_data_proc(reg[rn] - imm, imm);
             break;

          case 0x25:
             /* SUBS rd, rn, imm */
             arm_data_proc_sub_flags(reg[rn], imm, 1, imm);
             break;

          case 0x26:
             /* RSB rd, rn, imm */
             arm_data_proc(imm - reg[rn], imm);
             break;

          case 0x27:
             /* RSBS rd, rn, imm */
             arm_data_proc_sub_flags(imm, reg[rn], 1, imm);
             break;

          case 0x28:
             /* ADD rd, rn, imm */
             arm_data_proc(reg[rn] + imm, imm);
             break;

          case 0x29:
             /* ADDS rd, rn, imm */
             arm_data_proc_add_flags(reg[rn], imm, 0, imm);
             break;

          case 0x2A:
             /* ADC rd, rn, imm */
             arm_data_proc(reg[rn] + imm + c_flag, imm);
             break;

          case 0x2B:
             /* ADCS rd, rn, imm */
             arm_data_proc_add_flags(reg[rn], imm, c_flag, imm);
             break;

          case 0x2C:
             /* SBC rd, rn, imm */
             arm_data_proc(reg[rn] - imm + c_flag - 1, imm);
             break;

          case 0x2D:
             /* SBCS rd, rn, imm */
             arm_data_proc_sub_flags(reg[rn], imm, c_flag, imm);
             break;

          case 0x2E:
             /* RSC rd, rn, imm */
             arm_data_proc(imm - reg[rn] + c_flag - 1, imm);
             break;

          case 0x2F:
             /* RSCS rd, rn, imm */
             arm_data_proc_sub_flags(imm, reg[rn], c_flag, imm);
             break;

          case 0x30:
          case 0x31:
             /* TST rn, imm */
             arm_data_proc_test_logic(reg[rn] & imm, imm);
             break;

          case 0x32:
             /* MSR cpsr, imm */
             arm_psr(imm, store, cpsr);
             break;

          case 0x33:
             /* TEQ rn, imm */
             arm_data_proc_test_logic(reg[rn] ^ imm, imm);
             break;

          case 0x34:
          case 0x35:
             /* CMP rn, imm */
             arm_data_proc_test_sub(reg[rn], imm, imm);
             break;

          case 0x36:
             /* MSR spsr, imm */
             arm_psr(imm, store, spsr);
             break;

          case 0x37:
             /* CMN rn, imm */
             arm_data_proc_test_add(reg[rn], imm, imm);
             break;

          case 0x38:
             /* ORR rd, rn, imm */
             arm_data_proc(reg[rn] | imm, imm);
             break;

          case 0x39:
             /* ORRS rd, rn, imm */
             arm_data_proc_logic_flags(reg[rn] | imm, imm);
             break;

          case 0x3A:
             /* MOV rd, imm */
             arm_data_proc(imm, imm);
             break;

          case 0x3B:
             /* MOVS rd, imm */
             arm_data_proc_logic_flags(imm, imm);
             break;

          case 0x3C:
             /* BIC rd, rn, imm */
             arm_data_proc(reg[rn] & (~imm), imm);
             break;

          case 0x3D:
             /* BICS rd, rn, imm */
             arm_data_proc_logic_flags(reg[rn] & (~imm), imm);
             break;

          case 0x3E:
             /* MVN rd, imm */
             arm_data_proc(~imm, imm);
             break;

          case 0x3F:
             /* MVNS rd, imm */
             arm_data_proc_logic_flags(~imm, imm);
             break;

          case 0x40:
             /* STR rd, [rn], -imm */
             arm_access_memory(store, no_op, imm, u32, yes, - offset);
             break;

          case 0x41:
             /* LDR rd, [rn], -imm */
             arm_access_memory(load, no_op, imm, u32, yes, - offset);
             break;

          case 0x42:
             /* STRT rd, [rn], -imm */
             arm_access_memory(store, no_op, imm, u32, yes, - offset);
             break;

          case 0x43:
             /* LDRT rd, [rn], -imm */
             arm_access_memory(load, no_op, imm, u32, yes, - offset);
             break;

          case 0x44:
             /* STRB rd, [rn], -imm */
             arm_access_memory(store, no_op, imm, u8, yes, - offset);
             break;

          case 0x45:
             /* LDRB rd, [rn], -imm */
             arm_access_memory(load, no_op, imm, u8, yes, - offset);
             break;

          case 0x46:
             /* STRBT rd, [rn], -imm */
             arm_access_memory(store, no_op, imm, u8, yes, - offset);
             break;

          case 0x47:
             /* LDRBT rd, [rn], -imm */
             arm_access_memory(load, no_op, imm, u8, yes, - offset);
             break;

          case 0x48:
             /* STR rd, [rn], +imm */
             arm_access_memory(store, no_op, imm, u32, yes, + offset);
             break;

          case 0x49:
             /* LDR rd, [rn], +imm */
             arm_access_memory(load, no_op, imm, u32, yes, + offset);
             break;

          case 0x4A:
             /* STRT rd, [rn], +imm */
             arm_access_memory(store, no_op, imm, u32, yes, + offset);
             break;

          case 0x4B:
             /* LDRT rd, [rn], +imm */
             arm_access_memory(load, no_op, imm, u32, yes, + offset);
             break;

          case 0x4C:
             /* STRB rd, [rn], +imm */
             arm_access_memory(store, no_op, imm, u8, yes, + offset);
             break;

          case 0x4D:
             /* LDRB rd, [rn], +imm */
             arm_access_memory(load, no_op, imm, u8, yes, + offset);
             break;

          case 0x4E:
             /* STRBT rd, [rn], +imm */
             arm_access_memory(store, no_op, imm, u8, yes, + offset);
             break;

          case 0x4F:
             /* LDRBT rd, [rn], +imm */
             arm_access_memory(load, no_op, imm, u8, yes, + offset);
             break;

          case 0x50:
             /* STR rd, [rn - imm] */
             arm_access_memory(store, - offset, imm, u32, no, no_op);
             break;

          case 0x51:
             /* LDR rd, [rn - imm] */
             arm_access_memory(load, - offset, imm, u32, no, no_op);
             break;

          case 0x52:
             /* STR rd, [rn - imm]! */
             arm_access_memory(store, - offset, imm, u32, yes, no_op);
             break;

          case 0x53:
             /* LDR rd, [rn - imm]! */
             arm_access_memory(load, - offset, imm, u32, yes, no_op);
             break;

          case 0x54:
             /* STRB rd, [rn - imm] */
             arm_access_memory(store, - offset, imm, u8, no, no_op);
             break;

          case 0x55:
             /* LDRB rd, [rn - imm] */
             arm_access_memory(load, - offset, imm, u8, no, no_op);
             break;

          case 0x56:
             /* STRB rd, [rn - imm]! */
             arm_access_memory(store, - offset, imm, u8, yes, no_op);
             break;

          case 0x57:
             /* LDRB rd, [rn - imm]! */
             arm_access_memory(load, - offset, imm, u8, yes, no_op);
             break;

          case 0x58:
             /* STR rd, [rn + imm] */
             arm_access_memory(store, + offset, imm, u32, no, no_op);
             break;

          case 0x59:
             /* LDR rd, [rn + imm] */
             arm_access_memory(load, + offset, imm, u32, no, no_op);
             break;

          case 0x5A:
             /* STR rd, [rn + imm]! */
             arm_access_memory(store, + offset, imm, u32, yes, no_op);
             break;

          case 0x5B:
             /* LDR rd, [rn + imm]! */
             arm_access_memory(load, + offset, imm, u32, yes, no_op);
             break;

          case 0x5C:
             /* STRB rd, [rn + imm] */
             arm_access_memory(store, + offset, imm, u8, no, no_op);
             break;

          case 0x5D:
             /* LDRB rd, [rn + imm] */
             arm_access_memory(load, + offset, imm, u8, no, no_op);
             break;

          case 0x5E:
             /* STRB rd, [rn + imm]! */
             arm_access_memory(store, + offset, imm, u8, yes, no_op);
             break;

          case 0x5F:
             /* LDRBT rd, [rn + imm]! */
             arm_access_memory(load, + offset, imm, u8, yes, no_op);
             break;

          case 0x60:
             /* STR rd, [rn], -reg_op */
             arm_access_memory(store, no_op, reg, u32, yes, - reg_offset);
             break;

          case 0x61:
             /* LDR rd, [rn], -reg_op */
             arm_access_memory(load, no_op, reg, u32, yes, - reg_offset);
             break;

          case 0x62:
             /* STRT rd, [rn], -reg_op */
             arm_access_memory(store, no_op, reg, u32, yes, - reg_offset);
             break;

          case 0x63:
             /* LDRT rd, [rn], -reg_op */
             arm_access_memory(load, no_op, reg, u32, yes, - reg_offset);
             break;

          case 0x64:
             /* STRB rd, [rn], -reg_op */
             arm_access_memory(store, no_op, reg, u8, yes, - reg_offset);
             break;

          case 0x65:
             /* LDRB rd, [rn], -reg_op */
             arm_access_memory(load, no_op, reg, u8, yes, - reg_offset);
             break;

          case 0x66:
             /* STRBT rd, [rn], -reg_op */
             arm_access_memory(store, no_op, reg, u8, yes, - reg_offset);
             break;

          case 0x67:
             /* LDRBT rd, [rn], -reg_op */
             arm_access_memory(load, no_op, reg, u8, yes, - reg_offset);
             break;

          case 0x68:
             /* STR rd, [rn], +reg_op */
             arm_access_memory(store, no_op, reg, u32, yes, + reg_offset);
             break;

          case 0x69:
             /* LDR rd, [rn], +reg_op */
             arm_access_memory(load, no_op, reg, u32, yes, + reg_offset);
             break;

          case 0x6A:
             /* STRT rd, [rn], +reg_op */
             arm_access_memory(store, no_op, reg, u32, yes, + reg_offset);
             break;

          case 0x6B:
             /* LDRT rd, [rn], +reg_op */
             arm_access_memory(load, no_op, reg, u32, yes, + reg_offset);
             break;

          case 0x6C:
             /* STRB rd, [rn], +reg_op */
             arm_access_memory(store, no_op, reg, u8, yes, + reg_offset);
             break;

          case 0x6D:
             /* LDRB rd, [rn], +reg_op */
             arm_access_memory(load, no_op, reg, u8, yes, + reg_offset);
             break;

          case 0x6E:
             /* STRBT rd, [rn], +reg_op */
             arm_access_memory(store, no_op, reg, u8, yes, + reg_offset);
             break;

          case 0x6F:
             /* LDRBT rd, [rn], +reg_op */
             arm_access_memory(load, no_op, reg, u8, yes, + reg_offset);
             break;

          case 0x70:
             /* STR rd, [rn - reg_op] */
             arm_access_memory(store, - reg_offset, reg, u32, no, no_op);
             break;

          case 0x71:
             /* LDR rd, [rn - reg_op] */
             arm_access_memory(load, - reg_offset, reg, u32, no, no_op);
             break;

          case 0x72:
             /* STR rd, [rn - reg_op]! */
             arm_access_memory(store, - reg_offset, reg, u32, yes, no_op);
             break;

          case 0x73:
             /* LDR rd, [rn - reg_op]! */
             arm_access_memory(load, - reg_offset, reg, u32, yes, no_op);
             break;

          case 0x74:
             /* STRB rd, [rn - reg_op] */
             arm_access_memory(store, - reg_offset, reg, u8, no, no_op);
             break;

          case 0x75:
             /* LDRB rd, [rn - reg_op] */
             arm_access_memory(load, - reg_offset, reg, u8, no, no_op);
             break;

          case 0x76:
             /* STRB rd, [rn - reg_op]! */
             arm_access_memory(store, - reg_offset, reg, u8, yes, no_op);
             break;

          case 0x77:
             /* LDRB rd, [rn - reg_op]! */
             arm_access_memory(load, - reg_offset, reg, u8, yes, no_op);
             break;

          case 0x78:
             /* STR rd, [rn + reg_op] */
             arm_access_memory(store, + reg_offset, reg, u32, no, no_op);
             break;

          case 0x79:
             /* LDR rd, [rn + reg_op] */
             arm_access_memory(load, + reg_offset, reg, u32, no, no_op);
             break;

          case 0x7A:
             /* STR rd, [rn + reg_op]! */
             arm_access_memory(store, + reg_offset, reg, u32, yes, no_op);
             break;

          case 0x7B:
             /* LDR rd, [rn + reg_op]! */
             arm_access_memory(load, + reg_offset, reg, u32, yes, no_op);
             break;

          case 0x7C:
             /* STRB rd, [rn + reg_op] */
             arm_access_memory(store, + reg_offset, reg, u8, no, no_op);
             break;

          case 0x7D:
             /* LDRB rd, [rn + reg_op] */
             arm_access_memory(load, + reg_offset, reg, u8, no, no_op);
             break;

          case 0x7E:
             /* STRB rd, [rn + reg_op]! */
             arm_access_memory(store, + reg_offset, reg, u8, yes, no_op);
             break;

          case 0x7F:
             /* LDRBT rd, [rn + reg_op]! */
             arm_access_memory(load, + reg_offset, reg, u8, yes, no_op);
             break;

          /* STM instructions: STMDA, STMIA, STMDB, STMIB */

          case 0x80:   /* STMDA rn, rlist */
            collapse_flags();
            cpu_alert |= exec_arm_block_mem<AccStore, false, false, AddrPostDec>(
              (opcode >> 16) & 0x0F, opcode & 0xFFFF, cycles_remaining);
            break;
          case 0x88:   /* STMIA rn, rlist */
            collapse_flags();
            cpu_alert |= exec_arm_block_mem<AccStore, false, false, AddrPostInc>(
              (opcode >> 16) & 0x0F, opcode & 0xFFFF, cycles_remaining);
            break;
          case 0x90:   /* STMDB rn, rlist */
            collapse_flags();
            cpu_alert |= exec_arm_block_mem<AccStore, false, false, AddrPreDec>(
              (opcode >> 16) & 0x0F, opcode & 0xFFFF, cycles_remaining);
            break;
          case 0x98:   /* STMIB rn, rlist */
            collapse_flags();
            cpu_alert |= exec_arm_block_mem<AccStore, false, false, AddrPreInc>(
              (opcode >> 16) & 0x0F, opcode & 0xFFFF, cycles_remaining);
            break;

          case 0x82:   /* STMDA rn!, rlist */
            collapse_flags();
            cpu_alert |= exec_arm_block_mem<AccStore, true, false, AddrPostDec>(
              (opcode >> 16) & 0x0F, opcode & 0xFFFF, cycles_remaining);
            break;
          case 0x8A:   /* STMIA rn!, rlist */
            collapse_flags();
            cpu_alert |= exec_arm_block_mem<AccStore, true, false, AddrPostInc>(
              (opcode >> 16) & 0x0F, opcode & 0xFFFF, cycles_remaining);
            break;
          case 0x92:   /* STMDB rn!, rlist */
            collapse_flags();
            cpu_alert |= exec_arm_block_mem<AccStore, true, false, AddrPreDec>(
              (opcode >> 16) & 0x0F, opcode & 0xFFFF, cycles_remaining);
            break;
          case 0x9A:   /* STMIB rn!, rlist */
            collapse_flags();
            cpu_alert |= exec_arm_block_mem<AccStore, true, false, AddrPreInc>(
              (opcode >> 16) & 0x0F, opcode & 0xFFFF, cycles_remaining);
            break;

          case 0x84:   /* STMDA rn, rlist^ */
            collapse_flags();
            cpu_alert |= exec_arm_block_mem<AccStore, false, true, AddrPostDec>(
              (opcode >> 16) & 0x0F, opcode & 0xFFFF, cycles_remaining);
            break;
          case 0x8C:   /* STMIA rn, rlist^ */
            collapse_flags();
            cpu_alert |= exec_arm_block_mem<AccStore, false, true, AddrPostInc>(
              (opcode >> 16) & 0x0F, opcode & 0xFFFF, cycles_remaining);
            break;
          case 0x94:   /* STMDB rn, rlist^ */
            collapse_flags();
            cpu_alert |= exec_arm_block_mem<AccStore, false, true, AddrPreDec>(
              (opcode >> 16) & 0x0F, opcode & 0xFFFF, cycles_remaining);
            break;
          case 0x9C:   /* STMIB rn, rlist^ */
            collapse_flags();
            cpu_alert |= exec_arm_block_mem<AccStore, false, true, AddrPreInc>(
              (opcode >> 16) & 0x0F, opcode & 0xFFFF, cycles_remaining);
            break;

          case 0x86:   /* STMDA rn!, rlist^ */
            collapse_flags();
            cpu_alert |= exec_arm_block_mem<AccStore, true, true, AddrPostDec>(
              (opcode >> 16) & 0x0F, opcode & 0xFFFF, cycles_remaining);
            break;
          case 0x8E:   /* STMIA rn!, rlist^ */
            collapse_flags();
            cpu_alert |= exec_arm_block_mem<AccStore, true, true, AddrPostInc>(
              (opcode >> 16) & 0x0F, opcode & 0xFFFF, cycles_remaining);
            break;
          case 0x96:   /* STMDB rn!, rlist^ */
            collapse_flags();
            cpu_alert |= exec_arm_block_mem<AccStore, true, true, AddrPreDec>(
              (opcode >> 16) & 0x0F, opcode & 0xFFFF, cycles_remaining);
            break;
          case 0x9E:   /* STMIB rn!, rlist^ */
            collapse_flags();
            cpu_alert |= exec_arm_block_mem<AccStore, true, true, AddrPreInc>(
              (opcode >> 16) & 0x0F, opcode & 0xFFFF, cycles_remaining);
            break;


          /* LDM instructions: LDMDA, LDMIA, LDMDB, LDMIB */

          case 0x81:   /* LDMDA rn, rlist */
            collapse_flags();
            cpu_alert |= exec_arm_block_mem<AccLoad, false, false, AddrPostDec>(
              (opcode >> 16) & 0x0F, opcode & 0xFFFF, cycles_remaining);
            break;
          case 0x89:   /* LDMIA rn, rlist */
            collapse_flags();
            cpu_alert |= exec_arm_block_mem<AccLoad, false, false, AddrPostInc>(
              (opcode >> 16) & 0x0F, opcode & 0xFFFF, cycles_remaining);
            break;
          case 0x91:   /* LDMDB rn, rlist */
            collapse_flags();
            cpu_alert |= exec_arm_block_mem<AccLoad, false, false, AddrPreDec>(
              (opcode >> 16) & 0x0F, opcode & 0xFFFF, cycles_remaining);
            break;
          case 0x99:   /* LDMIB rn, rlist */
            collapse_flags();
            cpu_alert |= exec_arm_block_mem<AccLoad, false, false, AddrPreInc>(
              (opcode >> 16) & 0x0F, opcode & 0xFFFF, cycles_remaining);
            break;

          case 0x83:   /* LDMDA rn!, rlist */
            collapse_flags();
            cpu_alert |= exec_arm_block_mem<AccLoad, true, false, AddrPostDec>(
              (opcode >> 16) & 0x0F, opcode & 0xFFFF, cycles_remaining);
            break;
          case 0x8B:   /* LDMIA rn!, rlist */
            collapse_flags();
            cpu_alert |= exec_arm_block_mem<AccLoad, true, false, AddrPostInc>(
              (opcode >> 16) & 0x0F, opcode & 0xFFFF, cycles_remaining);
            break;
          case 0x93:   /* LDMDB rn!, rlist */
            collapse_flags();
            cpu_alert |= exec_arm_block_mem<AccLoad, true, false, AddrPreDec>(
              (opcode >> 16) & 0x0F, opcode & 0xFFFF, cycles_remaining);
            break;
          case 0x9B:   /* LDMIB rn!, rlist */
            collapse_flags();
            cpu_alert |= exec_arm_block_mem<AccLoad, true, false, AddrPreInc>(
              (opcode >> 16) & 0x0F, opcode & 0xFFFF, cycles_remaining);
            break;

          case 0x85:   /* LDMDA rn, rlist^ */
            collapse_flags();
            cpu_alert |= exec_arm_block_mem<AccLoad, false, true, AddrPostDec>(
              (opcode >> 16) & 0x0F, opcode & 0xFFFF, cycles_remaining);
            arm_spsr_restore_ldm_check();
            break;
          case 0x8D:   /* LDMIA rn, rlist^ */
            collapse_flags();
            cpu_alert |= exec_arm_block_mem<AccLoad, false, true, AddrPostInc>(
              (opcode >> 16) & 0x0F, opcode & 0xFFFF, cycles_remaining);
            arm_spsr_restore_ldm_check();
            break;
          case 0x95:   /* LDMDB rn, rlist^ */
            collapse_flags();
            cpu_alert |= exec_arm_block_mem<AccLoad, false, true, AddrPreDec>(
              (opcode >> 16) & 0x0F, opcode & 0xFFFF, cycles_remaining);
            arm_spsr_restore_ldm_check();
            break;
          case 0x9D:   /* LDMIB rn, rlist^ */
            collapse_flags();
            cpu_alert |= exec_arm_block_mem<AccLoad, false, true, AddrPreInc>(
              (opcode >> 16) & 0x0F, opcode & 0xFFFF, cycles_remaining);
            arm_spsr_restore_ldm_check();
            break;

          case 0x87:   /* LDMDA rn!, rlist^ */
            collapse_flags();
            cpu_alert |= exec_arm_block_mem<AccLoad, true, true, AddrPostDec>(
              (opcode >> 16) & 0x0F, opcode & 0xFFFF, cycles_remaining);
            arm_spsr_restore_ldm_check();
            break;
          case 0x8F:   /* LDMIA rn!, rlist^ */
            collapse_flags();
            cpu_alert |= exec_arm_block_mem<AccLoad, true, true, AddrPostInc>(
              (opcode >> 16) & 0x0F, opcode & 0xFFFF, cycles_remaining);
            arm_spsr_restore_ldm_check();
            break;
          case 0x97:   /* LDMDB rn!, rlist^ */
            collapse_flags();
            cpu_alert |= exec_arm_block_mem<AccLoad, true, true, AddrPreDec>(
              (opcode >> 16) & 0x0F, opcode & 0xFFFF, cycles_remaining);
            arm_spsr_restore_ldm_check();
            break;
          case 0x9F:   /* LDMIB rn!, rlist^ */
            collapse_flags();
            cpu_alert |= exec_arm_block_mem<AccLoad, true, true, AddrPreInc>(
              (opcode >> 16) & 0x0F, opcode & 0xFFFF, cycles_remaining);
            arm_spsr_restore_ldm_check();
            break;


          case 0xA0 ... 0xAF:
             {
                /* B offset */
                u32 branch_pc = reg[REG_PC];
                arm_decode_branch();
                reg[REG_PC] += offset + 8;
                cycles_remaining -= ws_cyc_nseq[reg[REG_PC] >> 24][1];
                idle_poll(branch_pc, false);
                break;
             }

          case 0xB0 ... 0xBF:
             {
                /* BL offset */
                arm_decode_branch();
                reg[REG_LR] = reg[REG_PC] + 4;
                reg[REG_PC] += offset + 8;
                cycles_remaining -= ws_cyc_nseq[reg[REG_PC] >> 24][1];
                break;
             }

#ifdef HAVE_UNUSED
          case 0xC0 ... 0xEF:
             /* coprocessor instructions, reserved on GBA */
             break;
#endif

          case 0xF0 ... 0xFF:
            collapse_flags();
            reg[REG_BUS_VALUE] = 0xe3a02004;  // After SWI, we read bios[0xE4]
            REG_MODE(MODE_SUPERVISOR)[6] = reg[REG_PC] + 4;
            REG_SPSR(MODE_SUPERVISOR) = reg[REG_CPSR];
            reg[REG_PC] = 0x00000008;
            // Move to ARM mode, Supervisor mode and disable IRQs
            reg[REG_CPSR] = (reg[REG_CPSR] & ~0x3F) | 0x13 | 0x80;
            set_cpu_mode(MODE_SUPERVISOR);
            break;
       }

skip_instruction:

       /* End of Execute ARM instruction */
       cycles_remaining -= ws_cyc_seq[(reg[REG_PC] >> 24) & 0xF][1];

       if (reg[REG_PC] == idle_loop_target_pc && cycles_remaining > 0) cycles_remaining = 0;

       if (cpu_alert & (CPU_ALERT_HALT | CPU_ALERT_IRQ))
         goto mode_changed;

