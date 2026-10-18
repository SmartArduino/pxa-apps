/* File-backed flash address space and bounded read cache. GPL-3.0-or-later. */
#include "vflash.h"
#include "port.h"
#include <stdio.h>
#include <errno.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
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
#ifdef PORT_VFLASH_PAGED_NVM
/* Keep the old NVM file as a read-only migration source. Publishing one small
 * page avoids LittleFS copying a large CTZ tail after every random NOR write.
 * Only touched pages acquire overlays; the old file is never rewritten. */
static char page_directory[320];
static uint8_t page_state[NVM_PAGES]; /* absent, present */
static FILE *page_file;
static unsigned page_index;
static uint8_t batch_block[PAGE];
static unsigned batch_index,batch_depth;
static int batch_valid,batch_dirty;
static void page_path(char *path, size_t capacity, unsigned index) {
  if(snprintf(path,capacity,"%s/%03u",page_directory,index)>=(int)capacity)
    port_error("NVM page path too long.");
}
static int read_overlay(unsigned index, unsigned within, void *out, unsigned count) {
  if(!page_state[index]) return 0;
  if(!page_file || page_index!=index) {
    if(page_file) {fclose(page_file);page_file=NULL;}
    char path[352];page_path(path,sizeof(path),index);
    page_file=fopen(path,"rb");
    if(!page_file) port_error("Cannot open NVM page.");
    page_index=index;
    uint32_t header[4],expected[4];
    memcpy(expected,"PXA-NVP1",8);expected[2]=nvm_pages*PAGE;expected[3]=index;
    if(fread(header,1,sizeof(header),page_file)!=sizeof(header) ||
        memcmp(header,expected,sizeof(header)) || fseek(page_file,0,SEEK_END) ||
        ftell(page_file)!=(long)(16+PAGE)) port_error("Invalid NVM page.");
  }
  if(fseek(page_file,16+within,SEEK_SET) || fread(out,1,count,page_file)!=count)
    port_error("NVM page read failed.");
  return 1;
}
static void write_overlay(unsigned index, const uint8_t *block) {
  if(page_file) {fclose(page_file);page_file=NULL;}
  char path[352],pending[352];page_path(path,sizeof(path),index);
  snprintf(pending,sizeof(pending),"%s/pending",page_directory);
  uint32_t header[4];
  memcpy(header,"PXA-NVP1",8);header[2]=nvm_pages*PAGE;header[3]=index;
  FILE *file=fopen(pending,"wb");
  if(!file) port_error("Cannot create NVM page.");
  int failed=fwrite(header,1,sizeof(header),file)!=sizeof(header) ||
      fwrite(block,1,PAGE,file)!=PAGE || fflush(file);
  if(fclose(file)) failed=1;
  if(failed || port_replace(pending,path)) port_error("Cannot publish NVM page.");
  page_state[index]=1;
}
static void flush_batch(void) {
  if(batch_dirty) {write_overlay(batch_index,batch_block);batch_dirty=0;}
}
#endif
void vflash_begin_batch(void) {
#ifdef PORT_VFLASH_PAGED_NVM
  if(batch_depth==UINT32_MAX)port_error("NVM batch overflow.");
  ++batch_depth;
#endif
}
void vflash_end_batch(void) {
#ifdef PORT_VFLASH_PAGED_NVM
  if(!batch_depth)port_error("Unbalanced NVM batch.");
  if(--batch_depth==0) {flush_batch();batch_valid=0;}
#endif
}
static void nvm_init(uint32_t bytes) {
  nvm_pages = bytes / PAGE;
  if (!nvm_pages || nvm_pages > NVM_PAGES || bytes % PAGE)
    port_error("Invalid NVM size.");
  memset(nvm_offsets, 0, sizeof(nvm_offsets));
  fseek(save_file,0,SEEK_END);
  long length=ftell(save_file);
  legacy_nvm = length == (long)bytes;
  if (legacy_nvm) goto ready;
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
ready:
#ifdef PORT_VFLASH_PAGED_NVM
  if(batch_depth || batch_dirty)port_error("Cannot reopen an unfinished NVM save.");
  batch_valid=0;
  if(page_file) {fclose(page_file);page_file=NULL;}
  memset(page_state,0,sizeof(page_state));
  if(snprintf(page_directory,sizeof(page_directory),"%s/nvm-%u",port_data_path(),bytes)
      >=(int)sizeof(page_directory)) port_error("NVM directory path too long.");
  if(mkdir(page_directory,0700)) {
    if(errno!=EEXIST) port_error("Cannot create NVM directory.");
    DIR *dir=opendir(page_directory);
    if(!dir) port_error("Cannot enumerate NVM directory.");
    struct dirent *entry;unsigned count=0;
    for(;;) {
      errno=0;entry=readdir(dir);
      if(!entry) {
        if(errno) port_error("NVM directory read failed.");
        break;
      }
      if(++count>NVM_PAGES+3) port_error("Too many NVM pages.");
      const char *name=entry->d_name;
      if(strlen(name)!=3 || name[0]<'0' || name[0]>'9' ||
          name[1]<'0' || name[1]>'9' || name[2]<'0' || name[2]>'9') continue;
      unsigned index=(name[0]-'0')*100+(name[1]-'0')*10+name[2]-'0';
      if(index>=nvm_pages) port_error("Invalid NVM page index.");
      page_state[index]=1;
    }
    closedir(dir);
  }
  /* The initial header is now committed; no further writes touch this file. */
  if(fclose(save_file)) port_error("Cannot close NVM migration source.");
  char source[320];snprintf(source,sizeof(source),"%s/state.nvm",port_data_path());
  save_file=fopen(source,"rb");
  if(!save_file) port_error("Cannot reopen NVM migration source.");
#else
  ;
#endif
}
static void nvm_read(uint32_t offset, void *dest, uint32_t length) {
  uint8_t *out=dest;
  while (length) {
    unsigned index=offset/PAGE, within=offset%PAGE, count=PAGE-within;
    if(count>length)count=length;
#ifdef PORT_VFLASH_PAGED_NVM
    if(batch_valid && batch_index==index) {
      memcpy(out,batch_block+within,count);
      offset+=count;out+=count;length-=count;continue;
    }
    uint64_t before=port_time_us();
    if(read_overlay(index,within,out,count)) {
      ++io_stats.nvm_reads;io_stats.nvm_read_us+=port_time_us()-before;
      offset+=count;out+=count;length-=count;continue;
    }
#endif
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
  uint8_t scratch[PAGE];
  while(length) {
    unsigned index=offset/PAGE, within=offset%PAGE, count=PAGE-within;
    if(count>length)count=length;
    uint8_t *block=scratch;
#ifdef PORT_VFLASH_PAGED_NVM
    if(batch_depth) {
      if(!batch_valid || batch_index!=index) {
        flush_batch();batch_valid=0;
        nvm_read(index*PAGE,batch_block,PAGE);
        batch_index=index;batch_valid=1;
      }
      block=batch_block;
    } else
#endif
      nvm_read(index*PAGE,block,PAGE);
    int changed=0;
    for (unsigned i=0;i<count;++i) {
      uint8_t value=erase ? 255 : block[within+i]&data[i];
      changed|=value!=block[within+i];block[within+i]=value;
    }
    if(!changed) {offset+=count;length-=count;if(!erase)data+=count;continue;}
#ifdef PORT_VFLASH_PAGED_NVM
    if(batch_depth)batch_dirty=1;
    else write_overlay(index,block);
#else
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
#endif
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
      port_cooperate_due(port_time_us(),PORT_COOPERATE_BUDGET_US)) port_sleep(0);
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
