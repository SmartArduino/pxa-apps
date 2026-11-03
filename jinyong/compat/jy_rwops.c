/* SPDX-License-Identifier: MIT */
#include "jy_sdl.h"
#include "../fs/jy_fs.h"
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
enum {JY_RW_FILE=1,JY_RW_MEMORY=2,JY_RW_CONST_MEMORY=3};
static int memory_seek(SDL_RWops *rw,int offset,int whence){
 int64_t size=rw->hidden.mem.base?rw->hidden.mem.stop-rw->hidden.mem.base:0,position=rw->hidden.mem.base?rw->hidden.mem.here-rw->hidden.mem.base:0;
 int64_t next=(whence==RW_SEEK_SET?0:whence==RW_SEEK_CUR?position:whence==RW_SEEK_END?size:-INT64_C(0x100000000))+offset;
 if(next<0||next>size||next>INT_MAX)return jy_sdl_fail("Memory RW seek is out of range");
 if(rw->hidden.mem.base)rw->hidden.mem.here=rw->hidden.mem.base+next;
 return (int)next;
}
static int memory_read(SDL_RWops *rw,void *out,int size,int count){
 if(size<=0||count<=0)return 0;
 if(!out)return jy_sdl_fail("Null memory RW read buffer");
 size_t available=rw->hidden.mem.base?(size_t)(rw->hidden.mem.stop-rw->hidden.mem.here):0;
 size_t objects=available/(unsigned)size;
 if(objects>(unsigned)count)objects=(unsigned)count;
 size_t bytes=objects*(unsigned)size;
 if(bytes){memcpy(out,rw->hidden.mem.here,bytes);
  rw->hidden.mem.here+=bytes;
  }return (int)objects;
}
static int memory_write(SDL_RWops *rw,const void *in,int size,int count){
 if(rw->type==JY_RW_CONST_MEMORY){SDL_SetError("Cannot write to const memory RW");
  return 0;
  }if(size<=0||count<=0)return 0;
 if(!in)return jy_sdl_fail("Null memory RW write buffer");
 size_t available=rw->hidden.mem.base?(size_t)(rw->hidden.mem.stop-rw->hidden.mem.here):0,objects=available/(unsigned)size;
 if(objects>(unsigned)count)objects=(unsigned)count;
 size_t bytes=objects*(unsigned)size;
 if(bytes){memcpy(rw->hidden.mem.here,in,bytes);
  rw->hidden.mem.here+=bytes;
  }return (int)objects;
}
static int memory_close(SDL_RWops *rw){SDL_FreeRW(rw);
 return 0;
 }
static int file_seek(SDL_RWops *rw,int offset,int whence){if(fseek(rw->hidden.stdio.fp,offset,whence))return jy_sdl_fail("FILE RW seek failed");
 long where=ftell(rw->hidden.stdio.fp);
 if(where<0||where>INT_MAX)return jy_sdl_fail("FILE RW offset exceeds SDL1 range");
 return (int)where;
 }
static int file_read(SDL_RWops *rw,void *out,int size,int count){if(size<=0||count<=0)return 0;
 if(!out)return jy_sdl_fail("Null FILE RW read buffer");
 size_t n=fread(out,(size_t)size,(size_t)count,rw->hidden.stdio.fp);
 if(ferror(rw->hidden.stdio.fp))SDL_SetError("FILE RW read error: %s",strerror(errno));
 return (int)n;
 }
static int file_write(SDL_RWops *rw,const void *in,int size,int count){if(size<=0||count<=0)return 0;
 if(!in)return jy_sdl_fail("Null FILE RW write buffer");
 size_t n=fwrite(in,(size_t)size,(size_t)count,rw->hidden.stdio.fp);
 if(n!=(size_t)count)SDL_SetError("FILE RW write error: %s",strerror(errno));
 return (int)n;
 }
static int file_close(SDL_RWops *rw){int result=rw->hidden.stdio.autoclose?jy_fs_fclose(rw->hidden.stdio.fp):0;
 SDL_FreeRW(rw);
 return result;
 }
SDL_RWops *SDL_AllocRW(void){SDL_RWops *rw=calloc(1,sizeof *rw);
 if(!rw)SDL_OutOfMemory();
 return rw;
 }
void SDL_FreeRW(SDL_RWops *rw){free(rw);
 }
SDL_RWops *SDL_RWFromFP(FILE *file,int autoclose){if(!file){jy_sdl_fail("Null FILE for RW");
  return NULL;
  }SDL_RWops *rw=SDL_AllocRW();
 if(!rw)return NULL;
 rw->type=JY_RW_FILE;
 rw->seek=file_seek;
 rw->read=file_read;
 rw->write=file_write;
 rw->close=file_close;
 rw->hidden.stdio.fp=file;
 rw->hidden.stdio.autoclose=!!autoclose;
 return rw;
 }
SDL_RWops *SDL_RWFromFile(const char *path,const char *mode){FILE *file=jy_fs_fopen(path,mode);
 if(!file){SDL_SetError("Cannot open %s: %s",path?path:"(null)",strerror(errno));
  return NULL;
  }SDL_RWops *rw=SDL_RWFromFP(file,1);
 if(!rw)jy_fs_fclose(file);
 return rw;
 }
static SDL_RWops *memory_rw(void *data,int size,int immutable){if(size<0||(!data&&size)){jy_sdl_fail("Invalid memory RW range");
  return NULL;
  }SDL_RWops *rw=SDL_AllocRW();
 if(!rw)return NULL;
 rw->type=immutable?JY_RW_CONST_MEMORY:JY_RW_MEMORY;
 rw->seek=memory_seek;
 rw->read=memory_read;
 rw->write=memory_write;
 rw->close=memory_close;
 rw->hidden.mem.base=data;
 rw->hidden.mem.here=data;
 rw->hidden.mem.stop=data?(Uint8 *)data+size:NULL;
 return rw;
 }
SDL_RWops *SDL_RWFromMem(void *data,int size){return memory_rw(data,size,0);
 }
SDL_RWops *SDL_RWFromConstMem(const void *data,int size){return memory_rw((void *)data,size,1);
 }
static Uint64 read_number(SDL_RWops *rw,int bytes,int big){Uint8 data[8];
 if(!rw||SDL_RWread(rw,data,1,bytes)!=bytes){SDL_SetError("Truncated endian RW read");
  return 0;
  }Uint64 value=0;
 for(int i=0;i<bytes;++i)value|=(Uint64)data[i]<<((big?bytes-i-1:i)*8);
 return value;
 }
static int write_number(SDL_RWops *rw,Uint64 value,int bytes,int big){Uint8 data[8];
 for(int i=0;i<bytes;++i)data[i]=(Uint8)(value>>((big?bytes-i-1:i)*8));
 return rw&&SDL_RWwrite(rw,data,bytes,1)==1;
 }
Uint16 SDL_ReadLE16(SDL_RWops *rw){return (Uint16)read_number(rw,2,0);
 }Uint16 SDL_ReadBE16(SDL_RWops *rw){return (Uint16)read_number(rw,2,1);
 }
Uint32 SDL_ReadLE32(SDL_RWops *rw){return (Uint32)read_number(rw,4,0);
 }Uint32 SDL_ReadBE32(SDL_RWops *rw){return (Uint32)read_number(rw,4,1);
 }
Uint64 SDL_ReadLE64(SDL_RWops *rw){return read_number(rw,8,0);
 }Uint64 SDL_ReadBE64(SDL_RWops *rw){return read_number(rw,8,1);
 }
int SDL_WriteLE16(SDL_RWops *rw,Uint16 value){return write_number(rw,value,2,0);
 }int SDL_WriteBE16(SDL_RWops *rw,Uint16 value){return write_number(rw,value,2,1);
 }
int SDL_WriteLE32(SDL_RWops *rw,Uint32 value){return write_number(rw,value,4,0);
 }int SDL_WriteBE32(SDL_RWops *rw,Uint32 value){return write_number(rw,value,4,1);
 }
int SDL_WriteLE64(SDL_RWops *rw,Uint64 value){return write_number(rw,value,8,0);
 }int SDL_WriteBE64(SDL_RWops *rw,Uint64 value){return write_number(rw,value,8,1);
 }
