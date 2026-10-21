/* Two bitplanes to eight palette indices. GPL-2.0-or-later. */
#pragma once
#include <stdint.h>
#include <string.h>
/* A nibble expands to four bytes. This 64-byte table replaces eight per-bit
 * decode iterations without a decoded-tile cache or VRAM invalidation work. */
static inline void gnuboy_decode_row(uint8_t *out,uint8_t low,uint8_t high,int reverse)
{
    static const uint32_t expand[16]={
        0x00000000,0x01000000,0x00010000,0x01010000,
        0x00000100,0x01000100,0x00010100,0x01010100,
        0x00000001,0x01000001,0x00010001,0x01010001,
        0x00000101,0x01000101,0x00010101,0x01010101
    };
#if !defined(IS_BIG_ENDIAN)
    uint32_t first=expand[low>>4]|(expand[high>>4]<<1);
    uint32_t last=expand[low&15]|(expand[high&15]<<1);
    if(reverse){uint32_t saved=first;first=__builtin_bswap32(last);last=__builtin_bswap32(saved);}
    memcpy(out,&first,4);memcpy(out+4,&last,4);
#else
    for(unsigned i=0;i<8;++i){unsigned bit=reverse?i:7-i;out[i]=((low>>bit)&1)|(((high>>bit)&1)<<1);}
#endif
}
