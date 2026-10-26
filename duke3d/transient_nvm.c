/* Volatile engine caches/checkpoints; user save slots keep their NVM format. */
#include "transient_nvm.h"
#include "vflash.h"
#include <string.h>

extern void port_error(const char *);
static const duke_transient_region_t *regions;
static unsigned region_count;
static uint32_t address_space;
void duke_transient_init(uint32_t space, const duke_transient_region_t *r, unsigned n) {
  if(!space || (space&(space-1)) || !r || !n)port_error("Invalid transient NVM layout.");
  uint32_t end=0;
  for(unsigned i=0;i<n;++i) {
    if(!r[i].bytes || !r[i].size || r[i].offset<end ||
       r[i].offset>space || r[i].size>space-r[i].offset)
      port_error("Invalid transient NVM range.");
    end=r[i].offset+r[i].size;
  }
  for(unsigned i=0;i<n;++i)memset(r[i].bytes,255,r[i].size);
  regions=r;region_count=n;address_space=space;
}
static uint8_t *window(uint32_t address,uint32_t *count) {
  for(unsigned i=0;i<region_count;++i) {
    const duke_transient_region_t *r=&regions[i];
    if(address<r->offset) {
      if(*count>r->offset-address)*count=r->offset-address;
      return NULL;
    }
    if(address-r->offset<r->size) {
      uint32_t left=r->size-(address-r->offset);
      if(*count>left)*count=left;
      return r->bytes+(address-r->offset);
    }
  }
  return NULL;
}
static uint32_t checked(uint32_t address,uint32_t n) {
  if(!address_space)port_error("Transient NVM is not initialized.");
  address&=address_space-1;
  if(n>address_space-address)port_error("Transient NVM access outside address space.");
  return address;
}
void duke_transient_read(uint32_t address,void *output,uint32_t n) {
  address=checked(address,n);
  if(!n)return;
  if(n && !output)port_error("Invalid transient NVM read.");
  /* Texture reads stay on the original fast ROM path. */
  if(n<=regions[0].offset && address<=regions[0].offset-n) {
    vflash_read(address,output,n);return;
  }
  uint8_t *out=output;
  while(n) {
    uint32_t count=n;uint8_t *p=window(address,&count);
    if(p)memcpy(out,p,count);else vflash_read(address,out,count);
    address+=count;out+=count;n-=count;
  }
}
void duke_transient_write(uint32_t address,const void *input,uint32_t n,int erase) {
  address=checked(address,n);
  if(!n)return;
  if(n && !erase && !input)port_error("Invalid transient NVM write.");
  const uint8_t *in=input;
  while(n) {
    uint32_t count=n;uint8_t *p=window(address,&count);
    if(p) {
      if(erase)memset(p,255,count);
      else for(uint32_t i=0;i<count;++i)p[i]&=in[i];
    } else vflash_write(address,in,count,erase);
    address+=count;if(!erase)in+=count;n-=count;
  }
}
