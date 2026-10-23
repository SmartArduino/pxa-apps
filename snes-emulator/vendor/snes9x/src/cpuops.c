#include "../../../apu_sync.h"
/* This file is part of Snes9x. See LICENSE file. */

/*****************************************************************************/
/* CPU-S9xOpcodes.CPP                                                        */
/* This file contains all the opcodes                                        */
/*****************************************************************************/

#include "snes9x.h"
#include "memmap.h"
#include "apu.h"
#include "cpuexec.h"
#include "cpuaddr.h"
#include "cpuops.h"
#include "cpumacro.h"


/* ADC */
static inline __attribute__((always_inline)) void Op69M1(void)
{
   Immediate8();
   ADC8();
}

static inline __attribute__((always_inline)) void Op69M0(void)
{
   Immediate16();
   ADC16();
}

static inline __attribute__((always_inline)) void Op65M1(void)
{
   Direct(true);
   ADC8();
}

static inline __attribute__((always_inline)) void Op65M0(void)
{
   Direct(true);
   ADC16();
}

static inline __attribute__((always_inline)) void Op75M1(void)
{
   DirectIndexedX(true);
   ADC8();
}

static inline __attribute__((always_inline)) void Op75M0(void)
{
   DirectIndexedX(true);
   ADC16();
}

static inline __attribute__((always_inline)) void Op72M1(void)
{
   DirectIndirect(true);
   ADC8();
}

static inline __attribute__((always_inline)) void Op72M0(void)
{
   DirectIndirect(true);
   ADC16();
}

static inline __attribute__((always_inline)) void Op61M1(void)
{
   DirectIndexedIndirect(true);
   ADC8();
}

static inline __attribute__((always_inline)) void Op61M0(void)
{
   DirectIndexedIndirect(true);
   ADC16();
}

static inline __attribute__((always_inline)) void Op71M1(void)
{
   DirectIndirectIndexed(true);
   ADC8();
}

static inline __attribute__((always_inline)) void Op71M0(void)
{
   DirectIndirectIndexed(true);
   ADC16();
}

static inline __attribute__((always_inline)) void Op67M1(void)
{
   DirectIndirectLong(true);
   ADC8();
}

static inline __attribute__((always_inline)) void Op67M0(void)
{
   DirectIndirectLong(true);
   ADC16();
}

static inline __attribute__((always_inline)) void Op77M1(void)
{
   DirectIndirectIndexedLong(true);
   ADC8();
}

static inline __attribute__((always_inline)) void Op77M0(void)
{
   DirectIndirectIndexedLong(true);
   ADC16();
}

static inline __attribute__((always_inline)) void Op6DM1(void)
{
   Absolute(true);
   ADC8();
}

static inline __attribute__((always_inline)) void Op6DM0(void)
{
   Absolute(true);
   ADC16();
}

static inline __attribute__((always_inline)) void Op7DM1(void)
{
   AbsoluteIndexedX(true);
   ADC8();
}

static inline __attribute__((always_inline)) void Op7DM0(void)
{
   AbsoluteIndexedX(true);
   ADC16();
}

static inline __attribute__((always_inline)) void Op79M1(void)
{
   AbsoluteIndexedY(true);
   ADC8();
}

static inline __attribute__((always_inline)) void Op79M0(void)
{
   AbsoluteIndexedY(true);
   ADC16();
}

static inline __attribute__((always_inline)) void Op6FM1(void)
{
   AbsoluteLong(true);
   ADC8();
}

static inline __attribute__((always_inline)) void Op6FM0(void)
{
   AbsoluteLong(true);
   ADC16();
}

static inline __attribute__((always_inline)) void Op7FM1(void)
{
   AbsoluteLongIndexedX(true);
   ADC8();
}

static inline __attribute__((always_inline)) void Op7FM0(void)
{
   AbsoluteLongIndexedX(true);
   ADC16();
}

static inline __attribute__((always_inline)) void Op63M1(void)
{
   StackRelative(true);
   ADC8();
}

static inline __attribute__((always_inline)) void Op63M0(void)
{
   StackRelative(true);
   ADC16();
}

static inline __attribute__((always_inline)) void Op73M1(void)
{
   StackRelativeIndirectIndexed(true);
   ADC8();
}

static inline __attribute__((always_inline)) void Op73M0(void)
{
   StackRelativeIndirectIndexed(true);
   ADC16();
}

/* AND */
static inline __attribute__((always_inline)) void Op29M1(void)
{
   ICPU.Registers.AL &= *CPU.PC++;
#ifndef SA1_OPCODES
   CPU.Cycles += CPU.MemSpeed;
#endif
   SetZN8(ICPU.Registers.AL);
}

static inline __attribute__((always_inline)) void Op29M0(void)
{
#ifdef FAST_LSB_WORD_ACCESS
   ICPU.Registers.A.W &= *(uint16_t*) CPU.PC;
#else
   ICPU.Registers.A.W &= *CPU.PC + (*(CPU.PC + 1) << 8);
#endif
   CPU.PC += 2;
#ifndef SA1_OPCODES
   CPU.Cycles += CPU.MemSpeedx2;
#endif
   SetZN16(ICPU.Registers.A.W);
}

static inline __attribute__((always_inline)) void Op25M1(void)
{
   Direct(true);
   AND8();
}

static inline __attribute__((always_inline)) void Op25M0(void)
{
   Direct(true);
   AND16();
}

static inline __attribute__((always_inline)) void Op35M1(void)
{
   DirectIndexedX(true);
   AND8();
}

static inline __attribute__((always_inline)) void Op35M0(void)
{
   DirectIndexedX(true);
   AND16();
}

static inline __attribute__((always_inline)) void Op32M1(void)
{
   DirectIndirect(true);
   AND8();
}

static inline __attribute__((always_inline)) void Op32M0(void)
{
   DirectIndirect(true);
   AND16();
}

static inline __attribute__((always_inline)) void Op21M1(void)
{
   DirectIndexedIndirect(true);
   AND8();
}

static inline __attribute__((always_inline)) void Op21M0(void)
{
   DirectIndexedIndirect(true);
   AND16();
}

static inline __attribute__((always_inline)) void Op31M1(void)
{
   DirectIndirectIndexed(true);
   AND8();
}

static inline __attribute__((always_inline)) void Op31M0(void)
{
   DirectIndirectIndexed(true);
   AND16();
}

static inline __attribute__((always_inline)) void Op27M1(void)
{
   DirectIndirectLong(true);
   AND8();
}

static inline __attribute__((always_inline)) void Op27M0(void)
{
   DirectIndirectLong(true);
   AND16();
}

static inline __attribute__((always_inline)) void Op37M1(void)
{
   DirectIndirectIndexedLong(true);
   AND8();
}

static inline __attribute__((always_inline)) void Op37M0(void)
{
   DirectIndirectIndexedLong(true);
   AND16();
}

static inline __attribute__((always_inline)) void Op2DM1(void)
{
   Absolute(true);
   AND8();
}

static inline __attribute__((always_inline)) void Op2DM0(void)
{
   Absolute(true);
   AND16();
}

static inline __attribute__((always_inline)) void Op3DM1(void)
{
   AbsoluteIndexedX(true);
   AND8();
}

static inline __attribute__((always_inline)) void Op3DM0(void)
{
   AbsoluteIndexedX(true);
   AND16();
}

static inline __attribute__((always_inline)) void Op39M1(void)
{
   AbsoluteIndexedY(true);
   AND8();
}

static inline __attribute__((always_inline)) void Op39M0(void)
{
   AbsoluteIndexedY(true);
   AND16();
}

static inline __attribute__((always_inline)) void Op2FM1(void)
{
   AbsoluteLong(true);
   AND8();
}

static inline __attribute__((always_inline)) void Op2FM0(void)
{
   AbsoluteLong(true);
   AND16();
}

static inline __attribute__((always_inline)) void Op3FM1(void)
{
   AbsoluteLongIndexedX(true);
   AND8();
}

static inline __attribute__((always_inline)) void Op3FM0(void)
{
   AbsoluteLongIndexedX(true);
   AND16();
}

static inline __attribute__((always_inline)) void Op23M1(void)
{
   StackRelative(true);
   AND8();
}

static inline __attribute__((always_inline)) void Op23M0(void)
{
   StackRelative(true);
   AND16();
}

static inline __attribute__((always_inline)) void Op33M1(void)
{
   StackRelativeIndirectIndexed(true);
   AND8();
}

static inline __attribute__((always_inline)) void Op33M0(void)
{
   StackRelativeIndirectIndexed(true);
   AND16();
}

/* ASL */
static inline __attribute__((always_inline)) void Op0AM1(void)
{
   A_ASL8();
}

static inline __attribute__((always_inline)) void Op0AM0(void)
{
   A_ASL16();
}

static inline __attribute__((always_inline)) void Op06M1(void)
{
   Direct(true);
   ASL8();
}

static inline __attribute__((always_inline)) void Op06M0(void)
{
   Direct(true);
   ASL16();
}

static inline __attribute__((always_inline)) void Op16M1(void)
{
   DirectIndexedX(true);
   ASL8();
}

static inline __attribute__((always_inline)) void Op16M0(void)
{
   DirectIndexedX(true);
   ASL16();
}

static inline __attribute__((always_inline)) void Op0EM1(void)
{
   Absolute(true);
   ASL8();
}

static inline __attribute__((always_inline)) void Op0EM0(void)
{
   Absolute(true);
   ASL16();
}

static inline __attribute__((always_inline)) void Op1EM1(void)
{
   AbsoluteIndexedX(true);
   ASL8();
}

static inline __attribute__((always_inline)) void Op1EM0(void)
{
   AbsoluteIndexedX(true);
   ASL16();
}

/* BIT */
static inline __attribute__((always_inline)) void Op89M1(void)
{
   ICPU._Zero = ICPU.Registers.AL & *CPU.PC++;
#ifndef SA1_OPCODES
   CPU.Cycles += CPU.MemSpeed;
#endif
}

static inline __attribute__((always_inline)) void Op89M0(void)
{
#ifdef FAST_LSB_WORD_ACCESS
   ICPU._Zero = (ICPU.Registers.A.W & *(uint16_t*) CPU.PC) != 0;
#else
   ICPU._Zero = (ICPU.Registers.A.W & (*CPU.PC + (*(CPU.PC + 1) << 8))) != 0;
#endif
#ifndef SA1_OPCODES
   CPU.Cycles += CPU.MemSpeedx2;
#endif
   CPU.PC += 2;
}

static inline __attribute__((always_inline)) void Op24M1(void)
{
   Direct(true);
   BIT8();
}

static inline __attribute__((always_inline)) void Op24M0(void)
{
   Direct(true);
   BIT16();
}

static inline __attribute__((always_inline)) void Op34M1(void)
{
   DirectIndexedX(true);
   BIT8();
}

static inline __attribute__((always_inline)) void Op34M0(void)
{
   DirectIndexedX(true);
   BIT16();
}

static inline __attribute__((always_inline)) void Op2CM1(void)
{
   Absolute(true);
   BIT8();
}

static inline __attribute__((always_inline)) void Op2CM0(void)
{
   Absolute(true);
   BIT16();
}

static inline __attribute__((always_inline)) void Op3CM1(void)
{
   AbsoluteIndexedX(true);
   BIT8();
}

static inline __attribute__((always_inline)) void Op3CM0(void)
{
   AbsoluteIndexedX(true);
   BIT16();
}

/* CMP */
static inline __attribute__((always_inline)) void OpC9M1(void)
{
   int32_t Int32 = (int32_t) ICPU.Registers.AL - (int32_t) *CPU.PC++;
   ICPU._Carry = Int32 >= 0;
   SetZN8((uint8_t) Int32);
#ifndef SA1_OPCODES
   CPU.Cycles += CPU.MemSpeed;
#endif
}

static inline __attribute__((always_inline)) void OpC9M0(void)
{
#ifdef FAST_LSB_WORD_ACCESS
   int32_t Int32 = (int32_t) ICPU.Registers.A.W - (int32_t) *(uint16_t*)CPU.PC;
#else
   int32_t Int32 = (int32_t) ICPU.Registers.A.W - (int32_t)(*CPU.PC + (*(CPU.PC + 1) << 8));
#endif
   ICPU._Carry = Int32 >= 0;
   SetZN16((uint16_t) Int32);
   CPU.PC += 2;
#ifndef SA1_OPCODES
   CPU.Cycles += CPU.MemSpeedx2;
#endif
}

static inline __attribute__((always_inline)) void OpC5M1(void)
{
   Direct(true);
   CMP8();
}

static inline __attribute__((always_inline)) void OpC5M0(void)
{
   Direct(true);
   CMP16();
}

static inline __attribute__((always_inline)) void OpD5M1(void)
{
   DirectIndexedX(true);
   CMP8();
}

static inline __attribute__((always_inline)) void OpD5M0(void)
{
   DirectIndexedX(true);
   CMP16();
}

static inline __attribute__((always_inline)) void OpD2M1(void)
{
   DirectIndirect(true);
   CMP8();
}

static inline __attribute__((always_inline)) void OpD2M0(void)
{
   DirectIndirect(true);
   CMP16();
}

static inline __attribute__((always_inline)) void OpC1M1(void)
{
   DirectIndexedIndirect(true);
   CMP8();
}

static inline __attribute__((always_inline)) void OpC1M0(void)
{
   DirectIndexedIndirect(true);
   CMP16();
}

static inline __attribute__((always_inline)) void OpD1M1(void)
{
   DirectIndirectIndexed(true);
   CMP8();
}

static inline __attribute__((always_inline)) void OpD1M0(void)
{
   DirectIndirectIndexed(true);
   CMP16();
}

static inline __attribute__((always_inline)) void OpC7M1(void)
{
   DirectIndirectLong(true);
   CMP8();
}

static inline __attribute__((always_inline)) void OpC7M0(void)
{
   DirectIndirectLong(true);
   CMP16();
}

static inline __attribute__((always_inline)) void OpD7M1(void)
{
   DirectIndirectIndexedLong(true);
   CMP8();
}

static inline __attribute__((always_inline)) void OpD7M0(void)
{
   DirectIndirectIndexedLong(true);
   CMP16();
}

static inline __attribute__((always_inline)) void OpCDM1(void)
{
   Absolute(true);
   CMP8();
}

static inline __attribute__((always_inline)) void OpCDM0(void)
{
   Absolute(true);
   CMP16();
}

static inline __attribute__((always_inline)) void OpDDM1(void)
{
   AbsoluteIndexedX(true);
   CMP8();
}

static inline __attribute__((always_inline)) void OpDDM0(void)
{
   AbsoluteIndexedX(true);
   CMP16();
}

static inline __attribute__((always_inline)) void OpD9M1(void)
{
   AbsoluteIndexedY(true);
   CMP8();
}

static inline __attribute__((always_inline)) void OpD9M0(void)
{
   AbsoluteIndexedY(true);
   CMP16();
}

static inline __attribute__((always_inline)) void OpCFM1(void)
{
   AbsoluteLong(true);
   CMP8();
}

static inline __attribute__((always_inline)) void OpCFM0(void)
{
   AbsoluteLong(true);
   CMP16();
}

static inline __attribute__((always_inline)) void OpDFM1(void)
{
   AbsoluteLongIndexedX(true);
   CMP8();
}

static inline __attribute__((always_inline)) void OpDFM0(void)
{
   AbsoluteLongIndexedX(true);
   CMP16();
}

static inline __attribute__((always_inline)) void OpC3M1(void)
{
   StackRelative(true);
   CMP8();
}

static inline __attribute__((always_inline)) void OpC3M0(void)
{
   StackRelative(true);
   CMP16();
}

static inline __attribute__((always_inline)) void OpD3M1(void)
{
   StackRelativeIndirectIndexed(true);
   CMP8();
}

static inline __attribute__((always_inline)) void OpD3M0(void)
{
   StackRelativeIndirectIndexed(true);
   CMP16();
}

/* CMX */
static inline __attribute__((always_inline)) void OpE0X1(void)
{
   int32_t Int32 = (int32_t) ICPU.Registers.XL - (int32_t) *CPU.PC++;
   ICPU._Carry = Int32 >= 0;
   SetZN8((uint8_t) Int32);
#ifndef SA1_OPCODES
   CPU.Cycles += CPU.MemSpeed;
#endif
}

static inline __attribute__((always_inline)) void OpE0X0(void)
{
#ifdef FAST_LSB_WORD_ACCESS
   int32_t Int32 = (int32_t) ICPU.Registers.X.W - (int32_t) *(uint16_t*)CPU.PC;
#else
   int32_t Int32 = (int32_t) ICPU.Registers.X.W - (int32_t)(*CPU.PC + (*(CPU.PC + 1) << 8));
#endif
   ICPU._Carry = Int32 >= 0;
   SetZN16((uint16_t) Int32);
   CPU.PC += 2;
#ifndef SA1_OPCODES
   CPU.Cycles += CPU.MemSpeedx2;
#endif
}

static inline __attribute__((always_inline)) void OpE4X1(void)
{
   Direct(true);
   CMX8();
}

static inline __attribute__((always_inline)) void OpE4X0(void)
{
   Direct(true);
   CMX16();
}

static inline __attribute__((always_inline)) void OpECX1(void)
{
   Absolute(true);
   CMX8();
}

static inline __attribute__((always_inline)) void OpECX0(void)
{
   Absolute(true);
   CMX16();
}

/* CMY */
static inline __attribute__((always_inline)) void OpC0X1(void)
{
   int32_t Int32 = (int32_t) ICPU.Registers.YL - (int32_t) *CPU.PC++;
   ICPU._Carry = Int32 >= 0;
   SetZN8((uint8_t) Int32);
#ifndef SA1_OPCODES
   CPU.Cycles += CPU.MemSpeed;
#endif
}

static inline __attribute__((always_inline)) void OpC0X0(void)
{
#ifdef FAST_LSB_WORD_ACCESS
   int32_t Int32 = (int32_t) ICPU.Registers.Y.W - (int32_t) *(uint16_t*)CPU.PC;
#else
   int32_t Int32 = (int32_t) ICPU.Registers.Y.W - (int32_t)(*CPU.PC + (*(CPU.PC + 1) << 8));
#endif
   ICPU._Carry = Int32 >= 0;
   SetZN16((uint16_t) Int32);
   CPU.PC += 2;
#ifndef SA1_OPCODES
   CPU.Cycles += CPU.MemSpeedx2;
#endif
}

static inline __attribute__((always_inline)) void OpC4X1(void)
{
   Direct(true);
   CMY8();
}

static inline __attribute__((always_inline)) void OpC4X0(void)
{
   Direct(true);
   CMY16();
}

static inline __attribute__((always_inline)) void OpCCX1(void)
{
   Absolute(true);
   CMY8();
}

static inline __attribute__((always_inline)) void OpCCX0(void)
{
   Absolute(true);
   CMY16();
}

/* DEC */
static inline __attribute__((always_inline)) void Op3AM1(void)
{
   A_DEC8();
}

static inline __attribute__((always_inline)) void Op3AM0(void)
{
   A_DEC16();
}

static inline __attribute__((always_inline)) void OpC6M1(void)
{
   Direct(true);
   DEC8();
}

static inline __attribute__((always_inline)) void OpC6M0(void)
{
   Direct(true);
   DEC16();
}

static inline __attribute__((always_inline)) void OpD6M1(void)
{
   DirectIndexedX(true);
   DEC8();
}

static inline __attribute__((always_inline)) void OpD6M0(void)
{
   DirectIndexedX(true);
   DEC16();
}

static inline __attribute__((always_inline)) void OpCEM1(void)
{
   Absolute(true);
   DEC8();
}

static inline __attribute__((always_inline)) void OpCEM0(void)
{
   Absolute(true);
   DEC16();
}

static inline __attribute__((always_inline)) void OpDEM1(void)
{
   AbsoluteIndexedX(true);
   DEC8();
}

static inline __attribute__((always_inline)) void OpDEM0(void)
{
   AbsoluteIndexedX(true);
   DEC16();
}

/* EOR */
static inline __attribute__((always_inline)) void Op49M1(void)
{
   ICPU.Registers.AL ^= *CPU.PC++;
#ifndef SA1_OPCODES
   CPU.Cycles += CPU.MemSpeed;
#endif
   SetZN8(ICPU.Registers.AL);
}

static inline __attribute__((always_inline)) void Op49M0(void)
{
#ifdef FAST_LSB_WORD_ACCESS
   ICPU.Registers.A.W ^= *(uint16_t*) CPU.PC;
#else
   ICPU.Registers.A.W ^= *CPU.PC + (*(CPU.PC + 1) << 8);
#endif
   CPU.PC += 2;
#ifndef SA1_OPCODES
   CPU.Cycles += CPU.MemSpeedx2;
#endif
   SetZN16(ICPU.Registers.A.W);
}

static inline __attribute__((always_inline)) void Op45M1(void)
{
   Direct(true);
   EOR8();
}

static inline __attribute__((always_inline)) void Op45M0(void)
{
   Direct(true);
   EOR16();
}

static inline __attribute__((always_inline)) void Op55M1(void)
{
   DirectIndexedX(true);
   EOR8();
}

static inline __attribute__((always_inline)) void Op55M0(void)
{
   DirectIndexedX(true);
   EOR16();
}

static inline __attribute__((always_inline)) void Op52M1(void)
{
   DirectIndirect(true);
   EOR8();
}

static inline __attribute__((always_inline)) void Op52M0(void)
{
   DirectIndirect(true);
   EOR16();
}

static inline __attribute__((always_inline)) void Op41M1(void)
{
   DirectIndexedIndirect(true);
   EOR8();
}

static inline __attribute__((always_inline)) void Op41M0(void)
{
   DirectIndexedIndirect(true);
   EOR16();
}

static inline __attribute__((always_inline)) void Op51M1(void)
{
   DirectIndirectIndexed(true);
   EOR8();
}

static inline __attribute__((always_inline)) void Op51M0(void)
{
   DirectIndirectIndexed(true);
   EOR16();
}

static inline __attribute__((always_inline)) void Op47M1(void)
{
   DirectIndirectLong(true);
   EOR8();
}

static inline __attribute__((always_inline)) void Op47M0(void)
{
   DirectIndirectLong(true);
   EOR16();
}

static inline __attribute__((always_inline)) void Op57M1(void)
{
   DirectIndirectIndexedLong(true);
   EOR8();
}

static inline __attribute__((always_inline)) void Op57M0(void)
{
   DirectIndirectIndexedLong(true);
   EOR16();
}

static inline __attribute__((always_inline)) void Op4DM1(void)
{
   Absolute(true);
   EOR8();
}

static inline __attribute__((always_inline)) void Op4DM0(void)
{
   Absolute(true);
   EOR16();
}

static inline __attribute__((always_inline)) void Op5DM1(void)
{
   AbsoluteIndexedX(true);
   EOR8();
}

static inline __attribute__((always_inline)) void Op5DM0(void)
{
   AbsoluteIndexedX(true);
   EOR16();
}

static inline __attribute__((always_inline)) void Op59M1(void)
{
   AbsoluteIndexedY(true);
   EOR8();
}

static inline __attribute__((always_inline)) void Op59M0(void)
{
   AbsoluteIndexedY(true);
   EOR16();
}

static inline __attribute__((always_inline)) void Op4FM1(void)
{
   AbsoluteLong(true);
   EOR8();
}

static inline __attribute__((always_inline)) void Op4FM0(void)
{
   AbsoluteLong(true);
   EOR16();
}

static inline __attribute__((always_inline)) void Op5FM1(void)
{
   AbsoluteLongIndexedX(true);
   EOR8();
}

static inline __attribute__((always_inline)) void Op5FM0(void)
{
   AbsoluteLongIndexedX(true);
   EOR16();
}

static inline __attribute__((always_inline)) void Op43M1(void)
{
   StackRelative(true);
   EOR8();
}

static inline __attribute__((always_inline)) void Op43M0(void)
{
   StackRelative(true);
   EOR16();
}

static inline __attribute__((always_inline)) void Op53M1(void)
{
   StackRelativeIndirectIndexed(true);
   EOR8();
}

static inline __attribute__((always_inline)) void Op53M0(void)
{
   StackRelativeIndirectIndexed(true);
   EOR16();
}

/* INC */
static inline __attribute__((always_inline)) void Op1AM1(void)
{
   A_INC8();
}

static inline __attribute__((always_inline)) void Op1AM0(void)
{
   A_INC16();
}

static inline __attribute__((always_inline)) void OpE6M1(void)
{
   Direct(true);
   INC8();
}

static inline __attribute__((always_inline)) void OpE6M0(void)
{
   Direct(true);
   INC16();
}

static inline __attribute__((always_inline)) void OpF6M1(void)
{
   DirectIndexedX(true);
   INC8();
}

static inline __attribute__((always_inline)) void OpF6M0(void)
{
   DirectIndexedX(true);
   INC16();
}

static inline __attribute__((always_inline)) void OpEEM1(void)
{
   Absolute(true);
   INC8();
}

static inline __attribute__((always_inline)) void OpEEM0(void)
{
   Absolute(true);
   INC16();
}

static inline __attribute__((always_inline)) void OpFEM1(void)
{
   AbsoluteIndexedX(true);
   INC8();
}

static inline __attribute__((always_inline)) void OpFEM0(void)
{
   AbsoluteIndexedX(true);
   INC16();
}

/* LDA */
static inline __attribute__((always_inline)) void OpA9M1(void)
{
   ICPU.Registers.AL = *CPU.PC++;
#ifndef SA1_OPCODES
   CPU.Cycles += CPU.MemSpeed;
#endif
   SetZN8(ICPU.Registers.AL);
}

static inline __attribute__((always_inline)) void OpA9M0(void)
{
#ifdef FAST_LSB_WORD_ACCESS
   ICPU.Registers.A.W = *(uint16_t*) CPU.PC;
#else
   ICPU.Registers.A.W = *CPU.PC + (*(CPU.PC + 1) << 8);
#endif

   CPU.PC += 2;
#ifndef SA1_OPCODES
   CPU.Cycles += CPU.MemSpeedx2;
#endif
   SetZN16(ICPU.Registers.A.W);
}

static inline __attribute__((always_inline)) void OpA5M1(void)
{
   Direct(true);
   LDA8();
}

static inline __attribute__((always_inline)) void OpA5M0(void)
{
   Direct(true);
   LDA16();
}

static inline __attribute__((always_inline)) void OpB5M1(void)
{
   DirectIndexedX(true);
   LDA8();
}

static inline __attribute__((always_inline)) void OpB5M0(void)
{
   DirectIndexedX(true);
   LDA16();
}

static inline __attribute__((always_inline)) void OpB2M1(void)
{
   DirectIndirect(true);
   LDA8();
}

static inline __attribute__((always_inline)) void OpB2M0(void)
{
   DirectIndirect(true);
   LDA16();
}

static inline __attribute__((always_inline)) void OpA1M1(void)
{
   DirectIndexedIndirect(true);
   LDA8();
}

static inline __attribute__((always_inline)) void OpA1M0(void)
{
   DirectIndexedIndirect(true);
   LDA16();
}

static inline __attribute__((always_inline)) void OpB1M1(void)
{
   DirectIndirectIndexed(true);
   LDA8();
}

static inline __attribute__((always_inline)) void OpB1M0(void)
{
   DirectIndirectIndexed(true);
   LDA16();
}

static inline __attribute__((always_inline)) void OpA7M1(void)
{
   DirectIndirectLong(true);
   LDA8();
}

static inline __attribute__((always_inline)) void OpA7M0(void)
{
   DirectIndirectLong(true);
   LDA16();
}

static inline __attribute__((always_inline)) void OpB7M1(void)
{
   DirectIndirectIndexedLong(true);
   LDA8();
}

static inline __attribute__((always_inline)) void OpB7M0(void)
{
   DirectIndirectIndexedLong(true);
   LDA16();
}

static inline __attribute__((always_inline)) void OpADM1(void)
{
   Absolute(true);
   LDA8();
}

static inline __attribute__((always_inline)) void OpADM0(void)
{
   Absolute(true);
   LDA16();
}

static inline __attribute__((always_inline)) void OpBDM1(void)
{
   AbsoluteIndexedX(true);
   LDA8();
}

static inline __attribute__((always_inline)) void OpBDM0(void)
{
   AbsoluteIndexedX(true);
   LDA16();
}

static inline __attribute__((always_inline)) void OpB9M1(void)
{
   AbsoluteIndexedY(true);
   LDA8();
}

static inline __attribute__((always_inline)) void OpB9M0(void)
{
   AbsoluteIndexedY(true);
   LDA16();
}

static inline __attribute__((always_inline)) void OpAFM1(void)
{
   AbsoluteLong(true);
   LDA8();
}

static inline __attribute__((always_inline)) void OpAFM0(void)
{
   AbsoluteLong(true);
   LDA16();
}

static inline __attribute__((always_inline)) void OpBFM1(void)
{
   AbsoluteLongIndexedX(true);
   LDA8();
}

static inline __attribute__((always_inline)) void OpBFM0(void)
{
   AbsoluteLongIndexedX(true);
   LDA16();
}

static inline __attribute__((always_inline)) void OpA3M1(void)
{
   StackRelative(true);
   LDA8();
}

static inline __attribute__((always_inline)) void OpA3M0(void)
{
   StackRelative(true);
   LDA16();
}

static inline __attribute__((always_inline)) void OpB3M1(void)
{
   StackRelativeIndirectIndexed(true);
   LDA8();
}

static inline __attribute__((always_inline)) void OpB3M0(void)
{
   StackRelativeIndirectIndexed(true);
   LDA16();
}

/* LDX */
static inline __attribute__((always_inline)) void OpA2X1(void)
{
   ICPU.Registers.XL = *CPU.PC++;
#ifndef SA1_OPCODES
   CPU.Cycles += CPU.MemSpeed;
#endif
   SetZN8(ICPU.Registers.XL);
}

static inline __attribute__((always_inline)) void OpA2X0(void)
{
#ifdef FAST_LSB_WORD_ACCESS
   ICPU.Registers.X.W = *(uint16_t*) CPU.PC;
#else
   ICPU.Registers.X.W = *CPU.PC + (*(CPU.PC + 1) << 8);
#endif
   CPU.PC += 2;
#ifndef SA1_OPCODES
   CPU.Cycles += CPU.MemSpeedx2;
#endif
   SetZN16(ICPU.Registers.X.W);
}

static inline __attribute__((always_inline)) void OpA6X1(void)
{
   Direct(true);
   LDX8();
}

static inline __attribute__((always_inline)) void OpA6X0(void)
{
   Direct(true);
   LDX16();
}

static inline __attribute__((always_inline)) void OpB6X1(void)
{
   DirectIndexedY(true);
   LDX8();
}

static inline __attribute__((always_inline)) void OpB6X0(void)
{
   DirectIndexedY(true);
   LDX16();
}

static inline __attribute__((always_inline)) void OpAEX1(void)
{
   Absolute(true);
   LDX8();
}

static inline __attribute__((always_inline)) void OpAEX0(void)
{
   Absolute(true);
   LDX16();
}

static inline __attribute__((always_inline)) void OpBEX1(void)
{
   AbsoluteIndexedY(true);
   LDX8();
}

static inline __attribute__((always_inline)) void OpBEX0(void)
{
   AbsoluteIndexedY(true);
   LDX16();
}

/* LDY */
static inline __attribute__((always_inline)) void OpA0X1(void)
{
   ICPU.Registers.YL = *CPU.PC++;
#ifndef SA1_OPCODES
   CPU.Cycles += CPU.MemSpeed;
#endif
   SetZN8(ICPU.Registers.YL);
}

static inline __attribute__((always_inline)) void OpA0X0(void)
{
#ifdef FAST_LSB_WORD_ACCESS
   ICPU.Registers.Y.W = *(uint16_t*) CPU.PC;
#else
   ICPU.Registers.Y.W = *CPU.PC + (*(CPU.PC + 1) << 8);
#endif

   CPU.PC += 2;
#ifndef SA1_OPCODES
   CPU.Cycles += CPU.MemSpeedx2;
#endif
   SetZN16(ICPU.Registers.Y.W);
}

static inline __attribute__((always_inline)) void OpA4X1(void)
{
   Direct(true);
   LDY8();
}

static inline __attribute__((always_inline)) void OpA4X0(void)
{
   Direct(true);
   LDY16();
}

static inline __attribute__((always_inline)) void OpB4X1(void)
{
   DirectIndexedX(true);
   LDY8();
}

static inline __attribute__((always_inline)) void OpB4X0(void)
{
   DirectIndexedX(true);
   LDY16();
}

static inline __attribute__((always_inline)) void OpACX1(void)
{
   Absolute(true);
   LDY8();
}

static inline __attribute__((always_inline)) void OpACX0(void)
{
   Absolute(true);
   LDY16();
}

static inline __attribute__((always_inline)) void OpBCX1(void)
{
   AbsoluteIndexedX(true);
   LDY8();
}

static inline __attribute__((always_inline)) void OpBCX0(void)
{
   AbsoluteIndexedX(true);
   LDY16();
}

/* LSR */
static inline __attribute__((always_inline)) void Op4AM1(void)
{
   A_LSR8();
}

static inline __attribute__((always_inline)) void Op4AM0(void)
{
   A_LSR16();
}

static inline __attribute__((always_inline)) void Op46M1(void)
{
   Direct(true);
   LSR8();
}

static inline __attribute__((always_inline)) void Op46M0(void)
{
   Direct(true);
   LSR16();
}

static inline __attribute__((always_inline)) void Op56M1(void)
{
   DirectIndexedX(true);
   LSR8();
}

static inline __attribute__((always_inline)) void Op56M0(void)
{
   DirectIndexedX(true);
   LSR16();
}

static inline __attribute__((always_inline)) void Op4EM1(void)
{
   Absolute(true);
   LSR8();
}

static inline __attribute__((always_inline)) void Op4EM0(void)
{
   Absolute(true);
   LSR16();
}

static inline __attribute__((always_inline)) void Op5EM1(void)
{
   AbsoluteIndexedX(true);
   LSR8();
}

static inline __attribute__((always_inline)) void Op5EM0(void)
{
   AbsoluteIndexedX(true);
   LSR16();
}

/* ORA */
static inline __attribute__((always_inline)) void Op09M1(void)
{
   ICPU.Registers.AL |= *CPU.PC++;
#ifndef SA1_OPCODES
   CPU.Cycles += CPU.MemSpeed;
#endif
   SetZN8(ICPU.Registers.AL);
}

static inline __attribute__((always_inline)) void Op09M0(void)
{
#ifdef FAST_LSB_WORD_ACCESS
   ICPU.Registers.A.W |= *(uint16_t*) CPU.PC;
#else
   ICPU.Registers.A.W |= *CPU.PC + (*(CPU.PC + 1) << 8);
#endif
   CPU.PC += 2;
#ifndef SA1_OPCODES
   CPU.Cycles += CPU.MemSpeedx2;
#endif
   SetZN16(ICPU.Registers.A.W);
}

static inline __attribute__((always_inline)) void Op05M1(void)
{
   Direct(true);
   ORA8();
}

static inline __attribute__((always_inline)) void Op05M0(void)
{
   Direct(true);
   ORA16();
}

static inline __attribute__((always_inline)) void Op15M1(void)
{
   DirectIndexedX(true);
   ORA8();
}

static inline __attribute__((always_inline)) void Op15M0(void)
{
   DirectIndexedX(true);
   ORA16();
}

static inline __attribute__((always_inline)) void Op12M1(void)
{
   DirectIndirect(true);
   ORA8();
}

static inline __attribute__((always_inline)) void Op12M0(void)
{
   DirectIndirect(true);
   ORA16();
}

static inline __attribute__((always_inline)) void Op01M1(void)
{
   DirectIndexedIndirect(true);
   ORA8();
}

static inline __attribute__((always_inline)) void Op01M0(void)
{
   DirectIndexedIndirect(true);
   ORA16();
}

static inline __attribute__((always_inline)) void Op11M1(void)
{
   DirectIndirectIndexed(true);
   ORA8();
}

static inline __attribute__((always_inline)) void Op11M0(void)
{
   DirectIndirectIndexed(true);
   ORA16();
}

static inline __attribute__((always_inline)) void Op07M1(void)
{
   DirectIndirectLong(true);
   ORA8();
}

static inline __attribute__((always_inline)) void Op07M0(void)
{
   DirectIndirectLong(true);
   ORA16();
}

static inline __attribute__((always_inline)) void Op17M1(void)
{
   DirectIndirectIndexedLong(true);
   ORA8();
}

static inline __attribute__((always_inline)) void Op17M0(void)
{
   DirectIndirectIndexedLong(true);
   ORA16();
}

static inline __attribute__((always_inline)) void Op0DM1(void)
{
   Absolute(true);
   ORA8();
}

static inline __attribute__((always_inline)) void Op0DM0(void)
{
   Absolute(true);
   ORA16();
}

static inline __attribute__((always_inline)) void Op1DM1(void)
{
   AbsoluteIndexedX(true);
   ORA8();
}

static inline __attribute__((always_inline)) void Op1DM0(void)
{
   AbsoluteIndexedX(true);
   ORA16();
}

static inline __attribute__((always_inline)) void Op19M1(void)
{
   AbsoluteIndexedY(true);
   ORA8();
}

static inline __attribute__((always_inline)) void Op19M0(void)
{
   AbsoluteIndexedY(true);
   ORA16();
}

static inline __attribute__((always_inline)) void Op0FM1(void)
{
   AbsoluteLong(true);
   ORA8();
}

static inline __attribute__((always_inline)) void Op0FM0(void)
{
   AbsoluteLong(true);
   ORA16();
}

static inline __attribute__((always_inline)) void Op1FM1(void)
{
   AbsoluteLongIndexedX(true);
   ORA8();
}

static inline __attribute__((always_inline)) void Op1FM0(void)
{
   AbsoluteLongIndexedX(true);
   ORA16();
}

static inline __attribute__((always_inline)) void Op03M1(void)
{
   StackRelative(true);
   ORA8();
}

static inline __attribute__((always_inline)) void Op03M0(void)
{
   StackRelative(true);
   ORA16();
}

static inline __attribute__((always_inline)) void Op13M1(void)
{
   StackRelativeIndirectIndexed(true);
   ORA8();
}

static inline __attribute__((always_inline)) void Op13M0(void)
{
   StackRelativeIndirectIndexed(true);
   ORA16();
}

/* ROL */
static inline __attribute__((always_inline)) void Op2AM1(void)
{
   A_ROL8();
}

static inline __attribute__((always_inline)) void Op2AM0(void)
{
   A_ROL16();
}

static inline __attribute__((always_inline)) void Op26M1(void)
{
   Direct(true);
   ROL8();
}

static inline __attribute__((always_inline)) void Op26M0(void)
{
   Direct(true);
   ROL16();
}

static inline __attribute__((always_inline)) void Op36M1(void)
{
   DirectIndexedX(true);
   ROL8();
}

static inline __attribute__((always_inline)) void Op36M0(void)
{
   DirectIndexedX(true);
   ROL16();
}

static inline __attribute__((always_inline)) void Op2EM1(void)
{
   Absolute(true);
   ROL8();
}

static inline __attribute__((always_inline)) void Op2EM0(void)
{
   Absolute(true);
   ROL16();
}

static inline __attribute__((always_inline)) void Op3EM1(void)
{
   AbsoluteIndexedX(true);
   ROL8();
}

static inline __attribute__((always_inline)) void Op3EM0(void)
{
   AbsoluteIndexedX(true);
   ROL16();
}

/* ROR */
static inline __attribute__((always_inline)) void Op6AM1(void)
{
   A_ROR8();
}

static inline __attribute__((always_inline)) void Op6AM0(void)
{
   A_ROR16();
}

static inline __attribute__((always_inline)) void Op66M1(void)
{
   Direct(true);
   ROR8();
}

static inline __attribute__((always_inline)) void Op66M0(void)
{
   Direct(true);
   ROR16();
}

static inline __attribute__((always_inline)) void Op76M1(void)
{
   DirectIndexedX(true);
   ROR8();
}

static inline __attribute__((always_inline)) void Op76M0(void)
{
   DirectIndexedX(true);
   ROR16();
}

static inline __attribute__((always_inline)) void Op6EM1(void)
{
   Absolute(true);
   ROR8();
}

static inline __attribute__((always_inline)) void Op6EM0(void)
{
   Absolute(true);
   ROR16();
}

static inline __attribute__((always_inline)) void Op7EM1(void)
{
   AbsoluteIndexedX(true);
   ROR8();
}

static inline __attribute__((always_inline)) void Op7EM0(void)
{
   AbsoluteIndexedX(true);
   ROR16();
}

/* SBC */
static inline __attribute__((always_inline)) void OpE9M1(void)
{
   Immediate8();
   SBC8();
}

static inline __attribute__((always_inline)) void OpE9M0(void)
{
   Immediate16();
   SBC16();
}

static inline __attribute__((always_inline)) void OpE5M1(void)
{
   Direct(true);
   SBC8();
}

static inline __attribute__((always_inline)) void OpE5M0(void)
{
   Direct(true);
   SBC16();
}

static inline __attribute__((always_inline)) void OpF5M1(void)
{
   DirectIndexedX(true);
   SBC8();
}

static inline __attribute__((always_inline)) void OpF5M0(void)
{
   DirectIndexedX(true);
   SBC16();
}

static inline __attribute__((always_inline)) void OpF2M1(void)
{
   DirectIndirect(true);
   SBC8();
}

static inline __attribute__((always_inline)) void OpF2M0(void)
{
   DirectIndirect(true);
   SBC16();
}

static inline __attribute__((always_inline)) void OpE1M1(void)
{
   DirectIndexedIndirect(true);
   SBC8();
}

static inline __attribute__((always_inline)) void OpE1M0(void)
{
   DirectIndexedIndirect(true);
   SBC16();
}

static inline __attribute__((always_inline)) void OpF1M1(void)
{
   DirectIndirectIndexed(true);
   SBC8();
}

static inline __attribute__((always_inline)) void OpF1M0(void)
{
   DirectIndirectIndexed(true);
   SBC16();
}

static inline __attribute__((always_inline)) void OpE7M1(void)
{
   DirectIndirectLong(true);
   SBC8();
}

static inline __attribute__((always_inline)) void OpE7M0(void)
{
   DirectIndirectLong(true);
   SBC16();
}

static inline __attribute__((always_inline)) void OpF7M1(void)
{
   DirectIndirectIndexedLong(true);
   SBC8();
}

static inline __attribute__((always_inline)) void OpF7M0(void)
{
   DirectIndirectIndexedLong(true);
   SBC16();
}

static inline __attribute__((always_inline)) void OpEDM1(void)
{
   Absolute(true);
   SBC8();
}

static inline __attribute__((always_inline)) void OpEDM0(void)
{
   Absolute(true);
   SBC16();
}

static inline __attribute__((always_inline)) void OpFDM1(void)
{
   AbsoluteIndexedX(true);
   SBC8();
}

static inline __attribute__((always_inline)) void OpFDM0(void)
{
   AbsoluteIndexedX(true);
   SBC16();
}

static inline __attribute__((always_inline)) void OpF9M1(void)
{
   AbsoluteIndexedY(true);
   SBC8();
}

static inline __attribute__((always_inline)) void OpF9M0(void)
{
   AbsoluteIndexedY(true);
   SBC16();
}

static inline __attribute__((always_inline)) void OpEFM1(void)
{
   AbsoluteLong(true);
   SBC8();
}

static inline __attribute__((always_inline)) void OpEFM0(void)
{
   AbsoluteLong(true);
   SBC16();
}

static inline __attribute__((always_inline)) void OpFFM1(void)
{
   AbsoluteLongIndexedX(true);
   SBC8();
}

static inline __attribute__((always_inline)) void OpFFM0(void)
{
   AbsoluteLongIndexedX(true);
   SBC16();
}

static inline __attribute__((always_inline)) void OpE3M1(void)
{
   StackRelative(true);
   SBC8();
}

static inline __attribute__((always_inline)) void OpE3M0(void)
{
   StackRelative(true);
   SBC16();
}

static inline __attribute__((always_inline)) void OpF3M1(void)
{
   StackRelativeIndirectIndexed(true);
   SBC8();
}

static inline __attribute__((always_inline)) void OpF3M0(void)
{
   StackRelativeIndirectIndexed(true);
   SBC16();
}

/* STA */
static inline __attribute__((always_inline)) void Op85M1(void)
{
   Direct(false);
   STA8();
}

static inline __attribute__((always_inline)) void Op85M0(void)
{
   Direct(false);
   STA16();
}

static inline __attribute__((always_inline)) void Op95M1(void)
{
   DirectIndexedX(false);
   STA8();
}

static inline __attribute__((always_inline)) void Op95M0(void)
{
   DirectIndexedX(false);
   STA16();
}

static inline __attribute__((always_inline)) void Op92M1(void)
{
   DirectIndirect(false);
   STA8();
}

static inline __attribute__((always_inline)) void Op92M0(void)
{
   DirectIndirect(false);
   STA16();
}

static inline __attribute__((always_inline)) void Op81M1(void)
{
   DirectIndexedIndirect(false);
   STA8();
#ifndef SA1_OPCODES
   if (CheckIndex())
      CPU.Cycles += ONE_CYCLE;
#endif
}

static inline __attribute__((always_inline)) void Op81M0(void)
{
   DirectIndexedIndirect(false);
   STA16();
#ifndef SA1_OPCODES
   if (CheckIndex())
      CPU.Cycles += ONE_CYCLE;
#endif
}

static inline __attribute__((always_inline)) void Op91M1(void)
{
   DirectIndirectIndexed(false);
   STA8();
}

static inline __attribute__((always_inline)) void Op91M0(void)
{
   DirectIndirectIndexed(false);
   STA16();
}

static inline __attribute__((always_inline)) void Op87M1(void)
{
   DirectIndirectLong(false);
   STA8();
}

static inline __attribute__((always_inline)) void Op87M0(void)
{
   DirectIndirectLong(false);
   STA16();
}

static inline __attribute__((always_inline)) void Op97M1(void)
{
   DirectIndirectIndexedLong(false);
   STA8();
}

static inline __attribute__((always_inline)) void Op97M0(void)
{
   DirectIndirectIndexedLong(false);
   STA16();
}

static inline __attribute__((always_inline)) void Op8DM1(void)
{
   Absolute(false);
   STA8();
}

static inline __attribute__((always_inline)) void Op8DM0(void)
{
   Absolute(false);
   STA16();
}

static inline __attribute__((always_inline)) void Op9DM1(void)
{
   AbsoluteIndexedX(false);
   STA8();
}

static inline __attribute__((always_inline)) void Op9DM0(void)
{
   AbsoluteIndexedX(false);
   STA16();
}

static inline __attribute__((always_inline)) void Op99M1(void)
{
   AbsoluteIndexedY(false);
   STA8();
}

static inline __attribute__((always_inline)) void Op99M0(void)
{
   AbsoluteIndexedY(false);
   STA16();
}

static inline __attribute__((always_inline)) void Op8FM1(void)
{
   AbsoluteLong(false);
   STA8();
}

static inline __attribute__((always_inline)) void Op8FM0(void)
{
   AbsoluteLong(false);
   STA16();
}

static inline __attribute__((always_inline)) void Op9FM1(void)
{
   AbsoluteLongIndexedX(false);
   STA8();
}

static inline __attribute__((always_inline)) void Op9FM0(void)
{
   AbsoluteLongIndexedX(false);
   STA16();
}

static inline __attribute__((always_inline)) void Op83M1(void)
{
   StackRelative(false);
   STA8();
}

static inline __attribute__((always_inline)) void Op83M0(void)
{
   StackRelative(false);
   STA16();
}

static inline __attribute__((always_inline)) void Op93M1(void)
{
   StackRelativeIndirectIndexed(false);
   STA8();
}

static inline __attribute__((always_inline)) void Op93M0(void)
{
   StackRelativeIndirectIndexed(false);
   STA16();
}

/* STX */
static inline __attribute__((always_inline)) void Op86X1(void)
{
   Direct(false);
   STX8();
}

static inline __attribute__((always_inline)) void Op86X0(void)
{
   Direct(false);
   STX16();
}

static inline __attribute__((always_inline)) void Op96X1(void)
{
   DirectIndexedY(false);
   STX8();
}

static inline __attribute__((always_inline)) void Op96X0(void)
{
   DirectIndexedY(false);
   STX16();
}

static inline __attribute__((always_inline)) void Op8EX1(void)
{
   Absolute(false);
   STX8();
}

static inline __attribute__((always_inline)) void Op8EX0(void)
{
   Absolute(false);
   STX16();
}

/* STY */
static inline __attribute__((always_inline)) void Op84X1(void)
{
   Direct(false);
   STY8();
}

static inline __attribute__((always_inline)) void Op84X0(void)
{
   Direct(false);
   STY16();
}

static inline __attribute__((always_inline)) void Op94X1(void)
{
   DirectIndexedX(false);
   STY8();
}

static inline __attribute__((always_inline)) void Op94X0(void)
{
   DirectIndexedX(false);
   STY16();
}

static inline __attribute__((always_inline)) void Op8CX1(void)
{
   Absolute(false);
   STY8();
}

static inline __attribute__((always_inline)) void Op8CX0(void)
{
   Absolute(false);
   STY16();
}

/* STZ */
static inline __attribute__((always_inline)) void Op64M1(void)
{
   Direct(false);
   STZ8();
}

static inline __attribute__((always_inline)) void Op64M0(void)
{
   Direct(false);
   STZ16();
}

static inline __attribute__((always_inline)) void Op74M1(void)
{
   DirectIndexedX(false);
   STZ8();
}

static inline __attribute__((always_inline)) void Op74M0(void)
{
   DirectIndexedX(false);
   STZ16();
}

static inline __attribute__((always_inline)) void Op9CM1(void)
{
   Absolute(false);
   STZ8();
}

static inline __attribute__((always_inline)) void Op9CM0(void)
{
   Absolute(false);
   STZ16();
}

static inline __attribute__((always_inline)) void Op9EM1(void)
{
   AbsoluteIndexedX(false);
   STZ8();
}

static inline __attribute__((always_inline)) void Op9EM0(void)
{
   AbsoluteIndexedX(false);
   STZ16();
}

/* TRB */
static inline __attribute__((always_inline)) void Op14M1(void)
{
   Direct(true);
   TRB8();
}

static inline __attribute__((always_inline)) void Op14M0(void)
{
   Direct(true);
   TRB16();
}

static inline __attribute__((always_inline)) void Op1CM1(void)
{
   Absolute(true);
   TRB8();
}

static inline __attribute__((always_inline)) void Op1CM0(void)
{
   Absolute(true);
   TRB16();
}

/* TSB */
static inline __attribute__((always_inline)) void Op04M1(void)
{
   Direct(true);
   TSB8();
}

static inline __attribute__((always_inline)) void Op04M0(void)
{
   Direct(true);
   TSB16();
}

static inline __attribute__((always_inline)) void Op0CM1(void)
{
   Absolute(true);
   TSB8();
}

static inline __attribute__((always_inline)) void Op0CM0(void)
{
   Absolute(true);
   TSB16();
}

/* Branch Instructions */
#ifndef SA1_OPCODES
#define BranchCheck() \
   if(CPU.BranchSkip) \
   { \
      CPU.BranchSkip = false; \
      if (CPU.PC - CPU.PCBase > OpAddress)\
         return; \
   }
#else
#define BranchCheck()
#endif

#ifndef SA1_OPCODES
static INLINE void CPUShutdown(void)
{
   if (Settings.Shutdown && CPU.PC == CPU.WaitAddress)
   {
      /* Don't skip cycles with a pending NMI or IRQ - could cause delayed
       * interrupt. Interrupts are delayed for a few cycles already, but
       * the delay could allow the shutdown code to cycle skip again.
       * Was causing screen flashing on Top Gear 3000. */
      if (CPU.WaitCounter == 0 && !(CPU.Flags & (IRQ_PENDING_FLAG | NMI_FLAG)))
      {
         CPU.WaitAddress = NULL;
#ifndef USE_BLARGG_APU
         snes_apu_flush();
         CPU.Cycles = CPU.NextEvent;
         if (IAPU.APUExecuting)
         {
            ICPU.CPUExecuting = false;
            APUExecuteUntilEvent();
            ICPU.CPUExecuting = true;
         }
#endif
      }
      else if (CPU.WaitCounter >= 2)
         CPU.WaitCounter = 1;
      else
         CPU.WaitCounter--;
   }
}
#else
static INLINE void CPUShutdown(void)
{
   if (Settings.Shutdown && CPU.PC == CPU.WaitAddress)
   {
      if (CPU.WaitCounter >= 1)
      {
         SA1.Executing = false;
         SA1.CPUExecuting = false;
      }
      else
         CPU.WaitCounter++;
   }
}
#endif

/* From the speed-hacks branch of CatSFC */
static INLINE void ForceShutdown(void)
{
#ifndef SA1_OPCODES
   CPU.WaitAddress = NULL;
#ifndef USE_BLARGG_APU
   snes_apu_flush();
   CPU.Cycles = CPU.NextEvent;
   if (IAPU.APUExecuting)
   {
      ICPU.CPUExecuting = false;
      APUExecuteUntilEvent();
      ICPU.CPUExecuting = true;
   }
#endif
#else
   SA1.Executing = false;
   SA1.CPUExecuting = false;
#endif
}

/* BCC */
static inline __attribute__((always_inline)) void Op90(void)
{
   Relative();
   BranchCheck();
   if (!CheckCarry())
   {
      CPU.PC = CPU.PCBase + OpAddress;
#ifndef SA1_OPCODES
      CPU.Cycles += ONE_CYCLE;
#endif
      CPUShutdown();
   }
}

/* BCS */
static inline __attribute__((always_inline)) void OpB0(void)
{
   Relative();
   BranchCheck();
   if (CheckCarry())
   {
      CPU.PC = CPU.PCBase + OpAddress;
#ifndef SA1_OPCODES
      CPU.Cycles += ONE_CYCLE;
#endif
      CPUShutdown();
   }
}

/* BEQ */
static inline __attribute__((always_inline)) void OpF0(void)
{
   Relative();
   BranchCheck();
   if (CheckZero())
   {
      CPU.PC = CPU.PCBase + OpAddress;
#ifndef SA1_OPCODES
      CPU.Cycles += ONE_CYCLE;
#endif
      CPUShutdown();
   }
}

/* BMI */
static inline __attribute__((always_inline)) void Op30(void)
{
   Relative();
   BranchCheck();
   if (CheckNegative())
   {
      CPU.PC = CPU.PCBase + OpAddress;
#ifndef SA1_OPCODES
      CPU.Cycles += ONE_CYCLE;
#endif
      CPUShutdown();
   }
}

/* BNE */
static inline __attribute__((always_inline)) void OpD0(void)
{
   Relative();
   BranchCheck();
   if (!CheckZero())
   {
      CPU.PC = CPU.PCBase + OpAddress;
#ifndef SA1_OPCODES
      CPU.Cycles += ONE_CYCLE;
#endif
      CPUShutdown();
   }
}

/* BPL */
static inline __attribute__((always_inline)) void Op10(void)
{
   Relative();
   BranchCheck();
   if (!CheckNegative())
   {
      CPU.PC = CPU.PCBase + OpAddress;
#ifndef SA1_OPCODES
      CPU.Cycles += ONE_CYCLE;
#endif
      CPUShutdown();
   }
}

/* BRA */
static inline __attribute__((always_inline)) void Op80(void)
{
   Relative();
   CPU.PC = CPU.PCBase + OpAddress;
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
   CPUShutdown();
}

/* BVC */
static inline __attribute__((always_inline)) void Op50(void)
{
   Relative();
   BranchCheck();
   if (!CheckOverflow())
   {
      CPU.PC = CPU.PCBase + OpAddress;
#ifndef SA1_OPCODES
      CPU.Cycles += ONE_CYCLE;
#endif
      CPUShutdown();
   }
}

/* BVS */
static inline __attribute__((always_inline)) void Op70(void)
{
   Relative();
   BranchCheck();
   if (CheckOverflow())
   {
      CPU.PC = CPU.PCBase + OpAddress;
#ifndef SA1_OPCODES
      CPU.Cycles += ONE_CYCLE;
#endif
      CPUShutdown();
   }
}

/* ClearFlag Instructions */
/* CLC */
static inline __attribute__((always_inline)) void Op18(void)
{
   ClearCarry();
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
}

/* CLD */
static inline __attribute__((always_inline)) void OpD8(void)
{
   ClearDecimal();
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
}

/* CLI */
static inline __attribute__((always_inline)) void Op58(void)
{
   ClearIRQ();
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
}

/* CLV */
static inline __attribute__((always_inline)) void OpB8(void)
{
   ClearOverflow();
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
}

/* DEX/DEY */
static inline __attribute__((always_inline)) void OpCAX1(void)
{
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
   CPU.WaitAddress = NULL;
   ICPU.Registers.XL--;
   SetZN8(ICPU.Registers.XL);
}

static inline __attribute__((always_inline)) void OpCAX0(void)
{
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
   CPU.WaitAddress = NULL;
   ICPU.Registers.X.W--;
   SetZN16(ICPU.Registers.X.W);
}

static inline __attribute__((always_inline)) void Op88X1(void)
{
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
   CPU.WaitAddress = NULL;
   ICPU.Registers.YL--;
   SetZN8(ICPU.Registers.YL);
}

static inline __attribute__((always_inline)) void Op88X0(void)
{
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
   CPU.WaitAddress = NULL;
   ICPU.Registers.Y.W--;
   SetZN16(ICPU.Registers.Y.W);
}

/* INX/INY */
static inline __attribute__((always_inline)) void OpE8X1(void)
{
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
   CPU.WaitAddress = NULL;
   ICPU.Registers.XL++;
   SetZN8(ICPU.Registers.XL);
}

static inline __attribute__((always_inline)) void OpE8X0(void)
{
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
   CPU.WaitAddress = NULL;
   ICPU.Registers.X.W++;
   SetZN16(ICPU.Registers.X.W);
}

static inline __attribute__((always_inline)) void OpC8X1(void)
{
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
   CPU.WaitAddress = NULL;
   ICPU.Registers.YL++;
   SetZN8(ICPU.Registers.YL);
}

static inline __attribute__((always_inline)) void OpC8X0(void)
{
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
   CPU.WaitAddress = NULL;
   ICPU.Registers.Y.W++;
   SetZN16(ICPU.Registers.Y.W);
}

/* NOP */
static inline __attribute__((always_inline)) void OpEA(void)
{
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
}

/* PUSH Instructions */
#define PushB(b)\
   S9xSetByte(b, ICPU.Registers.S.W--);

#define PushBE(b)\
   PushB(b);\
   ICPU.Registers.SH = 0x01

#define PushW(w)\
   S9xSetByte((w) >> 8, ICPU.Registers.S.W);\
   S9xSetByte((w) & 0xff, (ICPU.Registers.S.W - 1) & 0xffff);\
   ICPU.Registers.S.W -= 2

#define PushWE(w)\
   PushW(w); \
   ICPU.Registers.SH = 0x01

/* PEA NL */
static inline __attribute__((always_inline)) void OpF4E1(void)
{
   Absolute(false);
   PushWE((uint16_t)OpAddress);
}

static inline __attribute__((always_inline)) void OpF4(void)
{
   Absolute(false);
   PushW((uint16_t)OpAddress);
}

/* PEI NL */
static inline __attribute__((always_inline)) void OpD4E1(void)
{
   DirectIndirect(false);
   PushWE((uint16_t)OpAddress);
}

static inline __attribute__((always_inline)) void OpD4(void)
{
   DirectIndirect(false);
   PushW((uint16_t)OpAddress);
}

/* PER NL */
static inline __attribute__((always_inline)) void Op62E1(void)
{
   RelativeLong();
   PushWE((uint16_t)OpAddress);
}

static inline __attribute__((always_inline)) void Op62(void)
{
   RelativeLong();
   PushW((uint16_t)OpAddress);
}

/* PHA */
static inline __attribute__((always_inline)) void Op48E1(void)
{
   PushBE(ICPU.Registers.AL);
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
}

static inline __attribute__((always_inline)) void Op48M1(void)
{
   PushB(ICPU.Registers.AL);
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
}

static inline __attribute__((always_inline)) void Op48M0(void)
{
   PushW(ICPU.Registers.A.W);
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
}

/* PHB */
static inline __attribute__((always_inline)) void Op8BE1(void)
{
   PushBE(ICPU.Registers.DB);
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
}

static inline __attribute__((always_inline)) void Op8B(void)
{
   PushB(ICPU.Registers.DB);
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
}

/* PHD NL */
static inline __attribute__((always_inline)) void Op0BE1(void)
{
   PushWE(ICPU.Registers.D.W);
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
}

static inline __attribute__((always_inline)) void Op0B(void)
{
   PushW(ICPU.Registers.D.W);
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
}

/* PHK */
static inline __attribute__((always_inline)) void Op4BE1(void)
{
   PushBE(ICPU.Registers.PB);
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
}

static inline __attribute__((always_inline)) void Op4B(void)
{
   PushB(ICPU.Registers.PB);
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
}

/* PHP */
static inline __attribute__((always_inline)) void Op08E1(void)
{
   S9xPackStatus();
   PushBE(ICPU.Registers.PL);
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
}

static inline __attribute__((always_inline)) void Op08(void)
{
   S9xPackStatus();
   PushB(ICPU.Registers.PL);
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
}

/* PHX */
static inline __attribute__((always_inline)) void OpDAE1(void)
{
   PushBE(ICPU.Registers.XL);
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
}

static inline __attribute__((always_inline)) void OpDAX1(void)
{
   PushB(ICPU.Registers.XL);
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
}

static inline __attribute__((always_inline)) void OpDAX0(void)
{
   PushW(ICPU.Registers.X.W);
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
}

/* PHY */
static inline __attribute__((always_inline)) void Op5AE1(void)
{
   PushBE(ICPU.Registers.YL);
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
}

static inline __attribute__((always_inline)) void Op5AX1(void)
{
   PushB(ICPU.Registers.YL);
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
}

static inline __attribute__((always_inline)) void Op5AX0(void)
{
   PushW(ICPU.Registers.Y.W);
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
}

/* PULL Instructions */
#define PullB(b)\
   b = S9xGetByte (++ICPU.Registers.S.W)

#define PullBE(b)\
   PullB(b);\
   ICPU.Registers.SH = 0x01

#define PullW(w)\
   w = S9xGetByte(++ICPU.Registers.S.W);\
   w |= (S9xGetByte(++ICPU.Registers.S.W) << 8)

#define PullWE(w)\
   PullW(w);\
   ICPU.Registers.SH = 0x01

/* PLA */
static inline __attribute__((always_inline)) void Op68E1(void)
{
#ifndef SA1_OPCODES
   CPU.Cycles += TWO_CYCLES;
#endif
   PullBE(ICPU.Registers.AL);
   SetZN8(ICPU.Registers.AL);
}

static inline __attribute__((always_inline)) void Op68M1(void)
{
#ifndef SA1_OPCODES
   CPU.Cycles += TWO_CYCLES;
#endif
   PullB(ICPU.Registers.AL);
   SetZN8(ICPU.Registers.AL);
}

static inline __attribute__((always_inline)) void Op68M0(void)
{
#ifndef SA1_OPCODES
   CPU.Cycles += TWO_CYCLES;
#endif
   PullW(ICPU.Registers.A.W);
   SetZN16(ICPU.Registers.A.W);
}

/* PLB */
static inline __attribute__((always_inline)) void OpABE1(void)
{
#ifndef SA1_OPCODES
   CPU.Cycles += TWO_CYCLES;
#endif
   PullBE(ICPU.Registers.DB);
   SetZN8(ICPU.Registers.DB);
   ICPU.ShiftedDB = ICPU.Registers.DB << 16;
}

static inline __attribute__((always_inline)) void OpAB(void)
{
#ifndef SA1_OPCODES
   CPU.Cycles += TWO_CYCLES;
#endif
   PullB(ICPU.Registers.DB);
   SetZN8(ICPU.Registers.DB);
   ICPU.ShiftedDB = ICPU.Registers.DB << 16;
}

/* PLD NL */
static inline __attribute__((always_inline)) void Op2BE1(void)
{
#ifndef SA1_OPCODES
   CPU.Cycles += TWO_CYCLES;
#endif
   PullWE(ICPU.Registers.D.W);
   SetZN16(ICPU.Registers.D.W);
}

static inline __attribute__((always_inline)) void Op2B(void)
{
#ifndef SA1_OPCODES
   CPU.Cycles += TWO_CYCLES;
#endif
   PullW(ICPU.Registers.D.W);
   SetZN16(ICPU.Registers.D.W);
}

/* PLP */
static inline __attribute__((always_inline)) void Op28E1(void)
{
#ifndef SA1_OPCODES
   CPU.Cycles += TWO_CYCLES;
#endif
   PullBE(ICPU.Registers.PL);
   S9xUnpackStatus();

   if (CheckIndex())
   {
      ICPU.Registers.XH = 0;
      ICPU.Registers.YH = 0;
   }
   S9xFixCycles();
}

static inline __attribute__((always_inline)) void Op28(void)
{
#ifndef SA1_OPCODES
   CPU.Cycles += TWO_CYCLES;
#endif
   PullB(ICPU.Registers.PL);
   S9xUnpackStatus();

   if (CheckIndex())
   {
      ICPU.Registers.XH = 0;
      ICPU.Registers.YH = 0;
   }
   S9xFixCycles();
}

/* PLX */
static inline __attribute__((always_inline)) void OpFAE1(void)
{
#ifndef SA1_OPCODES
   CPU.Cycles += TWO_CYCLES;
#endif
   PullBE(ICPU.Registers.XL);
   SetZN8(ICPU.Registers.XL);
}

static inline __attribute__((always_inline)) void OpFAX1(void)
{
#ifndef SA1_OPCODES
   CPU.Cycles += TWO_CYCLES;
#endif
   PullB(ICPU.Registers.XL);
   SetZN8(ICPU.Registers.XL);
}

static inline __attribute__((always_inline)) void OpFAX0(void)
{
#ifndef SA1_OPCODES
   CPU.Cycles += TWO_CYCLES;
#endif
   PullW(ICPU.Registers.X.W);
   SetZN16(ICPU.Registers.X.W);
}

/* PLY */
static inline __attribute__((always_inline)) void Op7AE1(void)
{
#ifndef SA1_OPCODES
   CPU.Cycles += TWO_CYCLES;
#endif
   PullBE(ICPU.Registers.YL);
   SetZN8(ICPU.Registers.YL);
}

static inline __attribute__((always_inline)) void Op7AX1(void)
{
#ifndef SA1_OPCODES
   CPU.Cycles += TWO_CYCLES;
#endif
   PullB(ICPU.Registers.YL);
   SetZN8(ICPU.Registers.YL);
}

static inline __attribute__((always_inline)) void Op7AX0(void)
{
#ifndef SA1_OPCODES
   CPU.Cycles += TWO_CYCLES;
#endif
   PullW(ICPU.Registers.Y.W);
   SetZN16(ICPU.Registers.Y.W);
}

/* SEC */
static inline __attribute__((always_inline)) void Op38(void)
{
   SetCarry();
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
}

/* SED */
static inline __attribute__((always_inline)) void OpF8(void)
{
   SetDecimal();
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
}

/* SEI */
static inline __attribute__((always_inline)) void Op78(void)
{
   SetIRQ();
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
}

/* TAX8 */
static inline __attribute__((always_inline)) void OpAAX1(void)
{
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
   ICPU.Registers.XL = ICPU.Registers.AL;
   SetZN8(ICPU.Registers.XL);
}

/* TAX16 */
static inline __attribute__((always_inline)) void OpAAX0(void)
{
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
   ICPU.Registers.X.W = ICPU.Registers.A.W;
   SetZN16(ICPU.Registers.X.W);
}

/* TAY8 */
static inline __attribute__((always_inline)) void OpA8X1(void)
{
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
   ICPU.Registers.YL = ICPU.Registers.AL;
   SetZN8(ICPU.Registers.YL);
}

/* TAY16 */
static inline __attribute__((always_inline)) void OpA8X0(void)
{
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
   ICPU.Registers.Y.W = ICPU.Registers.A.W;
   SetZN16(ICPU.Registers.Y.W);
}

static inline __attribute__((always_inline)) void Op5B(void)
{
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
   ICPU.Registers.D.W = ICPU.Registers.A.W;
   SetZN16(ICPU.Registers.D.W);
}

static inline __attribute__((always_inline)) void Op1B(void)
{
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
   ICPU.Registers.S.W = ICPU.Registers.A.W;
   if (CheckEmulation())
      ICPU.Registers.SH = 1;
}

static inline __attribute__((always_inline)) void Op7B(void)
{
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
   ICPU.Registers.A.W = ICPU.Registers.D.W;
   SetZN16(ICPU.Registers.A.W);
}

static inline __attribute__((always_inline)) void Op3B(void)
{
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
   ICPU.Registers.A.W = ICPU.Registers.S.W;
   SetZN16(ICPU.Registers.A.W);
}

static inline __attribute__((always_inline)) void OpBAX1(void)
{
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
   ICPU.Registers.XL = ICPU.Registers.SL;
   SetZN8(ICPU.Registers.XL);
}

static inline __attribute__((always_inline)) void OpBAX0(void)
{
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
   ICPU.Registers.X.W = ICPU.Registers.S.W;
   SetZN16(ICPU.Registers.X.W);
}

static inline __attribute__((always_inline)) void Op8AM1(void)
{
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
   ICPU.Registers.AL = ICPU.Registers.XL;
   SetZN8(ICPU.Registers.AL);
}

static inline __attribute__((always_inline)) void Op8AM0(void)
{
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
   ICPU.Registers.A.W = ICPU.Registers.X.W;
   SetZN16(ICPU.Registers.A.W);
}

static inline __attribute__((always_inline)) void Op9A(void)
{
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
   ICPU.Registers.S.W = ICPU.Registers.X.W;
   if (CheckEmulation())
      ICPU.Registers.SH = 1;
}

static inline __attribute__((always_inline)) void Op9BX1(void)
{
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
   ICPU.Registers.YL = ICPU.Registers.XL;
   SetZN8(ICPU.Registers.YL);
}

static inline __attribute__((always_inline)) void Op9BX0(void)
{
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
   ICPU.Registers.Y.W = ICPU.Registers.X.W;
   SetZN16(ICPU.Registers.Y.W);
}

static inline __attribute__((always_inline)) void Op98M1(void)
{
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
   ICPU.Registers.AL = ICPU.Registers.YL;
   SetZN8(ICPU.Registers.AL);
}

static inline __attribute__((always_inline)) void Op98M0(void)
{
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
   ICPU.Registers.A.W = ICPU.Registers.Y.W;
   SetZN16(ICPU.Registers.A.W);
}

static inline __attribute__((always_inline)) void OpBBX1(void)
{
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
   ICPU.Registers.XL = ICPU.Registers.YL;
   SetZN8(ICPU.Registers.XL);
}

static inline __attribute__((always_inline)) void OpBBX0(void)
{
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
   ICPU.Registers.X.W = ICPU.Registers.Y.W;
   SetZN16(ICPU.Registers.X.W);
}

/* XCE */
static inline __attribute__((always_inline)) void OpFB(void)
{
   uint8_t A1, A2;
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
   A1 = ICPU._Carry;
   A2 = ICPU.Registers.PH;
   ICPU._Carry = A2 & 1;
   ICPU.Registers.PH = A1;

   if (CheckEmulation())
   {
      SetFlags(MemoryFlag | IndexFlag);
      ICPU.Registers.SH = 1;
   }
   if (CheckIndex())
   {
      ICPU.Registers.XH = 0;
      ICPU.Registers.YH = 0;
   }
   S9xFixCycles();
}

/* BRK */
static inline __attribute__((always_inline)) void Op00(void)
{
   if (!CheckEmulation())
   {
      PushB(ICPU.Registers.PB);
      PushW(CPU.PC - CPU.PCBase + 1);
      S9xPackStatus();
      PushB(ICPU.Registers.PL);
      OpenBus = ICPU.Registers.PL;
      ClearDecimal();
      SetIRQ();

      ICPU.Registers.PB = 0;
      ICPU.ShiftedPB = 0;
      S9xSetPCBase(S9xGetWord(0xFFE6));
#ifndef SA1_OPCODES
      CPU.Cycles += TWO_CYCLES;
#endif
   }
   else
   {
      PushW(CPU.PC - CPU.PCBase);
      S9xPackStatus();
      PushB(ICPU.Registers.PL);
      OpenBus = ICPU.Registers.PL;
      ClearDecimal();
      SetIRQ();

      ICPU.Registers.PB = 0;
      ICPU.ShiftedPB = 0;
      S9xSetPCBase(S9xGetWord(0xFFFE));
#ifndef SA1_OPCODES
      CPU.Cycles += ONE_CYCLE;
#endif
   }
}

/* BRL */
static inline __attribute__((always_inline)) void Op82(void)
{
   RelativeLong();
   S9xSetPCBase(ICPU.ShiftedPB + OpAddress);
}

/* IRQ */
void S9xOpcode_IRQ(void)
{
   if (!CheckEmulation())
   {
      PushB(ICPU.Registers.PB);
      PushW(CPU.PC - CPU.PCBase);
      S9xPackStatus();
      PushB(ICPU.Registers.PL);
      OpenBus = ICPU.Registers.PL;
      ClearDecimal();
      SetIRQ();

      ICPU.Registers.PB = 0;
      ICPU.ShiftedPB = 0;
#ifdef SA1_OPCODES
      S9xSA1SetPCBase(Memory.FillRAM [0x2207] | (Memory.FillRAM [0x2208] << 8));
#else
      if (Settings.SA1 && (Memory.FillRAM [0x2209] & 0x40))
         S9xSetPCBase(Memory.FillRAM [0x220e] | (Memory.FillRAM [0x220f] << 8));
      else
         S9xSetPCBase(S9xGetWord(0xFFEE));
      CPU.Cycles += TWO_CYCLES;
#endif
   }
   else
   {
      PushW(CPU.PC - CPU.PCBase);
      S9xPackStatus();
      PushB(ICPU.Registers.PL);
      OpenBus = ICPU.Registers.PL;
      ClearDecimal();
      SetIRQ();

      ICPU.Registers.PB = 0;
      ICPU.ShiftedPB = 0;
#ifdef SA1_OPCODES
      S9xSA1SetPCBase(Memory.FillRAM [0x2207] | (Memory.FillRAM [0x2208] << 8));
#else
      if (Settings.SA1 && (Memory.FillRAM [0x2209] & 0x40))
         S9xSetPCBase(Memory.FillRAM [0x220e] | (Memory.FillRAM [0x220f] << 8));
      else
         S9xSetPCBase(S9xGetWord(0xFFFE));
      CPU.Cycles += ONE_CYCLE;
#endif
   }
}

/* NMI */
void S9xOpcode_NMI(void)
{
   if (!CheckEmulation())
   {
      PushB(ICPU.Registers.PB);
      PushW(CPU.PC - CPU.PCBase);
      S9xPackStatus();
      PushB(ICPU.Registers.PL);
      OpenBus = ICPU.Registers.PL;
      ClearDecimal();
      SetIRQ();

      ICPU.Registers.PB = 0;
      ICPU.ShiftedPB = 0;
#ifdef SA1_OPCODES
      S9xSA1SetPCBase(Memory.FillRAM [0x2205] | (Memory.FillRAM [0x2206] << 8));
#else
      if (Settings.SA1 && (Memory.FillRAM [0x2209] & 0x20))
         S9xSetPCBase(Memory.FillRAM [0x220c] | (Memory.FillRAM [0x220d] << 8));
      else
         S9xSetPCBase(S9xGetWord(0xFFEA));
      CPU.Cycles += TWO_CYCLES;
#endif
   }
   else
   {
      PushW(CPU.PC - CPU.PCBase);
      S9xPackStatus();
      PushB(ICPU.Registers.PL);
      OpenBus = ICPU.Registers.PL;
      ClearDecimal();
      SetIRQ();

      ICPU.Registers.PB = 0;
      ICPU.ShiftedPB = 0;
#ifdef SA1_OPCODES
      S9xSA1SetPCBase(Memory.FillRAM [0x2205] | (Memory.FillRAM [0x2206] << 8));
#else
      if (Settings.SA1 && (Memory.FillRAM [0x2209] & 0x20))
         S9xSetPCBase(Memory.FillRAM [0x220c] | (Memory.FillRAM [0x220d] << 8));
      else
         S9xSetPCBase(S9xGetWord(0xFFFA));
      CPU.Cycles += ONE_CYCLE;
#endif
   }
}

/* COP */
static inline __attribute__((always_inline)) void Op02(void)
{
   if (!CheckEmulation())
   {
      PushB(ICPU.Registers.PB);
      PushW(CPU.PC - CPU.PCBase + 1);
      S9xPackStatus();
      PushB(ICPU.Registers.PL);
      OpenBus = ICPU.Registers.PL;
      ClearDecimal();
      SetIRQ();

      ICPU.Registers.PB = 0;
      ICPU.ShiftedPB = 0;
      S9xSetPCBase(S9xGetWord(0xFFE4));
#ifndef SA1_OPCODES
      CPU.Cycles += TWO_CYCLES;
#endif
   }
   else
   {
      PushW(CPU.PC - CPU.PCBase);
      S9xPackStatus();
      PushB(ICPU.Registers.PL);
      OpenBus = ICPU.Registers.PL;
      ClearDecimal();
      SetIRQ();

      ICPU.Registers.PB = 0;
      ICPU.ShiftedPB = 0;
      S9xSetPCBase(S9xGetWord(0xFFF4));
#ifndef SA1_OPCODES
      CPU.Cycles += ONE_CYCLE;
#endif
   }
}

/* JML */
static inline __attribute__((always_inline)) void OpDC(void)
{
   AbsoluteIndirectLong(false);
   ICPU.Registers.PB = (uint8_t)(OpAddress >> 16);
   ICPU.ShiftedPB = OpAddress & 0xff0000;
   S9xSetPCBase(OpAddress);
#ifndef SA1_OPCODES
   CPU.Cycles += TWO_CYCLES;
#endif
}

static inline __attribute__((always_inline)) void Op5C(void)
{
   AbsoluteLong(false);
   ICPU.Registers.PB = (uint8_t)(OpAddress >> 16);
   ICPU.ShiftedPB = OpAddress & 0xff0000;
   S9xSetPCBase(OpAddress);
}

/* JMP */
static inline __attribute__((always_inline)) void Op4C(void)
{
   Absolute(false);
   S9xSetPCBase(ICPU.ShiftedPB + (OpAddress & 0xffff));
#ifdef SA1_OPCODES
   CPUShutdown();
#endif
}

static inline __attribute__((always_inline)) void Op6C(void)
{
   AbsoluteIndirect(false);
   S9xSetPCBase(ICPU.ShiftedPB + (OpAddress & 0xffff));
}

static inline __attribute__((always_inline)) void Op7C(void)
{
   AbsoluteIndexedIndirect(false);
   S9xSetPCBase(ICPU.ShiftedPB + OpAddress);
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
}

/* JSL/RTL */
static inline __attribute__((always_inline)) void Op22E1(void)
{
   AbsoluteLong(false);
   PushB(ICPU.Registers.PB);
   PushWE(CPU.PC - CPU.PCBase - 1);
   ICPU.Registers.PB = (uint8_t)(OpAddress >> 16);
   ICPU.ShiftedPB = OpAddress & 0xff0000;
   S9xSetPCBase(OpAddress);
}

static inline __attribute__((always_inline)) void Op22(void)
{
   AbsoluteLong(false);
   PushB(ICPU.Registers.PB);
   PushW(CPU.PC - CPU.PCBase - 1);
   ICPU.Registers.PB = (uint8_t)(OpAddress >> 16);
   ICPU.ShiftedPB = OpAddress & 0xff0000;
   S9xSetPCBase(OpAddress);
}

static inline __attribute__((always_inline)) void Op6BE1(void)
{
   PullWE(ICPU.Registers.PC);
   PullB(ICPU.Registers.PB);
   ICPU.ShiftedPB = ICPU.Registers.PB << 16;
   S9xSetPCBase(ICPU.ShiftedPB + ((ICPU.Registers.PC + 1) & 0xffff));
#ifndef SA1_OPCODES
   CPU.Cycles += TWO_CYCLES;
#endif
}

static inline __attribute__((always_inline)) void Op6B(void)
{
   PullW(ICPU.Registers.PC);
   PullB(ICPU.Registers.PB);
   ICPU.ShiftedPB = ICPU.Registers.PB << 16;
   S9xSetPCBase(ICPU.ShiftedPB + ((ICPU.Registers.PC + 1) & 0xffff));
#ifndef SA1_OPCODES
   CPU.Cycles += TWO_CYCLES;
#endif
}

/* JSR/RTS */
static inline __attribute__((always_inline)) void Op20(void)
{
   Absolute(false);
   PushW(CPU.PC - CPU.PCBase - 1);
   S9xSetPCBase(ICPU.ShiftedPB + (OpAddress & 0xffff));
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
}

/* JSR a,x */
static inline __attribute__((always_inline)) void OpFCE1(void)
{
   AbsoluteIndexedIndirect(false);
   PushWE(CPU.PC - CPU.PCBase - 1);
   S9xSetPCBase(ICPU.ShiftedPB + OpAddress);
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
}

static inline __attribute__((always_inline)) void OpFC(void)
{
   AbsoluteIndexedIndirect(false);
   PushW(CPU.PC - CPU.PCBase - 1);
   S9xSetPCBase(ICPU.ShiftedPB + OpAddress);
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE;
#endif
}

static inline __attribute__((always_inline)) void Op60(void)
{
   PullW(ICPU.Registers.PC);
   S9xSetPCBase(ICPU.ShiftedPB + ((ICPU.Registers.PC + 1) & 0xffff));
#ifndef SA1_OPCODES
   CPU.Cycles += ONE_CYCLE * 3;
#endif
}

/* MVN/MVP */
static inline __attribute__((always_inline)) void Op54X1(void)
{
#ifndef SA1_OPCODES
   CPU.Cycles += CPU.MemSpeedx2 + TWO_CYCLES;
#endif

   ICPU.Registers.DB = *CPU.PC++;
   ICPU.ShiftedDB = ICPU.Registers.DB << 16;
   OpenBus = *CPU.PC++;

   S9xSetByte(S9xGetByte((OpenBus << 16) + ICPU.Registers.X.W), ICPU.ShiftedDB + ICPU.Registers.Y.W);

   ICPU.Registers.XL++;
   ICPU.Registers.YL++;
   ICPU.Registers.A.W--;
   if (ICPU.Registers.A.W != 0xffff)
      CPU.PC -= 3;
}

static inline __attribute__((always_inline)) void Op54X0(void)
{
#ifndef SA1_OPCODES
   CPU.Cycles += CPU.MemSpeedx2 + TWO_CYCLES;
#endif

   ICPU.Registers.DB = *CPU.PC++;
   ICPU.ShiftedDB = ICPU.Registers.DB << 16;
   OpenBus = *CPU.PC++;

   S9xSetByte(S9xGetByte((OpenBus << 16) + ICPU.Registers.X.W), ICPU.ShiftedDB + ICPU.Registers.Y.W);

   ICPU.Registers.X.W++;
   ICPU.Registers.Y.W++;
   ICPU.Registers.A.W--;
   if (ICPU.Registers.A.W != 0xffff)
      CPU.PC -= 3;
}

static inline __attribute__((always_inline)) void Op44X1(void)
{
#ifndef SA1_OPCODES
   CPU.Cycles += CPU.MemSpeedx2 + TWO_CYCLES;
#endif
   ICPU.Registers.DB = *CPU.PC++;
   ICPU.ShiftedDB = ICPU.Registers.DB << 16;
   OpenBus = *CPU.PC++;
   S9xSetByte(S9xGetByte((OpenBus << 16) + ICPU.Registers.X.W), ICPU.ShiftedDB + ICPU.Registers.Y.W);

   ICPU.Registers.XL--;
   ICPU.Registers.YL--;
   ICPU.Registers.A.W--;
   if (ICPU.Registers.A.W != 0xffff)
      CPU.PC -= 3;
}

static inline __attribute__((always_inline)) void Op44X0(void)
{
#ifndef SA1_OPCODES
   CPU.Cycles += CPU.MemSpeedx2 + TWO_CYCLES;
#endif
   ICPU.Registers.DB = *CPU.PC++;
   ICPU.ShiftedDB = ICPU.Registers.DB << 16;
   OpenBus = *CPU.PC++;
   S9xSetByte(S9xGetByte((OpenBus << 16) + ICPU.Registers.X.W), ICPU.ShiftedDB + ICPU.Registers.Y.W);

   ICPU.Registers.X.W--;
   ICPU.Registers.Y.W--;
   ICPU.Registers.A.W--;
   if (ICPU.Registers.A.W != 0xffff)
      CPU.PC -= 3;
}

/* REP/SEP */
static inline __attribute__((always_inline)) void OpC2(void)
{
   uint8_t Work8 = ~*CPU.PC++;
   ICPU.Registers.PL &= Work8;
   ICPU._Carry &= Work8;
   ICPU._Overflow &= (Work8 >> 6);
   ICPU._Negative &= Work8;
   ICPU._Zero |= ~Work8 & Zero;

#ifndef SA1_OPCODES
   CPU.Cycles += CPU.MemSpeed + ONE_CYCLE;
#endif
   if (CheckEmulation())
      SetFlags(MemoryFlag | IndexFlag);
   if (CheckIndex())
   {
      ICPU.Registers.XH = 0;
      ICPU.Registers.YH = 0;
   }
   S9xFixCycles();
}

static inline __attribute__((always_inline)) void OpE2(void)
{
   uint8_t Work8 = *CPU.PC++;
   ICPU.Registers.PL |= Work8;
   ICPU._Carry |= Work8 & 1;
   ICPU._Overflow |= (Work8 >> 6) & 1;
   ICPU._Negative |= Work8;
   if (Work8 & Zero)
      ICPU._Zero = 0;
#ifndef SA1_OPCODES
   CPU.Cycles += CPU.MemSpeed + ONE_CYCLE;
#endif
   if (CheckEmulation())
      SetFlags(MemoryFlag | IndexFlag);
   if (CheckIndex())
   {
      ICPU.Registers.XH = 0;
      ICPU.Registers.YH = 0;
   }
   S9xFixCycles();
}

/* XBA */
static inline __attribute__((always_inline)) void OpEB(void)
{
   uint8_t Work8 = ICPU.Registers.AL;
   ICPU.Registers.AL = ICPU.Registers.AH;
   ICPU.Registers.AH = Work8;
   SetZN8(ICPU.Registers.AL);
#ifndef SA1_OPCODES
   CPU.Cycles += TWO_CYCLES;
#endif
}

/* RTI */
static inline __attribute__((always_inline)) void Op40(void)
{
   PullB(ICPU.Registers.PL);
   S9xUnpackStatus();
   PullW(ICPU.Registers.PC);
   if (!CheckEmulation())
   {
      PullB(ICPU.Registers.PB);
      ICPU.ShiftedPB = ICPU.Registers.PB << 16;
   }
   else
      SetFlags(MemoryFlag | IndexFlag);
   S9xSetPCBase(ICPU.ShiftedPB + ICPU.Registers.PC);
   if (CheckIndex())
   {
      ICPU.Registers.XH = 0;
      ICPU.Registers.YH = 0;
   }
#ifndef SA1_OPCODES
   CPU.Cycles += TWO_CYCLES;
#endif
   S9xFixCycles();
}

/* WAI */
static inline __attribute__((always_inline)) void OpCB(void)
{
#ifdef SA1_OPCODES
   SA1.WaitingForInterrupt = true;
   SA1.PC--;
#else /* SA_OPCODES */
   CPU.WaitingForInterrupt = true;
   CPU.PC--;
   if (Settings.Shutdown)
   {
      snes_apu_flush();
      CPU.Cycles = CPU.NextEvent;
#ifndef USE_BLARGG_APU
      if (IAPU.APUExecuting)
      {
         ICPU.CPUExecuting = false;
         APUExecuteUntilEvent();
         ICPU.CPUExecuting = true;
      }
#endif
   }
#endif
}

/* Usually an STP opcode; SNESAdvance speed hack, not implemented in Snes9xTYL | Snes9x-Euphoria (from the speed-hacks branch of CatSFC) */
static inline __attribute__((always_inline)) void OpDB(void)
{
#ifndef NO_SPEEDHACKS
   int8_t BranchOffset;
   uint8_t NextByte = *CPU.PC++;

   ForceShutdown();

   BranchOffset = (NextByte & 0x7F) | ((NextByte & 0x40) << 1);
   /* ^ -64 .. +63, sign extend bit 6 into 7 for unpacking */
   OpAddress = ((int32_t) (CPU.PC - CPU.PCBase) + BranchOffset) & 0xffff;

   switch (NextByte & 0x80)
   {
   case 0x00: /* BNE */
      BranchCheck();
      if (!CheckZero ())
      {
         CPU.PC = CPU.PCBase + OpAddress;
#ifndef SA1_OPCODES
         CPU.Cycles += ONE_CYCLE;
#endif
         CPUShutdown ();
      }
      return;
   case 0x80: /* BEQ */
      BranchCheck();
      if (CheckZero ())
      {
         CPU.PC = CPU.PCBase + OpAddress;
#ifndef SA1_OPCODES
         CPU.Cycles += ONE_CYCLE;
#endif
         CPUShutdown ();
      }
      return;
   }
#else
     CPU.PC--;
     CPU.Flags |= DEBUG_MODE_FLAG;
#endif
}

/* SNESAdvance speed hack, as implemented in Snes9xTYL / Snes9x-Euphoria (from the speed-hacks branch of CatSFC) */
static inline __attribute__((always_inline)) void Op42(void)
{
#ifndef NO_SPEEDHACKS
   int8_t BranchOffset;
   uint8_t NextByte = *CPU.PC++;

   ForceShutdown();

   BranchOffset = 0xF0 | (NextByte & 0xF); /* always negative */
   OpAddress = ((int32_t) (CPU.PC - CPU.PCBase) + BranchOffset) & 0xffff;

   switch (NextByte & 0xF0)
   {
   case 0x10: /* BPL */
      BranchCheck();
      if (!CheckNegative ())
      {
         CPU.PC = CPU.PCBase + OpAddress;
#ifndef SA1_OPCODES
         CPU.Cycles += ONE_CYCLE;
#endif
         CPUShutdown ();
      }
      return;
   case 0x30: /* BMI */
      BranchCheck();
      if (CheckNegative ())
      {
         CPU.PC = CPU.PCBase + OpAddress;
#ifndef SA1_OPCODES
         CPU.Cycles += ONE_CYCLE;
#endif
         CPUShutdown ();
      }
      return;
   case 0x50: /* BVC */
      BranchCheck();
      if (!CheckOverflow ())
      {
         CPU.PC = CPU.PCBase + OpAddress;
#ifndef SA1_OPCODES
         CPU.Cycles += ONE_CYCLE;
#endif
         CPUShutdown ();
      }
      return;
   case 0x70: /* BVS */
      BranchCheck();
      if (CheckOverflow ())
      {
         CPU.PC = CPU.PCBase + OpAddress;
#ifndef SA1_OPCODES
         CPU.Cycles += ONE_CYCLE;
#endif
         CPUShutdown ();
      }
      return;
   case 0x80: /* BRA */
      CPU.PC = CPU.PCBase + OpAddress;
#ifndef SA1_OPCODES
      CPU.Cycles += ONE_CYCLE;
#endif
      CPUShutdown ();
      return;
   case 0x90: /* BCC */
      BranchCheck();
      if (!CheckCarry ())
      {
         CPU.PC = CPU.PCBase + OpAddress;
#ifndef SA1_OPCODES
         CPU.Cycles += ONE_CYCLE;
#endif
         CPUShutdown ();
      }
      return;
   case 0xB0: /* BCS */
      BranchCheck();
      if (CheckCarry ())
      {
         CPU.PC = CPU.PCBase + OpAddress;
#ifndef SA1_OPCODES
         CPU.Cycles += ONE_CYCLE;
#endif
         CPUShutdown ();
      }
      return;
   case 0xD0: /* BNE */
      BranchCheck();
      if (!CheckZero ())
      {
         CPU.PC = CPU.PCBase + OpAddress;
#ifndef SA1_OPCODES
         CPU.Cycles += ONE_CYCLE;
#endif
         CPUShutdown ();
      }
      return;
   case 0xF0: /* BEQ */
      BranchCheck();
      if (CheckZero ())
      {
         CPU.PC = CPU.PCBase + OpAddress;
#ifndef SA1_OPCODES
         CPU.Cycles += ONE_CYCLE;
#endif
         CPUShutdown ();
      }
      return;
   }
#endif
}

/* CPU-S9xOpcodes Definitions */
/* Internal dispatch IDs are never serialized. */
enum {
#define SNES_OPCODE(name) SNES_OPCODE_ID_##name,
#include "pxa_snes_opcodes.inc"
#undef SNES_OPCODE
};

const SOpcodes S9xOpcodesM1X1[256] =
{
   {SNES_OPCODE_ID_Op00},      {SNES_OPCODE_ID_Op01M1},    {SNES_OPCODE_ID_Op02},      {SNES_OPCODE_ID_Op03M1},    {SNES_OPCODE_ID_Op04M1},
   {SNES_OPCODE_ID_Op05M1},    {SNES_OPCODE_ID_Op06M1},    {SNES_OPCODE_ID_Op07M1},    {SNES_OPCODE_ID_Op08},      {SNES_OPCODE_ID_Op09M1},
   {SNES_OPCODE_ID_Op0AM1},    {SNES_OPCODE_ID_Op0B},      {SNES_OPCODE_ID_Op0CM1},    {SNES_OPCODE_ID_Op0DM1},    {SNES_OPCODE_ID_Op0EM1},
   {SNES_OPCODE_ID_Op0FM1},    {SNES_OPCODE_ID_Op10},      {SNES_OPCODE_ID_Op11M1},    {SNES_OPCODE_ID_Op12M1},    {SNES_OPCODE_ID_Op13M1},
   {SNES_OPCODE_ID_Op14M1},    {SNES_OPCODE_ID_Op15M1},    {SNES_OPCODE_ID_Op16M1},    {SNES_OPCODE_ID_Op17M1},    {SNES_OPCODE_ID_Op18},
   {SNES_OPCODE_ID_Op19M1},    {SNES_OPCODE_ID_Op1AM1},    {SNES_OPCODE_ID_Op1B},      {SNES_OPCODE_ID_Op1CM1},    {SNES_OPCODE_ID_Op1DM1},
   {SNES_OPCODE_ID_Op1EM1},    {SNES_OPCODE_ID_Op1FM1},    {SNES_OPCODE_ID_Op20},      {SNES_OPCODE_ID_Op21M1},    {SNES_OPCODE_ID_Op22},
   {SNES_OPCODE_ID_Op23M1},    {SNES_OPCODE_ID_Op24M1},    {SNES_OPCODE_ID_Op25M1},    {SNES_OPCODE_ID_Op26M1},    {SNES_OPCODE_ID_Op27M1},
   {SNES_OPCODE_ID_Op28},      {SNES_OPCODE_ID_Op29M1},    {SNES_OPCODE_ID_Op2AM1},    {SNES_OPCODE_ID_Op2B},      {SNES_OPCODE_ID_Op2CM1},
   {SNES_OPCODE_ID_Op2DM1},    {SNES_OPCODE_ID_Op2EM1},    {SNES_OPCODE_ID_Op2FM1},    {SNES_OPCODE_ID_Op30},      {SNES_OPCODE_ID_Op31M1},
   {SNES_OPCODE_ID_Op32M1},    {SNES_OPCODE_ID_Op33M1},    {SNES_OPCODE_ID_Op34M1},    {SNES_OPCODE_ID_Op35M1},    {SNES_OPCODE_ID_Op36M1},
   {SNES_OPCODE_ID_Op37M1},    {SNES_OPCODE_ID_Op38},      {SNES_OPCODE_ID_Op39M1},    {SNES_OPCODE_ID_Op3AM1},    {SNES_OPCODE_ID_Op3B},
   {SNES_OPCODE_ID_Op3CM1},    {SNES_OPCODE_ID_Op3DM1},    {SNES_OPCODE_ID_Op3EM1},    {SNES_OPCODE_ID_Op3FM1},    {SNES_OPCODE_ID_Op40},
   {SNES_OPCODE_ID_Op41M1},    {SNES_OPCODE_ID_Op42},      {SNES_OPCODE_ID_Op43M1},    {SNES_OPCODE_ID_Op44X1},    {SNES_OPCODE_ID_Op45M1},
   {SNES_OPCODE_ID_Op46M1},    {SNES_OPCODE_ID_Op47M1},    {SNES_OPCODE_ID_Op48M1},    {SNES_OPCODE_ID_Op49M1},    {SNES_OPCODE_ID_Op4AM1},
   {SNES_OPCODE_ID_Op4B},      {SNES_OPCODE_ID_Op4C},      {SNES_OPCODE_ID_Op4DM1},    {SNES_OPCODE_ID_Op4EM1},    {SNES_OPCODE_ID_Op4FM1},
   {SNES_OPCODE_ID_Op50},      {SNES_OPCODE_ID_Op51M1},    {SNES_OPCODE_ID_Op52M1},    {SNES_OPCODE_ID_Op53M1},    {SNES_OPCODE_ID_Op54X1},
   {SNES_OPCODE_ID_Op55M1},    {SNES_OPCODE_ID_Op56M1},    {SNES_OPCODE_ID_Op57M1},    {SNES_OPCODE_ID_Op58},      {SNES_OPCODE_ID_Op59M1},
   {SNES_OPCODE_ID_Op5AX1},    {SNES_OPCODE_ID_Op5B},      {SNES_OPCODE_ID_Op5C},      {SNES_OPCODE_ID_Op5DM1},    {SNES_OPCODE_ID_Op5EM1},
   {SNES_OPCODE_ID_Op5FM1},    {SNES_OPCODE_ID_Op60},      {SNES_OPCODE_ID_Op61M1},    {SNES_OPCODE_ID_Op62},      {SNES_OPCODE_ID_Op63M1},
   {SNES_OPCODE_ID_Op64M1},    {SNES_OPCODE_ID_Op65M1},    {SNES_OPCODE_ID_Op66M1},    {SNES_OPCODE_ID_Op67M1},    {SNES_OPCODE_ID_Op68M1},
   {SNES_OPCODE_ID_Op69M1},    {SNES_OPCODE_ID_Op6AM1},    {SNES_OPCODE_ID_Op6B},      {SNES_OPCODE_ID_Op6C},      {SNES_OPCODE_ID_Op6DM1},
   {SNES_OPCODE_ID_Op6EM1},    {SNES_OPCODE_ID_Op6FM1},    {SNES_OPCODE_ID_Op70},      {SNES_OPCODE_ID_Op71M1},    {SNES_OPCODE_ID_Op72M1},
   {SNES_OPCODE_ID_Op73M1},    {SNES_OPCODE_ID_Op74M1},    {SNES_OPCODE_ID_Op75M1},    {SNES_OPCODE_ID_Op76M1},    {SNES_OPCODE_ID_Op77M1},
   {SNES_OPCODE_ID_Op78},      {SNES_OPCODE_ID_Op79M1},    {SNES_OPCODE_ID_Op7AX1},    {SNES_OPCODE_ID_Op7B},      {SNES_OPCODE_ID_Op7C},
   {SNES_OPCODE_ID_Op7DM1},    {SNES_OPCODE_ID_Op7EM1},    {SNES_OPCODE_ID_Op7FM1},    {SNES_OPCODE_ID_Op80},      {SNES_OPCODE_ID_Op81M1},
   {SNES_OPCODE_ID_Op82},      {SNES_OPCODE_ID_Op83M1},    {SNES_OPCODE_ID_Op84X1},    {SNES_OPCODE_ID_Op85M1},    {SNES_OPCODE_ID_Op86X1},
   {SNES_OPCODE_ID_Op87M1},    {SNES_OPCODE_ID_Op88X1},    {SNES_OPCODE_ID_Op89M1},    {SNES_OPCODE_ID_Op8AM1},    {SNES_OPCODE_ID_Op8B},
   {SNES_OPCODE_ID_Op8CX1},    {SNES_OPCODE_ID_Op8DM1},    {SNES_OPCODE_ID_Op8EX1},    {SNES_OPCODE_ID_Op8FM1},    {SNES_OPCODE_ID_Op90},
   {SNES_OPCODE_ID_Op91M1},    {SNES_OPCODE_ID_Op92M1},    {SNES_OPCODE_ID_Op93M1},    {SNES_OPCODE_ID_Op94X1},    {SNES_OPCODE_ID_Op95M1},
   {SNES_OPCODE_ID_Op96X1},    {SNES_OPCODE_ID_Op97M1},    {SNES_OPCODE_ID_Op98M1},    {SNES_OPCODE_ID_Op99M1},    {SNES_OPCODE_ID_Op9A},
   {SNES_OPCODE_ID_Op9BX1},    {SNES_OPCODE_ID_Op9CM1},    {SNES_OPCODE_ID_Op9DM1},    {SNES_OPCODE_ID_Op9EM1},    {SNES_OPCODE_ID_Op9FM1},
   {SNES_OPCODE_ID_OpA0X1},    {SNES_OPCODE_ID_OpA1M1},    {SNES_OPCODE_ID_OpA2X1},    {SNES_OPCODE_ID_OpA3M1},    {SNES_OPCODE_ID_OpA4X1},
   {SNES_OPCODE_ID_OpA5M1},    {SNES_OPCODE_ID_OpA6X1},    {SNES_OPCODE_ID_OpA7M1},    {SNES_OPCODE_ID_OpA8X1},    {SNES_OPCODE_ID_OpA9M1},
   {SNES_OPCODE_ID_OpAAX1},    {SNES_OPCODE_ID_OpAB},      {SNES_OPCODE_ID_OpACX1},    {SNES_OPCODE_ID_OpADM1},    {SNES_OPCODE_ID_OpAEX1},
   {SNES_OPCODE_ID_OpAFM1},    {SNES_OPCODE_ID_OpB0},      {SNES_OPCODE_ID_OpB1M1},    {SNES_OPCODE_ID_OpB2M1},    {SNES_OPCODE_ID_OpB3M1},
   {SNES_OPCODE_ID_OpB4X1},    {SNES_OPCODE_ID_OpB5M1},    {SNES_OPCODE_ID_OpB6X1},    {SNES_OPCODE_ID_OpB7M1},    {SNES_OPCODE_ID_OpB8},
   {SNES_OPCODE_ID_OpB9M1},    {SNES_OPCODE_ID_OpBAX1},    {SNES_OPCODE_ID_OpBBX1},    {SNES_OPCODE_ID_OpBCX1},    {SNES_OPCODE_ID_OpBDM1},
   {SNES_OPCODE_ID_OpBEX1},    {SNES_OPCODE_ID_OpBFM1},    {SNES_OPCODE_ID_OpC0X1},    {SNES_OPCODE_ID_OpC1M1},    {SNES_OPCODE_ID_OpC2},
   {SNES_OPCODE_ID_OpC3M1},    {SNES_OPCODE_ID_OpC4X1},    {SNES_OPCODE_ID_OpC5M1},    {SNES_OPCODE_ID_OpC6M1},    {SNES_OPCODE_ID_OpC7M1},
   {SNES_OPCODE_ID_OpC8X1},    {SNES_OPCODE_ID_OpC9M1},    {SNES_OPCODE_ID_OpCAX1},    {SNES_OPCODE_ID_OpCB},      {SNES_OPCODE_ID_OpCCX1},
   {SNES_OPCODE_ID_OpCDM1},    {SNES_OPCODE_ID_OpCEM1},    {SNES_OPCODE_ID_OpCFM1},    {SNES_OPCODE_ID_OpD0},      {SNES_OPCODE_ID_OpD1M1},
   {SNES_OPCODE_ID_OpD2M1},    {SNES_OPCODE_ID_OpD3M1},    {SNES_OPCODE_ID_OpD4},      {SNES_OPCODE_ID_OpD5M1},    {SNES_OPCODE_ID_OpD6M1},
   {SNES_OPCODE_ID_OpD7M1},    {SNES_OPCODE_ID_OpD8},      {SNES_OPCODE_ID_OpD9M1},    {SNES_OPCODE_ID_OpDAX1},    {SNES_OPCODE_ID_OpDB},
   {SNES_OPCODE_ID_OpDC},      {SNES_OPCODE_ID_OpDDM1},    {SNES_OPCODE_ID_OpDEM1},    {SNES_OPCODE_ID_OpDFM1},    {SNES_OPCODE_ID_OpE0X1},
   {SNES_OPCODE_ID_OpE1M1},    {SNES_OPCODE_ID_OpE2},      {SNES_OPCODE_ID_OpE3M1},    {SNES_OPCODE_ID_OpE4X1},    {SNES_OPCODE_ID_OpE5M1},
   {SNES_OPCODE_ID_OpE6M1},    {SNES_OPCODE_ID_OpE7M1},    {SNES_OPCODE_ID_OpE8X1},    {SNES_OPCODE_ID_OpE9M1},    {SNES_OPCODE_ID_OpEA},
   {SNES_OPCODE_ID_OpEB},      {SNES_OPCODE_ID_OpECX1},    {SNES_OPCODE_ID_OpEDM1},    {SNES_OPCODE_ID_OpEEM1},    {SNES_OPCODE_ID_OpEFM1},
   {SNES_OPCODE_ID_OpF0},      {SNES_OPCODE_ID_OpF1M1},    {SNES_OPCODE_ID_OpF2M1},    {SNES_OPCODE_ID_OpF3M1},    {SNES_OPCODE_ID_OpF4},
   {SNES_OPCODE_ID_OpF5M1},    {SNES_OPCODE_ID_OpF6M1},    {SNES_OPCODE_ID_OpF7M1},    {SNES_OPCODE_ID_OpF8},      {SNES_OPCODE_ID_OpF9M1},
   {SNES_OPCODE_ID_OpFAX1},    {SNES_OPCODE_ID_OpFB},      {SNES_OPCODE_ID_OpFC},      {SNES_OPCODE_ID_OpFDM1},    {SNES_OPCODE_ID_OpFEM1},
   {SNES_OPCODE_ID_OpFFM1}
};

const SOpcodes S9xOpcodesE1[256] =
{
    {SNES_OPCODE_ID_Op00},	     {SNES_OPCODE_ID_Op01M1},    {SNES_OPCODE_ID_Op02},      {SNES_OPCODE_ID_Op03M1},    {SNES_OPCODE_ID_Op04M1},
    {SNES_OPCODE_ID_Op05M1},    {SNES_OPCODE_ID_Op06M1},    {SNES_OPCODE_ID_Op07M1},    {SNES_OPCODE_ID_Op08E1},    {SNES_OPCODE_ID_Op09M1},
    {SNES_OPCODE_ID_Op0AM1},    {SNES_OPCODE_ID_Op0BE1},    {SNES_OPCODE_ID_Op0CM1},    {SNES_OPCODE_ID_Op0DM1},    {SNES_OPCODE_ID_Op0EM1},
    {SNES_OPCODE_ID_Op0FM1},    {SNES_OPCODE_ID_Op10},      {SNES_OPCODE_ID_Op11M1},    {SNES_OPCODE_ID_Op12M1},    {SNES_OPCODE_ID_Op13M1},
    {SNES_OPCODE_ID_Op14M1},    {SNES_OPCODE_ID_Op15M1},    {SNES_OPCODE_ID_Op16M1},    {SNES_OPCODE_ID_Op17M1},    {SNES_OPCODE_ID_Op18},
    {SNES_OPCODE_ID_Op19M1},    {SNES_OPCODE_ID_Op1AM1},    {SNES_OPCODE_ID_Op1B},      {SNES_OPCODE_ID_Op1CM1},    {SNES_OPCODE_ID_Op1DM1},
    {SNES_OPCODE_ID_Op1EM1},    {SNES_OPCODE_ID_Op1FM1},    {SNES_OPCODE_ID_Op20},      {SNES_OPCODE_ID_Op21M1},    {SNES_OPCODE_ID_Op22E1},
    {SNES_OPCODE_ID_Op23M1},    {SNES_OPCODE_ID_Op24M1},    {SNES_OPCODE_ID_Op25M1},    {SNES_OPCODE_ID_Op26M1},    {SNES_OPCODE_ID_Op27M1},
    {SNES_OPCODE_ID_Op28},      {SNES_OPCODE_ID_Op29M1},    {SNES_OPCODE_ID_Op2AM1},    {SNES_OPCODE_ID_Op2BE1},    {SNES_OPCODE_ID_Op2CM1},
    {SNES_OPCODE_ID_Op2DM1},    {SNES_OPCODE_ID_Op2EM1},    {SNES_OPCODE_ID_Op2FM1},    {SNES_OPCODE_ID_Op30},      {SNES_OPCODE_ID_Op31M1},
    {SNES_OPCODE_ID_Op32M1},    {SNES_OPCODE_ID_Op33M1},    {SNES_OPCODE_ID_Op34M1},    {SNES_OPCODE_ID_Op35M1},    {SNES_OPCODE_ID_Op36M1},
    {SNES_OPCODE_ID_Op37M1},    {SNES_OPCODE_ID_Op38},      {SNES_OPCODE_ID_Op39M1},    {SNES_OPCODE_ID_Op3AM1},    {SNES_OPCODE_ID_Op3B},
    {SNES_OPCODE_ID_Op3CM1},    {SNES_OPCODE_ID_Op3DM1},    {SNES_OPCODE_ID_Op3EM1},    {SNES_OPCODE_ID_Op3FM1},    {SNES_OPCODE_ID_Op40},
    {SNES_OPCODE_ID_Op41M1},    {SNES_OPCODE_ID_Op42},      {SNES_OPCODE_ID_Op43M1},    {SNES_OPCODE_ID_Op44X1},    {SNES_OPCODE_ID_Op45M1},
    {SNES_OPCODE_ID_Op46M1},    {SNES_OPCODE_ID_Op47M1},    {SNES_OPCODE_ID_Op48E1},    {SNES_OPCODE_ID_Op49M1},    {SNES_OPCODE_ID_Op4AM1},
    {SNES_OPCODE_ID_Op4BE1},    {SNES_OPCODE_ID_Op4C},      {SNES_OPCODE_ID_Op4DM1},    {SNES_OPCODE_ID_Op4EM1},    {SNES_OPCODE_ID_Op4FM1},
    {SNES_OPCODE_ID_Op50},      {SNES_OPCODE_ID_Op51M1},    {SNES_OPCODE_ID_Op52M1},    {SNES_OPCODE_ID_Op53M1},    {SNES_OPCODE_ID_Op54X1},
    {SNES_OPCODE_ID_Op55M1},    {SNES_OPCODE_ID_Op56M1},    {SNES_OPCODE_ID_Op57M1},    {SNES_OPCODE_ID_Op58},      {SNES_OPCODE_ID_Op59M1},
    {SNES_OPCODE_ID_Op5AE1},    {SNES_OPCODE_ID_Op5B},      {SNES_OPCODE_ID_Op5C},      {SNES_OPCODE_ID_Op5DM1},    {SNES_OPCODE_ID_Op5EM1},
    {SNES_OPCODE_ID_Op5FM1},    {SNES_OPCODE_ID_Op60},      {SNES_OPCODE_ID_Op61M1},    {SNES_OPCODE_ID_Op62E1},    {SNES_OPCODE_ID_Op63M1},
    {SNES_OPCODE_ID_Op64M1},    {SNES_OPCODE_ID_Op65M1},    {SNES_OPCODE_ID_Op66M1},    {SNES_OPCODE_ID_Op67M1},    {SNES_OPCODE_ID_Op68E1},
    {SNES_OPCODE_ID_Op69M1},    {SNES_OPCODE_ID_Op6AM1},    {SNES_OPCODE_ID_Op6BE1},    {SNES_OPCODE_ID_Op6C},      {SNES_OPCODE_ID_Op6DM1},
    {SNES_OPCODE_ID_Op6EM1},    {SNES_OPCODE_ID_Op6FM1},    {SNES_OPCODE_ID_Op70},      {SNES_OPCODE_ID_Op71M1},    {SNES_OPCODE_ID_Op72M1},
    {SNES_OPCODE_ID_Op73M1},    {SNES_OPCODE_ID_Op74M1},    {SNES_OPCODE_ID_Op75M1},    {SNES_OPCODE_ID_Op76M1},    {SNES_OPCODE_ID_Op77M1},
    {SNES_OPCODE_ID_Op78},      {SNES_OPCODE_ID_Op79M1},    {SNES_OPCODE_ID_Op7AE1},    {SNES_OPCODE_ID_Op7B},      {SNES_OPCODE_ID_Op7C},
    {SNES_OPCODE_ID_Op7DM1},    {SNES_OPCODE_ID_Op7EM1},    {SNES_OPCODE_ID_Op7FM1},    {SNES_OPCODE_ID_Op80},      {SNES_OPCODE_ID_Op81M1},
    {SNES_OPCODE_ID_Op82},      {SNES_OPCODE_ID_Op83M1},    {SNES_OPCODE_ID_Op84X1},    {SNES_OPCODE_ID_Op85M1},    {SNES_OPCODE_ID_Op86X1},
    {SNES_OPCODE_ID_Op87M1},    {SNES_OPCODE_ID_Op88X1},    {SNES_OPCODE_ID_Op89M1},    {SNES_OPCODE_ID_Op8AM1},    {SNES_OPCODE_ID_Op8BE1},
    {SNES_OPCODE_ID_Op8CX1},    {SNES_OPCODE_ID_Op8DM1},    {SNES_OPCODE_ID_Op8EX1},    {SNES_OPCODE_ID_Op8FM1},    {SNES_OPCODE_ID_Op90},
    {SNES_OPCODE_ID_Op91M1},    {SNES_OPCODE_ID_Op92M1},    {SNES_OPCODE_ID_Op93M1},    {SNES_OPCODE_ID_Op94X1},    {SNES_OPCODE_ID_Op95M1},
    {SNES_OPCODE_ID_Op96X1},    {SNES_OPCODE_ID_Op97M1},    {SNES_OPCODE_ID_Op98M1},    {SNES_OPCODE_ID_Op99M1},    {SNES_OPCODE_ID_Op9A},
    {SNES_OPCODE_ID_Op9BX1},    {SNES_OPCODE_ID_Op9CM1},    {SNES_OPCODE_ID_Op9DM1},    {SNES_OPCODE_ID_Op9EM1},    {SNES_OPCODE_ID_Op9FM1},
    {SNES_OPCODE_ID_OpA0X1},    {SNES_OPCODE_ID_OpA1M1},    {SNES_OPCODE_ID_OpA2X1},    {SNES_OPCODE_ID_OpA3M1},    {SNES_OPCODE_ID_OpA4X1},
    {SNES_OPCODE_ID_OpA5M1},    {SNES_OPCODE_ID_OpA6X1},    {SNES_OPCODE_ID_OpA7M1},    {SNES_OPCODE_ID_OpA8X1},    {SNES_OPCODE_ID_OpA9M1},
    {SNES_OPCODE_ID_OpAAX1},    {SNES_OPCODE_ID_OpABE1},    {SNES_OPCODE_ID_OpACX1},    {SNES_OPCODE_ID_OpADM1},    {SNES_OPCODE_ID_OpAEX1},
    {SNES_OPCODE_ID_OpAFM1},    {SNES_OPCODE_ID_OpB0},      {SNES_OPCODE_ID_OpB1M1},    {SNES_OPCODE_ID_OpB2M1},    {SNES_OPCODE_ID_OpB3M1},
    {SNES_OPCODE_ID_OpB4X1},    {SNES_OPCODE_ID_OpB5M1},    {SNES_OPCODE_ID_OpB6X1},    {SNES_OPCODE_ID_OpB7M1},    {SNES_OPCODE_ID_OpB8},
    {SNES_OPCODE_ID_OpB9M1},    {SNES_OPCODE_ID_OpBAX1},    {SNES_OPCODE_ID_OpBBX1},    {SNES_OPCODE_ID_OpBCX1},    {SNES_OPCODE_ID_OpBDM1},
    {SNES_OPCODE_ID_OpBEX1},    {SNES_OPCODE_ID_OpBFM1},    {SNES_OPCODE_ID_OpC0X1},    {SNES_OPCODE_ID_OpC1M1},    {SNES_OPCODE_ID_OpC2},
    {SNES_OPCODE_ID_OpC3M1},    {SNES_OPCODE_ID_OpC4X1},    {SNES_OPCODE_ID_OpC5M1},    {SNES_OPCODE_ID_OpC6M1},    {SNES_OPCODE_ID_OpC7M1},
    {SNES_OPCODE_ID_OpC8X1},    {SNES_OPCODE_ID_OpC9M1},    {SNES_OPCODE_ID_OpCAX1},    {SNES_OPCODE_ID_OpCB},      {SNES_OPCODE_ID_OpCCX1},
    {SNES_OPCODE_ID_OpCDM1},    {SNES_OPCODE_ID_OpCEM1},    {SNES_OPCODE_ID_OpCFM1},    {SNES_OPCODE_ID_OpD0},      {SNES_OPCODE_ID_OpD1M1},
    {SNES_OPCODE_ID_OpD2M1},    {SNES_OPCODE_ID_OpD3M1},    {SNES_OPCODE_ID_OpD4E1},    {SNES_OPCODE_ID_OpD5M1},    {SNES_OPCODE_ID_OpD6M1},
    {SNES_OPCODE_ID_OpD7M1},    {SNES_OPCODE_ID_OpD8},      {SNES_OPCODE_ID_OpD9M1},    {SNES_OPCODE_ID_OpDAE1},    {SNES_OPCODE_ID_OpDB},
    {SNES_OPCODE_ID_OpDC},      {SNES_OPCODE_ID_OpDDM1},    {SNES_OPCODE_ID_OpDEM1},    {SNES_OPCODE_ID_OpDFM1},    {SNES_OPCODE_ID_OpE0X1},
    {SNES_OPCODE_ID_OpE1M1},    {SNES_OPCODE_ID_OpE2},      {SNES_OPCODE_ID_OpE3M1},    {SNES_OPCODE_ID_OpE4X1},    {SNES_OPCODE_ID_OpE5M1},
    {SNES_OPCODE_ID_OpE6M1},    {SNES_OPCODE_ID_OpE7M1},    {SNES_OPCODE_ID_OpE8X1},    {SNES_OPCODE_ID_OpE9M1},    {SNES_OPCODE_ID_OpEA},
    {SNES_OPCODE_ID_OpEB},      {SNES_OPCODE_ID_OpECX1},    {SNES_OPCODE_ID_OpEDM1},    {SNES_OPCODE_ID_OpEEM1},    {SNES_OPCODE_ID_OpEFM1},
    {SNES_OPCODE_ID_OpF0},      {SNES_OPCODE_ID_OpF1M1},    {SNES_OPCODE_ID_OpF2M1},    {SNES_OPCODE_ID_OpF3M1},    {SNES_OPCODE_ID_OpF4E1},
    {SNES_OPCODE_ID_OpF5M1},    {SNES_OPCODE_ID_OpF6M1},    {SNES_OPCODE_ID_OpF7M1},    {SNES_OPCODE_ID_OpF8},      {SNES_OPCODE_ID_OpF9M1},
    {SNES_OPCODE_ID_OpFAE1},    {SNES_OPCODE_ID_OpFB},      {SNES_OPCODE_ID_OpFCE1},    {SNES_OPCODE_ID_OpFDM1},    {SNES_OPCODE_ID_OpFEM1},
    {SNES_OPCODE_ID_OpFFM1}
};

const SOpcodes S9xOpcodesM1X0[256] =
{
   {SNES_OPCODE_ID_Op00},  {SNES_OPCODE_ID_Op01M1},    {SNES_OPCODE_ID_Op02},      {SNES_OPCODE_ID_Op03M1},    {SNES_OPCODE_ID_Op04M1},
   {SNES_OPCODE_ID_Op05M1},    {SNES_OPCODE_ID_Op06M1},    {SNES_OPCODE_ID_Op07M1},    {SNES_OPCODE_ID_Op08},      {SNES_OPCODE_ID_Op09M1},
   {SNES_OPCODE_ID_Op0AM1},    {SNES_OPCODE_ID_Op0B},      {SNES_OPCODE_ID_Op0CM1},    {SNES_OPCODE_ID_Op0DM1},    {SNES_OPCODE_ID_Op0EM1},
   {SNES_OPCODE_ID_Op0FM1},    {SNES_OPCODE_ID_Op10},      {SNES_OPCODE_ID_Op11M1},    {SNES_OPCODE_ID_Op12M1},    {SNES_OPCODE_ID_Op13M1},
   {SNES_OPCODE_ID_Op14M1},    {SNES_OPCODE_ID_Op15M1},    {SNES_OPCODE_ID_Op16M1},    {SNES_OPCODE_ID_Op17M1},    {SNES_OPCODE_ID_Op18},
   {SNES_OPCODE_ID_Op19M1},    {SNES_OPCODE_ID_Op1AM1},    {SNES_OPCODE_ID_Op1B},      {SNES_OPCODE_ID_Op1CM1},    {SNES_OPCODE_ID_Op1DM1},
   {SNES_OPCODE_ID_Op1EM1},    {SNES_OPCODE_ID_Op1FM1},    {SNES_OPCODE_ID_Op20},      {SNES_OPCODE_ID_Op21M1},    {SNES_OPCODE_ID_Op22},
   {SNES_OPCODE_ID_Op23M1},    {SNES_OPCODE_ID_Op24M1},    {SNES_OPCODE_ID_Op25M1},    {SNES_OPCODE_ID_Op26M1},    {SNES_OPCODE_ID_Op27M1},
   {SNES_OPCODE_ID_Op28},      {SNES_OPCODE_ID_Op29M1},    {SNES_OPCODE_ID_Op2AM1},    {SNES_OPCODE_ID_Op2B},      {SNES_OPCODE_ID_Op2CM1},
   {SNES_OPCODE_ID_Op2DM1},    {SNES_OPCODE_ID_Op2EM1},    {SNES_OPCODE_ID_Op2FM1},    {SNES_OPCODE_ID_Op30},      {SNES_OPCODE_ID_Op31M1},
   {SNES_OPCODE_ID_Op32M1},    {SNES_OPCODE_ID_Op33M1},    {SNES_OPCODE_ID_Op34M1},    {SNES_OPCODE_ID_Op35M1},    {SNES_OPCODE_ID_Op36M1},
   {SNES_OPCODE_ID_Op37M1},    {SNES_OPCODE_ID_Op38},      {SNES_OPCODE_ID_Op39M1},    {SNES_OPCODE_ID_Op3AM1},    {SNES_OPCODE_ID_Op3B},
   {SNES_OPCODE_ID_Op3CM1},    {SNES_OPCODE_ID_Op3DM1},    {SNES_OPCODE_ID_Op3EM1},    {SNES_OPCODE_ID_Op3FM1},    {SNES_OPCODE_ID_Op40},
   {SNES_OPCODE_ID_Op41M1},    {SNES_OPCODE_ID_Op42},      {SNES_OPCODE_ID_Op43M1},    {SNES_OPCODE_ID_Op44X0},    {SNES_OPCODE_ID_Op45M1},
   {SNES_OPCODE_ID_Op46M1},    {SNES_OPCODE_ID_Op47M1},    {SNES_OPCODE_ID_Op48M1},    {SNES_OPCODE_ID_Op49M1},    {SNES_OPCODE_ID_Op4AM1},
   {SNES_OPCODE_ID_Op4B},      {SNES_OPCODE_ID_Op4C},      {SNES_OPCODE_ID_Op4DM1},    {SNES_OPCODE_ID_Op4EM1},    {SNES_OPCODE_ID_Op4FM1},
   {SNES_OPCODE_ID_Op50},      {SNES_OPCODE_ID_Op51M1},    {SNES_OPCODE_ID_Op52M1},    {SNES_OPCODE_ID_Op53M1},    {SNES_OPCODE_ID_Op54X0},
   {SNES_OPCODE_ID_Op55M1},    {SNES_OPCODE_ID_Op56M1},    {SNES_OPCODE_ID_Op57M1},    {SNES_OPCODE_ID_Op58},      {SNES_OPCODE_ID_Op59M1},
   {SNES_OPCODE_ID_Op5AX0},    {SNES_OPCODE_ID_Op5B},      {SNES_OPCODE_ID_Op5C},      {SNES_OPCODE_ID_Op5DM1},    {SNES_OPCODE_ID_Op5EM1},
   {SNES_OPCODE_ID_Op5FM1},    {SNES_OPCODE_ID_Op60},      {SNES_OPCODE_ID_Op61M1},    {SNES_OPCODE_ID_Op62},      {SNES_OPCODE_ID_Op63M1},
   {SNES_OPCODE_ID_Op64M1},    {SNES_OPCODE_ID_Op65M1},    {SNES_OPCODE_ID_Op66M1},    {SNES_OPCODE_ID_Op67M1},    {SNES_OPCODE_ID_Op68M1},
   {SNES_OPCODE_ID_Op69M1},    {SNES_OPCODE_ID_Op6AM1},    {SNES_OPCODE_ID_Op6B},      {SNES_OPCODE_ID_Op6C},      {SNES_OPCODE_ID_Op6DM1},
   {SNES_OPCODE_ID_Op6EM1},    {SNES_OPCODE_ID_Op6FM1},    {SNES_OPCODE_ID_Op70},      {SNES_OPCODE_ID_Op71M1},    {SNES_OPCODE_ID_Op72M1},
   {SNES_OPCODE_ID_Op73M1},    {SNES_OPCODE_ID_Op74M1},    {SNES_OPCODE_ID_Op75M1},    {SNES_OPCODE_ID_Op76M1},    {SNES_OPCODE_ID_Op77M1},
   {SNES_OPCODE_ID_Op78},      {SNES_OPCODE_ID_Op79M1},    {SNES_OPCODE_ID_Op7AX0},    {SNES_OPCODE_ID_Op7B},      {SNES_OPCODE_ID_Op7C},
   {SNES_OPCODE_ID_Op7DM1},    {SNES_OPCODE_ID_Op7EM1},    {SNES_OPCODE_ID_Op7FM1},    {SNES_OPCODE_ID_Op80},      {SNES_OPCODE_ID_Op81M1},
   {SNES_OPCODE_ID_Op82},      {SNES_OPCODE_ID_Op83M1},    {SNES_OPCODE_ID_Op84X0},    {SNES_OPCODE_ID_Op85M1},    {SNES_OPCODE_ID_Op86X0},
   {SNES_OPCODE_ID_Op87M1},    {SNES_OPCODE_ID_Op88X0},    {SNES_OPCODE_ID_Op89M1},    {SNES_OPCODE_ID_Op8AM1},    {SNES_OPCODE_ID_Op8B},
   {SNES_OPCODE_ID_Op8CX0},    {SNES_OPCODE_ID_Op8DM1},    {SNES_OPCODE_ID_Op8EX0},    {SNES_OPCODE_ID_Op8FM1},    {SNES_OPCODE_ID_Op90},
   {SNES_OPCODE_ID_Op91M1},    {SNES_OPCODE_ID_Op92M1},    {SNES_OPCODE_ID_Op93M1},    {SNES_OPCODE_ID_Op94X0},    {SNES_OPCODE_ID_Op95M1},
   {SNES_OPCODE_ID_Op96X0},    {SNES_OPCODE_ID_Op97M1},    {SNES_OPCODE_ID_Op98M1},    {SNES_OPCODE_ID_Op99M1},    {SNES_OPCODE_ID_Op9A},
   {SNES_OPCODE_ID_Op9BX0},    {SNES_OPCODE_ID_Op9CM1},    {SNES_OPCODE_ID_Op9DM1},    {SNES_OPCODE_ID_Op9EM1},    {SNES_OPCODE_ID_Op9FM1},
   {SNES_OPCODE_ID_OpA0X0},    {SNES_OPCODE_ID_OpA1M1},    {SNES_OPCODE_ID_OpA2X0},    {SNES_OPCODE_ID_OpA3M1},    {SNES_OPCODE_ID_OpA4X0},
   {SNES_OPCODE_ID_OpA5M1},    {SNES_OPCODE_ID_OpA6X0},    {SNES_OPCODE_ID_OpA7M1},    {SNES_OPCODE_ID_OpA8X0},    {SNES_OPCODE_ID_OpA9M1},
   {SNES_OPCODE_ID_OpAAX0},    {SNES_OPCODE_ID_OpAB},      {SNES_OPCODE_ID_OpACX0},    {SNES_OPCODE_ID_OpADM1},    {SNES_OPCODE_ID_OpAEX0},
   {SNES_OPCODE_ID_OpAFM1},    {SNES_OPCODE_ID_OpB0},      {SNES_OPCODE_ID_OpB1M1},    {SNES_OPCODE_ID_OpB2M1},    {SNES_OPCODE_ID_OpB3M1},
   {SNES_OPCODE_ID_OpB4X0},    {SNES_OPCODE_ID_OpB5M1},    {SNES_OPCODE_ID_OpB6X0},    {SNES_OPCODE_ID_OpB7M1},    {SNES_OPCODE_ID_OpB8},
   {SNES_OPCODE_ID_OpB9M1},    {SNES_OPCODE_ID_OpBAX0},    {SNES_OPCODE_ID_OpBBX0},    {SNES_OPCODE_ID_OpBCX0},    {SNES_OPCODE_ID_OpBDM1},
   {SNES_OPCODE_ID_OpBEX0},    {SNES_OPCODE_ID_OpBFM1},    {SNES_OPCODE_ID_OpC0X0},    {SNES_OPCODE_ID_OpC1M1},    {SNES_OPCODE_ID_OpC2},
   {SNES_OPCODE_ID_OpC3M1},    {SNES_OPCODE_ID_OpC4X0},    {SNES_OPCODE_ID_OpC5M1},    {SNES_OPCODE_ID_OpC6M1},    {SNES_OPCODE_ID_OpC7M1},
   {SNES_OPCODE_ID_OpC8X0},    {SNES_OPCODE_ID_OpC9M1},    {SNES_OPCODE_ID_OpCAX0},    {SNES_OPCODE_ID_OpCB},      {SNES_OPCODE_ID_OpCCX0},
   {SNES_OPCODE_ID_OpCDM1},    {SNES_OPCODE_ID_OpCEM1},    {SNES_OPCODE_ID_OpCFM1},    {SNES_OPCODE_ID_OpD0},      {SNES_OPCODE_ID_OpD1M1},
   {SNES_OPCODE_ID_OpD2M1},    {SNES_OPCODE_ID_OpD3M1},    {SNES_OPCODE_ID_OpD4},      {SNES_OPCODE_ID_OpD5M1},    {SNES_OPCODE_ID_OpD6M1},
   {SNES_OPCODE_ID_OpD7M1},    {SNES_OPCODE_ID_OpD8},      {SNES_OPCODE_ID_OpD9M1},    {SNES_OPCODE_ID_OpDAX0},    {SNES_OPCODE_ID_OpDB},
   {SNES_OPCODE_ID_OpDC},      {SNES_OPCODE_ID_OpDDM1},    {SNES_OPCODE_ID_OpDEM1},    {SNES_OPCODE_ID_OpDFM1},    {SNES_OPCODE_ID_OpE0X0},
   {SNES_OPCODE_ID_OpE1M1},    {SNES_OPCODE_ID_OpE2},      {SNES_OPCODE_ID_OpE3M1},    {SNES_OPCODE_ID_OpE4X0},    {SNES_OPCODE_ID_OpE5M1},
   {SNES_OPCODE_ID_OpE6M1},    {SNES_OPCODE_ID_OpE7M1},    {SNES_OPCODE_ID_OpE8X0},    {SNES_OPCODE_ID_OpE9M1},    {SNES_OPCODE_ID_OpEA},
   {SNES_OPCODE_ID_OpEB},      {SNES_OPCODE_ID_OpECX0},    {SNES_OPCODE_ID_OpEDM1},    {SNES_OPCODE_ID_OpEEM1},    {SNES_OPCODE_ID_OpEFM1},
   {SNES_OPCODE_ID_OpF0},      {SNES_OPCODE_ID_OpF1M1},    {SNES_OPCODE_ID_OpF2M1},    {SNES_OPCODE_ID_OpF3M1},    {SNES_OPCODE_ID_OpF4},
   {SNES_OPCODE_ID_OpF5M1},    {SNES_OPCODE_ID_OpF6M1},    {SNES_OPCODE_ID_OpF7M1},    {SNES_OPCODE_ID_OpF8},      {SNES_OPCODE_ID_OpF9M1},
   {SNES_OPCODE_ID_OpFAX0},    {SNES_OPCODE_ID_OpFB},      {SNES_OPCODE_ID_OpFC},      {SNES_OPCODE_ID_OpFDM1},    {SNES_OPCODE_ID_OpFEM1},
   {SNES_OPCODE_ID_OpFFM1}
};

const SOpcodes S9xOpcodesM0X0[256] =
{
   {SNES_OPCODE_ID_Op00},  {SNES_OPCODE_ID_Op01M0},    {SNES_OPCODE_ID_Op02},      {SNES_OPCODE_ID_Op03M0},    {SNES_OPCODE_ID_Op04M0},
   {SNES_OPCODE_ID_Op05M0},    {SNES_OPCODE_ID_Op06M0},    {SNES_OPCODE_ID_Op07M0},    {SNES_OPCODE_ID_Op08},      {SNES_OPCODE_ID_Op09M0},
   {SNES_OPCODE_ID_Op0AM0},    {SNES_OPCODE_ID_Op0B},      {SNES_OPCODE_ID_Op0CM0},    {SNES_OPCODE_ID_Op0DM0},    {SNES_OPCODE_ID_Op0EM0},
   {SNES_OPCODE_ID_Op0FM0},    {SNES_OPCODE_ID_Op10},      {SNES_OPCODE_ID_Op11M0},    {SNES_OPCODE_ID_Op12M0},    {SNES_OPCODE_ID_Op13M0},
   {SNES_OPCODE_ID_Op14M0},    {SNES_OPCODE_ID_Op15M0},    {SNES_OPCODE_ID_Op16M0},    {SNES_OPCODE_ID_Op17M0},    {SNES_OPCODE_ID_Op18},
   {SNES_OPCODE_ID_Op19M0},    {SNES_OPCODE_ID_Op1AM0},    {SNES_OPCODE_ID_Op1B},      {SNES_OPCODE_ID_Op1CM0},    {SNES_OPCODE_ID_Op1DM0},
   {SNES_OPCODE_ID_Op1EM0},    {SNES_OPCODE_ID_Op1FM0},    {SNES_OPCODE_ID_Op20},      {SNES_OPCODE_ID_Op21M0},    {SNES_OPCODE_ID_Op22},
   {SNES_OPCODE_ID_Op23M0},    {SNES_OPCODE_ID_Op24M0},    {SNES_OPCODE_ID_Op25M0},    {SNES_OPCODE_ID_Op26M0},    {SNES_OPCODE_ID_Op27M0},
   {SNES_OPCODE_ID_Op28},      {SNES_OPCODE_ID_Op29M0},    {SNES_OPCODE_ID_Op2AM0},    {SNES_OPCODE_ID_Op2B},      {SNES_OPCODE_ID_Op2CM0},
   {SNES_OPCODE_ID_Op2DM0},    {SNES_OPCODE_ID_Op2EM0},    {SNES_OPCODE_ID_Op2FM0},    {SNES_OPCODE_ID_Op30},      {SNES_OPCODE_ID_Op31M0},
   {SNES_OPCODE_ID_Op32M0},    {SNES_OPCODE_ID_Op33M0},    {SNES_OPCODE_ID_Op34M0},    {SNES_OPCODE_ID_Op35M0},    {SNES_OPCODE_ID_Op36M0},
   {SNES_OPCODE_ID_Op37M0},    {SNES_OPCODE_ID_Op38},      {SNES_OPCODE_ID_Op39M0},    {SNES_OPCODE_ID_Op3AM0},    {SNES_OPCODE_ID_Op3B},
   {SNES_OPCODE_ID_Op3CM0},    {SNES_OPCODE_ID_Op3DM0},    {SNES_OPCODE_ID_Op3EM0},    {SNES_OPCODE_ID_Op3FM0},    {SNES_OPCODE_ID_Op40},
   {SNES_OPCODE_ID_Op41M0},    {SNES_OPCODE_ID_Op42},      {SNES_OPCODE_ID_Op43M0},    {SNES_OPCODE_ID_Op44X0},    {SNES_OPCODE_ID_Op45M0},
   {SNES_OPCODE_ID_Op46M0},    {SNES_OPCODE_ID_Op47M0},    {SNES_OPCODE_ID_Op48M0},    {SNES_OPCODE_ID_Op49M0},    {SNES_OPCODE_ID_Op4AM0},
   {SNES_OPCODE_ID_Op4B},      {SNES_OPCODE_ID_Op4C},      {SNES_OPCODE_ID_Op4DM0},    {SNES_OPCODE_ID_Op4EM0},    {SNES_OPCODE_ID_Op4FM0},
   {SNES_OPCODE_ID_Op50},      {SNES_OPCODE_ID_Op51M0},    {SNES_OPCODE_ID_Op52M0},    {SNES_OPCODE_ID_Op53M0},    {SNES_OPCODE_ID_Op54X0},
   {SNES_OPCODE_ID_Op55M0},    {SNES_OPCODE_ID_Op56M0},    {SNES_OPCODE_ID_Op57M0},    {SNES_OPCODE_ID_Op58},      {SNES_OPCODE_ID_Op59M0},
   {SNES_OPCODE_ID_Op5AX0},    {SNES_OPCODE_ID_Op5B},      {SNES_OPCODE_ID_Op5C},      {SNES_OPCODE_ID_Op5DM0},    {SNES_OPCODE_ID_Op5EM0},
   {SNES_OPCODE_ID_Op5FM0},    {SNES_OPCODE_ID_Op60},      {SNES_OPCODE_ID_Op61M0},    {SNES_OPCODE_ID_Op62},      {SNES_OPCODE_ID_Op63M0},
   {SNES_OPCODE_ID_Op64M0},    {SNES_OPCODE_ID_Op65M0},    {SNES_OPCODE_ID_Op66M0},    {SNES_OPCODE_ID_Op67M0},    {SNES_OPCODE_ID_Op68M0},
   {SNES_OPCODE_ID_Op69M0},    {SNES_OPCODE_ID_Op6AM0},    {SNES_OPCODE_ID_Op6B},      {SNES_OPCODE_ID_Op6C},      {SNES_OPCODE_ID_Op6DM0},
   {SNES_OPCODE_ID_Op6EM0},    {SNES_OPCODE_ID_Op6FM0},    {SNES_OPCODE_ID_Op70},      {SNES_OPCODE_ID_Op71M0},    {SNES_OPCODE_ID_Op72M0},
   {SNES_OPCODE_ID_Op73M0},    {SNES_OPCODE_ID_Op74M0},    {SNES_OPCODE_ID_Op75M0},    {SNES_OPCODE_ID_Op76M0},    {SNES_OPCODE_ID_Op77M0},
   {SNES_OPCODE_ID_Op78},      {SNES_OPCODE_ID_Op79M0},    {SNES_OPCODE_ID_Op7AX0},    {SNES_OPCODE_ID_Op7B},      {SNES_OPCODE_ID_Op7C},
   {SNES_OPCODE_ID_Op7DM0},    {SNES_OPCODE_ID_Op7EM0},    {SNES_OPCODE_ID_Op7FM0},    {SNES_OPCODE_ID_Op80},      {SNES_OPCODE_ID_Op81M0},
   {SNES_OPCODE_ID_Op82},      {SNES_OPCODE_ID_Op83M0},    {SNES_OPCODE_ID_Op84X0},    {SNES_OPCODE_ID_Op85M0},    {SNES_OPCODE_ID_Op86X0},
   {SNES_OPCODE_ID_Op87M0},    {SNES_OPCODE_ID_Op88X0},    {SNES_OPCODE_ID_Op89M0},    {SNES_OPCODE_ID_Op8AM0},    {SNES_OPCODE_ID_Op8B},
   {SNES_OPCODE_ID_Op8CX0},    {SNES_OPCODE_ID_Op8DM0},    {SNES_OPCODE_ID_Op8EX0},    {SNES_OPCODE_ID_Op8FM0},    {SNES_OPCODE_ID_Op90},
   {SNES_OPCODE_ID_Op91M0},    {SNES_OPCODE_ID_Op92M0},    {SNES_OPCODE_ID_Op93M0},    {SNES_OPCODE_ID_Op94X0},    {SNES_OPCODE_ID_Op95M0},
   {SNES_OPCODE_ID_Op96X0},    {SNES_OPCODE_ID_Op97M0},    {SNES_OPCODE_ID_Op98M0},    {SNES_OPCODE_ID_Op99M0},    {SNES_OPCODE_ID_Op9A},
   {SNES_OPCODE_ID_Op9BX0},    {SNES_OPCODE_ID_Op9CM0},    {SNES_OPCODE_ID_Op9DM0},    {SNES_OPCODE_ID_Op9EM0},    {SNES_OPCODE_ID_Op9FM0},
   {SNES_OPCODE_ID_OpA0X0},    {SNES_OPCODE_ID_OpA1M0},    {SNES_OPCODE_ID_OpA2X0},    {SNES_OPCODE_ID_OpA3M0},    {SNES_OPCODE_ID_OpA4X0},
   {SNES_OPCODE_ID_OpA5M0},    {SNES_OPCODE_ID_OpA6X0},    {SNES_OPCODE_ID_OpA7M0},    {SNES_OPCODE_ID_OpA8X0},    {SNES_OPCODE_ID_OpA9M0},
   {SNES_OPCODE_ID_OpAAX0},    {SNES_OPCODE_ID_OpAB},      {SNES_OPCODE_ID_OpACX0},    {SNES_OPCODE_ID_OpADM0},    {SNES_OPCODE_ID_OpAEX0},
   {SNES_OPCODE_ID_OpAFM0},    {SNES_OPCODE_ID_OpB0},      {SNES_OPCODE_ID_OpB1M0},    {SNES_OPCODE_ID_OpB2M0},    {SNES_OPCODE_ID_OpB3M0},
   {SNES_OPCODE_ID_OpB4X0},    {SNES_OPCODE_ID_OpB5M0},    {SNES_OPCODE_ID_OpB6X0},    {SNES_OPCODE_ID_OpB7M0},    {SNES_OPCODE_ID_OpB8},
   {SNES_OPCODE_ID_OpB9M0},    {SNES_OPCODE_ID_OpBAX0},    {SNES_OPCODE_ID_OpBBX0},    {SNES_OPCODE_ID_OpBCX0},    {SNES_OPCODE_ID_OpBDM0},
   {SNES_OPCODE_ID_OpBEX0},    {SNES_OPCODE_ID_OpBFM0},    {SNES_OPCODE_ID_OpC0X0},    {SNES_OPCODE_ID_OpC1M0},    {SNES_OPCODE_ID_OpC2},
   {SNES_OPCODE_ID_OpC3M0},    {SNES_OPCODE_ID_OpC4X0},    {SNES_OPCODE_ID_OpC5M0},    {SNES_OPCODE_ID_OpC6M0},    {SNES_OPCODE_ID_OpC7M0},
   {SNES_OPCODE_ID_OpC8X0},    {SNES_OPCODE_ID_OpC9M0},    {SNES_OPCODE_ID_OpCAX0},    {SNES_OPCODE_ID_OpCB},      {SNES_OPCODE_ID_OpCCX0},
   {SNES_OPCODE_ID_OpCDM0},    {SNES_OPCODE_ID_OpCEM0},    {SNES_OPCODE_ID_OpCFM0},    {SNES_OPCODE_ID_OpD0},      {SNES_OPCODE_ID_OpD1M0},
   {SNES_OPCODE_ID_OpD2M0},    {SNES_OPCODE_ID_OpD3M0},    {SNES_OPCODE_ID_OpD4},      {SNES_OPCODE_ID_OpD5M0},    {SNES_OPCODE_ID_OpD6M0},
   {SNES_OPCODE_ID_OpD7M0},    {SNES_OPCODE_ID_OpD8},      {SNES_OPCODE_ID_OpD9M0},    {SNES_OPCODE_ID_OpDAX0},    {SNES_OPCODE_ID_OpDB},
   {SNES_OPCODE_ID_OpDC},      {SNES_OPCODE_ID_OpDDM0},    {SNES_OPCODE_ID_OpDEM0},    {SNES_OPCODE_ID_OpDFM0},    {SNES_OPCODE_ID_OpE0X0},
   {SNES_OPCODE_ID_OpE1M0},    {SNES_OPCODE_ID_OpE2},      {SNES_OPCODE_ID_OpE3M0},    {SNES_OPCODE_ID_OpE4X0},    {SNES_OPCODE_ID_OpE5M0},
   {SNES_OPCODE_ID_OpE6M0},    {SNES_OPCODE_ID_OpE7M0},    {SNES_OPCODE_ID_OpE8X0},    {SNES_OPCODE_ID_OpE9M0},    {SNES_OPCODE_ID_OpEA},
   {SNES_OPCODE_ID_OpEB},      {SNES_OPCODE_ID_OpECX0},    {SNES_OPCODE_ID_OpEDM0},    {SNES_OPCODE_ID_OpEEM0},    {SNES_OPCODE_ID_OpEFM0},
   {SNES_OPCODE_ID_OpF0},      {SNES_OPCODE_ID_OpF1M0},    {SNES_OPCODE_ID_OpF2M0},    {SNES_OPCODE_ID_OpF3M0},    {SNES_OPCODE_ID_OpF4},
   {SNES_OPCODE_ID_OpF5M0},    {SNES_OPCODE_ID_OpF6M0},    {SNES_OPCODE_ID_OpF7M0},    {SNES_OPCODE_ID_OpF8},      {SNES_OPCODE_ID_OpF9M0},
   {SNES_OPCODE_ID_OpFAX0},    {SNES_OPCODE_ID_OpFB},      {SNES_OPCODE_ID_OpFC},      {SNES_OPCODE_ID_OpFDM0},    {SNES_OPCODE_ID_OpFEM0},
   {SNES_OPCODE_ID_OpFFM0}
};

const SOpcodes S9xOpcodesM0X1[256] =
{
   {SNES_OPCODE_ID_Op00},  {SNES_OPCODE_ID_Op01M0},    {SNES_OPCODE_ID_Op02},      {SNES_OPCODE_ID_Op03M0},    {SNES_OPCODE_ID_Op04M0},
   {SNES_OPCODE_ID_Op05M0},    {SNES_OPCODE_ID_Op06M0},    {SNES_OPCODE_ID_Op07M0},    {SNES_OPCODE_ID_Op08},      {SNES_OPCODE_ID_Op09M0},
   {SNES_OPCODE_ID_Op0AM0},    {SNES_OPCODE_ID_Op0B},      {SNES_OPCODE_ID_Op0CM0},    {SNES_OPCODE_ID_Op0DM0},    {SNES_OPCODE_ID_Op0EM0},
   {SNES_OPCODE_ID_Op0FM0},    {SNES_OPCODE_ID_Op10},      {SNES_OPCODE_ID_Op11M0},    {SNES_OPCODE_ID_Op12M0},    {SNES_OPCODE_ID_Op13M0},
   {SNES_OPCODE_ID_Op14M0},    {SNES_OPCODE_ID_Op15M0},    {SNES_OPCODE_ID_Op16M0},    {SNES_OPCODE_ID_Op17M0},    {SNES_OPCODE_ID_Op18},
   {SNES_OPCODE_ID_Op19M0},    {SNES_OPCODE_ID_Op1AM0},    {SNES_OPCODE_ID_Op1B},      {SNES_OPCODE_ID_Op1CM0},    {SNES_OPCODE_ID_Op1DM0},
   {SNES_OPCODE_ID_Op1EM0},    {SNES_OPCODE_ID_Op1FM0},    {SNES_OPCODE_ID_Op20},      {SNES_OPCODE_ID_Op21M0},    {SNES_OPCODE_ID_Op22},
   {SNES_OPCODE_ID_Op23M0},    {SNES_OPCODE_ID_Op24M0},    {SNES_OPCODE_ID_Op25M0},    {SNES_OPCODE_ID_Op26M0},    {SNES_OPCODE_ID_Op27M0},
   {SNES_OPCODE_ID_Op28},      {SNES_OPCODE_ID_Op29M0},    {SNES_OPCODE_ID_Op2AM0},    {SNES_OPCODE_ID_Op2B},      {SNES_OPCODE_ID_Op2CM0},
   {SNES_OPCODE_ID_Op2DM0},    {SNES_OPCODE_ID_Op2EM0},    {SNES_OPCODE_ID_Op2FM0},    {SNES_OPCODE_ID_Op30},      {SNES_OPCODE_ID_Op31M0},
   {SNES_OPCODE_ID_Op32M0},    {SNES_OPCODE_ID_Op33M0},    {SNES_OPCODE_ID_Op34M0},    {SNES_OPCODE_ID_Op35M0},    {SNES_OPCODE_ID_Op36M0},
   {SNES_OPCODE_ID_Op37M0},    {SNES_OPCODE_ID_Op38},      {SNES_OPCODE_ID_Op39M0},    {SNES_OPCODE_ID_Op3AM0},    {SNES_OPCODE_ID_Op3B},
   {SNES_OPCODE_ID_Op3CM0},    {SNES_OPCODE_ID_Op3DM0},    {SNES_OPCODE_ID_Op3EM0},    {SNES_OPCODE_ID_Op3FM0},    {SNES_OPCODE_ID_Op40},
   {SNES_OPCODE_ID_Op41M0},    {SNES_OPCODE_ID_Op42},      {SNES_OPCODE_ID_Op43M0},    {SNES_OPCODE_ID_Op44X1},    {SNES_OPCODE_ID_Op45M0},
   {SNES_OPCODE_ID_Op46M0},    {SNES_OPCODE_ID_Op47M0},    {SNES_OPCODE_ID_Op48M0},    {SNES_OPCODE_ID_Op49M0},    {SNES_OPCODE_ID_Op4AM0},
   {SNES_OPCODE_ID_Op4B},      {SNES_OPCODE_ID_Op4C},      {SNES_OPCODE_ID_Op4DM0},    {SNES_OPCODE_ID_Op4EM0},    {SNES_OPCODE_ID_Op4FM0},
   {SNES_OPCODE_ID_Op50},      {SNES_OPCODE_ID_Op51M0},    {SNES_OPCODE_ID_Op52M0},    {SNES_OPCODE_ID_Op53M0},    {SNES_OPCODE_ID_Op54X1},
   {SNES_OPCODE_ID_Op55M0},    {SNES_OPCODE_ID_Op56M0},    {SNES_OPCODE_ID_Op57M0},    {SNES_OPCODE_ID_Op58},      {SNES_OPCODE_ID_Op59M0},
   {SNES_OPCODE_ID_Op5AX1},    {SNES_OPCODE_ID_Op5B},      {SNES_OPCODE_ID_Op5C},      {SNES_OPCODE_ID_Op5DM0},    {SNES_OPCODE_ID_Op5EM0},
   {SNES_OPCODE_ID_Op5FM0},    {SNES_OPCODE_ID_Op60},      {SNES_OPCODE_ID_Op61M0},    {SNES_OPCODE_ID_Op62},      {SNES_OPCODE_ID_Op63M0},
   {SNES_OPCODE_ID_Op64M0},    {SNES_OPCODE_ID_Op65M0},    {SNES_OPCODE_ID_Op66M0},    {SNES_OPCODE_ID_Op67M0},    {SNES_OPCODE_ID_Op68M0},
   {SNES_OPCODE_ID_Op69M0},    {SNES_OPCODE_ID_Op6AM0},    {SNES_OPCODE_ID_Op6B},      {SNES_OPCODE_ID_Op6C},      {SNES_OPCODE_ID_Op6DM0},
   {SNES_OPCODE_ID_Op6EM0},    {SNES_OPCODE_ID_Op6FM0},    {SNES_OPCODE_ID_Op70},      {SNES_OPCODE_ID_Op71M0},    {SNES_OPCODE_ID_Op72M0},
   {SNES_OPCODE_ID_Op73M0},    {SNES_OPCODE_ID_Op74M0},    {SNES_OPCODE_ID_Op75M0},    {SNES_OPCODE_ID_Op76M0},    {SNES_OPCODE_ID_Op77M0},
   {SNES_OPCODE_ID_Op78},      {SNES_OPCODE_ID_Op79M0},    {SNES_OPCODE_ID_Op7AX1},    {SNES_OPCODE_ID_Op7B},      {SNES_OPCODE_ID_Op7C},
   {SNES_OPCODE_ID_Op7DM0},    {SNES_OPCODE_ID_Op7EM0},    {SNES_OPCODE_ID_Op7FM0},    {SNES_OPCODE_ID_Op80},      {SNES_OPCODE_ID_Op81M0},
   {SNES_OPCODE_ID_Op82},      {SNES_OPCODE_ID_Op83M0},    {SNES_OPCODE_ID_Op84X1},    {SNES_OPCODE_ID_Op85M0},    {SNES_OPCODE_ID_Op86X1},
   {SNES_OPCODE_ID_Op87M0},    {SNES_OPCODE_ID_Op88X1},    {SNES_OPCODE_ID_Op89M0},    {SNES_OPCODE_ID_Op8AM0},    {SNES_OPCODE_ID_Op8B},
   {SNES_OPCODE_ID_Op8CX1},    {SNES_OPCODE_ID_Op8DM0},    {SNES_OPCODE_ID_Op8EX1},    {SNES_OPCODE_ID_Op8FM0},    {SNES_OPCODE_ID_Op90},
   {SNES_OPCODE_ID_Op91M0},    {SNES_OPCODE_ID_Op92M0},    {SNES_OPCODE_ID_Op93M0},    {SNES_OPCODE_ID_Op94X1},    {SNES_OPCODE_ID_Op95M0},
   {SNES_OPCODE_ID_Op96X1},    {SNES_OPCODE_ID_Op97M0},    {SNES_OPCODE_ID_Op98M0},    {SNES_OPCODE_ID_Op99M0},    {SNES_OPCODE_ID_Op9A},
   {SNES_OPCODE_ID_Op9BX1},    {SNES_OPCODE_ID_Op9CM0},    {SNES_OPCODE_ID_Op9DM0},    {SNES_OPCODE_ID_Op9EM0},    {SNES_OPCODE_ID_Op9FM0},
   {SNES_OPCODE_ID_OpA0X1},    {SNES_OPCODE_ID_OpA1M0},    {SNES_OPCODE_ID_OpA2X1},    {SNES_OPCODE_ID_OpA3M0},    {SNES_OPCODE_ID_OpA4X1},
   {SNES_OPCODE_ID_OpA5M0},    {SNES_OPCODE_ID_OpA6X1},    {SNES_OPCODE_ID_OpA7M0},    {SNES_OPCODE_ID_OpA8X1},    {SNES_OPCODE_ID_OpA9M0},
   {SNES_OPCODE_ID_OpAAX1},    {SNES_OPCODE_ID_OpAB},      {SNES_OPCODE_ID_OpACX1},    {SNES_OPCODE_ID_OpADM0},    {SNES_OPCODE_ID_OpAEX1},
   {SNES_OPCODE_ID_OpAFM0},    {SNES_OPCODE_ID_OpB0},      {SNES_OPCODE_ID_OpB1M0},    {SNES_OPCODE_ID_OpB2M0},    {SNES_OPCODE_ID_OpB3M0},
   {SNES_OPCODE_ID_OpB4X1},    {SNES_OPCODE_ID_OpB5M0},    {SNES_OPCODE_ID_OpB6X1},    {SNES_OPCODE_ID_OpB7M0},    {SNES_OPCODE_ID_OpB8},
   {SNES_OPCODE_ID_OpB9M0},    {SNES_OPCODE_ID_OpBAX1},    {SNES_OPCODE_ID_OpBBX1},    {SNES_OPCODE_ID_OpBCX1},    {SNES_OPCODE_ID_OpBDM0},
   {SNES_OPCODE_ID_OpBEX1},    {SNES_OPCODE_ID_OpBFM0},    {SNES_OPCODE_ID_OpC0X1},    {SNES_OPCODE_ID_OpC1M0},    {SNES_OPCODE_ID_OpC2},
   {SNES_OPCODE_ID_OpC3M0},    {SNES_OPCODE_ID_OpC4X1},    {SNES_OPCODE_ID_OpC5M0},    {SNES_OPCODE_ID_OpC6M0},    {SNES_OPCODE_ID_OpC7M0},
   {SNES_OPCODE_ID_OpC8X1},    {SNES_OPCODE_ID_OpC9M0},    {SNES_OPCODE_ID_OpCAX1},    {SNES_OPCODE_ID_OpCB},      {SNES_OPCODE_ID_OpCCX1},
   {SNES_OPCODE_ID_OpCDM0},    {SNES_OPCODE_ID_OpCEM0},    {SNES_OPCODE_ID_OpCFM0},    {SNES_OPCODE_ID_OpD0},      {SNES_OPCODE_ID_OpD1M0},
   {SNES_OPCODE_ID_OpD2M0},    {SNES_OPCODE_ID_OpD3M0},    {SNES_OPCODE_ID_OpD4},      {SNES_OPCODE_ID_OpD5M0},    {SNES_OPCODE_ID_OpD6M0},
   {SNES_OPCODE_ID_OpD7M0},    {SNES_OPCODE_ID_OpD8},      {SNES_OPCODE_ID_OpD9M0},    {SNES_OPCODE_ID_OpDAX1},    {SNES_OPCODE_ID_OpDB},
   {SNES_OPCODE_ID_OpDC},      {SNES_OPCODE_ID_OpDDM0},    {SNES_OPCODE_ID_OpDEM0},    {SNES_OPCODE_ID_OpDFM0},    {SNES_OPCODE_ID_OpE0X1},
   {SNES_OPCODE_ID_OpE1M0},    {SNES_OPCODE_ID_OpE2},      {SNES_OPCODE_ID_OpE3M0},    {SNES_OPCODE_ID_OpE4X1},    {SNES_OPCODE_ID_OpE5M0},
   {SNES_OPCODE_ID_OpE6M0},    {SNES_OPCODE_ID_OpE7M0},    {SNES_OPCODE_ID_OpE8X1},    {SNES_OPCODE_ID_OpE9M0},    {SNES_OPCODE_ID_OpEA},
   {SNES_OPCODE_ID_OpEB},      {SNES_OPCODE_ID_OpECX1},    {SNES_OPCODE_ID_OpEDM0},    {SNES_OPCODE_ID_OpEEM0},    {SNES_OPCODE_ID_OpEFM0},
   {SNES_OPCODE_ID_OpF0},      {SNES_OPCODE_ID_OpF1M0},    {SNES_OPCODE_ID_OpF2M0},    {SNES_OPCODE_ID_OpF3M0},    {SNES_OPCODE_ID_OpF4},
   {SNES_OPCODE_ID_OpF5M0},    {SNES_OPCODE_ID_OpF6M0},    {SNES_OPCODE_ID_OpF7M0},    {SNES_OPCODE_ID_OpF8},      {SNES_OPCODE_ID_OpF9M0},
   {SNES_OPCODE_ID_OpFAX1},    {SNES_OPCODE_ID_OpFB},      {SNES_OPCODE_ID_OpFC},      {SNES_OPCODE_ID_OpFDM0},    {SNES_OPCODE_ID_OpFEM0},
   {SNES_OPCODE_ID_OpFFM0}
};

/* Keep the dispatch loop in this translation unit so each instruction
 * shares the loop's AOT memory base/bounds and local register allocation. */
#include "cpuexec.c"
