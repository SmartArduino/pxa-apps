/* Archive reuse must preserve overlays, Host cookies and bounded lifetimes. */
#include "pxa.h"
#include "pxa_fs.h"
#include "pxa_fs_roots.h"
#include "rpc.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned opens,closes,reads,fail_next,mutation;
static uint64_t now=1,cookies[4];
static const uint64_t base=UINT64_C(0x100000000);
int port_external_open_status(void){return PXA_STATUS_NOT_FOUND;}
const pxa_event_t *port_rpc_packet(const uint8_t *p,uint32_t n) {
    static pxa_event_t e;static uint8_t payload[4];pxa_wire_view_t v;
    assert(pxa_wire_decode(p,n,PXA_MAX_CONTROL_BYTES,&v) && v.service==PXA_FS_SERVICE);
    assert(v.opcode==PXA_FS_OPEN);++opens;
    pxa_store_u32(payload,(uint32_t)PXA_STATUS_NOT_FOUND);
    e=(pxa_event_t){v.service,v.opcode,1,payload,4};return &e;
}
const pxa_event_t *port_rpc(uint16_t s,uint16_t o,const void *p,uint32_t n) {
    (void)s;(void)o;(void)p;(void)n;assert(!"Unexpected Assets query");return NULL;
}
int port_external_open(const char *path,uint64_t *handle,uint32_t *size,uint64_t *features) {
    assert(!strncmp(path,"data/file",9));unsigned file=path[9]-'0';assert(file>=1&&file<=3);
    *handle=base+file;*size=9000;*features=PXA_FS_FILE_FEATURE_READ_AT;cookies[file]=0;return 1;
}
int32_t pxa_call(uint32_t route,uint64_t token,const uint8_t *p,uint32_t n) {
    (void)token;assert(route==(PXA_CORE_SERVICE|(PXA_CORE_CLOSE_HANDLE<<16)) && n==8);
    assert(pxa_load_u64(p)>base);++closes;return 0;
}
int32_t pxa_io(uint64_t handle,uint32_t op,uint8_t *data,uint32_t length) {
    assert(op==16 && length>=24);unsigned file=handle-base;assert(file>=1&&file<=3);
    unsigned offset=pxa_load_u64(data),n=pxa_load_u32(data+16);
    uint64_t cookie=pxa_load_u64(data+8);assert(cookie>cookies[file]);cookies[file]=cookie;
    assert(n<=4096 && length==24+n);++reads;
    if(fail_next){fail_next=0;return PXA_STATUS_IO_ERROR;}
    if(offset>=9000)return 0;if(n>9000-offset)n=9000-offset;
    for(unsigned i=0;i<n;++i)data[24+i]=(offset+i+file+mutation)&255;
    return n;
}
uint64_t port_time_us(void){return now;}
int port_cooperate_due(uint64_t t,uint32_t budget){(void)t;(void)budget;return 0;}
void port_sleep(unsigned ms){(void)ms;assert(!"Unexpected sleep");}
extern FILE *__wrap_fopen(const char *,const char *);
extern int __wrap_fgetc(FILE *),__wrap_fclose(FILE *),__wrap_fseek(FILE *,long,int);
extern void port_filesystem_tick(uint64_t);
int main(void) {
    FILE *a=__wrap_fopen("/data/file1.mix","rb");assert(a);
    assert(__wrap_fgetc(a)==1 && reads==1);assert(!__wrap_fclose(a));
    assert(opens==1 && closes==0);
    for(unsigned i=0;i<100;++i) {
        a=__wrap_fopen("/data/file1.mix","rb");assert(a);
        assert(__wrap_fgetc(a)==1);assert(!__wrap_fseek(a,8192,SEEK_SET));
        assert(__wrap_fgetc(a)==1);assert(!__wrap_fclose(a));
    }
    assert(opens==1 && reads==2 && closes==0);
    now+=500000;port_filesystem_tick(now);assert(closes==1);
    ++mutation;a=__wrap_fopen("/data/file1.mix","rb");assert(a);
    assert(__wrap_fgetc(a)==2 && opens==2 && reads==3);assert(!__wrap_fclose(a));
    /* Even an unsuccessful private create invalidates an overlay decision. */
    assert(!__wrap_fopen("/data/override","wb"));
    a=__wrap_fopen("/data/file1.mix","rb");assert(a);
    assert(closes==2 && __wrap_fgetc(a)==2 && reads==4);assert(!__wrap_fclose(a));
    a=__wrap_fopen("/data/file2.mix","rb");assert(a);assert(!__wrap_fclose(a));
    a=__wrap_fopen("/data/file3.mix","rb");assert(a);assert(!__wrap_fclose(a));
    assert(closes==3);now+=500000;port_filesystem_tick(now);assert(closes==5);
    a=__wrap_fopen("/data/file1.mix","rb");assert(a);fail_next=1;
    assert(__wrap_fgetc(a)==EOF);assert(!__wrap_fclose(a) && closes==6);
    a=__wrap_fopen("/data/file1.aud","rb");assert(a);assert(!__wrap_fclose(a) && closes==7);
    puts("PASS: MIX handles, zero-position reopen, cookies, page reuse, expiry, private mutation, eviction and I/O errors");
}
