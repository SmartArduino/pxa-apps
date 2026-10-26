/* SPDX-License-Identifier: MIT. Small software Print API for original game text. */
#ifndef ARDUBOY_PXA_PRINT_H
#define ARDUBOY_PXA_PRINT_H
#include "Arduino.h"
class Print {
public:
 virtual size_t write(uint8_t value)=0;
 size_t write(const uint8_t*p,size_t n){size_t total=0;while(n--)total+=write(*p++);return total;}
 size_t write(const char*p){return write((const uint8_t*)p,strlen(p));}
 size_t print(const char*p){return write(p);}
 size_t print(const __FlashStringHelper*p){return print((const char*)p);}
 size_t print(char c){return write((uint8_t)c);}
 size_t print(unsigned char value,int base=DEC){return number(value,base);}
 size_t print(unsigned int value,int base=DEC){return number(value,base);}
 size_t print(unsigned long value,int base=DEC){return number((uint32_t)value,base);}
 size_t print(int value,int base=DEC){return signedNumber(value,base);}
 size_t print(long value,int base=DEC){return signedNumber((int32_t)value,base);}
 size_t println(){return write('\r')+write('\n');}
 template<class T>size_t println(const T& value){return print(value)+println();}
private:
 size_t number(uint32_t value,int base){if(base==0)return write((uint8_t)value);if(base<2)base=10;char buf[33];unsigned n=0;do{unsigned digit=value%(unsigned)base;buf[n++]=(char)(digit<10?'0'+digit:'A'+digit-10);value/=(unsigned)base;}while(value);size_t count=0;while(n)count+=write((uint8_t)buf[--n]);return count;}
 size_t signedNumber(int32_t value,int base){if(base==10&&value<0)return write('-')+number(0u-(uint32_t)value,base);return number((uint32_t)value,base);}
};
#endif
