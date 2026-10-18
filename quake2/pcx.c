/* Stream compressed bytes without an arena-sized copy of the PCX file. */
#include "ref_soft/r_local.h"
#include "other/q_arena.h"
#include "pcx_stream.h"
typedef struct {FILE *file;long base;} Source;
static int read_image(void *context,uint32_t offset,void *out,uint32_t count){
  Source *s=context;
  FS_ReadAt(s->file,s->base,offset,out,count);
  return 1;
}
void Pxa_LoadPCX(char *name,byte **pic,byte **palette,int *width,int *height,int permanent){
  Source context={0};*pic=NULL;if(palette)*palette=NULL;
  int bytes=FS_FOpenFile(name,&context.file);
  if(!context.file)return;
  context.base=ftell(context.file);
  port_pcx_source_t source={&context,(uint32_t)bytes,read_image};port_pcx_info_t info;
  if(bytes<0 || context.base<0 || !port_pcx_header(&source,&info)){
    fclose(context.file);ri.Con_Printf(PRINT_DEVELOPER,"Bad pcx file %s\n",name);return;
  }
  byte *pixels=permanent?Q_Arena_Alloc(info.width*info.height):Q_Arena_AllocTop(info.width*info.height);
  if(!pixels){fclose(context.file);ri.Sys_Error(ERR_DROP,"LoadPCX: out of memory for %s",name);return;}
  byte *colors=palette?malloc(768):NULL;
  int good=(!palette || colors) && port_pcx_pixels(&source,&info,pixels,info.width*info.height) &&
           (!palette || port_pcx_palette(&source,colors));
  fclose(context.file);
  if(!good){Q_Arena_Free(pixels);free(colors);ri.Con_Printf(PRINT_ALL,"Bad pcx file %s\n",name);return;}
  *pic=pixels;if(palette)*palette=colors;if(width)*width=info.width;if(height)*height=info.height;
}
