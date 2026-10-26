/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Posix rand_r function added May 1999 by Wes Peters <wes@softweyr.com>.
 *
 * $Id$
 */


/* SPDX-License-Identifier: MIT. Owner-local portability implementation.
 * AVR rand independent15bit stream. Historical reference BSD notice retained. */
#include "owner_compat.h"

#include "Arduboy2.h"
static uint32_t rand_state=1,rand_calls=0;
static uint8_t owner_fault=0;
int obono_rand(){int32_t x=(int32_t)rand_state;if(!x)x=123459876;int32_t n=16807*(x%127773)-2836*(x/127773);if(n<0)n+=2147483647;rand_state=(uint32_t)n;++rand_calls;return(int)(rand_state&32767);}
static size_t addr(const void*p,size_t n){uintptr_t a=(uintptr_t)p;if(a>1024||n>1024-a){owner_fault=1;return 1024;}return(size_t)a;}
uint8_t eeprom_read_byte(const uint8_t*p){size_t a=addr(p,1);return a<1024?EEPROM.read((int)a):255;}
uint16_t eeprom_read_word(const uint16_t*p){size_t a=addr(p,2);return a<1024?(uint16_t)(EEPROM.read(a)|((uint16_t)EEPROM.read(a+1)<<8)):65535;}
uint32_t eeprom_read_dword(const uint32_t*p){size_t a=addr(p,4);uint32_t v=0;if(a>=1024)return UINT32_MAX;for(int i=0;i<4;i++)v|=(uint32_t)EEPROM.read(a+i)<<(i*8);return v;}
void eeprom_read_block(void*out,const void*p,size_t n){size_t a=addr(p,n);if(a>=1024){memset(out,255,n);return;}for(size_t i=0;i<n;i++)((uint8_t*)out)[i]=EEPROM.read(a+i);}
void eeprom_write_byte(uint8_t*p,uint8_t v){size_t a=addr(p,1);if(a<1024)EEPROM.write(a,v);}
void eeprom_write_word(uint16_t*p,uint16_t v){size_t a=addr(p,2);if(a<1024)for(int i=0;i<2;i++)EEPROM.write(a+i,(uint8_t)(v>>(i*8)));}
void eeprom_write_dword(uint32_t*p,uint32_t v){size_t a=addr(p,4);if(a<1024)for(int i=0;i<4;i++)EEPROM.write(a+i,(uint8_t)(v>>(i*8)));}
void eeprom_write_block(const void*in,void*p,size_t n){size_t a=addr(p,n);if(a<1024)for(size_t i=0;i<n;i++)EEPROM.write(a+i,((const uint8_t*)in)[i]);}
void obono_owner_reset(){rand_state=1;rand_calls=0;owner_fault=0;obono_audio_reset();}
void obono_owner_pack(ObonoWriter&w){w.u32(rand_state);w.u32(rand_calls);w.u8(owner_fault);}
void Arduboy2Core::setRGBled(uint8_t,uint8_t,uint8_t){}
void Arduboy2Core::setRGBled(uint8_t,uint8_t){}
