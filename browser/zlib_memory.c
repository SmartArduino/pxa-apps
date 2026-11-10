#include <string.h>
#include "vendor/zlib/zutil.h"

/* Z_SOLO leaves memory operations to the embedding application. */
void zmemcpy(void* dest,const void* source,z_size_t length){memcpy(dest,source,length);}
int zmemcmp(const void* a,const void* b,z_size_t length){return memcmp(a,b,length);}
void zmemzero(void* dest,z_size_t length){memset(dest,0,length);}
