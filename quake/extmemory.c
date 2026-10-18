#include "extMemory.h"
#include "vflash.h"
#include "port.h"
uint32_t currentExtAddress, extCacheLastBlock = ~0u;
const uint8_t *extCacheLastData;
uint8_t extMemAsynchByte;
unsigned extMemByteReads;
/* An independent immutable page keeps the engine's inline byte path valid
 * even when an unrelated bulk read evicts the corresponding vflash slot. */
static uint8_t byte_page[EXT_CACHE_BLOCK_SIZE];
void extMemPollBytes(void) {
#ifdef PORT_VFLASH_COOPERATE
  if (port_cooperate_due(port_time_us(), PORT_COOPERATE_BUDGET_US))
    port_sleep(0);
#endif
}
int extMemInit(void) {
  extCacheLastBlock = ~0u;
  extMemByteReads = 0;
  vflash_init("/data/PAK0.PAK", EXT_FLASH_VIRTUAL_SIZE, 4 * 1024 * 1024);
  return 0;
}
void *extMemRead(uint32_t a, void *p, uint32_t n) {
  return vflash_read(a, p, n);
}
uint8_t extMemReadByteSlow(uint32_t a) {
  a &= SPI_ADDRESS_MASK;
  uint32_t offset = a - PAK_FILE_OFFSET;
  uint32_t start = offset & ~(EXT_CACHE_BLOCK_SIZE - 1u);
  uint32_t size = vflash_data_size();
  if (a >= PAK_FILE_OFFSET && start <= size &&
      EXT_CACHE_BLOCK_SIZE <= size - start) {
    vflash_read(start + PAK_FILE_OFFSET, byte_page, sizeof(byte_page));
    extCacheLastData = byte_page;
    extCacheLastBlock = start / EXT_CACHE_BLOCK_SIZE;
    return byte_page[offset - start];
  }
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
