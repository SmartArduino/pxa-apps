/* GPL-2.0-or-later. Mapper 241: a 32 KiB PRG register, fixed CHR.
 * Register behavior cross-checked with Mesen Core/Mapper241.h:
 * https://github.com/SourMesen/Mesen/blob/master/Core/Mapper241.h
 */
#include "nes/nes.h"

static uint8 prg_bank;
static void map_write(uint32 address, uint8 value)
{
    (void)address;
    prg_bank = value;
    mmc_bankrom(32, 0x8000, prg_bank);
}
static void map_init(rom_t *cart)
{
    (void)cart;
    map_write(0x8000, 0);
    mmc_bankvrom(8, 0x0000, 0);
}
static void map_getstate(uint8 *state) { state[0] = prg_bank; }
static void map_setstate(uint8 *state) { map_write(0x8000, state[0]); }

mapintf_t map241_intf = {
    .number = 241,
    .name = "Mapper 241",
    .init = map_init,
    .get_state = map_getstate,
    .set_state = map_setstate,
    .mem_write = {{0x8000, 0xFFFF, map_write}},
};
