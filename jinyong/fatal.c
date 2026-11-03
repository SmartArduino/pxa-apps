/* SPDX-License-Identifier: MIT. PXA lifecycle replaces unsupported WASI
 * process termination. Broken invariants remain real fatal traps. */
#include <stdint.h>
#include "pxa_port_log.h"
#include "protected_bridge.h"
#define NORETURN __attribute__((noreturn))
NORETURN void __wrap_abort(void){pxa_log_write(4,"JY fatal=abort");__builtin_trap();}
NORETURN void __wrap___assert_fail(const char *condition,const char *file,unsigned line,const char *function){
 pxlog_t log;pxlog_init(&log);pxlog_text(&log,"JY fatal=assert condition=");pxlog_text(&log,condition?condition:"");pxlog_text(&log," file=");pxlog_text(&log,file?file:"");pxlog_field(&log,"line",line);pxlog_text(&log," function=");pxlog_text(&log,function?function:"");pxlog_send(&log);__builtin_trap();
}
/* jy_finish is intentionally a returning C signature so Asyncify can unwind
 * before the unreachable continuation. This wrapper is never resumed. */
NORETURN void __wrap_exit(int code){jy_finish(code);__builtin_unreachable();}
NORETURN void __wrap__Exit(int code){jy_finish(code);__builtin_unreachable();}
NORETURN void __wrap__exit(int code){jy_finish(code);__builtin_unreachable();}
NORETURN void __wrap___wasi_proc_exit(uint32_t code){jy_finish((int)code);__builtin_unreachable();}
