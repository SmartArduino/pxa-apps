// SPDX-License-Identifier: MIT
#include "../pxa_landscape.h"
#include <assert.h>
#include <stdio.h>
static int result;
int32_t pxa_call(uint32_t route,uint64_t token,const uint8_t *data,uint32_t length) {
    assert((route&65535)==PXA_WINDOW_SERVICE && (route>>16)==PXA_WINDOW_CONFIGURE);
    assert(!token && length==5);
    assert(pxa_load_u16(data)==(9|0x8000) && pxa_load_u16(data+2)==1 && data[4]==2);
    return result;
}
int main(void) {
    assert(!pxa_app_prefer_landscape());
    result=-3;assert(pxa_app_prefer_landscape()==-3);
    puts("PASS: optional landscape preference, no fixed angle and unsupported Host result");
}
