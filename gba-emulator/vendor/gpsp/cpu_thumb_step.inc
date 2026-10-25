/* gpSP instruction semantics extracted from cpu.cc; GPL-2.0-or-later. */
       /* Execute THUMB instruction */

       using_instruction(thumb);
       GBA_PROFILE_INSTRUCTION(1, reg[REG_PC]);
       check_pc_region();
       reg[REG_PC] &= ~0x01;
       opcode = cpu_readmap16(pc_address_block, (reg[REG_PC] & 0x7FFF));

       #ifdef TRACE_INSTRUCTIONS
       interp_trace_instruction(reg[REG_PC], 0);
       #endif

       switch((opcode >> 8) & 0xFF)
       {
          case 0x00 ... 0x07:
             /* LSL rd, rs, offset */
             thumb_shift(shift, lsl, imm);
             break;

          case 0x08 ... 0x0F:
             /* LSR rd, rs, offset */
             thumb_shift(shift, lsr, imm);
             break;

          case 0x10 ... 0x17:
             /* ASR rd, rs, offset */
             thumb_shift(shift, asr, imm);
             break;

          case 0x18:
          case 0x19:
             /* ADD rd, rs, rn */
             thumb_add(add_sub, rd, reg[rs], reg[rn], 0);
             break;

          case 0x1A:
          case 0x1B:
             /* SUB rd, rs, rn */
             thumb_sub(add_sub, rd, reg[rs], reg[rn], 1);
             break;

          case 0x1C:
          case 0x1D:
             /* ADD rd, rs, imm */
             thumb_add(add_sub_imm, rd, reg[rs], imm, 0);
             break;

          case 0x1E:
          case 0x1F:
             /* SUB rd, rs, imm */
             thumb_sub(add_sub_imm, rd, reg[rs], imm, 1);
             break;

          case 0x20 ... 0x27:
             /* MOV r0..7, imm */
             thumb_logic(imm, ((opcode >> 8) & 7), imm);
             break;

          case 0x28 ... 0x2F:
             /* CMP r0..7, imm */
             thumb_test_sub(imm, reg[(opcode >> 8) & 7], imm);
             break;

          case 0x30 ... 0x37:
             /* ADD r0..7, imm */
             thumb_add(imm, ((opcode >> 8) & 7), reg[(opcode >> 8) & 7], imm, 0);
             break;

          case 0x38 ... 0x3F:
             /* SUB r0..7, imm */
             thumb_sub(imm, ((opcode >> 8) & 7), reg[(opcode >> 8) & 7], imm, 1);
             break;

          case 0x40:
             switch((opcode >> 6) & 0x03)
             {
                case 0x00:
                   /* AND rd, rs */
                   thumb_logic(alu_op, rd, reg[rd] & reg[rs]);
                   break;

                case 0x01:
                   /* EOR rd, rs */
                   thumb_logic(alu_op, rd, reg[rd] ^ reg[rs]);
                   break;

                case 0x02:
                   /* LSL rd, rs */
                   thumb_shift(alu_op, lsl, reg);
                   break;

                case 0x03:
                   /* LSR rd, rs */
                   thumb_shift(alu_op, lsr, reg);
                   break;
             }
             break;

          case 0x41:
             switch((opcode >> 6) & 0x03)
             {
                case 0x00:
                   /* ASR rd, rs */
                   thumb_shift(alu_op, asr, reg);
                   break;

                case 0x01:
                   /* ADC rd, rs */
                   thumb_add(alu_op, rd, reg[rd], reg[rs], c_flag);
                   break;

                case 0x02:
                   /* SBC rd, rs */
                   thumb_sub(alu_op, rd, reg[rd], reg[rs], c_flag);
                   break;

                case 0x03:
                   /* ROR rd, rs */
                   thumb_shift(alu_op, ror, reg);
                   break;
             }
             break;

          case 0x42:
             switch((opcode >> 6) & 0x03)
             {
                case 0x00:
                   /* TST rd, rs */
                   thumb_test_logic(alu_op, reg[rd] & reg[rs]);
                   break;

                case 0x01:
                   /* NEG rd, rs */
                   thumb_sub(alu_op, rd, 0, reg[rs], 1);
                   break;

                case 0x02:
                   /* CMP rd, rs */
                   thumb_test_sub(alu_op, reg[rd], reg[rs]);
                   break;

                case 0x03:
                   /* CMN rd, rs */
                   thumb_test_add(alu_op, reg[rd], reg[rs]);
                   break;
             }
             break;

          case 0x43:
             switch((opcode >> 6) & 0x03)
             {
                case 0x00:
                   /* ORR rd, rs */
                   thumb_logic(alu_op, rd, reg[rd] | reg[rs]);
                   break;

                case 0x01:
                   /* MUL rd, rs */
                   thumb_logic(alu_op, rd, reg[rd] * reg[rs]);
                   break;

                case 0x02:
                   /* BIC rd, rs */
                   thumb_logic(alu_op, rd, reg[rd] & (~reg[rs]));
                   break;

                case 0x03:
                   /* MVN rd, rs */
                   thumb_logic(alu_op, rd, ~reg[rs]);
                   break;
             }
             break;

          case 0x44:
             /* ADD rd, rs */
             thumb_hireg_op(reg[rd] + reg[rs]);
             break;

          case 0x45:
             /* CMP rd, rs */
             {
                thumb_pc_offset(4);
                thumb_decode_hireg_op();
                u32 _sa = reg[rd];
                u32 _sb = reg[rs];
                u32 dest = _sa - _sb;
                thumb_pc_offset(-2);
                calculate_flags_sub(dest, _sa, _sb, 1);
             }
             break;

          case 0x46:
             /* MOV rd, rs */
             thumb_hireg_op(reg[rs]);
             break;

          case 0x47:
             /* BX rs */
             {
                thumb_decode_hireg_op();
                u32 src;
                thumb_pc_offset(4);
                src = reg[rs];
                if(src & 0x01)
                {
                   reg[REG_PC] = src - 1;
                }
                else
                {
                   /* Switch to ARM mode */
                   reg[REG_PC] = src;
                   reg[REG_CPSR] &= ~0x20;
                   collapse_flags();
                   goto arm_loop;
                }
             }
             break;

          case 0x48 ... 0x4F:
             /* LDR r0..7, [pc + imm] */
             thumb_access_memory(load, imm, ((reg[REG_PC] - 2) & ~2) + (imm * 4) + 4, reg[(opcode >> 8) & 7], u32);
             break;

          case 0x50:
          case 0x51:
             /* STR rd, [rb + ro] */
             thumb_access_memory(store, mem_reg, reg[rb] + reg[ro], reg[rd], u32);
             break;

          case 0x52:
          case 0x53:
             /* STRH rd, [rb + ro] */
             thumb_access_memory(store, mem_reg, reg[rb] + reg[ro], reg[rd], u16);
             break;

          case 0x54:
          case 0x55:
             /* STRB rd, [rb + ro] */
             thumb_access_memory(store, mem_reg, reg[rb] + reg[ro], reg[rd], u8);
             break;

          case 0x56:
          case 0x57:
             /* LDSB rd, [rb + ro] */
             thumb_access_memory(load, mem_reg, reg[rb] + reg[ro], reg[rd], s8);
             break;

          case 0x58:
          case 0x59:
             /* LDR rd, [rb + ro] */
             thumb_access_memory(load, mem_reg, reg[rb] + reg[ro], reg[rd], u32);
             break;

          case 0x5A:
          case 0x5B:
             /* LDRH rd, [rb + ro] */
             thumb_access_memory(load, mem_reg, reg[rb] + reg[ro], reg[rd], u16);
             break;

          case 0x5C:
          case 0x5D:
             /* LDRB rd, [rb + ro] */
             thumb_access_memory(load, mem_reg, reg[rb] + reg[ro], reg[rd], u8);
             break;

          case 0x5E:
          case 0x5F:
             /* LDSH rd, [rb + ro] */
             thumb_access_memory(load, mem_reg, reg[rb] + reg[ro], reg[rd], s16);
             break;

          case 0x60 ... 0x67:
             /* STR rd, [rb + imm] */
             thumb_access_memory(store, mem_imm, reg[rb] + (imm * 4), reg[rd], u32);
             break;

          case 0x68 ... 0x6F:
             /* LDR rd, [rb + imm] */
             thumb_access_memory(load, mem_imm, reg[rb] + (imm * 4), reg[rd], u32);
             break;

          case 0x70 ... 0x77:
             /* STRB rd, [rb + imm] */
             thumb_access_memory(store, mem_imm, reg[rb] + imm, reg[rd], u8);
             break;

          case 0x78 ... 0x7F:
             /* LDRB rd, [rb + imm] */
             thumb_access_memory(load, mem_imm, reg[rb] + imm, reg[rd], u8);
             break;

          case 0x80 ... 0x87:
             /* STRH rd, [rb + imm] */
             thumb_access_memory(store, mem_imm, reg[rb] + (imm * 2), reg[rd], u16);
             break;

          case 0x88 ... 0x8F:
             /* LDRH rd, [rb + imm] */
             thumb_access_memory(load, mem_imm, reg[rb] + (imm * 2), reg[rd], u16);
             break;

          case 0x90 ... 0x97:
             /* STR r0..7, [sp + imm] */
             thumb_access_memory(store, imm, reg[REG_SP] + (imm * 4), reg[(opcode >> 8) & 7], u32);
             break;

          case 0x98 ... 0x9F:
             /* LDR r0..7, [sp + imm] */
             thumb_access_memory(load, imm, reg[REG_SP] + (imm * 4), reg[(opcode >> 8) & 7], u32);
             break;

          case 0xA0 ... 0xA7:
             /* ADD r0..7, pc, +imm */
             thumb_add_noflags(imm, ((opcode >> 8) & 7), (reg[REG_PC] & ~2) + 4, (imm * 4));
             break;

          case 0xA8 ... 0xAF:
             /* ADD r0..7, sp, +imm */
             thumb_add_noflags(imm, ((opcode >> 8) & 7), reg[REG_SP], (imm * 4));
             break;

          case 0xB0:
          case 0xB1:
          case 0xB2:
          case 0xB3:
             if((opcode >> 7) & 0x01)
             {
                /* ADD sp, -imm */
                thumb_add_noflags(add_sp, 13, reg[REG_SP], -(imm * 4));
             }
             else
             {
                /* ADD sp, +imm */
                thumb_add_noflags(add_sp, 13, reg[REG_SP], (imm * 4));
             }
             break;

          case 0xB4:  /* PUSH rlist */
             collapse_flags();
             cpu_alert |= exec_thumb_block_mem<AccStore, AddrPreDec>(
               REG_SP, opcode & 0xFF, cycles_remaining);
             break;

          case 0xB5:  /* PUSH rlist, lr */
             collapse_flags();
             cpu_alert |= exec_thumb_block_mem<AccStore, AddrPreDec>(
               REG_SP, (opcode & 0xFF) | (1 << REG_LR), cycles_remaining);
             break;

          case 0xBC:  /* POP rlist */
             collapse_flags();
             cpu_alert |= exec_thumb_block_mem<AccLoad, AddrPostInc>(
               REG_SP, opcode & 0xFF, cycles_remaining);
             break;

          case 0xBD:  /* POP rlist, pc */
             collapse_flags();
             cpu_alert |= exec_thumb_block_mem<AccLoad, AddrPostInc>(
               REG_SP, (opcode & 0xFF) | (1 << REG_PC), cycles_remaining);
             break;

          case 0xC0 ... 0xC7:    /* STMIA r0..7!, rlist */
             collapse_flags();
             cpu_alert |= exec_thumb_block_mem<AccStore, AddrPostInc>(
               (opcode >> 8) & 7, (opcode & 0xFF), cycles_remaining);
             break;

          case 0xC8 ... 0xCF:    /* LDMIA r0..7!, rlist */
             collapse_flags();
             cpu_alert |= exec_thumb_block_mem<AccLoad, AddrPostInc>(
               (opcode >> 8) & 7, (opcode & 0xFF), cycles_remaining);
             break;

          case 0xD0:   /* BEQ label */
             thumb_conditional_branch(z_flag == 1);
             break;
          case 0xD1:   /* BNE label */
             thumb_conditional_branch(z_flag == 0);
             break;
          case 0xD2:   /* BCS label */
             thumb_conditional_branch(c_flag == 1);
             break;
          case 0xD3:   /* BCC label */
             thumb_conditional_branch(c_flag == 0);
             break;
          case 0xD4:   /* BMI label */
             thumb_conditional_branch(n_flag == 1);
             break;
          case 0xD5:   /* BPL label */
             thumb_conditional_branch(n_flag == 0);
             break;
          case 0xD6:   /* BVS label */
             thumb_conditional_branch(v_flag == 1);
             break;
          case 0xD7:   /* BVC label */
             thumb_conditional_branch(v_flag == 0);
             break;
          case 0xD8:   /* BHI label */
             thumb_conditional_branch(c_flag & (z_flag ^ 1));
             break;
          case 0xD9:   /* BLS label */
             thumb_conditional_branch((c_flag == 0) | z_flag);
             break;
          case 0xDA:   /* BGE label */
             thumb_conditional_branch(n_flag == v_flag);
             break;
          case 0xDB:   /* BLT label */
             thumb_conditional_branch(n_flag != v_flag);
             break;
          case 0xDC:   /* BGT label */
             thumb_conditional_branch((z_flag == 0) & (n_flag == v_flag));
             break;
          case 0xDD:   /* BLE label */
             thumb_conditional_branch(z_flag | (n_flag != v_flag));
             break;

          case 0xDF:
             collapse_flags();
             REG_MODE(MODE_SUPERVISOR)[6] = reg[REG_PC] + 2;
             REG_SPSR(MODE_SUPERVISOR) = reg[REG_CPSR];
             reg[REG_PC] = 0x00000008;
             // Move to ARM mode, Supervisor mode and disable IRQs
             reg[REG_CPSR] = (reg[REG_CPSR] & ~0x3F) | 0x13 | 0x80;
             set_cpu_mode(MODE_SUPERVISOR);
             reg[REG_BUS_VALUE] = 0xe3a02004;  // After SWI, we read bios[0xE4]
             goto arm_loop;
             break;

          case 0xE0 ... 0xE7:
             {
                /* B label */
                u32 branch_pc = reg[REG_PC];
                thumb_decode_branch();
                s32 br_offset = ((s32)(offset << 21) >> 20) + 4;
                reg[REG_PC] += br_offset;
                cycles_remaining -= ws_cyc_nseq[reg[REG_PC] >> 24][0];
                idle_poll(branch_pc, true);
                break;
             }

          case 0xF0 ... 0xF7:
             {
                /* (low word) BL label */
                thumb_decode_branch();
                reg[REG_LR] = reg[REG_PC] + 4 + ((s32)(offset << 21) >> 9);
                thumb_pc_offset(2);
                break;
             }

          case 0xF8 ... 0xFF:
             {
                /* (high word) BL label */
                thumb_decode_branch();
                u32 newlr = (reg[REG_PC] + 2) | 0x01;
                u32 newpc = reg[REG_LR] + (offset * 2);
                reg[REG_LR] = newlr;
                reg[REG_PC] = newpc;
                cycles_remaining -= ws_cyc_nseq[newpc >> 24][0];
                break;
             }
       }

       /* End of Execute THUMB instruction */
       cycles_remaining -= ws_cyc_seq[(reg[REG_PC] >> 24) & 0xF][0];

       if (reg[REG_PC] == idle_loop_target_pc && cycles_remaining > 0) cycles_remaining = 0;

       if (cpu_alert & (CPU_ALERT_HALT | CPU_ALERT_IRQ))
          goto mode_changed;

