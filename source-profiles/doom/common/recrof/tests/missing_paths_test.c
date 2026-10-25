/* Real stdio bridge: repeated misses, aliases, mutations and transient errors. */
#include "pxa.h"
#include "pxa_fs.h"
#include "rpc.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
static unsigned private_opens,media_opens;
static uint64_t now=1;
static int private_status=PXA_STATUS_NOT_FOUND,media_status=PXA_STATUS_NOT_FOUND;
const pxa_event_t *port_rpc_packet(const uint8_t *p,uint32_t n){
  static pxa_event_t e;static uint8_t payload[4];pxa_wire_view_t v;
  assert(pxa_wire_decode(p,n,PXA_MAX_CONTROL_BYTES,&v));
  assert(v.service==PXA_FS_SERVICE && v.opcode==PXA_FS_OPEN);++private_opens;
  pxa_store_u32(payload,(uint32_t)private_status);
  e=(pxa_event_t){v.service,v.opcode,1,payload,4};return &e;
}
const pxa_event_t *port_rpc(uint16_t s,uint16_t o,const void *p,uint32_t n){
  (void)s;(void)o;(void)p;(void)n;assert(!"Unexpected asset query");return NULL;
}
int port_external_open(const char *p,uint64_t *h,uint32_t *s,uint64_t *f){
  (void)p;(void)h;(void)s;(void)f;++media_opens;return 0;
}
int port_external_open_status(void){return media_status;}
uint64_t port_time_us(void){return now;}
extern FILE *__wrap_fopen(const char *,const char *);
static void absent(const char *path){assert(!__wrap_fopen(path,"rb") && errno==ENOENT);}
int main(void){
  absent("/DATA/SCM00EA.INI");
  absent("/data/.//scm00ea.ini");absent("scm00ea.ini");
  assert(private_opens==1 && media_opens==1);
  now+=1000000;absent("/data/scm00ea.ini");assert(private_opens==2 && media_opens==2);
  assert(!__wrap_fopen("/data/new.ini","wb"));
  absent("/data/scm00ea.ini");assert(private_opens==4 && media_opens==3);
  media_status=PXA_STATUS_IO_ERROR;
  absent("/data/transient.ini");absent("/data/transient.ini");
  assert(private_opens==6 && media_opens==5);
  media_status=PXA_STATUS_NOT_FOUND;
  absent("/data/transient.ini");absent("/data/transient.ini");
  assert(private_opens==7 && media_opens==6);
  private_status=PXA_STATUS_IO_ERROR;
  absent("/data/private-error.ini");absent("/data/private-error.ini");
  assert(private_opens==9 && media_opens==8);
  puts("PASS: canonical repeated misses avoid RPCs; expiry, private writes and transient I/O remain visible");
}
