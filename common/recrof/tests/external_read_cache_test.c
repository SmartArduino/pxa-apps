/* Exercise the real stdio bridge and READ_AT wire contract, without assets. */
#include "pxa.h"
#include "pxa_fs.h"
#include "pxa_fs_roots.h"
#include "rpc.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static unsigned reads,closes,fail_next,revision;
static uint64_t cookies[4];
static const uint64_t base=UINT64_C(0x100000000);
static uint8_t expected(unsigned index,unsigned file) {
    return (uint8_t)(index*37+(index/4096)*17+revision+file*19);
}
const pxa_event_t *port_rpc_packet(const uint8_t *p,uint32_t n) {
    static pxa_event_t event;static uint8_t payload[4];pxa_wire_view_t v;
    assert(pxa_wire_decode(p,n,PXA_MAX_CONTROL_BYTES,&v));
    assert(v.service==PXA_FS_SERVICE && v.opcode==PXA_FS_OPEN);
    pxa_store_u32(payload,(uint32_t)PXA_STATUS_NOT_FOUND);
    event=(pxa_event_t){v.service,v.opcode,1,payload,4};return &event;
}
const pxa_event_t *port_rpc(uint16_t s,uint16_t o,const void *p,uint32_t n) {
    (void)s;(void)o;(void)p;(void)n;assert(!"Unexpected asset query");return NULL;
}
int port_external_open(const char *path,uint64_t *handle,uint32_t *size,uint64_t *features) {
    if(!strcmp(path,"data/missing"))return 0;
    assert(!strncmp(path,"data/file",9));unsigned file=path[9]-'0';assert(file>=1&&file<=3);
    *handle=base+file;*size=9000;*features=PXA_FS_FILE_FEATURE_READ_AT;cookies[file]=0;return 1;
}
int32_t pxa_call(uint32_t route,uint64_t token,const uint8_t *p,uint32_t n) {
    (void)token;assert(route==(PXA_CORE_SERVICE|(PXA_CORE_CLOSE_HANDLE<<16)) && n==8);
    assert(pxa_load_u64(p)>base);++closes;return 0;
}
int32_t pxa_io(uint64_t handle,uint32_t op,uint8_t *data,uint32_t length) {
    assert(op==16 && length>=24);unsigned file=handle-base;if(file<1||file>3)fprintf(stderr,"IO handle=%llx base=%llx file=%u\n",(unsigned long long)handle,(unsigned long long)base,file);assert(file>=1&&file<=3);
    unsigned offset=pxa_load_u64(data),n=pxa_load_u32(data+16);
    uint64_t cookie=pxa_load_u64(data+8);assert(cookie>cookies[file]);cookies[file]=cookie;
    assert(n<=4096 && length==24+n && !pxa_load_u32(data+20));++reads;
    if(fail_next){fail_next=0;return PXA_STATUS_IO_ERROR;}
    if(offset>=9000)return 0;if(n>9000-offset)n=9000-offset;
    for(unsigned i=0;i<n;++i)data[24+i]=expected(offset+i,file);
    return n;
}
uint64_t port_time_us(void){return 1;}
int port_cooperate_due(uint64_t t,uint32_t budget){(void)t;(void)budget;return 0;}
void port_sleep(unsigned ms){(void)ms;assert(!"Unexpected sleep");}
extern FILE *__wrap_fopen(const char *,const char *);
extern int __wrap_fgetc(FILE *),__wrap_fclose(FILE *),__wrap_feof(FILE *),__wrap_ferror(FILE *);
extern int __wrap_fseek(FILE *,long,int);
extern size_t __wrap_fread(void *,size_t,size_t,FILE *);
extern void __wrap_clearerr(FILE *);
int main(void) {
    assert(!__wrap_fopen("/data/missing","rb") && errno==ENOENT);
    FILE *a=__wrap_fopen("/DATA/FILE1","rb");assert(a);
    for(unsigned i=0;i<8000;++i)assert(__wrap_fgetc(a)==expected(i,1));
    assert(reads==2); // Two pages, rather than 8000 synchronous SD requests.
    assert(!__wrap_fseek(a,4095,SEEK_SET));uint8_t bytes[6000];
    assert(__wrap_fread(bytes,1,4,a)==4);
    for(unsigned i=0;i<4;++i)assert(bytes[i]==expected(4095+i,1));
    FILE *b=__wrap_fopen("/data/file2","rb");assert(b);
    assert(__wrap_fgetc(b)==expected(0,2));
    assert(!__wrap_fseek(a,8990,SEEK_SET));assert(__wrap_fread(bytes,1,20,a)==10);
    assert(__wrap_feof(a));for(unsigned i=0;i<10;++i)assert(bytes[i]==expected(8990+i,1));
    assert(!__wrap_fclose(a));++revision;
    a=__wrap_fopen("/data/file1","rb");assert(a); // Deliberately reused Host handle.
    assert(__wrap_fgetc(a)==expected(0,1));
    assert(!__wrap_fseek(a,0,SEEK_SET));unsigned before=reads;
    assert(__wrap_fread(bytes,1,6000,a)==6000);assert(reads==before+2);
    for(unsigned i=0;i<6000;++i)assert(bytes[i]==expected(i,1));
    FILE *c=__wrap_fopen("/data/file3","rb");assert(c);fail_next=1;
    assert(__wrap_fgetc(c)==EOF && __wrap_ferror(c));__wrap_clearerr(c);
    assert(__wrap_fgetc(c)==expected(0,3));
    assert(!__wrap_fseek(c,10000,SEEK_SET));before=reads;
    assert(__wrap_fgetc(c)==EOF && __wrap_feof(c) && reads==before);
    assert(!__wrap_fclose(a)&&!__wrap_fclose(b)&&!__wrap_fclose(c)&&closes==4);
    puts("PASS: SD byte cache, READ_AT cookies, page crossing, handles, reopen, bulk bypass, EOF and I/O failure");
}
