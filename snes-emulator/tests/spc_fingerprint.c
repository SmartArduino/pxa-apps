/* Differential probe for SPC interpreter refactors. Link this same source
 * against the reference and candidate core, then compare stdout exactly.
 * The ROM is only used to initialize the adapter; no game or save is written. */
#include "core.h"
#include "vendor/snes9x/snes9x.h"
#include "apu_sync.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t word(uint32_t crc, uint32_t value) {
    const uint8_t bytes[4] = {value, value >> 8, value >> 16, value >> 24};
    return emu_port_crc32(crc, bytes, sizeof(bytes));
}

static uint32_t address(const uint8_t *p) {
    return p ? (uint32_t)(p - IAPU.RAM) + 1 : 0;
}

static uint32_t state(uint32_t crc) {
    const uint32_t fields[] = {
        IAPU.Registers.P, IAPU.Registers.YA.W, IAPU.Registers.X,
        IAPU.Registers.S, IAPU.Registers.PC, address(IAPU.PC),
        address(IAPU.DirectPage), IAPU.APUExecuting, IAPU.Bit, IAPU.Address,
        address(IAPU.WaitAddress1), address(IAPU.WaitAddress2),
        IAPU.WaitCounter, IAPU._Carry, IAPU._Zero, IAPU._Overflow,
        APU.Cycles, APU.ShowROM, APU.KeyedChannels, CPU.Cycles,
        CPU.NextEvent, Settings.APUEnabled
    };
    for (unsigned i = 0; i < sizeof(fields) / sizeof(fields[0]); ++i)
        crc = word(crc, fields[i]);
    for (unsigned i = 0; i < 3; ++i) {
        crc = word(crc, APU.Timer[i]);
        crc = word(crc, APU.TimerTarget[i]);
        crc = word(crc, APU.TimerEnabled[i]);
    }
    crc = emu_port_crc32(crc, APU.OutPorts, sizeof(APU.OutPorts));
    crc = emu_port_crc32(crc, APU.DSP, sizeof(APU.DSP));
    crc = emu_port_crc32(crc, APU.ExtraRAM, sizeof(APU.ExtraRAM));
    return emu_port_crc32(crc, IAPU.RAM, 65536);
}

int main(int argc, char **argv) {
    assert(argc == 2);
    FILE *file = fopen(argv[1], "rb");
    assert(file && !fseek(file, 0, SEEK_END));
    long length = ftell(file);
    assert(length > 0);
    rewind(file);
    uint8_t *rom = calloc(1, EMU_ROM_CAPACITY((size_t)length));
    assert(rom && fread(rom, 1, (size_t)length, file) == (size_t)length);
    fclose(file);
    assert(!emu_core_load(rom, (size_t)length));
    static const uint8_t edge[] = {0, 1, 0x7f, 0x80, 0xff, 0xf0, 0xf3, 0xfd};
    for (unsigned opcode = 0; opcode < 256; ++opcode) {
        uint32_t crc = 0;
        for (unsigned seed = 0; seed < 32; ++seed) {
            S9xResetAPU();
            Settings.Shutdown = true;
            ICPU.CPUExecuting = (seed & 2) != 0;
            CPU.Cycles = 19; CPU.NextEvent = 711;
            IAPU.Address = 0x5678; IAPU.Bit = 3;
            for (unsigned i = 0; i < 256; ++i)
                IAPU.RAM[i] = (uint8_t)(i * 13 + seed * 29);
            for (unsigned i = 1; i < 256; ++i)
                memcpy(IAPU.RAM + i * 256, IAPU.RAM, 256);
            IAPU.PC = IAPU.RAM + 0x200;
            IAPU.PC[0] = opcode;
            IAPU.PC[1] = edge[seed & 7];
            IAPU.PC[2] = seed & 8 ? 0 : edge[(seed + 3) & 7];
            IAPU.Registers.P = (uint8_t)(seed * 37);
            IAPU.Registers.YA.B.A = edge[seed & 7];
            IAPU.Registers.YA.B.Y = edge[(seed + 2) & 7];
            IAPU.Registers.X = edge[(seed + 1) & 7];
            IAPU.Registers.S = edge[(seed + 4) & 7];
            IAPU.DirectPage = IAPU.RAM + (IAPU.Registers.P & 32 ? 256 : 0);
            IAPU.WaitAddress1 = IAPU.PC;
            IAPU.WaitAddress2 = IAPU.PC + 2;
            IAPU.WaitCounter = seed & 3;
            S9xAPUUnpackStatus();
            APUExecute(false, NULL);
            crc = state(crc);
        }
        printf("%02x %08x\n", opcode, crc);
    }
    emu_core_close(); free(rom);
    return 0;
}
