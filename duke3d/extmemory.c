#include "extMemory.h"
#include "vflash.h"
static uint32_t address;
uint8_t *ext_memory = (uint8_t *)EXT_FLASH_BASE;
bool extMemInit(void) {
  vflash_init("/data/DUKE3D.GRP", EXT_MEMORY_SIZE, 4 * 1024 * 1024);
  return true;
}
uint32_t extMemGetGrpSize(void) { return vflash_data_size(); }
void extMemSetCurrentAddress(uint32_t a) {
  address = a & EXT_MEMORY_ADDRESS_MASK;
}
void *extMemGetDataFromCurrentAddress(void *p, unsigned n) {
  vflash_read(address, p, n);
  address += n;
  return p;
}
void *extMemGetDataFromAddress(void *p, const void *s, unsigned n) {
  extMemSetCurrentAddress((uint32_t)s);
  return extMemGetDataFromCurrentAddress(p, n);
}
void *extMemGetDataAligned(void *p, unsigned n) {
  address = (address + 3) & ~3u;
  return extMemGetDataFromCurrentAddress(p, n);
}
uint8_t extMemGetByteFromAddress(const void *s) {
  uint8_t b;
  extMemGetDataFromAddress(&b, s, 1);
  return b;
}
short extMemFlashGetShortFromAddress(const void *s) {
  short b;
  extMemGetDataFromAddress(&b, s, 2);
  return b;
}
void *extMemStartAsynchDataRead(uint32_t a, void *p, uint32_t n) {
  unsigned align = a & 3;
  extMemGetDataFromAddress(p, (void *)(a & ~3u), (align + n + 3) & ~3u);
  address = a + n;
  return (uint8_t *)p + align;
}
void extMemSetMemBusyBackgroundTask(void *t) { (void)t; }
void extMemErase(uint32_t a, uint32_t n) { vflash_write(a, 0, n, 1); }
void extMemProgram(uint32_t a, uint8_t *p, uint32_t n) {
  vflash_write(a, p, n, 0);
}
void extMemWrite(uint32_t a, void *p, uint32_t n) { vflash_write(a, p, n, 0); }
void extMemEraseNvm(void) {
  vflash_write(EXT_MEMORY_SIZE - 4 * 1024 * 1024, 0, 4 * 1024 * 1024, 1);
}
