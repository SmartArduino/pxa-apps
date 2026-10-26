#include "extMemory.h"
#include "vflash.h"
#include "port.h"
#include "transient_nvm.h"
#include "duke3d.h"
#include <stddef.h>
/* These fields are engine scratch and a checkpoint used only after death in
 * this running game. Named user saves and settings remain in private storage. */
static uint8_t temporary_shot[sizeof(((nvmData_t*)0)->tempScreenshot)];
static uint8_t checkpoint_shot[sizeof(((nvmData_t*)0)->screenshots[0])];
static uint8_t checkpoint[SAVE_GAME_SIZE];
static uint8_t tile_pointers[sizeof(((nvmData_t*)0)->extMemTileDataPtrBackup)];
#define NVM_FIELD(field) (EXT_MEMORY_SIZE-sizeof(nvmData_t)+offsetof(nvmData_t,field))
static const duke_transient_region_t transient[]={
  {NVM_FIELD(tempScreenshot),sizeof(temporary_shot),temporary_shot},
  {NVM_FIELD(screenshots)+MAX_SAVEGAMES*sizeof(checkpoint_shot),sizeof(checkpoint_shot),checkpoint_shot},
  {NVM_FIELD(savegames)+MAX_SAVEGAMES*SAVE_GAME_SIZE,sizeof(checkpoint),checkpoint},
  {NVM_FIELD(extMemTileDataPtrBackup),sizeof(tile_pointers),tile_pointers},
};
_Static_assert(sizeof(nvmData_t)<=4u*1024u*1024u,"NVM layout exceeds reserved storage");
static uint32_t address;
static int memory_busy;
uint8_t *ext_memory = (uint8_t *)EXT_FLASH_BASE;
bool extMemInit(void) {
  vflash_init("/data/DUKE3D.GRP", EXT_MEMORY_SIZE, 4 * 1024 * 1024);
  duke_transient_init(EXT_MEMORY_SIZE,transient,sizeof(transient)/sizeof(*transient));
  port_log("PXA DUKE transient_bytes=%u user_save_slots=persistent",
      (unsigned)(sizeof(temporary_shot)+sizeof(checkpoint_shot)+sizeof(checkpoint)+sizeof(tile_pointers)));
  return true;
}
uint32_t extMemGetGrpSize(void) { return vflash_data_size(); }
void extMemSetCurrentAddress(uint32_t a) {
  address = a & EXT_MEMORY_ADDRESS_MASK;
}
void *extMemGetDataFromCurrentAddress(void *p, unsigned n) {
  duke_transient_read(address, p, n);
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
void extMemSetMemBusyBackgroundTask(void *t) {
  if(t && !memory_busy) {vflash_begin_batch();memory_busy=1;}
  if(!t && memory_busy) {vflash_end_batch();memory_busy=0;}
}
void extMemErase(uint32_t a, uint32_t n) { duke_transient_write(a, 0, n, 1); }
void extMemProgram(uint32_t a, uint8_t *p, uint32_t n) {
  duke_transient_write(a, p, n, 0);
}
void extMemWrite(uint32_t a, void *p, uint32_t n) { duke_transient_write(a, p, n, 0); }
void extMemEraseNvm(void) {
  duke_transient_write(EXT_MEMORY_SIZE - 4 * 1024 * 1024, 0, 4 * 1024 * 1024, 1);
}
