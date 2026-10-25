/* GPL-2.0-or-later. Unaligned/tail signatures and deterministic precedence. */
#include "core.h"
#include "common.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void) {
    const char *signatures[]={"SRAM_V","EEPROM_V","FLASH_V","FLASH512_V","FLASH1M_V"};
    const unsigned types[]={BACKUP_SRAM,BACKUP_EEPROM,BACKUP_FLASH,BACKUP_FLASH,BACKUP_FLASH};
    u8 *rom=calloc(1,32768);assert(rom);
    for(unsigned sig=0;sig<5;++sig)for(unsigned tail=0;tail<2;++tail) {
        memset(rom,0,32768);memcpy(rom+0xac,"ZZZZ",4);
        size_t n=strlen(signatures[sig]),offset=tail?32768-n:0x123;
        memcpy(rom+offset,signatures[sig],n);
        assert(!emu_core_load(rom,32768));assert(backup_type==types[sig]);
        if(sig==4)assert(flash_bank_cnt==FLASH_SIZE_128KB);
        emu_core_close();
    }
    memset(rom,0,32768);memcpy(rom+0xac,"ZZZZ",4);
    for(unsigned sig=0;sig<5;++sig)memcpy(rom+0x123+sig*16,signatures[sig],strlen(signatures[sig]));
    assert(!emu_core_load(rom,32768));assert(backup_type==BACKUP_EEPROM);emu_core_close();
    memset(rom+0x123,0,80);memcpy(rom+32763,"FLASH",5);
    assert(!emu_core_load(rom,32768));assert(backup_type==BACKUP_UNKN);emu_core_close();
    free(rom);puts("PASS: backup signatures at unaligned/tail positions, precedence and truncated strings");
}
