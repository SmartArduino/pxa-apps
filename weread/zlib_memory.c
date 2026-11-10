#include <string.h>
#include "vendor/zlib/zutil.h"

/* Z_SOLO leaves memory operations to the embedding application. */
void zmemcpy(Bytef* dest,const Bytef* source,uInt length){memcpy(dest,source,length);}
int zmemcmp(const Bytef* a,const Bytef* b,uInt length){return memcmp(a,b,length);}
void zmemzero(Bytef* dest,uInt length){memset(dest,0,length);}
