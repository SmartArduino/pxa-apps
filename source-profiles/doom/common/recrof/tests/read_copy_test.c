/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "port.h"
static uint8_t source[16400], original[16400], actual[16400], expected[16400];
static uint32_t random_state=61821;
static uint32_t random32(void) { random_state^=random_state<<13;random_state^=random_state>>17;random_state^=random_state<<5;return random_state; }
int main(void) {
    for(unsigned i=0;i<sizeof(source);++i)source[i]=random32();
    memcpy(original,source,sizeof(source));
    for(unsigned i=0;i<2048;++i) {
        size_t bytes=i<260?i:random32()%8193;
        size_t from=random32()%8,to=random32()%8;
        memset(actual,0xa5,sizeof(actual));memset(expected,0xa5,sizeof(expected));
        memcpy(expected+to,source+from,bytes);
        port_copy_read(actual+to,source+from,bytes);
        assert(!memcmp(actual,expected,sizeof(actual)));
        assert(!memcmp(source,original,sizeof(source)));
    }
    puts("PASS: 2048 file/cache copies; threshold, all byte alignments, zero/small/large lengths, immutable source and complete destination guards");
    return 0;
}
