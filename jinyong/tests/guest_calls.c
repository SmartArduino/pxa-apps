/* SPDX-License-Identifier: MIT
 * Exercise the import boundary with a scheduler that asserts Host calls run
 * outside the engine. The real Wasm continuation is tested by the simulator.
 */
#include "pxa_core.h"
#include "guest_calls.h"
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>

extern int32_t __wrap_pxa_call(uint32_t,uint64_t,const uint8_t *,uint32_t);
extern int32_t __wrap_pxa_io(uint64_t,uint32_t,uint8_t *,uint32_t);
static int active,suspensions,imports,escape,result;
static uint32_t seen_route,seen_bytes;
static uint64_t seen_token,seen_handle;
static uint8_t seen_data[PXA_MAX_CONTROL_BYTES];
static jmp_buf cancelled;
int jy_coroutine_active(void){return active;}
void jy_suspend(uint64_t us) {
    assert(!us&&active);suspensions++;active=0;
    assert(jy_host_call_pending()&&!jy_host_result_ready());
    uint32_t kind,operation;jy_host_describe(&kind,&operation);
    assert(kind==1||kind==2);assert(operation);
    if(escape)longjmp(cancelled,1);
    assert(jy_host_dispatch());assert(jy_host_result_ready()&&!jy_host_call_pending());
    assert(jy_host_last_result()==result);
    assert(!jy_host_dispatch());active=1;
}
int32_t jy_native_call(uint32_t route,uint64_t token,const uint8_t *data,uint32_t bytes) {
    assert(!active);imports++;seen_route=route;seen_token=token;seen_bytes=bytes;
    assert(bytes<=sizeof seen_data);if(bytes)memcpy(seen_data,data,bytes);return result;
}
int32_t jy_native_io(uint64_t handle,uint32_t operation,uint8_t *data,uint32_t bytes) {
    assert(!active);imports++;seen_handle=handle;seen_bytes=bytes;
    if(result>0){assert((uint32_t)result<=bytes);if(operation==1)memset(data,0x5a,(size_t)result);else memcpy(seen_data,data,(size_t)result);}
    return result;
}
int main(void) {
    uint8_t input[40],output[40];for(unsigned i=0;i<sizeof input;i++)input[i]=(uint8_t)(i*7);
    jy_host_calls_reset();assert(!jy_host_dispatch());
    active=0;result=23;assert(__wrap_pxa_call(0x20001,17,input,8)==23);
    assert(imports==1&&!suspensions&&seen_route==0x20001&&seen_token==17&&seen_bytes==8&&!memcmp(seen_data,input,8));
    active=1;result=0;assert(!__wrap_pxa_call(0x20001,19,input,8));
    assert(imports==2&&suspensions==1&&active&&!jy_host_result_ready()&&!memcmp(seen_data,input,8));
    result=PXA_STATUS_RESOURCE_LIMIT;assert(__wrap_pxa_call(9,21,input,8)==result);
    assert(imports==3&&suspensions==2);
    assert(__wrap_pxa_call(9,0,input,PXA_MAX_CONTROL_BYTES+1)==PXA_STATUS_LIMIT_EXCEEDED);
    assert(imports==3&&suspensions==2);
    memset(output,0,sizeof output);result=7;assert(__wrap_pxa_io(55,1,output,sizeof output)==7);
    assert(seen_handle==55&&seen_bytes==sizeof output&&output[0]==0x5a&&output[6]==0x5a&&!output[7]);
    result=11;assert(__wrap_pxa_io(57,2,input,sizeof input)==11&&!memcmp(seen_data,input,11));
    result=PXA_STATUS_WOULD_BLOCK;assert(__wrap_pxa_io(59,1,output,sizeof output)==result);
    int before=imports;escape=1;
    if(!setjmp(cancelled)){(void)__wrap_pxa_call(99,0,input,8);assert(0);}
    assert(!active&&imports==before&&jy_host_call_pending());jy_host_calls_reset();assert(!jy_host_dispatch()&&!jy_host_call_pending());
    escape=0;active=1;result=0;assert(!__wrap_pxa_call(101,23,input,8));
    assert(imports==before+1&&seen_route==101&&seen_token==23);
    puts("PASS root-only Host execution, control arguments, direct I/O, short results, backpressure, submission errors and cancellation");
    return 0;
}
