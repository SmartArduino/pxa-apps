// SPDX-License-Identifier: MIT
#include <string.h>
#include "vendor/zlib/zutil.h"

/* Z_SOLO delegates memory operations to the embedding application. */
void zmemcpy(Bytef *destination, const Bytef *source, uInt length) {
    memcpy(destination, source, length);
}
int zmemcmp(const Bytef *left, const Bytef *right, uInt length) {
    return memcmp(left, right, length);
}
void zmemzero(Bytef *destination, uInt length) {
    memset(destination, 0, length);
}
