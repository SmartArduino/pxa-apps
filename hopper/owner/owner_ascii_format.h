/* SPDX-License-Identifier: MIT. Exact bounded original Hopper seconds string. */
#pragma once
#include <stddef.h>
#include <stdint.h>
inline void obono_format_seconds(char *out,size_t capacity,uint32_t frames){
 if(capacity<5){if(capacity)out[0]=0;return;}
 const unsigned seconds=frames/60%60;
 out[0]='\'';out[1]=(char)('0'+seconds/10);out[2]=(char)('0'+seconds%10);
 out[3]='"';out[4]=0;
}
