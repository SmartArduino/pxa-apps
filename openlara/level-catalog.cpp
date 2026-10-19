#include "level-catalog.h"
#include "engine.h"
#include "levels.h"
#include "pxa_fs_roots.h"
#include <stdio.h>
#include <string.h>

static constexpr uint64_t TOKEN=0x434154;
static uint64_t media;
static uint32_t mask;
static unsigned level,variant;
static uint8_t locations[OL_LEVEL_COUNT];
static bool busy;
static char path[64];

static const char *name(unsigned id,unsigned location) {
    const char *base=ol_level_file(id);
    if(!location)return base;
    snprintf(path,sizeof(path),"%s/%s",location<=2?"DATA":"data",base);
    if(location==1 || location==3)for(char *p=strchr(path,'/')+1;*p;++p)
        if(*p>='a' && *p<='z')*p-=32;
    return path;
}
static void next() {
    while(level<OL_LEVEL_COUNT) {
        const char *file=name(level,variant+1);
        if(!pxa_fs_request_open_at(TOKEN,media,file,strlen(file),PXA_FS_OPEN_READ))return;
        if(++variant==4) {variant=0;++level;}
    }
    busy=false;
}
void catalog_start(uint64_t root,uint32_t installed) {
    media=root;mask=installed;level=variant=0;memset(locations,0,sizeof(locations));
    busy=root!=0;if(busy)next();
}
bool catalog_busy() {return busy;}
uint32_t catalog_mask() {return mask;}
const char *catalog_level_path(unsigned id) {return id<OL_LEVEL_COUNT?name(id,locations[id]):"";}
bool catalog_event(const pxa_event_t &e) {
    if(e.service!=PXA_FS_SERVICE || e.token!=TOKEN)return false;
    if(!busy)return true;
    pxa_fs_at_result_t r;
    bool valid=false;
    if(pxa_fs_parse_open_at(&e,TOKEN,&r) && !r.status) {
        uint8_t header[4];
        valid=r.kind==PXA_FS_KIND_REGULAR && r.size>=32 && r.size<=OL_LEVEL_CAPACITY &&
            pxa_fs_read(r.handle,header,sizeof(header))==sizeof(header) && pxa_load_u32(header)==0x20;
        pxa_close_handle(r.handle);
    }
    if(valid) {mask|=1u<<level;locations[level]=variant+1;variant=0;++level;}
    else if(++variant==4) {variant=0;++level;}
    next();return true;
}
