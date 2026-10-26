/* gameplaySP
 *
 * Copyright (C) 2006 Exophase <exophase@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

extern "C" {
  #include "common.h"
  #include "cpu_instrument.h"
  #include "pxa_gpsp_profile.h"
}

const u8 bit_count[256] =
{
  0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4, 1, 2, 2, 3, 2, 3, 3,
  4, 2, 3, 3, 4, 3, 4, 4, 5, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4,
  4, 5, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 1, 2, 2, 3, 2,
  3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5,
  4, 5, 5, 6, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4,
  5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3,
  3, 4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 2,
  3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6,
  4, 5, 5, 6, 5, 6, 6, 7, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5,
  6, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 3, 4, 4, 5, 4, 5,
  5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6,
  7, 7, 8
};

static const u8 gba_header_logo[156] = {
  0x24, 0xFF, 0xAE, 0x51, 0x69, 0x9A, 0xA2, 0x21, 0x3D, 0x84, 0x82, 0x0A,
  0x84, 0xE4, 0x09, 0xAD, 0x11, 0x24, 0x8B, 0x98, 0xC0, 0x81, 0x7F, 0x21,
  0xA3, 0x52, 0xBE, 0x19, 0x93, 0x09, 0xCE, 0x20, 0x10, 0x46, 0x4A, 0x4A,
  0xF8, 0x27, 0x31, 0xEC, 0x58, 0xC7, 0xE8, 0x33, 0x82, 0xE3, 0xCE, 0xBF,
  0x85, 0xF4, 0xDF, 0x94, 0xCE, 0x4B, 0x09, 0xC1, 0x94, 0x56, 0x8A, 0xC0,
  0x13, 0x72, 0xA7, 0xFC, 0x9F, 0x84, 0x4D, 0x73, 0xA3, 0xCA, 0x9A, 0x61,
  0x58, 0x97, 0xA3, 0x27, 0xFC, 0x03, 0x98, 0x76, 0x23, 0x1D, 0xC7, 0x61,
  0x03, 0x04, 0xAE, 0x56, 0xBF, 0x38, 0x84, 0x00, 0x40, 0xA7, 0x0E, 0xFD,
  0xFF, 0x52, 0xFE, 0x03, 0x6F, 0x95, 0x30, 0xF1, 0x97, 0xFB, 0xC0, 0x85,
  0x60, 0xD6, 0x80, 0x25, 0xA9, 0x63, 0xBE, 0x03, 0x01, 0x4E, 0x38, 0xE2,
  0xF9, 0xA2, 0x34, 0xFF, 0xBB, 0x3E, 0x03, 0x44, 0x78, 0x00, 0x90, 0xCB,
  0x88, 0x11, 0x3A, 0x94, 0x65, 0xC0, 0x7C, 0x63, 0x87, 0xF0, 0x3C, 0xAF,
  0xD6, 0x25, 0xE4, 0x8B, 0x38, 0x0A, 0xAC, 0x72, 0x21, 0xD4, 0xF8, 0x07
};


#define arm_decode_data_proc_reg(opcode)                                      \
  u32 rn = (opcode >> 16) & 0x0F;                                             \
  u32 rd = (opcode >> 12) & 0x0F;                                             \
  u32 rm = opcode & 0x0F;                                                     \
  (void)rd;                                                                   \
  (void)rn;                                                                   \
  using_register(arm, rd, op_dest);                                           \
  using_register(arm, rn, op_src);                                            \
  using_register(arm, rm, op_src)                                             \

#define arm_decode_data_proc_imm(opcode)                                      \
  u32 imm;                                                                    \
  u32 rn = (opcode >> 16) & 0x0F;                                             \
  u32 rd = (opcode >> 12) & 0x0F;                                             \
  u32 imm_ror = ((opcode >> 8) & 0xF) << 1;                                   \
  (void)rd;                                                                   \
  (void)rn;                                                                   \
  ror(imm, opcode & 0xFF, imm_ror);                                           \
  using_register(arm, rd, op_dest);                                           \
  using_register(arm, rn, op_src)                                             \

#define arm_decode_psr_reg(opcode)                                            \
  u32 psr_pfield = ((opcode >> 16) & 1) | ((opcode >> 18) & 2);               \
  u32 rd = (opcode >> 12) & 0x0F;                                             \
  u32 rm = opcode & 0x0F;                                                     \
  (void)rd;                                                                   \
  (void)rm;                                                                   \
  (void)psr_pfield;                                                           \
  using_register(arm, rd, op_dest);                                           \
  using_register(arm, rm, op_src)                                             \

#define arm_decode_psr_imm(opcode)                                            \
  u32 imm;                                                                    \
  u32 psr_pfield = ((opcode >> 16) & 1) | ((opcode >> 18) & 2);               \
  u32 rd = (opcode >> 12) & 0x0F;                                             \
  (void)rd;                                                                   \
  ror(imm, opcode & 0xFF, ((opcode >> 8) & 0x0F) * 2);                        \
  using_register(arm, rd, op_dest)                                            \

#define arm_decode_branchx(opcode)                                            \
  u32 rn = opcode & 0x0F;                                                     \
  using_register(arm, rn, branch_target)                                      \

#define arm_decode_multiply()                                                 \
  u32 rd = (opcode >> 16) & 0x0F;                                             \
  u32 rn = (opcode >> 12) & 0x0F;                                             \
  u32 rs = (opcode >> 8) & 0x0F;                                              \
  u32 rm = opcode & 0x0F;                                                     \
  (void)rn;                                                                   \
  using_register(arm, rd, op_dest);                                           \
  using_register(arm, rn, op_src);                                            \
  using_register(arm, rm, op_src)                                             \

#define arm_decode_multiply_long()                                            \
  u32 rdhi = (opcode >> 16) & 0x0F;                                           \
  u32 rdlo = (opcode >> 12) & 0x0F;                                           \
  u32 rn = (opcode >> 8) & 0x0F;                                              \
  u32 rm = opcode & 0x0F;                                                     \
  using_register(arm, rdhi, op_dest);                                         \
  using_register(arm, rdlo, op_dest);                                         \
  using_register(arm, rn, op_src);                                            \
  using_register(arm, rm, op_src)                                             \

#define arm_decode_swap()                                                     \
  u32 rn = (opcode >> 16) & 0x0F;                                             \
  u32 rd = (opcode >> 12) & 0x0F;                                             \
  u32 rm = opcode & 0x0F;                                                     \
  using_register(arm, rd, memory_target);                                     \
  using_register(arm, rn, memory_base);                                       \
  using_register(arm, rm, memory_target)                                      \

#define arm_decode_half_trans_r()                                             \
  u32 rn = (opcode >> 16) & 0x0F;                                             \
  u32 rd = (opcode >> 12) & 0x0F;                                             \
  u32 rm = opcode & 0x0F;                                                     \
  using_register(arm, rd, memory_target);                                     \
  using_register(arm, rn, memory_base);                                       \
  using_register(arm, rm, memory_offset)                                      \

#define arm_decode_half_trans_of()                                            \
  u32 rn = (opcode >> 16) & 0x0F;                                             \
  u32 rd = (opcode >> 12) & 0x0F;                                             \
  u32 offset = ((opcode >> 4) & 0xF0) | (opcode & 0x0F);                      \
  using_register(arm, rd, memory_target);                                     \
  using_register(arm, rn, memory_base)                                        \

#define arm_decode_data_trans_imm()                                           \
  u32 rn = (opcode >> 16) & 0x0F;                                             \
  u32 rd = (opcode >> 12) & 0x0F;                                             \
  u32 offset = opcode & 0x0FFF;                                               \
  using_register(arm, rd, memory_target);                                     \
  using_register(arm, rn, memory_base)                                        \

#define arm_decode_data_trans_reg()                                           \
  u32 rn = (opcode >> 16) & 0x0F;                                             \
  u32 rd = (opcode >> 12) & 0x0F;                                             \
  u32 rm = opcode & 0x0F;                                                     \
  using_register(arm, rd, memory_target);                                     \
  using_register(arm, rn, memory_base);                                       \
  using_register(arm, rm, memory_offset)                                      \

#define arm_decode_block_trans()                                              \
  u32 rn = (opcode >> 16) & 0x0F;                                             \
  u32 reg_list = opcode & 0xFFFF;                                             \
  using_register(arm, rn, memory_base);                                       \
  using_register_list(arm, reg_list, 16)                                      \

#define arm_decode_branch()                                                   \
  s32 offset = ((s32)((u32)(opcode << 8))) >> 6                               \


#define thumb_decode_shift()                                                  \
  u32 imm = (opcode >> 6) & 0x1F;                                             \
  u32 rs = (opcode >> 3) & 0x07;                                              \
  u32 rd = opcode & 0x07;                                                     \
  using_register(thumb, rd, op_dest);                                         \
  using_register(thumb, rs, op_shift)                                         \

#define thumb_decode_add_sub()                                                \
  u32 rn = (opcode >> 6) & 0x07;                                              \
  u32 rs = (opcode >> 3) & 0x07;                                              \
  u32 rd = opcode & 0x07;                                                     \
  using_register(thumb, rd, op_dest);                                         \
  using_register(thumb, rn, op_src);                                          \
  using_register(thumb, rs, op_src)                                           \

#define thumb_decode_add_sub_imm()                                            \
  u32 imm = (opcode >> 6) & 0x07;                                             \
  u32 rs = (opcode >> 3) & 0x07;                                              \
  u32 rd = opcode & 0x07;                                                     \
  using_register(thumb, rd, op_src_dest);                                     \
  using_register(thumb, rs, op_src)                                           \

#define thumb_decode_imm()                                                    \
  u32 imm = opcode & 0xFF;                                                    \
  using_register(thumb, ((opcode >> 8) & 0x07), op_dest)                      \

#define thumb_decode_alu_op()                                                 \
  u32 rs = (opcode >> 3) & 0x07;                                              \
  u32 rd = opcode & 0x07;                                                     \
  using_register(thumb, rd, op_src_dest);                                     \
  using_register(thumb, rs, op_src)                                           \

#define thumb_decode_hireg_op()                                               \
  u32 rs = (opcode >> 3) & 0x0F;                                              \
  u32 rd = ((opcode >> 4) & 0x08) | (opcode & 0x07);                          \
  (void)rd;                                                                   \
  using_register(thumb, rd, op_src_dest);                                     \
  using_register(thumb, rs, op_src)                                           \


#define thumb_decode_mem_reg()                                                \
  u32 ro = (opcode >> 6) & 0x07;                                              \
  u32 rb = (opcode >> 3) & 0x07;                                              \
  u32 rd = opcode & 0x07;                                                     \
  using_register(thumb, rd, memory_target);                                   \
  using_register(thumb, rb, memory_base);                                     \
  using_register(thumb, ro, memory_offset)                                    \


#define thumb_decode_mem_imm()                                                \
  u32 imm = (opcode >> 6) & 0x1F;                                             \
  u32 rb = (opcode >> 3) & 0x07;                                              \
  u32 rd = opcode & 0x07;                                                     \
  using_register(thumb, rd, memory_target);                                   \
  using_register(thumb, rb, memory_base)                                      \


#define thumb_decode_add_sp()                                                 \
  u32 imm = opcode & 0x7F;                                                    \
  using_register(thumb, REG_SP, op_dest)                                      \

#define thumb_decode_rlist()                                                  \
  u32 reg_list = opcode & 0xFF;                                               \
  using_register_list(thumb, rlist, 8)                                        \

#define thumb_decode_branch_cond()                                            \
  s32 offset = (s8)(opcode & 0xFF)                                            \

#define thumb_decode_branch()                                                 \
  u32 offset = opcode & 0x07FF                                                \


#define get_shift_register(dest)                                              \
  u32 shift = reg[(opcode >> 8) & 0x0F] & 0xFF;                               \
  using_register(arm, ((opcode >> 8) & 0x0F), op_shift);                      \
  dest = reg[rm];                                                             \
  if(rm == 15)                                                                \
    dest += 4                                                                 \


#define calculate_z_flag(dest)                                                \
  z_flag = (dest == 0)                                                        \

#define calculate_n_flag(dest)                                                \
  n_flag = ((signed)dest < 0)                                                 \

#define calculate_c_flag_sub(dest, src_a, src_b, carry)                       \
  c_flag = (carry) ? ((unsigned)src_b <= (unsigned)src_a) :                   \
                     ((unsigned)src_b < (unsigned)src_a);                     \

#define calculate_v_flag_sub(dest, src_a, src_b)                              \
  v_flag = (((src_a ^ src_b) & (~src_b ^ dest)) >> 31)

#define calculate_v_flag_add(dest, src_a, src_b)                              \
  v_flag = ((~((src_a) ^ (src_b)) & ((src_a) ^ (dest))) >> 31)

#define calculate_reg_sh_inline()                                                    \
  u32 reg_sh = 0;                                                             \
  switch((opcode >> 4) & 0x07)                                                \
  {                                                                           \
    /* LSL imm */                                                             \
    case 0x0:                                                                 \
    {                                                                         \
      reg_sh = reg[rm] << ((opcode >> 7) & 0x1F);                             \
      break;                                                                  \
    }                                                                         \
                                                                              \
    /* LSL reg */                                                             \
    case 0x1:                                                                 \
    {                                                                         \
      get_shift_register(reg_sh);                                             \
      if(shift <= 31)                                                         \
        reg_sh = reg_sh << shift;                                             \
      else                                                                    \
        reg_sh = 0;                                                           \
      break;                                                                  \
    }                                                                         \
                                                                              \
    /* LSR imm */                                                             \
    case 0x2:                                                                 \
    {                                                                         \
      u32 imm = (opcode >> 7) & 0x1F;                                         \
      if(imm == 0)                                                            \
        reg_sh = 0;                                                           \
      else                                                                    \
        reg_sh = reg[rm] >> imm;                                              \
      break;                                                                  \
    }                                                                         \
                                                                              \
    /* LSR reg */                                                             \
    case 0x3:                                                                 \
    {                                                                         \
      get_shift_register(reg_sh);                                             \
      if(shift <= 31)                                                         \
        reg_sh = reg_sh >> shift;                                             \
      else                                                                    \
        reg_sh = 0;                                                           \
      break;                                                                  \
    }                                                                         \
                                                                              \
    /* ASR imm */                                                             \
    case 0x4:                                                                 \
    {                                                                         \
      u32 imm = (opcode >> 7) & 0x1F;                                         \
      reg_sh = reg[rm];                                                       \
                                                                              \
      if(imm == 0)                                                            \
        reg_sh = (s32)reg_sh >> 31;                                           \
      else                                                                    \
        reg_sh = (s32)reg_sh >> imm;                                          \
      break;                                                                  \
    }                                                                         \
                                                                              \
    /* ASR reg */                                                             \
    case 0x5:                                                                 \
    {                                                                         \
      get_shift_register(reg_sh);                                             \
      if(shift <= 31)                                                         \
        reg_sh = (s32)reg_sh >> shift;                                        \
      else                                                                    \
        reg_sh = (s32)reg_sh >> 31;                                           \
      break;                                                                  \
    }                                                                         \
                                                                              \
    /* ROR imm */                                                             \
    case 0x6:                                                                 \
    {                                                                         \
      u32 imm = (opcode >> 7) & 0x1F;                                         \
                                                                              \
      if(imm == 0)                                                            \
        reg_sh = (reg[rm] >> 1) | (c_flag << 31);                             \
      else                                                                    \
        ror(reg_sh, reg[rm], imm);                                            \
      break;                                                                  \
    }                                                                         \
                                                                              \
    /* ROR reg */                                                             \
    case 0x7:                                                                 \
    {                                                                         \
      get_shift_register(reg_sh);                                             \
      ror(reg_sh, reg_sh, shift);                                             \
      break;                                                                  \
    }                                                                         \
  }                                                                           \

#define calculate_reg_sh_flags_inline()                                              \
  u32 reg_sh = 0;                                                             \
  switch((opcode >> 4) & 0x07)                                                \
  {                                                                           \
    /* LSL imm */                                                             \
    case 0x0:                                                                 \
    {                                                                         \
      u32 imm = (opcode >> 7) & 0x1F;                                         \
      reg_sh = reg[rm];                                                       \
                                                                              \
      if(imm != 0)                                                            \
      {                                                                       \
        c_flag = (reg_sh >> (32 - imm)) & 0x01;                               \
        reg_sh <<= imm;                                                       \
      }                                                                       \
                                                                              \
      break;                                                                  \
    }                                                                         \
                                                                              \
    /* LSL reg */                                                             \
    case 0x1:                                                                 \
    {                                                                         \
      get_shift_register(reg_sh);                                             \
      if(shift != 0)                                                          \
      {                                                                       \
        if(shift > 31)                                                        \
        {                                                                     \
          if(shift == 32)                                                     \
            c_flag = reg_sh & 0x01;                                           \
          else                                                                \
            c_flag = 0;                                                       \
          reg_sh = 0;                                                         \
        }                                                                     \
        else                                                                  \
        {                                                                     \
          c_flag = (reg_sh >> (32 - shift)) & 0x01;                           \
          reg_sh <<= shift;                                                   \
        }                                                                     \
      }                                                                       \
      break;                                                                  \
    }                                                                         \
                                                                              \
    /* LSR imm */                                                             \
    case 0x2:                                                                 \
    {                                                                         \
      u32 imm = (opcode >> 7) & 0x1F;                                         \
      reg_sh = reg[rm];                                                       \
      if(imm == 0)                                                            \
      {                                                                       \
        c_flag = reg_sh >> 31;                                                \
        reg_sh = 0;                                                           \
      }                                                                       \
      else                                                                    \
      {                                                                       \
        c_flag = (reg_sh >> (imm - 1)) & 0x01;                                \
        reg_sh >>= imm;                                                       \
      }                                                                       \
      break;                                                                  \
    }                                                                         \
                                                                              \
    /* LSR reg */                                                             \
    case 0x3:                                                                 \
    {                                                                         \
      get_shift_register(reg_sh);                                             \
      if(shift != 0)                                                          \
      {                                                                       \
        if(shift > 31)                                                        \
        {                                                                     \
          if(shift == 32)                                                     \
            c_flag = (reg_sh >> 31) & 0x01;                                   \
          else                                                                \
            c_flag = 0;                                                       \
          reg_sh = 0;                                                         \
        }                                                                     \
        else                                                                  \
        {                                                                     \
          c_flag = (reg_sh >> (shift - 1)) & 0x01;                            \
          reg_sh >>= shift;                                                   \
        }                                                                     \
      }                                                                       \
      break;                                                                  \
    }                                                                         \
                                                                              \
    /* ASR imm */                                                             \
    case 0x4:                                                                 \
    {                                                                         \
      u32 imm = (opcode >> 7) & 0x1F;                                         \
      reg_sh = reg[rm];                                                       \
      if(imm == 0)                                                            \
      {                                                                       \
        reg_sh = (s32)reg_sh >> 31;                                           \
        c_flag = reg_sh & 0x01;                                               \
      }                                                                       \
      else                                                                    \
      {                                                                       \
        c_flag = (reg_sh >> (imm - 1)) & 0x01;                                \
        reg_sh = (s32)reg_sh >> imm;                                          \
      }                                                                       \
      break;                                                                  \
    }                                                                         \
                                                                              \
    /* ASR reg */                                                             \
    case 0x5:                                                                 \
    {                                                                         \
      get_shift_register(reg_sh);                                             \
      if(shift != 0)                                                          \
      {                                                                       \
        if(shift > 31)                                                        \
        {                                                                     \
          reg_sh = (s32)reg_sh >> 31;                                         \
          c_flag = reg_sh & 0x01;                                             \
        }                                                                     \
        else                                                                  \
        {                                                                     \
          c_flag = (reg_sh >> (shift - 1)) & 0x01;                            \
          reg_sh = (s32)reg_sh >> shift;                                      \
        }                                                                     \
      }                                                                       \
      break;                                                                  \
    }                                                                         \
                                                                              \
    /* ROR imm */                                                             \
    case 0x6:                                                                 \
    {                                                                         \
      u32 imm = (opcode >> 7) & 0x1F;                                         \
      reg_sh = reg[rm];                                                       \
      if(imm == 0)                                                            \
      {                                                                       \
        u32 old_c_flag = c_flag;                                              \
        c_flag = reg_sh & 0x01;                                               \
        reg_sh = (reg_sh >> 1) | (old_c_flag << 31);                          \
      }                                                                       \
      else                                                                    \
      {                                                                       \
        c_flag = (reg_sh >> (imm - 1)) & 0x01;                                \
        ror(reg_sh, reg_sh, imm);                                             \
      }                                                                       \
      break;                                                                  \
    }                                                                         \
                                                                              \
    /* ROR reg */                                                             \
    case 0x7:                                                                 \
    {                                                                         \
      get_shift_register(reg_sh);                                             \
      if(shift != 0)                                                          \
      {                                                                       \
        c_flag = (reg_sh >> (shift - 1)) & 0x01;                              \
        ror(reg_sh, reg_sh, shift);                                           \
      }                                                                       \
      break;                                                                  \
    }                                                                         \
  }                                                                           \

/* Keep the common immediate LSL inline, but share the other shifters.
 * Replicating their carry and register-shift edge cases for every ARM ALU
 * operation substantially exceeds the embedded instruction cache. Returning
 * value/carry together avoids borrowing the interpreter's flag locals. */
static __attribute__((noinline)) u32 arm_shift_operand(u32 opcode,u32 c_flag)
{
  u32 rm=opcode&15;
  calculate_reg_sh_inline();
  return reg_sh;
}
static __attribute__((noinline)) u64 arm_shift_operand_flags(u32 opcode,u32 c_flag)
{
  u32 rm=opcode&15;
  calculate_reg_sh_flags_inline();
  return reg_sh|((u64)c_flag<<32);
}
#define calculate_reg_sh() \
  u32 reg_sh = !(opcode&0x70) ? reg[rm]<<((opcode>>7)&31) : \
      arm_shift_operand(opcode,c_flag)
#define calculate_reg_sh_flags() \
  u32 reg_sh; \
  if(!(opcode&0x70)) { \
    u32 shift=(opcode>>7)&31,source=reg[rm]; \
    reg_sh=source<<shift; \
    if(shift)c_flag=(source>>(32-shift))&1; \
  } else { \
    u64 shifted=arm_shift_operand_flags(opcode,c_flag); \
    reg_sh=(u32)shifted;c_flag=(u32)(shifted>>32); \
  }

#define calculate_reg_offset()                                                \
  u32 reg_offset = 0;                                                         \
  switch((opcode >> 5) & 0x03)                                                \
  {                                                                           \
    /* LSL imm */                                                             \
    case 0x0:                                                                 \
    {                                                                         \
      reg_offset = reg[rm] << ((opcode >> 7) & 0x1F);                         \
      break;                                                                  \
    }                                                                         \
                                                                              \
    /* LSR imm */                                                             \
    case 0x1:                                                                 \
    {                                                                         \
      u32 imm = (opcode >> 7) & 0x1F;                                         \
      if(imm == 0)                                                            \
        reg_offset = 0;                                                       \
      else                                                                    \
        reg_offset = reg[rm] >> imm;                                          \
      break;                                                                  \
    }                                                                         \
                                                                              \
    /* ASR imm */                                                             \
    case 0x2:                                                                 \
    {                                                                         \
      u32 imm = (opcode >> 7) & 0x1F;                                         \
      if(imm == 0)                                                            \
        reg_offset = (s32)reg[rm] >> 31;                                      \
      else                                                                    \
        reg_offset = (s32)reg[rm] >> imm;                                     \
      break;                                                                  \
    }                                                                         \
                                                                              \
    /* ROR imm */                                                             \
    case 0x3:                                                                 \
    {                                                                         \
      u32 imm = (opcode >> 7) & 0x1F;                                         \
      if(imm == 0)                                                            \
        reg_offset = (reg[rm] >> 1) | (c_flag << 31);                         \
      else                                                                    \
        ror(reg_offset, reg[rm], imm);                                        \
      break;                                                                  \
    }                                                                         \
  }                                                                           \

#define calculate_flags_add(dest, src_a, src_b)                               \
  calculate_z_flag(dest);                                                     \
  calculate_n_flag(dest);                                                     \
  calculate_v_flag_add(dest, src_a, src_b)                                    \

#define calculate_flags_sub(dest, src_a, src_b, carry)                        \
  calculate_z_flag(dest);                                                     \
  calculate_n_flag(dest);                                                     \
  calculate_c_flag_sub(dest, src_a, src_b, carry);                            \
  calculate_v_flag_sub(dest, src_a, src_b)                                    \

#define calculate_flags_logic(dest)                                           \
  calculate_z_flag(dest);                                                     \
  calculate_n_flag(dest)                                                      \

#define extract_flags()                                                       \
  n_flag = reg[REG_CPSR] >> 31;                                               \
  z_flag = (reg[REG_CPSR] >> 30) & 0x01;                                      \
  c_flag = (reg[REG_CPSR] >> 29) & 0x01;                                      \
  v_flag = (reg[REG_CPSR] >> 28) & 0x01;                                      \

#define collapse_flags()                                                      \
  reg[REG_CPSR] = (n_flag << 31) | (z_flag << 30) | (c_flag << 29) |          \
   (v_flag << 28) | (reg[REG_CPSR] & 0xFF)                                    \

#define check_pc_region()                                                     \
  new_pc_region = (reg[REG_PC] >> 15);                                        \
  if(new_pc_region != pc_region)                                              \
  {                                                                           \
    pc_region = new_pc_region;                                                \
    pc_address_block = memory_map_read[new_pc_region];                        \
    touch_gamepak_page(pc_region);                                            \
                                                                              \
    if(!pc_address_block)                                                     \
      pc_address_block = load_gamepak_page(pc_region & 0x3FF);                \
  }                                                                           \


#define arm_pc_offset(val)                                                    \
  reg[REG_PC] += val                                                          \

#define arm_next_instruction()                                                \
{                                                                             \
  if (idle.branch == reg[REG_PC]) idle.branch = ~0u;                           \
  arm_pc_offset(4);                                                           \
  goto skip_instruction;                                                      \
}                                                                             \

#define thumb_pc_offset(val)                                                  \
  reg[REG_PC] += val                                                          \


// It should be okay to still generate result flags, spsr will overwrite them.
// This is pretty infrequent (returning from interrupt handlers, et al) so
// probably not worth optimizing for.

#define check_for_interrupts()                                                \
  if((read_ioreg(REG_IE) & read_ioreg(REG_IF)) &&                             \
   read_ioreg(REG_IME) && ((reg[REG_CPSR] & 0x80) == 0))                      \
  {                                                                           \
    REG_MODE(MODE_IRQ)[6] = reg[REG_PC] + 4;                                  \
    REG_SPSR(MODE_IRQ) = reg[REG_CPSR];                                       \
    reg[REG_CPSR] = 0xD2;                                                     \
    reg[REG_PC] = 0x00000018;                                                 \
    set_cpu_mode(MODE_IRQ);                                                   \
    goto arm_loop;                                                            \
  }                                                                           \

#define arm_spsr_restore()                                                    \
  {                                                                           \
    if(reg[CPU_MODE] != MODE_USER && reg[CPU_MODE] != MODE_SYSTEM)            \
    {                                                                         \
      reg[REG_CPSR] = REG_SPSR(reg[CPU_MODE]);                                \
      extract_flags();                                                        \
      set_cpu_mode(cpu_modes[reg[REG_CPSR] & 0xF]);                           \
      check_for_interrupts();                                                 \
    }                                                                         \
                                                                              \
    if(reg[REG_CPSR] & 0x20)                                                  \
      goto thumb_loop;                                                        \
  }                                                                           \

#define arm_spsr_restore_check()                                              \
  if(rd == REG_PC)                                                            \
  {                                                                           \
    arm_spsr_restore()                                                        \
  }                                                                           \

#define arm_spsr_restore_ldm_check()                                          \
  if (opcode & 0x8000)   /* PC is in the LDM reg list */                      \
  {                                                                           \
    arm_spsr_restore()                                                        \
  }                                                                           \

#define arm_data_proc_flags_reg()                                             \
  arm_decode_data_proc_reg(opcode);                                           \
  calculate_reg_sh_flags()                                                    \

#define arm_data_proc_reg()                                                   \
  arm_decode_data_proc_reg(opcode);                                           \
  calculate_reg_sh()                                                          \

#define arm_data_proc_flags_imm()                                             \
  arm_decode_data_proc_imm(opcode)                                            \
  if(imm_ror)                                                                 \
    c_flag = (imm >> 31);  /* imm is rotated already! */                      \

#define arm_data_proc_imm()                                                   \
  arm_decode_data_proc_imm(opcode)                                            \

#define arm_data_proc(expr, type)                                             \
{                                                                             \
  u32 dest;                                                                   \
  arm_pc_offset(8);                                                           \
  arm_data_proc_##type();                                                     \
  dest = expr;                                                                \
  arm_pc_offset(-4);                                                          \
  reg[rd] = dest;                                                             \
}                                                                             \

#define flags_vars(src_a, src_b)                                              \
  u32 dest;                                                                   \
  const u32 _sa = src_a;                                                      \
  const u32 _sb = src_b                                                       \

#define arm_data_proc_logic_flags(expr, type)                                 \
{                                                                             \
  arm_pc_offset(8);                                                           \
  arm_data_proc_flags_##type();                                               \
  u32 dest = expr;                                                            \
  calculate_flags_logic(dest);                                                \
  arm_pc_offset(-4);                                                          \
  reg[rd] = dest;                                                             \
  arm_spsr_restore_check();                                                   \
}                                                                             \

#define arm_data_proc_add_flags(src_a, src_b, src_c, type)                    \
{                                                                             \
  u32 _sc = src_c;                                                            \
  arm_pc_offset(8);                                                           \
  arm_data_proc_##type();                                                     \
  flags_vars(src_a, src_b);                                                   \
  dest = _sa + _sb;                                                           \
  c_flag = (dest < _sb);                                                      \
  dest += _sc;                                                                \
  c_flag |= (dest < _sc);                                                     \
  calculate_flags_add(dest, _sa, _sb);                                        \
  arm_pc_offset(-4);                                                          \
  reg[rd] = dest;                                                             \
  arm_spsr_restore_check();                                                   \
}

#define arm_data_proc_sub_flags(src_a, src_b, src_c, type)                    \
{                                                                             \
  u32 _sc = src_c;                                                            \
  arm_pc_offset(8);                                                           \
  arm_data_proc_##type();                                                     \
  flags_vars(src_a, src_b);                                                   \
  dest = _sa + (~(_sb)) + _sc;                                                \
  calculate_flags_sub(dest, _sa, _sb, _sc);                                   \
  arm_pc_offset(-4);                                                          \
  reg[rd] = dest;                                                             \
  arm_spsr_restore_check();                                                   \
}                                                                             \

#define arm_data_proc_test_logic(expr, type)                                  \
{                                                                             \
  arm_pc_offset(8);                                                           \
  arm_data_proc_flags_##type();                                               \
  u32 dest = expr;                                                            \
  calculate_flags_logic(dest);                                                \
  arm_pc_offset(-4);                                                          \
}                                                                             \

#define arm_data_proc_test_add(src_a, src_b, type)                            \
{                                                                             \
  arm_pc_offset(8);                                                           \
  arm_data_proc_##type();                                                     \
  flags_vars(src_a, src_b);                                                   \
  dest = _sa + _sb;                                                           \
  c_flag = (dest < _sb);                                                      \
  calculate_flags_add(dest, _sa, _sb);                                        \
  arm_pc_offset(-4);                                                          \
}                                                                             \

#define arm_data_proc_test_sub(src_a, src_b, type)                            \
{                                                                             \
  arm_pc_offset(8);                                                           \
  arm_data_proc_##type();                                                     \
  flags_vars(src_a, src_b);                                                   \
  dest = _sa - _sb;                                                           \
  calculate_flags_sub(dest, _sa, _sb, 1);                                     \
  arm_pc_offset(-4);                                                          \
}                                                                             \

#define arm_multiply_flags_yes(_dest)                                         \
  calculate_z_flag(_dest);                                                    \
  calculate_n_flag(_dest);                                                    \

#define arm_multiply_flags_no(_dest)                                          \

#define arm_multiply_long_flags_yes(_dest_lo, _dest_hi)                       \
  z_flag = (_dest_lo == 0) & (_dest_hi == 0);                                 \
  calculate_n_flag(_dest_hi)                                                  \

#define arm_multiply_long_flags_no(_dest_lo, _dest_hi)                        \

#define arm_multiply(add_op, flags)                                           \
{                                                                             \
  u32 dest;                                                                   \
  arm_decode_multiply();                                                      \
  dest = (reg[rm] * reg[rs]) add_op;                                          \
  arm_multiply_flags_##flags(dest);                                           \
  reg[rd] = dest;                                                             \
  arm_pc_offset(4);                                                           \
}                                                                             \

#define arm_multiply_long_addop(type)                                         \
  + ((type##64)((((type##64)reg[rdhi]) << 32) | reg[rdlo]));                  \

#define arm_multiply_long(add_op, flags, type)                                \
{                                                                             \
  type##64 dest;                                                              \
  u32 dest_lo;                                                                \
  u32 dest_hi;                                                                \
  arm_decode_multiply_long();                                                 \
  dest = ((type##64)((type##32)reg[rm]) *                                     \
   (type##64)((type##32)reg[rn])) add_op;                                     \
  dest_lo = (u32)dest;                                                        \
  dest_hi = (u32)(dest >> 32);                                                \
  arm_multiply_long_flags_##flags(dest_lo, dest_hi);                          \
  reg[rdlo] = dest_lo;                                                        \
  reg[rdhi] = dest_hi;                                                        \
  arm_pc_offset(4);                                                           \
}                                                                             \

// Index by PRS fields (1 and 4 only!) and User-Privileged mode
// In user mode some bits are read only
// Bit #4 is always set to one (so all modes are 1XXXX)
// Reserved bits are always zero and cannot be modified
const u32 cpsr_masks[4][2] =
{
  // User, Privileged
  {0x00000000, 0x00000000},
  {0x00000020, 0x000000EF},
  {0xF0000000, 0xF0000000},
  {0xF0000020, 0xF00000EF}
};

// SPSR is always a privileged instruction
const u32 spsr_masks[4] = { 0x00000000, 0x000000EF, 0xF0000000, 0xF00000EF };

#define arm_psr_read(dummy, psr_reg)                                          \
  collapse_flags();                                                           \
  reg[rd] = psr_reg                                                           \

#define arm_psr_store_cpsr(source)                                            \
  collapse_flags();                                                           \
  const u32 store_mask = cpsr_masks[psr_pfield][PRIVMODE(reg[CPU_MODE])];     \
  reg[REG_CPSR] = (source & store_mask) | (reg[REG_CPSR] & (~store_mask));    \
  extract_flags();                                                            \
  if(store_mask & 0xFF)                                                       \
  {                                                                           \
    set_cpu_mode(cpu_modes[reg[REG_CPSR] & 0xF]);                             \
    check_for_interrupts();                                                   \
  }                                                                           \

#define arm_psr_store_spsr(source)                                            \
  const u32 store_mask = spsr_masks[psr_pfield];                              \
  u32 _psr = REG_SPSR(reg[CPU_MODE]);                                         \
  REG_SPSR(reg[CPU_MODE]) = (source & store_mask) | (_psr & (~store_mask))    \

#define arm_psr_store(source, psr_reg)                                        \
  arm_psr_store_##psr_reg(source)                                             \

#define arm_psr_src_reg reg[rm]

#define arm_psr_src_imm imm

#define arm_psr(op_type, transfer_type, psr_reg)                              \
{                                                                             \
  arm_decode_psr_##op_type(opcode);                                           \
  arm_pc_offset(4);                                                           \
  arm_psr_##transfer_type(arm_psr_src_##op_type, psr_reg);                    \
}                                                                             \

#define arm_data_trans_reg()                                                  \
  arm_decode_data_trans_reg();                                                \
  calculate_reg_offset()                                                      \

#define arm_data_trans_imm()                                                  \
  arm_decode_data_trans_imm()                                                 \

#define arm_data_trans_half_reg()                                             \
  arm_decode_half_trans_r()                                                   \

#define arm_data_trans_half_imm()                                             \
  arm_decode_half_trans_of()                                                  \

#define aligned_address_mask8  0xF0000000
#define aligned_address_mask16 0xF0000001
#define aligned_address_mask32 0xF0000003

/* Every mapped page is backed by an aligned allocation/array, and these
 * accessors are only used after checking the emulated address alignment.
 * Express that invariant in the effective Wasm address: an alignment hint
 * alone cannot prove a word load safe to the RV32 AOT compiler. */
#define cpu_readmap8(base, offset) readaddress8(base, offset)
#define cpu_readmap16(base, offset) \
  readaddress16((u8 *)(((uintptr_t)(base) + (offset)) & ~(uintptr_t)1), 0)
#define cpu_readmap32(base, offset) \
  readaddress32((u8 *)(((uintptr_t)(base) + (offset)) & ~(uintptr_t)3), 0)

#include "cpu_idle.h"
#include "cpu_fusion.h"
/* Reject ordinary branches before packing flags and borrowing stack locals
 * for the out-of-line polling analysis. */
#define idle_poll(edge_pc, thumb) do { \
  u32 target=reg[REG_PC]; \
  if(cycles_remaining>0 && target<(edge_pc) && (edge_pc)-target<=((thumb)?16u:32u)) \
    idle_poll_backedge(idle, edge_pc, thumb, \
      (n_flag << 3) | (z_flag << 2) | (c_flag << 1) | v_flag, cycles_remaining); \
  else if(idle.branch==(edge_pc))idle.branch=~0u; \
} while(0)

/* Plain memory has no I/O side effects. Keep palette/OAM/GPIO/EEPROM and
 * byte VRAM stores on the existing handlers. A native JIT still needs its
 * own self-modifying-code bookkeeping, so this is interpreter-only. */
template<unsigned bits> static inline bool store_plain_memory(u32 address, u32 value)
{
#ifndef HAVE_DYNAREC
  u8 *base;
  u32 offset;
  switch (address >> 24) {
  case 2: base=ewram; offset=address & 0x3ffff; break;
  case 3: base=iwram+0x8000; offset=address & 0x7fff; break;
  case 6:
    if (bits == 8) return false;
    base=vram; offset=address & 0x1ffff;
    if (offset >= 0x18000) offset-=0x8000;
    break;
  default: return false;
  }
  if (bits == 8) address8(base,offset)=eswap8((u8)value);
  if (bits == 16) address16(base,offset)=eswap16((u16)value);
  if (bits == 32) address32(base,offset)=eswap32(value);
  return true;
#else
  return false;
#endif
}

#define fast_read_memory(size, type, addr, dest, readfn)                      \
{                                                                             \
  u8 *map;                                                                    \
  u32 _address = addr;                                                        \
  bool _io_handler = ((_address >> 24) == 0x04) &&                            \
                     io_read_requires_handler(_address);                       \
                                                                              \
  if(_address < 0x10000000)                                                   \
  {                                                                           \
    /* Account for cycles and other stats */                                  \
    u8 region = _address >> 24;                                               \
    cycles_remaining -= ws_cyc_nseq[region][(size - 8) / 16];                 \
    STATS_MEMORY_ACCESS(read, type, region);                                  \
  }                                                                           \
                                                                              \
  if (                                                                        \
     (((_address >> 24) == 0) && (reg[REG_PC] >= 0x4000)) ||  /* BIOS read */ \
     (_address & aligned_address_mask##size) ||      /* Unaligned access */   \
     _io_handler ||                                  /* Special I/O read */   \
     !(map = memory_map_read[_address >> 15])        /* Unmapped memory */    \
  )                                                                           \
  {                                                                           \
    if (_io_handler)                                                          \
    {                                                                         \
      /* The interpreter has already advanced PC for this instruction.        \
       * read_open* expects the current instruction PC, like dynarec stubs. */ \
      u32 _saved_pc = reg[REG_PC];                                            \
      reg[REG_PC] -= (reg[REG_CPSR] & 0x20) ? 2 : 4;                         \
      dest = (type)(readfn)(_address);                                        \
      reg[REG_PC] = _saved_pc;                                                \
    }                                                                         \
    else                                                                      \
      dest = (type)(readfn)(_address);                                        \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    /* Aligned and mapped read */                                             \
    dest = (type)cpu_readmap##size(map, (_address & 0x7FFF));                 \
  }                                                                           \
}                                                                             \

#define fast_write_memory(size, type, address, value)                         \
{                                                                             \
  fusion_write_barrier();                                                     \
  u32 _address = (address) & ~(aligned_address_mask##size & 0x03);            \
  if(_address < 0x10000000)                                                   \
  {                                                                           \
    u8 region = _address >> 24;                                               \
    cycles_remaining -= ws_cyc_nseq[region][(size - 8) / 16];                 \
    STATS_MEMORY_ACCESS(write, type, region);                                 \
  }                                                                           \
                                                                              \
  if (!store_plain_memory<size>(_address, value)) {                           \
    collapse_flags();                                                         \
    cpu_alert |= write_memory##size(_address, value);                         \
  }                                                                           \
}                                                                             \

#define load_aligned32(address, dest)                                         \
{                                                                             \
  u32 _address = address;                                                     \
  u8 *map = _address < 0x10000000 ? memory_map_read[_address >> 15] : NULL;    \
  bool _io_handler = ((_address >> 24) == 0x04) &&                            \
                     io_read_requires_handler(_address);                       \
  if(_address < 0x10000000)                                                   \
  {                                                                           \
    /* Account for cycles and other stats */                                  \
    u8 region = _address >> 24;                                               \
    cycles_remaining -= ws_cyc_seq[region][1];                                \
    STATS_MEMORY_ACCESS(read, u32, region);                                   \
  }                                                                           \
  if(_address < 0x10000000 && map && !_io_handler)                            \
  {                                                                           \
    dest = cpu_readmap32(map, _address & 0x7FFF);                             \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    if (_io_handler)                                                          \
    {                                                                         \
      u32 _saved_pc = reg[REG_PC];                                            \
      reg[REG_PC] -= (reg[REG_CPSR] & 0x20) ? 2 : 4;                         \
      dest = read_memory32(_address);                                         \
      reg[REG_PC] = _saved_pc;                                                \
    }                                                                         \
    else                                                                      \
      dest = read_memory32(_address);                                         \
  }                                                                           \
}                                                                             \

#define store_aligned32(address, value)                                       \
{                                                                             \
  fusion_write_barrier();                                                     \
  u32 _address = address;                                                     \
  if(_address < 0x10000000)                                                   \
  {                                                                           \
    /* Account for cycles and other stats */                                  \
    u8 region = _address >> 24;                                               \
    cycles_remaining -= ws_cyc_seq[region][1];                                \
    STATS_MEMORY_ACCESS(write, u32, region);                                  \
  }                                                                           \
  if (!store_plain_memory<32>(_address, value))                               \
    cpu_alert |= write_memory32(_address, value);                             \
}                                                                             \

#define load_memory_u8(address, dest)                                         \
  fast_read_memory(8, u8, address, dest, read_memory8)                        \

#define load_memory_u16(address, dest)                                        \
  fast_read_memory(16, u16, address, dest, read_memory16)                     \

#define load_memory_u32(address, dest)                                        \
  fast_read_memory(32, u32, address, dest, read_memory32)                     \

#define load_memory_s8(address, dest)                                         \
  fast_read_memory(8, s8, address, dest, read_memory8)                        \

#define load_memory_s16(address, dest)                                        \
  fast_read_memory(16, s16, address, dest, read_memory16_signed)              \

#define store_memory_u8(address, value)                                       \
  fast_write_memory(8, u8, address, value)                                    \

#define store_memory_u16(address, value)                                      \
  fast_write_memory(16, u16, address, value)                                  \

#define store_memory_u32(address, value)                                      \
  fast_write_memory(32, u32, address, value)                                  \

#define no_op                                                                 \

#define arm_access_memory_writeback_yes(off_op)                               \
  reg[rn] = address off_op                                                    \

#define arm_access_memory_writeback_no(off_op)                                \

#define arm_access_memory_pc_preadjust_load()                                 \

#define arm_access_memory_pc_preadjust_store()                                \
  u32 reg_op = reg[rd];                                                       \
  if(rd == 15)                                                                \
    reg_op += 4                                                               \

#define load_reg_op reg[rd]                                                   \

#define store_reg_op reg_op                                                   \

#define arm_access_memory(access_type, off_op, off_type, mem_type,            \
 wb, wb_off_op)                                                               \
{                                                                             \
  arm_pc_offset(8);                                                           \
  arm_data_trans_##off_type();                                                \
  u32 address = reg[rn] off_op;                                               \
  arm_access_memory_pc_preadjust_##access_type();                             \
                                                                              \
  arm_pc_offset(-4);                                                          \
  arm_access_memory_writeback_##wb(wb_off_op);                                \
  access_type##_memory_##mem_type(address, access_type##_reg_op);             \
}                                                                             \

// Excutes an LDM/STM instruction

typedef enum { AccLoad, AccStore } AccMode;
typedef enum { AddrPreInc, AddrPreDec, AddrPostInc, AddrPostDec } AddrMode;

template<AccMode mode, bool writeback, bool sbit, AddrMode addr_mode>
inline cpu_alert_type exec_arm_block_mem(u32 rn, u32 reglist, s32 &cycles_remaining) {
  cpu_alert_type cpu_alert = CPU_ALERT_NONE;
  // Register register usage stats.
  using_register(arm, rn, memory_base);
  using_register_list(arm, reglist, 16);

  // Calcualte base address.
  u32 base = reg[rn];
  u32 numops = (bit_count[reglist >> 8] + bit_count[reglist & 0xFF]);
  s32 addr_off = (addr_mode == AddrPreInc || addr_mode == AddrPostInc) ? 4 : -4;  // Address incr/decr amount.
  u32 endaddr = base + addr_off * numops;

  u32 address = (addr_mode == AddrPreInc)  ? base + 4 :
                (addr_mode == AddrPostInc) ? base :
                (addr_mode == AddrPreDec)  ? endaddr : endaddr + 4;
  address &= ~3U;

  // If sbit is set, change to user mode and back, so to write the user regs.
  // However for LDM {PC} we restore CPSR from SPSR.
  // TODO: implement CPSR restore, only USER mode is now implemented.
  u32 old_cpsr = reg[REG_CPSR];
  if (sbit && (mode == AccStore || rn != REG_PC))
    set_cpu_mode(MODE_USER);

  // If base is in the reglist and writeback is enabled, the value of the
  // written register depends on the write cycle (ARM7TDM manual 4.11.6).
  // If the register is the first, the written value is the original value,
  // otherwise the update base register is written. For LDM loaded date
  // takes always precendence.
  bool wrbck_base = (1 << rn) & reglist;
  bool base_first = (((1 << rn) - 1) & reglist) == 0;
  bool writeback_first = (mode == AccLoad) || !(wrbck_base && base_first);

  if (writeback && writeback_first)
    reg[rn] = endaddr;

  arm_pc_offset(4);  // Advance PC

  for (u32 i = 0; i < 16; i++)  {
    if ((reglist >> i) & 0x01) {
      if (mode == AccLoad) {
        load_aligned32(address, reg[i]);
      } else {
        store_aligned32(address, (i == REG_PC) ? reg[REG_PC] + 4 : reg[i]);
      }
      address += 4;
    }
  }

  if (writeback && !writeback_first)
    reg[rn] = endaddr;

  if (sbit && (mode == AccStore || rn != REG_PC))
    set_cpu_mode(cpu_modes[old_cpsr & 0xF]);

  return cpu_alert;
}

template<AccMode mode, AddrMode addr_mode>
inline cpu_alert_type exec_thumb_block_mem(u32 rn, u32 reglist, s32 &cycles_remaining) {
  cpu_alert_type cpu_alert = CPU_ALERT_NONE;
  // Register register usage stats.
  using_register(arm, rn, memory_base);
  using_register_list(arm, reglist, 16);

  // Calcualte base address.
  u32 base = reg[rn];
  u32 numops = bit_count[reglist & 0xFF] + (bit_count[reglist >> 8] ? 1 : 0);
  s32 addr_off = (addr_mode == AddrPreInc || addr_mode == AddrPostInc) ? 4 : -4;  // Address incr/decr amount.
  u32 endaddr = base + addr_off * numops;

  u32 address = (addr_mode == AddrPreInc)  ? base + 4 :
                (addr_mode == AddrPostInc) ? base :
                (addr_mode == AddrPreDec)  ? endaddr : endaddr + 4;
  address &= ~3U;

  // Similar to the ARM version, just a bit simpler. See above.
  bool wrbck_base = (1 << rn) & reglist;
  bool base_first = (((1 << rn) - 1) & reglist) == 0;
  bool writeback_first = (mode == AccLoad) || !(wrbck_base && base_first);

  if (writeback_first)
    reg[rn] = endaddr;

  thumb_pc_offset(2);  // Advance PC

  if (mode == AccLoad) {
    for (u32 i = 0; i < 8; i++)  {
      if ((reglist >> i) & 0x01) {
        load_aligned32(address, reg[i]);
        address += 4;
      }
    }
    if (reglist & (1 << REG_PC)) {
      load_aligned32(address, reg[REG_PC]);
      reg[REG_PC] &= ~0x01;
    }
  } else {
    for (u32 i = 0; i < 8; i++)  {
      if ((reglist >> i) & 0x01) {
        store_aligned32(address, reg[i]);
        address += 4;
      }
    }
    if (reglist & (1 << REG_LR)) {
      store_aligned32(address, reg[REG_LR]);
    }
  }

  if (!writeback_first)
    reg[rn] = endaddr;

  return cpu_alert;
}

#define arm_swap(type)                                                        \
{                                                                             \
  arm_decode_swap();                                                          \
  u32 temp;                                                                   \
  load_memory_##type(reg[rn], temp);                                          \
  store_memory_##type(reg[rn], reg[rm]);                                      \
  reg[rd] = temp;                                                             \
  arm_pc_offset(4);                                                           \
}                                                                             \

// Types: add_sub, add_sub_imm, alu_op, imm
// Affects N/Z/C/V flags

#define thumb_add(type, dest_reg, src_a, src_b, src_c)                        \
{                                                                             \
  const u32 _sc = src_c;                                                      \
  thumb_decode_##type();                                                      \
  const u32 _sa = src_a;                                                      \
  const u32 _sb = src_b;                                                      \
  u32 dest = _sa + _sb;                                                       \
  c_flag = (dest < _sb);                                                      \
  dest += _sc;                                                                \
  c_flag |= (dest < _sc);                                                     \
  calculate_flags_add(dest, _sa, _sb);                                        \
  reg[dest_reg] = dest;                                                       \
  thumb_pc_offset(2);                                                         \
}                                                                             \

#define thumb_add_noflags(type, dest_reg, src_a, src_b)                       \
{                                                                             \
  thumb_decode_##type();                                                      \
  u32 dest = (src_a) + (src_b);                                               \
  reg[dest_reg] = dest;                                                       \
  thumb_pc_offset(2);                                                         \
}                                                                             \

#define thumb_sub(type, dest_reg, src_a, src_b, src_c)                        \
{                                                                             \
  thumb_decode_##type();                                                      \
  const u32 _sa = src_a;                                                      \
  const u32 _sb = src_b;                                                      \
  const u32 _sc = src_c;                                                      \
  u32 dest = _sa + (~_sb) + _sc;                                              \
  calculate_flags_sub(dest, _sa, _sb, _sc);                                   \
  reg[dest_reg] = dest;                                                       \
  thumb_pc_offset(2);                                                         \
}                                                                             \

// Affects N/Z flags

#define thumb_logic(type, dest_reg, expr)                                     \
{                                                                             \
  thumb_decode_##type();                                                      \
  u32 dest = expr;                                                            \
  calculate_flags_logic(dest);                                                \
  reg[dest_reg] = dest;                                                       \
  thumb_pc_offset(2);                                                         \
}                                                                             \

// Decode types: shift, alu_op
// Operation types: lsl, lsr, asr, ror
// Affects N/Z/C flags

#define thumb_shift_lsl_reg()                                                 \
  u32 shift = reg[rs];                                                        \
  u32 dest = reg[rd];                                                         \
  if(shift != 0)                                                              \
  {                                                                           \
    if(shift > 31)                                                            \
    {                                                                         \
      if(shift == 32)                                                         \
        c_flag = dest & 0x01;                                                 \
      else                                                                    \
        c_flag = 0;                                                           \
      dest = 0;                                                               \
    }                                                                         \
    else                                                                      \
    {                                                                         \
      c_flag = (dest >> (32 - shift)) & 0x01;                                 \
      dest <<= shift;                                                         \
    }                                                                         \
  }                                                                           \

#define thumb_shift_lsr_reg()                                                 \
  u32 shift = reg[rs];                                                        \
  u32 dest = reg[rd];                                                         \
  if(shift != 0)                                                              \
  {                                                                           \
    if(shift > 31)                                                            \
    {                                                                         \
      if(shift == 32)                                                         \
        c_flag = dest >> 31;                                                  \
      else                                                                    \
        c_flag = 0;                                                           \
      dest = 0;                                                               \
    }                                                                         \
    else                                                                      \
    {                                                                         \
      c_flag = (dest >> (shift - 1)) & 0x01;                                  \
      dest >>= shift;                                                         \
    }                                                                         \
  }                                                                           \

#define thumb_shift_asr_reg()                                                 \
  u32 shift = reg[rs];                                                        \
  u32 dest = reg[rd];                                                         \
  if(shift != 0)                                                              \
  {                                                                           \
    if(shift > 31)                                                            \
    {                                                                         \
      dest = (s32)dest >> 31;                                                 \
      c_flag = dest & 0x01;                                                   \
    }                                                                         \
    else                                                                      \
    {                                                                         \
      c_flag = (dest >> (shift - 1)) & 0x01;                                  \
      dest = (s32)dest >> shift;                                              \
    }                                                                         \
  }                                                                           \

#define thumb_shift_ror_reg()                                                 \
  u32 shift = reg[rs];                                                        \
  u32 dest = reg[rd];                                                         \
  if(shift != 0)                                                              \
  {                                                                           \
    c_flag = (dest >> (shift - 1)) & 0x01;                                    \
    ror(dest, dest, shift);                                                   \
  }                                                                           \

#define thumb_shift_lsl_imm()                                                 \
  u32 dest = reg[rs];                                                         \
  if(imm != 0)                                                                \
  {                                                                           \
    c_flag = (dest >> (32 - imm)) & 0x01;                                     \
    dest <<= imm;                                                             \
  }                                                                           \

#define thumb_shift_lsr_imm()                                                 \
  u32 dest;                                                                   \
  if(imm == 0)                                                                \
  {                                                                           \
    dest = 0;                                                                 \
    c_flag = reg[rs] >> 31;                                                   \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    dest = reg[rs];                                                           \
    c_flag = (dest >> (imm - 1)) & 0x01;                                      \
    dest >>= imm;                                                             \
  }                                                                           \

#define thumb_shift_asr_imm()                                                 \
  u32 dest;                                                                   \
  if(imm == 0)                                                                \
  {                                                                           \
    dest = (s32)reg[rs] >> 31;                                                \
    c_flag = dest & 0x01;                                                     \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    dest = reg[rs];                                                           \
    c_flag = (dest >> (imm - 1)) & 0x01;                                      \
    dest = (s32)dest >> imm;                                                  \
  }                                                                           \

#define thumb_shift_ror_imm()                                                 \
  u32 dest = reg[rs];                                                         \
  if(imm == 0)                                                                \
  {                                                                           \
    u32 old_c_flag = c_flag;                                                  \
    c_flag = dest & 0x01;                                                     \
    dest = (dest >> 1) | (old_c_flag << 31);                                  \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    c_flag = (dest >> (imm - 1)) & 0x01;                                      \
    ror(dest, dest, imm);                                                     \
  }                                                                           \

#define thumb_shift(decode_type, op_type, value_type)                         \
{                                                                             \
  thumb_decode_##decode_type();                                               \
  thumb_shift_##op_type##_##value_type();                                     \
  calculate_flags_logic(dest);                                                \
  reg[rd] = dest;                                                             \
  thumb_pc_offset(2);                                                         \
}                                                                             \

#define thumb_test_add(type, src_a, src_b)                                    \
{                                                                             \
  thumb_decode_##type();                                                      \
  const u32 _sa = src_a;                                                      \
  const u32 _sb = src_b;                                                      \
  u32 dest = _sa + _sb;                                                       \
  c_flag = (dest < _sb);                                                      \
  calculate_flags_add(dest, src_a, src_b);                                    \
  thumb_pc_offset(2);                                                         \
}                                                                             \

#define thumb_test_sub(type, src_a, src_b)                                    \
{                                                                             \
  thumb_decode_##type();                                                      \
  const u32 _sa = src_a;                                                      \
  const u32 _sb = src_b;                                                      \
  u32 dest = _sa - _sb;                                                       \
  calculate_flags_sub(dest, src_a, src_b, 1);                                 \
  thumb_pc_offset(2);                                                         \
}                                                                             \

#define thumb_test_logic(type, expr)                                          \
{                                                                             \
  thumb_decode_##type();                                                      \
  u32 dest = expr;                                                            \
  calculate_flags_logic(dest);                                                \
  thumb_pc_offset(2);                                                         \
}

#define thumb_hireg_op(expr)                                                  \
{                                                                             \
  thumb_pc_offset(4);                                                         \
  thumb_decode_hireg_op();                                                    \
  u32 dest = expr;                                                            \
  thumb_pc_offset(-2);                                                        \
  if(rd == 15)                                                                \
  {                                                                           \
    reg[REG_PC] = dest & ~0x01;                                               \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    reg[rd] = dest;                                                           \
  }                                                                           \
}                                                                             \

// Operation types: imm, mem_reg, mem_imm

#define thumb_access_memory(access_type, op_type, address, reg_op,            \
 mem_type)                                                                    \
{                                                                             \
  thumb_pc_offset(2);                                                         \
  thumb_decode_##op_type();                                                   \
  access_type##_memory_##mem_type(address, reg_op);                           \
}                                                                             \

#define thumb_conditional_branch(condition)                                   \
{                                                                             \
  thumb_decode_branch_cond();                                                 \
  u32 branch_pc = reg[REG_PC];                                                \
  if(condition)                                                               \
  {                                                                           \
    thumb_pc_offset((offset * 2) + 4);                                        \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    thumb_pc_offset(2);                                                       \
  }                                                                           \
  cycles_remaining -= ws_cyc_nseq[reg[REG_PC] >> 24][0];                      \
  idle_poll(branch_pc, true);                                                 \
}                                                                             \

// When a mode change occurs from non-FIQ to non-FIQ retire the current
// reg[13] and reg[14] into reg_mode[cpu_mode][5] and reg_mode[cpu_mode][6]
// respectively and load into reg[13] and reg[14] reg_mode[new_mode][5] and
// reg_mode[new_mode][6]. When swapping to/from FIQ retire/load reg[8]
// through reg[14] to/from reg_mode[MODE_FIQ][0] through reg_mode[MODE_FIQ][6].

const u32 cpu_modes[16] =
{
  MODE_USER, MODE_FIQ, MODE_IRQ, MODE_SUPERVISOR,
  MODE_INVALID, MODE_INVALID, MODE_INVALID, MODE_ABORT,
  MODE_INVALID, MODE_INVALID, MODE_INVALID, MODE_UNDEFINED, 
  MODE_INVALID, MODE_INVALID, MODE_INVALID, MODE_SYSTEM
};

// ARM/Thumb mode is stored in the flags directly, this is simpler than
// shadowing it since it has a constant 1bit represenation.

u32 instruction_count = 0;

void set_cpu_mode(cpu_mode_type new_mode)
{
  cpu_mode_type cpu_mode = reg[CPU_MODE];

  if(cpu_mode == new_mode)
     return;

  if(new_mode == MODE_FIQ)
  {
     for (u32 i = 8; i < 15; i++)
        REG_MODE(cpu_mode)[i - 8] = reg[i];
  }
  else
  {
     REG_MODE(cpu_mode)[5] = reg[REG_SP];
     REG_MODE(cpu_mode)[6] = reg[REG_LR];
  }

  if(cpu_mode == MODE_FIQ)
  {
     for (u32 i = 8; i < 15; i++)
        reg[i] = REG_MODE(new_mode)[i - 8];
  }
  else
  {
     reg[REG_SP] = REG_MODE(new_mode)[5];
     reg[REG_LR] = REG_MODE(new_mode)[6];
  }

  reg[CPU_MODE] = new_mode;
}

#define cpu_has_interrupt()                                 \
  (!(reg[REG_CPSR] & 0x80) && read_ioreg(REG_IME) &&        \
    (read_ioreg(REG_IE) & read_ioreg(REG_IF)))

// Returns whether the CPU has a pending interrupt.
cpu_alert_type check_interrupt() {
  return (cpu_has_interrupt()) ? CPU_ALERT_IRQ : CPU_ALERT_NONE;
}

// Checks for pending IRQs and raises them. This changes the CPU mode
// which means that it must be called with a valid CPU state.
u32 check_and_raise_interrupts()
{
  // Check any IRQ flag pending, IME and CPSR-IRQ enabled
  if (cpu_has_interrupt())
  {
    // Value after the FIQ returns, should be improved
    reg[REG_BUS_VALUE] = 0xe55ec002;

    // Interrupt handler in BIOS
    REG_MODE(MODE_IRQ)[6] = reg[REG_PC] + 4;
    REG_SPSR(MODE_IRQ) = reg[REG_CPSR];
    reg[REG_CPSR] = 0xD2;
    reg[REG_PC] = 0x00000018;

    set_cpu_mode(MODE_IRQ);

    // Wake up CPU if it is stopped/sleeping.
    if (reg[CPU_HALT_STATE] == CPU_STOP ||
        reg[CPU_HALT_STATE] == CPU_HALT)
      reg[CPU_HALT_STATE] = CPU_ACTIVE;

    return 1;
  }
  return 0;
}

// This function marks a pending interrupt but does not raise it.
// It simply updates IF register and returns whether the IRQ needs
// to be raised (that is, IE/IME/CPSR enable the IRQ).
// Safe to call via dynarec without proper registers saved.
cpu_alert_type flag_interrupt(irq_type irq_raised)
{
  // Flag interrupt
  write_ioreg(REG_IF, read_ioreg(REG_IF) | irq_raised);

  return check_interrupt();
}

#ifndef HAVE_DYNAREC

// When switching modes set spsr[new_mode] to cpsr. Modifying PC as the
// target of a data proc instruction will set cpsr to spsr[cpu_mode].
u32 reg[64];
u32 spsr[6];
u32 reg_mode[7][7];

u8 *memory_map_read [8 * 1024];
u16 oam_ram[512];
u16 palette_ram[512];
u16 palette_ram_converted[512];
u8 ewram[1024 * 256 * 2];
u8 iwram[1024 * 32 * 2];
u8 vram[1024 * 96];
u16 io_registers[512];
#endif

/* Crossing ARM/Thumb mode exits this slice. Keeping the two dispatch loops
 * in separate functions avoids an irreducible control-flow graph in Wasm.
 * NZCV stays in local variables between instructions. Publish it before PSR
 * accesses, memory writes, cheats, hardware updates and every slice exit;
 * publishing on every ALU/branch instruction adds needless guest stores.
 * The cycle budget is local too; only slice exits publish it to the caller. */
#define thumb_loop mode_changed
static bool execute_arm_slice(s32 &next_cycles, bool vram_only, u32 &result)
{
  s32 cycles_remaining = next_cycles;
  u32 opcode, condition;
  u32 n_flag, z_flag, c_flag, v_flag;
  u32 pc_region = reg[REG_PC] >> 15, new_pc_region;
  u8 *pc_address_block = memory_map_read[pc_region];
  cpu_alert_type cpu_alert = CPU_ALERT_NONE;
  cpu_idle_state idle;
  fusion_write_barrier();
  if (!pc_address_block) pc_address_block = load_gamepak_page(pc_region & 0x3FF);
  touch_gamepak_page(pc_region);
  extract_flags();
  do
  {
arm_loop:
    if (vram_only && (reg[REG_PC] >> 24) != 0x06) {
      collapse_flags();
      result = cycles_remaining > 0 ? (u32)cycles_remaining : update_gba(cycles_remaining);
      next_cycles = cycles_remaining;
      return true;
    }
    if (reg[REG_PC] == cheat_master_hook) { collapse_flags(); fusion_write_barrier(); process_cheats(); }
#include "cpu_arm_step.inc"
  } while (cycles_remaining > 0);
  collapse_flags();
  result = update_gba(cycles_remaining);
  if (completed_frame(result)) { next_cycles = cycles_remaining; return true; }
  cycles_remaining = cycles_to_run(result);
  next_cycles = cycles_remaining;
  return false;
mode_changed:
  collapse_flags();
  next_cycles = cycles_remaining;
  return false;
}
#undef thumb_loop

/* Crossing ARM/Thumb mode exits this slice. Keeping the two dispatch loops
 * in separate functions avoids an irreducible control-flow graph in Wasm. */
#define arm_loop mode_changed
static bool execute_thumb_slice(s32 &next_cycles, bool vram_only, u32 &result)
{
  s32 cycles_remaining = next_cycles;
  u32 opcode, condition;
  u32 n_flag, z_flag, c_flag, v_flag;
  u32 pc_region = reg[REG_PC] >> 15, new_pc_region;
  u8 *pc_address_block = memory_map_read[pc_region];
  cpu_alert_type cpu_alert = CPU_ALERT_NONE;
  cpu_idle_state idle;
  fusion_write_barrier();
  if (!pc_address_block) pc_address_block = load_gamepak_page(pc_region & 0x3FF);
  touch_gamepak_page(pc_region);
  extract_flags();
  do
  {
thumb_loop:
    if (vram_only && (reg[REG_PC] >> 24) != 0x06) {
      collapse_flags();
      result = cycles_remaining > 0 ? (u32)cycles_remaining : update_gba(cycles_remaining);
      next_cycles = cycles_remaining;
      return true;
    }
    if (reg[REG_PC] == cheat_master_hook) { collapse_flags(); fusion_write_barrier(); process_cheats(); }
#include "cpu_thumb_step.inc"
  } while (cycles_remaining > 0);
  collapse_flags();
  result = update_gba(cycles_remaining);
  if (completed_frame(result)) { next_cycles = cycles_remaining; return true; }
  cycles_remaining = cycles_to_run(result);
  next_cycles = cycles_remaining;
  return false;
mode_changed:
  collapse_flags();
  next_cycles = cycles_remaining;
  return false;
}
#undef arm_loop

static u32 execute_arm_internal(u32 cycles, bool vram_only)
{
  s32 remaining = (s32)cycles;
  u32 result;
  for (;;) {
    if (reg[CPU_HALT_STATE] != CPU_ACTIVE) {
      result = update_gba(remaining);
      if (completed_frame(result)) return result;
      remaining = cycles_to_run(result);
    }
    bool finished = (reg[REG_CPSR] & 0x20)
        ? execute_thumb_slice(remaining, vram_only, result)
        : execute_arm_slice(remaining, vram_only, result);
    if (finished) return result;
  }
}

void execute_arm(u32 cycles)
{
  execute_arm_internal(cycles, false);
}

u32 function_cc execute_arm_vram(u32 cycles)
{
  /* VRAM instructions are never cached. This also covers code rewritten by
   * scalar stores, store-multiple instructions, DMA and mirrored addresses. */
  u32 result;
  if ((s32)cycles <= 0) {
    result = update_gba(cycles);
    if (!completed_frame(result) && (reg[REG_PC] >> 24) == 0x06)
      result = execute_arm_internal(cycles_to_run(result), true);
  } else {
    result = execute_arm_internal(cycles, true);
  }
#ifdef HAVE_DYNAREC
  /* Interpreted routines may write I/EWRAM without the JIT's SMC handlers.
   * Discard those translations before returning to compiled execution. */
  flush_translation_cache_ram();
#endif
  return result;
}

void init_cpu(void)
{
  // Initialize CPU registers
  memset(reg, 0, REG_USERDEF * sizeof(u32));
  memset(reg_mode, 0, sizeof(reg_mode));
  for (u32 i = 0; i < sizeof(spsr)/sizeof(spsr[0]); i++)
    spsr[i] = 0x00000010;

  reg[CPU_HALT_STATE] = CPU_ACTIVE;
  reg[REG_SLEEP_CYCLES] = 0;

  bool force_game_boot = false;
  if (selected_boot_mode == boot_bios)
  {
    u8 *rom0 = memory_map_read[0x8000000 / (32 * 1024)];
    if (!rom0 || memcmp(&rom0[0x04], gba_header_logo, sizeof(gba_header_logo)) != 0)
      force_game_boot = true;

  }

  if (selected_boot_mode == boot_game || force_game_boot) {
    reg[REG_SP] = 0x03007F00;
    reg[REG_PC] = 0x08000000;
    reg[REG_CPSR] = 0x0000001F;   // system mode
    reg[CPU_MODE] = MODE_SYSTEM;
  } else {
    reg[REG_SP] = 0x03007F00;
    reg[REG_PC] = 0x00000000;
    reg[REG_CPSR] = 0x00000013 | 0xC0;  // supervisor
    reg[CPU_MODE] = MODE_SUPERVISOR;
  }

  // Stack pointers are set by BIOS, we set them
  // nevertheless, should we not boot from BIOS
  REG_MODE(MODE_USER)[5] = 0x03007F00;
  REG_MODE(MODE_IRQ)[5] = 0x03007FA0;
  REG_MODE(MODE_FIQ)[5] = 0x03007FA0;
  REG_MODE(MODE_SUPERVISOR)[5] = 0x03007FE0;
}

bool cpu_check_savestate(const u8 *src)
{
  const u8 *cpudoc = bson_find_key(src, "cpu");
  if (!cpudoc)
    return false;

  return bson_contains_key(cpudoc, "bus-value", BSON_TYPE_INT32) &&
         bson_contains_key(cpudoc, "regs", BSON_TYPE_ARR) &&
         bson_contains_key(cpudoc, "spsr", BSON_TYPE_ARR) &&
         bson_contains_key(cpudoc, "regmod", BSON_TYPE_ARR);
}


bool cpu_read_savestate(const u8 *src)
{
  const u8 *cpudoc = bson_find_key(src, "cpu");
  return bson_read_int32(cpudoc, "bus-value", &reg[REG_BUS_VALUE]) &&
         bson_read_int32_array(cpudoc, "regs", reg, REG_ARCH_COUNT) &&
         bson_read_int32_array(cpudoc, "spsr", spsr, 6) &&
         bson_read_int32_array(cpudoc, "regmod", (u32*)reg_mode, 7*7);
}

unsigned cpu_write_savestate(u8 *dst)
{
  u8 *wbptr, *startp = dst;
  bson_start_document(dst, "cpu", wbptr);
  bson_write_int32array(dst, "regs", reg, REG_ARCH_COUNT);
  bson_write_int32array(dst, "spsr", spsr, 6);
  bson_write_int32array(dst, "regmod", reg_mode, 7*7);
  bson_write_int32(dst, "bus-value", reg[REG_BUS_VALUE]);

  bson_finish_document(dst, wbptr);
  return (unsigned int)(dst - startp);
}
