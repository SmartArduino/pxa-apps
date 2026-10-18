#include "extMemory.h"
#include "vflash.h"
uint32_t currentExtAddress, extCacheLastBlock = ~0u;
const uint8_t *extCacheLastData;
uint8_t extMemAsynchByte;
int extMemInit(void) {
  vflash_init("/data/PAK0.PAK", EXT_FLASH_VIRTUAL_SIZE, 4 * 1024 * 1024);
  return 0;
}
void *extMemRead(uint32_t a, void *p, uint32_t n) {
  return vflash_read(a, p, n);
}
uint8_t extMemReadByteSlow(uint32_t a) {
  uint8_t b;
  vflash_read(a, &b, 1);
  return b;
}
void extMemErase(uint32_t a, uint32_t n) { vflash_write(a, 0, n, 1); }
void extMemProgram(uint32_t a, uint8_t *p, uint32_t n) {
  vflash_write(a, p, n, 0);
}
void extMemWrite(uint32_t a, void *p, uint32_t n) { vflash_write(a, p, n, 0); }
uint32_t extMemGetPakSize(void) { return vflash_data_size(); }
uint32_t extMemGetCacheMisses(void) { return 0; }
