/* The Guest is linked with -nostdlib, so the handful of libc symbols the
 * compiler may still emit (struct copies and bulk zeroing) are provided here.
 * -fno-builtin keeps clang from turning these bodies into recursive calls. */
#include <stddef.h>

void *memcpy(void *destination, const void *source, size_t count) {
    unsigned char *out = (unsigned char *)destination;
    const unsigned char *in = (const unsigned char *)source;
    size_t index;
    for (index = 0; index < count; ++index) out[index] = in[index];
    return destination;
}

void *memmove(void *destination, const void *source, size_t count) {
    unsigned char *out = (unsigned char *)destination;
    const unsigned char *in = (const unsigned char *)source;
    if (out == in || count == 0) return destination;
    if (out < in) {
        size_t index;
        for (index = 0; index < count; ++index) out[index] = in[index];
    } else {
        size_t index = count;
        while (index != 0) {
            --index;
            out[index] = in[index];
        }
    }
    return destination;
}

void *memset(void *destination, int value, size_t count) {
    unsigned char *out = (unsigned char *)destination;
    size_t index;
    for (index = 0; index < count; ++index) out[index] = (unsigned char)value;
    return destination;
}

int memcmp(const void *left, const void *right, size_t count) {
    const unsigned char *a = (const unsigned char *)left;
    const unsigned char *b = (const unsigned char *)right;
    size_t index;
    for (index = 0; index < count; ++index) {
        if (a[index] != b[index]) return (int)a[index] - (int)b[index];
    }
    return 0;
}
