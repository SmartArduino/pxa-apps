// miniz 1.15 is public domain; its complete notice remains in vendor/miniz.c.
#include "vendor/miniz.c"
extern "C" bool carrot_inflate(const unsigned char* input,size_t bytes,
                                unsigned char* output,size_t size) {
  return tinfl_decompress_mem_to_mem(output,size,input,bytes,
                                    TINFL_FLAG_PARSE_ZLIB_HEADER)==size;
}
