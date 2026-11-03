/* SPDX-License-Identifier: MIT
 * Run Host imports after the original engine has unwound to the event root.
 * In particular, closing a private file can flush LittleFS. That Host work
 * must not share the native task stack with the Lua/SDL call chain.
 */
#include "pxa_core.h"
#include "protected_bridge.h"
#include "guest_calls.h"
#include <string.h>

__attribute__((import_module("pxa.core.v2"),import_name("pxa_call")))
extern int32_t jy_native_call(uint32_t,uint64_t,const uint8_t *,uint32_t);
__attribute__((import_module("pxa.core.v2"),import_name("pxa_io")))
extern int32_t jy_native_io(uint64_t,uint32_t,uint8_t *,uint32_t);

static struct {
    volatile int kind,ready;
    volatile int32_t result;
    uint32_t route,operation,bytes;
    uint64_t token,handle;
    uint8_t *io_data;
    uint8_t control[PXA_MAX_CONTROL_BYTES];
} pending;
static unsigned busy_count;
static int32_t last_error;
unsigned jy_host_busy_count(void){return busy_count;}
int32_t jy_host_last_error(void){return last_error;}
int32_t jy_host_last_result(void){return pending.result;}

int32_t __wrap_pxa_call(uint32_t route,uint64_t token,const uint8_t *data,uint32_t bytes) {
    if(!jy_coroutine_active())return jy_native_call(route,token,data,bytes);
    if(pending.kind){busy_count++;return PXA_STATUS_BAD_STATE;}
    if(bytes>sizeof pending.control)return PXA_STATUS_LIMIT_EXCEEDED;
    pending.kind=1;pending.ready=0;pending.route=route;pending.token=token;pending.bytes=bytes;
    if(bytes)memcpy(pending.control,data,bytes);
    jy_suspend(0);
    if(!pending.ready)return PXA_STATUS_BAD_STATE;
    int32_t result=pending.result;pending.kind=pending.ready=0;return result;
}
int32_t __wrap_pxa_io(uint64_t handle,uint32_t operation,uint8_t *data,uint32_t bytes) {
    if(!jy_coroutine_active())return jy_native_io(handle,operation,data,bytes);
    if(pending.kind){busy_count++;return PXA_STATUS_BAD_STATE;}
    pending.kind=2;pending.ready=0;pending.handle=handle;pending.operation=operation;
    /* The separate game C stack and its heap remain live while suspended;
     * the Host C stack cannot overwrite this read/write buffer. */
    pending.io_data=data;pending.bytes=bytes;
    jy_suspend(0);
    if(!pending.ready)return PXA_STATUS_BAD_STATE;
    int32_t result=pending.result;pending.kind=pending.ready=0;return result;
}
int jy_host_result_ready(void){return pending.kind&&pending.ready;}
int jy_host_call_pending(void){return pending.kind&&!pending.ready;}
void jy_host_describe(uint32_t *kind,uint32_t *operation){
    *kind=(uint32_t)pending.kind;
    *operation=pending.kind==1?pending.route:pending.operation;
}
int jy_host_dispatch(void) {
    if(!pending.kind||pending.ready)return 0;
    pending.result=pending.kind==1?
        jy_native_call(pending.route,pending.token,pending.control,pending.bytes):
        jy_native_io(pending.handle,pending.operation,pending.io_data,pending.bytes);
    if(pending.result<0)last_error=pending.result;
    pending.ready=1;return 1;
}
void jy_host_calls_reset(void){memset(&pending,0,sizeof pending);}
