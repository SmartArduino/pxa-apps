#ifndef STORE_BINDING_H
#define STORE_BINDING_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>
/* Only a transport decoder. The Host independently verifies trust/signature,
 * expiry and all fields before assigning an author namespace. */
static inline size_t store_binding_decode(const char *text,uint8_t out[396]) {
    static const char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    if(!text || !out)return 0;
    size_t size=strlen(text),written=0;
    if(!size || size>528 || size%4)return 0;
    for(size_t i=0;i<size;i+=4) {
        unsigned value=0,padding=0;
        for(unsigned j=0;j<4;++j) {
            char c=text[i+j];
            if(c=='='){if(i+4!=size || j<2)return 0;++padding;value<<=6;}
            else {const char *p=strchr(alphabet,c);if(!p || padding)return 0;value=(value<<6)|(unsigned)(p-alphabet);}
        }
        if(padding>2 || (padding==1 && (value&255u)) || (padding==2 && (value&65535u)))return 0;
        for(unsigned j=0;j<3-padding;++j){if(written>=396)return 0;out[written++]=(uint8_t)(value>>(16-j*8));}
    }
    return written>=180?written:0;
}
static inline int store_binding_matches(const uint8_t *binding,size_t size,const char *identity) {
    if(!binding || size<180 || size>396 || !identity || strlen(identity)<66 || identity[64]!=':' ||
       memcmp(binding,"PXPB\1\0\0\0",8))return 0;
    size_t length=binding[112];
    if(!length || length>64 || 116+length>size-64 || strlen(identity+65)!=length || memcmp(binding+116,identity+65,length))return 0;
    for(size_t i=0;i<32;++i) {
        unsigned value=0;
        for(size_t j=0;j<2;++j){char c=identity[2*i+j];int n=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;if(n<0)return 0;value=(value<<4)|(unsigned)n;}
        if(binding[56+i]!=value)return 0;
    }
    return 1;
}

#endif
