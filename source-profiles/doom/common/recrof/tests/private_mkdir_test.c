/* An SD resource directory must not prevent creating its private save layer. */
#include "pxa.h"
#include "rpc.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
static int exists,creates,external_queries;
static int32_t stat_status=PXA_STATUS_NOT_FOUND;
static pxa_event_t event;
static uint8_t payload[16];
const pxa_event_t *port_rpc_packet(const uint8_t *packet,uint32_t size) {
  pxa_wire_view_t v;
  assert(pxa_wire_decode(packet,size,PXA_MAX_CONTROL_BYTES,&v));
  memset(&event,0,sizeof(event));memset(payload,0,sizeof(payload));
  event.service=v.service;event.opcode=v.opcode;event.token=7;
  event.payload=payload;event.payload_size=4;
  if(v.opcode==5) {
    int32_t status=exists?0:stat_status;
    pxa_store_u32(payload,(uint32_t)status);
    if(!status) {event.payload_size=13;payload[4]=2;}
  } else if(v.opcode==2) {++creates;exists=1;}
  else assert(!"Unexpected private operation");
  return &event;
}
int port_external_stat(const char *p,uint8_t *kind,uint64_t *size) {
  assert(!strcmp(p,"data/baseq2"));++external_queries;*kind=2;*size=0;return 1;
}
const pxa_event_t *port_rpc(uint16_t service,uint16_t opcode,const void *p,uint32_t n) {
  (void)service;(void)opcode;(void)p;(void)n;assert(!"Unexpected asset query");return NULL;
}
extern int __wrap_mkdir(const char *,mode_t);
extern int __wrap_stat(const char *,struct stat *);
int main(void) {
  struct stat st;
  assert(__wrap_stat("/data/baseq2",&st)==0 && S_ISDIR(st.st_mode));
  assert(external_queries==1 && creates==0);
  assert(__wrap_mkdir("/data/baseq2",0700)==0 && creates==1 && exists);
  assert(external_queries==1);
  assert(__wrap_mkdir("/data/baseq2",0700)==-1 && errno==EEXIST && creates==1);
  exists=0;stat_status=PXA_STATUS_IO_ERROR;
  assert(__wrap_mkdir("/data/baseq2",0700)==-1 && errno==EIO && creates==1);
  assert(__wrap_mkdir("/elsewhere",0700)==-1 && errno==EACCES && creates==1);
  puts("PASS: SD directory visibility, private mkdir, existing path, I/O errors and confinement");
}
