/* SPDX-License-Identifier: MIT. Allocation-free Arduino string/diagnostic boundary. */
#pragma once
#include <string.h>
#include <stdint.h>
class String {
 const char *value_;
public:
 String(const char* value):value_(value){}
 String(const __FlashStringHelper* value):value_(reinterpret_cast<const char*>(value)){}
 size_t length() const{return strlen(value_);}
 char charAt(unsigned index) const{return index<length()?value_[index]:0;}
 operator const char*() const{return value_;}
};
inline char* itoa(int32_t value,char* out,int base){
 if(base!=10){out[0]=0;return out;}
 uint32_t n=(value<0)?0u-(uint32_t)value:(uint32_t)value;
 char digits[10];unsigned count=0,pos=0;
 do{digits[count++]=(char)('0'+n%10);n/=10;}while(n);
 if(value<0)out[pos++]='-';while(count)out[pos++]=digits[--count];out[pos]=0;return out;
}
struct PortWriter {
 uint8_t* out;size_t capacity,count=0;bool overflow=false;
 PortWriter(uint8_t* p,size_t n):out(p),capacity(n){}
 void u8(uint8_t n){if(out){if(count<capacity)out[count]=n;else overflow=true;}++count;}
 void u16(uint16_t n){u8(n);u8(n>>8);}
 void u32(uint32_t n){u16(n);u16(n>>16);}
 size_t finish(){if(overflow){arduboy_pxa_state.fault=5;return 0;}return count;}
};
#ifdef __wasm__
#include "pxa_port_log.h"
inline void port_log(const char* action,int menu,int type,int title,int level,int lives,int32_t score,int x,int enemies,int wait){
 pxlog_t l;pxlog_init(&l);pxlog_text(&l,"CLASSIC invaders action=");pxlog_text(&l,action);
 pxlog_field(&l,"menu",menu);pxlog_field(&l,"type",type);pxlog_field(&l,"title",title);
 pxlog_field(&l,"level",level);pxlog_field(&l,"lives",lives);pxlog_field(&l,"score",score);
 pxlog_field(&l,"x",x);pxlog_field(&l,"enemies",enemies);pxlog_field(&l,"wait",wait);pxlog_send(&l);
}
#else
#include <stdio.h>
inline void port_log(const char* action,int menu,int type,int title,int level,int lives,int32_t score,int x,int enemies,int wait){
 printf("CLASSIC invaders action=%s menu=%d type=%d title=%d level=%d lives=%d score=%d x=%d enemies=%d wait=%d\n",action,menu,type,title,level,lives,score,x,enemies,wait);
}
#endif
