/* SPDX-License-Identifier: MIT */
#include "jy_ft_protected.h"
#include "protected_bridge.h"
#define NI __attribute__((noinline))
static unsigned entries, throws, releases, faults;
NI int jy_ft_checkpoint(jy_ft_jmp_buf buffer) {
    int resumed_throw = jy_protected_checkpoint(buffer, &buffer[0].status);
    if (!resumed_throw) {
        /* The initial buffer need not contain an initialized status value. */
        ++entries;
        return 0;
    }
    int status = buffer[0].status;
    if (status <= 0 || status == 4) {
        /* Bridge resource/inactive/capacity faults are not valid fonts and
         * must not be mistaken for gray-raster retryable overflow. */
        ++faults;
        __builtin_trap();
    }
    return status;
}
NI void jy_ft_throw(jy_ft_jmp_buf buffer, int status) {
    ++throws;
    jy_protected_throw(buffer, status ? status : 1);
}
void jy_ft_release(jy_ft_jmp_buf buffer) {
    ++releases;
    jy_protected_release(buffer);
}
unsigned jy_ft_entry_count(void) { return entries; }
unsigned jy_ft_throw_count(void) { return throws; }
unsigned jy_ft_release_count(void) { return releases; }
unsigned jy_ft_fault_count(void) { return faults; }
