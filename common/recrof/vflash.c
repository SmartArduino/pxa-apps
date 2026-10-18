/* File-backed flash address space and bounded read cache. GPL-3.0-or-later. */
#include "vflash.h"
#include "port.h"
#include <stdio.h>
#include <string.h>
#define PAGE 8192u
#ifndef PORT_VFLASH_CACHE_PAGES
#define PORT_VFLASH_CACHE_PAGES 16u
#endif
#define PAGES PORT_VFLASH_CACHE_PAGES
static FILE *data_file, *save_file;
static uint32_t data_size, mask, nvm_start, virtual_end;
static uint32_t tags[PAGES], ages[PAGES], clock_tick;
static uint8_t cache[PAGES][PAGE];
static unsigned recent_page;
static vflash_stats_t io_stats;
#ifdef PORT_VFLASH_COOPERATE
static uint32_t read_calls;
#endif
void vflash_stats(vflash_stats_t *out, int reset) {
  if(out) { *out=io_stats; out->cache_bytes=sizeof(cache); }
  if(reset) memset(&io_stats,0,sizeof(io_stats));
}
#define NVM_PAGES 512u
static uint32_t nvm_offsets[NVM_PAGES], nvm_pages, save_length;
static int legacy_nvm;
static void nvm_init(uint32_t bytes) {
  nvm_pages = bytes / PAGE;
  if (!nvm_pages || nvm_pages > NVM_PAGES || bytes % PAGE)
    port_error("Invalid NVM size.");
  memset(nvm_offsets, 0, sizeof(nvm_offsets));
  fseek(save_file,0,SEEK_END);
  long length=ftell(save_file);
  legacy_nvm = length == (long)bytes;
  if (legacy_nvm) return;
  uint32_t header[4];
  memcpy(header,"PXA-NVM2",8);header[2]=bytes;header[3]=PAGE;
  if (!length) {
    if (fseek(save_file,0,SEEK_SET) ||
        fwrite(header,1,sizeof(header),save_file)!=sizeof(header) ||
        fwrite(nvm_offsets,4,nvm_pages,save_file)!=nvm_pages || fflush(save_file))
      port_error("Cannot initialize compact NVM.");
    length=sizeof(header)+nvm_pages*4;
  } else {
    uint32_t actual[4];
    if (length < (long)(sizeof(header)+nvm_pages*4) ||
        fseek(save_file,0,SEEK_SET) || fread(actual,1,sizeof(actual),save_file)!=sizeof(actual) ||
        memcmp(actual,header,sizeof(header)) ||
        fread(nvm_offsets,4,nvm_pages,save_file)!=nvm_pages)
      port_error("Invalid compact NVM header.");
    for (unsigned i=0;i<nvm_pages;++i)
      if (nvm_offsets[i] && (nvm_offsets[i]<sizeof(header)+nvm_pages*4 ||
          nvm_offsets[i]>(uint32_t)length || PAGE>(uint32_t)length-nvm_offsets[i]))
        port_error("Invalid compact NVM page.");
  }
  save_length=(uint32_t)length;
}
static void nvm_read(uint32_t offset, void *dest, uint32_t length) {
  uint8_t *out=dest;
  while (length) {
    unsigned index=offset/PAGE, within=offset%PAGE, count=PAGE-within;
    if(count>length)count=length;
    uint32_t at=legacy_nvm ? offset : nvm_offsets[index]+within;
    if (!legacy_nvm && !nvm_offsets[index]) memset(out,255,count);
    else {
      uint64_t before=port_time_us();
      if(fseek(save_file,at,SEEK_SET) || fread(out,1,count,save_file)!=count)
        port_error("Save data read failed.");
      ++io_stats.nvm_reads; io_stats.nvm_read_us+=port_time_us()-before;
    }
    offset+=count;out+=count;length-=count;
  }
}
static void nvm_write(uint32_t offset, const uint8_t *data, uint32_t length,int erase) {
  uint8_t block[PAGE];
  while(length) {
    unsigned index=offset/PAGE, within=offset%PAGE, count=PAGE-within;
    if(count>length)count=length;
    nvm_read(index*PAGE,block,PAGE);
    for (unsigned i=0;i<count;++i) block[within+i]=erase ? 255 : block[within+i]&data[i];
    uint32_t at=legacy_nvm ? index*PAGE : nvm_offsets[index];
    if (!legacy_nvm && !at) {
      unsigned i=0;while(i<PAGE && block[i]==255)++i;
      if(i==PAGE) {offset+=count;length-=count;if(!erase)data+=count;continue;}
      at=save_length;
    }
    if(fseek(save_file,at,SEEK_SET) || fwrite(block,1,PAGE,save_file)!=PAGE || fflush(save_file))
      port_error("Save write failed.");
    if(!legacy_nvm && !nvm_offsets[index]) {
      // Publish the page only after its complete contents have been written.
      if(fseek(save_file,16+index*4,SEEK_SET) || fwrite(&at,4,1,save_file)!=1 || fflush(save_file))
        port_error("Save index write failed.");
      nvm_offsets[index]=at;save_length+=PAGE;
    }
    offset+=count;length-=count;if(!erase)data+=count;
  }
}
uint32_t vflash_data_size(void) { return data_size; }
void vflash_init(const char *path, uint32_t virtual_size, uint32_t nvm_size) {
  mask = virtual_size - 1;
  virtual_end = virtual_size;
  nvm_start = virtual_size - nvm_size;
  for (unsigned i = 0; i < PAGES; i++)
    tags[i] = ~0u;
  recent_page=0;
  if (data_file) fclose(data_file);
  if (save_file) fclose(save_file);
  data_file = fopen(path, "rb");
  if (!data_file)
    port_error("Missing game data.\nUse the included import tool, then reopen "
               "the game.");
  fseek(data_file, 0, SEEK_END);
  long size = ftell(data_file);
  if (size < 12 || (uint32_t)size > nvm_start - 4)
    port_error("Invalid or oversized game data.");
  data_size = size;
  char save_path[320];
  snprintf(save_path,sizeof(save_path),"%s/state.nvm",port_data_path());
  save_file = fopen(save_path,"r+b");
  if (!save_file) save_file = fopen(save_path,"w+b");
  if (!save_file) port_error("Cannot create game save file.");
  nvm_init(nvm_size);
}
static const uint8_t *page(uint32_t address) {
  uint32_t tag = address / PAGE;
  if(tags[recent_page]==tag) {
    ++io_stats.hits; ages[recent_page]=++clock_tick;
    return cache[recent_page];
  }
  unsigned best = 0;
  for (unsigned i = 0; i < PAGES; i++) {
    if (tags[i] == tag) {
      ++io_stats.hits; recent_page=i;
      ages[i] = ++clock_tick;
      return cache[i];
    }
    if (tags[i] == ~0u || ages[i] < ages[best])
      best = i;
  }
  ++io_stats.misses;
  uint64_t before=port_time_us();
  if (fseek(data_file, tag * PAGE, SEEK_SET))
    port_error("Game data seek failed.");
  memset(cache[best], 255, PAGE);
  size_t n = fread(cache[best], 1, PAGE, data_file);
  if (!n)
    port_error("Game data read failed.");
  tags[best] = tag;
  ages[best] = ++clock_tick;
  recent_page=best; io_stats.read_us+=port_time_us()-before;
  return cache[best];
}
void *vflash_read(uint32_t address, void *dest, uint32_t length) {
#ifdef PORT_VFLASH_COOPERATE
  /* A hot page cache removes the I/O waits that otherwise service input and
   * bound callbacks. Poll at a bounded call interval, not per copied byte. */
  if ((++read_calls & 255u) == 0 &&
      port_cooperate_due(port_time_us(),8000)) port_sleep(0);
#endif
  address &= mask;
  uint8_t *out = dest;
  if (length > virtual_end - address)
    port_error("Game data address overflow.");
  if (address >= nvm_start) {
    nvm_read(address-nvm_start,dest,length);
    return dest;
  }
  if (address < 4) {
    unsigned n = 4 - address;
    if (n > length)
      n = length;
    memset(out, 255, n);
    out += n;
    address += n;
    length -= n;
  }
  uint32_t offset = address - 4;
  if (offset > data_size || length > data_size - offset)
    port_error("Game read outside data archive.");
  while (length) {
    unsigned n = PAGE - offset % PAGE;
    if (n > length)
      n = length;
    memcpy(out, page(offset) + offset % PAGE, n);
    out += n;
    offset += n;
    length -= n;
  }
  return dest;
}
void vflash_write(uint32_t address, const void *data, uint32_t length,
                  int erase) {
  address &= mask;
  if (address < nvm_start || length > virtual_end - address)
    port_error("Save write outside NVM region.");
  nvm_write(address-nvm_start,data,length,erase);
}
