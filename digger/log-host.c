/* SPDX-License-Identifier: MIT. Single-threaded local logging facade. */
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include "digger-host.h"
extern FILE *digger_log;
static uint64_t beginning;
void digger_log_start(void){beginning=digger_port_time_us();}
void digger_log_vprintf(const char *fmt,va_list ap){if(digger_log)vfprintf(digger_log,fmt,ap);}
void digger_log_printf(const char *fmt,...){va_list ap;va_start(ap,fmt);digger_log_vprintf(fmt,ap);va_end(ap);}
const char *digger_log_ts(char *buf,size_t len){
 uint64_t ms=(digger_port_time_us()-beginning)/1000;
 snprintf(buf,len,"%llu.%03u",(unsigned long long)(ms/1000),(unsigned)(ms%1000));return buf;
}

