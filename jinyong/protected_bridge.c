/* SPDX-License-Identifier: MIT

 * Real multi-anchor Lua protected calls via Binaryen Asyncify.

 * This is not setjmp=0: each live protected frame owns its saved Wasm locals

 * and the exact C stack pointer. Surviving caller C-frame memory remains live.
 * Throws discard the current continuation

 * and rewind the exact live anchor with its original volatile error status.

 */

#include "protected_bridge.h"

#include <stddef.h>

#include <stdlib.h>

#include <string.h>

#define NI __attribute__((noinline))

#define GAME_C_BYTES (128u*1024u-64u)

#define HOST_C_BYTES (64u*1024u)

#define ASYNC_BYTES (64u*1024u)

#define MAX_ANCHORS 64u

#define ANCHOR_BUDGET (8u*1024u*1024u)

__attribute__((import_module("asyncify"),import_name("start_unwind"))) void jy_async_start_unwind(void *);

__attribute__((import_module("asyncify"),import_name("stop_unwind"))) void jy_async_stop_unwind(void);

__attribute__((import_module("asyncify"),import_name("start_rewind"))) void jy_async_start_rewind(void *);

__attribute__((import_module("asyncify"),import_name("stop_rewind"))) void jy_async_stop_rewind(void);

extern uintptr_t jy_stack_enter(int,uintptr_t);

extern uintptr_t jy_stack_pointer(void);

extern unsigned char __stack_low[],__stack_high[];

static unsigned char *game_c;

static _Alignas(16) unsigned char saved_async[ASYNC_BYTES];

static struct {uint32_t cursor,end;} async_data;

typedef struct {

 void *identity;volatile int *status;

 uintptr_t sp;uint32_t wasm_bytes,c_bytes;

 unsigned char *wasm,*c;

} anchor;

static anchor anchors[MAX_ANCHORS];

static unsigned count,peak;

static uint32_t allocated,allocated_peak,saved_peak;

static uintptr_t game_sp,live_sp;

static int phase,reason,rewind_kind,active,error_code;

static unsigned selected;

static uint64_t wait_value;
static int finish_code;
static int fault_code;
int jy_fault_code(void){return fault_code;}
static int fault(int code){fault_code=code;return -1;}

static void (*entry_point)(void);

NI void jy_engine_entry(int unused){(void)unused;entry_point();}

static void free_anchor(unsigned i){

 allocated-=anchors[i].wasm_bytes+anchors[i].c_bytes;

 free(anchors[i].wasm);free(anchors[i].c);memset(&anchors[i],0,sizeof anchors[i]);

}

static int find_anchor(void *identity){for(unsigned i=count;i;i--)if(anchors[i-1].identity==identity)return (int)i-1;return -1;}

static void fresh_async(void){async_data.cursor=(uint32_t)(uintptr_t)saved_async;async_data.end=(uint32_t)(uintptr_t)(saved_async+sizeof saved_async);}

static int save_anchor(unsigned i){

 anchor *a=&anchors[i];

 uint32_t w=async_data.cursor-(uint32_t)(uintptr_t)saved_async;

 if(a->sp<(uintptr_t)game_c||a->sp>(uintptr_t)(game_c+GAME_C_BYTES)||w>ASYNC_BYTES)return fault(1);

 uint32_t c=(uint32_t)((uintptr_t)(game_c+GAME_C_BYTES)-a->sp);

 if((uint64_t)allocated+w>ANCHOR_BUDGET)return fault(2);

 a->wasm=malloc(w?w:1);a->c=malloc(c?c:1);

 if(!a->wasm||!a->c){int code=!a->wasm?3:4;free(a->wasm);free(a->c);a->wasm=a->c=NULL;return fault(code);}

 memcpy(a->wasm,saved_async,w);memcpy(a->c,(void*)a->sp,c);

 a->wasm_bytes=w;a->c_bytes=c;allocated+=w+c;

 if(allocated>allocated_peak)allocated_peak=allocated;

 return 0;

}

NI static int jy_drive(void){

 active=1;game_sp=jy_stack_enter(0,game_sp);active=0;

 if(!reason){phase=JY_DONE;return phase;}

 jy_async_stop_unwind();game_sp=live_sp;
 if(reason==5){while(count)free_anchor(--count);return phase=JY_DONE;}

 uint32_t n=async_data.cursor-(uint32_t)(uintptr_t)saved_async;

 if(n>saved_peak)saved_peak=n;

 if(n>ASYNC_BYTES||game_sp<(uintptr_t)game_c||game_sp>(uintptr_t)(game_c+GAME_C_BYTES)){fault_code=5;return phase=JY_FAULT;}

 if(reason==JY_CHECKPOINT){

  if(save_anchor(selected)<0)return phase=JY_FAULT;

 }

 return phase=reason;

}

NI int jy_coroutine_begin(void (*entry)(void)){
 fault_code=0;

 if(active||phase!=JY_IDLE||!entry)return -1;

 game_c=(unsigned char*)((uintptr_t)__stack_low+64u);

 if((uintptr_t)__stack_high-(uintptr_t)__stack_low<GAME_C_BYTES+64u+HOST_C_BYTES)return -1;

 entry_point=entry;game_sp=(uintptr_t)(game_c+GAME_C_BYTES);

 fresh_async();phase=JY_IDLE;reason=rewind_kind=0;return jy_drive();

}

NI int jy_coroutine_resume(void){

 if(active||!(phase==JY_WAIT||phase==JY_CHECKPOINT||phase==JY_THROW))return -1;

 int old=phase;

 if(old==JY_THROW){

  if(selected>=count)return phase=JY_FAULT;

  while(count>selected+1)free_anchor(--count);

  anchor *a=&anchors[selected];

  /* Ordinary longjmp preserves memory in surviving caller C frames.
   * The separate legal game stack is still live: do not roll it back. */
  memcpy(saved_async,a->wasm,a->wasm_bytes);

  async_data.cursor=(uint32_t)(uintptr_t)saved_async+a->wasm_bytes;

  async_data.end=(uint32_t)(uintptr_t)(saved_async+sizeof saved_async);

  game_sp=a->sp;*a->status=error_code;

 }

 reason=0;rewind_kind=old;jy_async_start_rewind(&async_data);return jy_drive();

}

NI int jy_protected_checkpoint(void *identity,volatile int *status){

 if(rewind_kind==JY_CHECKPOINT||rewind_kind==JY_THROW){

  int ret=rewind_kind==JY_THROW;rewind_kind=0;jy_async_stop_rewind();return ret;

 }

 if(!active||count==MAX_ANCHORS){*status=4;return 1;}

 selected=count++;if(count>peak)peak=count;

 anchor *a=&anchors[selected];memset(a,0,sizeof *a);a->identity=identity;a->status=status;a->sp=jy_stack_pointer();

 live_sp=a->sp;reason=JY_CHECKPOINT;fresh_async();jy_async_start_unwind(&async_data);return 0;

}

void jy_protected_release(void *identity){

 int i=find_anchor(identity);if(i<0)return;

 /* A protected call may complete only after all its nested calls complete. */

 if((unsigned)i!=count-1){phase=JY_FAULT;__builtin_trap();}

 free_anchor(--count);

}

extern void jy_port_protected_error(int);
NI void jy_protected_throw(void *identity,int status){
 jy_port_protected_error(status);

 int i=find_anchor(identity);if(!active||i<0){phase=JY_FAULT;__builtin_trap();}

 selected=(unsigned)i;error_code=status;live_sp=jy_stack_pointer();reason=JY_THROW;fresh_async();jy_async_start_unwind(&async_data);

}

NI void jy_suspend(uint64_t us){

 if(rewind_kind==JY_WAIT){rewind_kind=0;jy_async_stop_rewind();return;}

 if(!active){phase=JY_FAULT;__builtin_trap();}

 wait_value=us;live_sp=jy_stack_pointer();reason=JY_WAIT;fresh_async();jy_async_start_unwind(&async_data);

}

NI void jy_finish(int code){
 if(!active){phase=JY_FAULT;__builtin_trap();}
 finish_code=code;live_sp=jy_stack_pointer();reason=5;fresh_async();jy_async_start_unwind(&async_data);
}
int jy_finish_code(void){return finish_code;}

/* Only the event root cancels. A trapped Guest may have skipped the normal
 * jy_drive epilogue, so clear active before closing any Host-backed files. */
NI void jy_coroutine_cancel(void){active=0;while(count)free_anchor(--count);phase=JY_DONE;reason=rewind_kind=0;}

int jy_coroutine_phase(void){return phase;}
int jy_coroutine_active(void){return active;}

uint64_t jy_wait_us(void){return wait_value;}

unsigned jy_anchor_count(void){return count;}

unsigned jy_anchor_peak(void){return peak;}

uint32_t jy_saved_peak(void){return saved_peak;}

uint32_t jy_anchor_bytes_peak(void){return allocated_peak;}

