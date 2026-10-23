/* Differential synchronization test: compare observable SPC state against
 * eager execution, including sleeping at an intermediate CPU boundary. */
#include "core.h"
#include "vendor/snes9x/snes9x.h"
#include "apu_sync.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned rng;
static unsigned next(void) {
    rng = rng * 1664525u + 1013904223u;
    return rng;
}
static uint32_t snapshot(uint32_t crc) {
    crc = emu_port_crc32(crc, &APU, sizeof(APU));
    crc = emu_port_crc32(crc, &IAPU, sizeof(IAPU));
    return emu_port_crc32(crc, IAPU.RAM, 65536);
}
static uint32_t run(const uint8_t *program, size_t bytes, unsigned seed, int deferred) {
    uint8_t *ram = IAPU.RAM;
    int one = IAPU.OneCycle;
    memset(&APU, 0, sizeof(APU));
    memset(&IAPU, 0, sizeof(IAPU));
    IAPU.RAM = ram;
    IAPU.OneCycle = one;
    snes_apu_count = 0;
    S9xResetAPU();
    Settings.Shutdown = true;
    ICPU.CPUExecuting = true;
    CPU.Cycles = 0;
    IAPU.PC = IAPU.RAM + 0x200;
    memcpy(IAPU.PC, program, bytes);
    IAPU.RAM[0xfd] = 0;
    IAPU.RAM[0xf4] = 0;
    rng = seed;
    uint32_t crc = 0;
    for (unsigned i = 0; i < 900; i++) {
        if (deferred)
            SNES_APU_DEFER();
        else if (IAPU.APUExecuting && APU.Cycles <= CPU.Cycles)
            APUExecute(true, &CPU.Cycles);
        CPU.Cycles += 3;
        if (seed <= 16 && i % 37 == 0) {
            S9xAPUWritePort(0x2140, (uint8_t)(next() & 3));
            unsigned v = S9xAPUReadPort(0x2141);
            crc = emu_port_crc32(crc, &v, sizeof(v));
            crc = snapshot(crc);
        }
        if (seed <= 16 && i % 97 == 0) {
            snes_apu_flush();
            crc = snapshot(crc);
            if (IAPU.APUExecuting)
                APU.Cycles -= CPU.Cycles;
            else
                APU.Cycles = 0;
            CPU.Cycles = 0;
            IAPU.RAM[0xfd] = (next() & 7);
            IAPU.APUExecuting = Settings.APUEnabled;
        }
        CPU.Cycles += 6 + (next() % (seed <= 16 ? 27 : 127));
    }
    snes_apu_flush();
    return snapshot(crc);
}
/* Shutdown must keep the old do/while semantics: always one instruction,
 * then continue only while strictly before CPU.NextEvent, including an idle
 * hint that moves both CPUs to the event and SLEEP/STOP mid-batch. */
static uint32_t shutdown_run(const uint8_t *program, long event, long start,
                             unsigned seed, int batched) {
    uint8_t *ram = IAPU.RAM;
    int one = IAPU.OneCycle;
    memset(&APU, 0, sizeof(APU));
    memset(&IAPU, 0, sizeof(IAPU));
    IAPU.RAM = ram; IAPU.OneCycle = one;
    snes_apu_reset();
    srand(1);
    S9xResetAPU();
    Settings.Shutdown = true;
    ICPU.CPUExecuting = false;
    CPU.Cycles = CPU.NextEvent = event;
    APU.Cycles = start;
    IAPU.PC = IAPU.RAM + 0x200;
    memcpy(IAPU.PC, program, 8);
    IAPU.RAM[0xfd] = seed & 1;
    IAPU.RAM[0xf4] = seed & 1;
    IAPU.WaitAddress1 = IAPU.PC;
    IAPU.WaitAddress2 = IAPU.PC + 2;
    IAPU.WaitCounter = seed & 3;
    if (batched)
        APUExecuteUntilEvent();
    else {
        do { APUExecute(false, NULL); } while (APU.Cycles < CPU.NextEvent);
    }
    uint32_t crc = snapshot(0);
    crc = emu_port_crc32(crc, &CPU.Cycles, sizeof(CPU.Cycles));
    crc = emu_port_crc32(crc, &CPU.NextEvent, sizeof(CPU.NextEvent));
    return emu_port_crc32(crc, &Settings.APUEnabled, sizeof(Settings.APUEnabled));
}

int main(int argc, char **argv) {
    assert(argc == 2);
    FILE *f = fopen(argv[1], "rb");
    assert(f);
    fseek(f, 0, SEEK_END);
    size_t n = ftell(f);
    rewind(f);
    uint8_t *r = calloc(1, EMU_ROM_CAPACITY(n));
    assert(r && fread(r, 1, n, f) == n);
    fclose(f);
    assert(!emu_core_load(r, n));
    const uint8_t programs[][8] = {{0xe8, 0x7f, 0xbc, 0x2f, 0xfd},
                                   {0xe4, 0xfd, 0xf0, 0xfc, 0x2f, 0xfa},
                                   {0xe4, 0xf4, 0xf0, 0xfc, 0xc4, 0xf5, 0x2f, 0xf8},
                                   {0xef},
                                   {0xff}};
    for (unsigned p = 0; p < 5; p++)
        for (unsigned seed = 1; seed <= 32; seed++) {
            srand(1);
            uint32_t eager = run(programs[p], 8, seed, 0);
            srand(1);
            uint32_t batched = run(programs[p], 8, seed, 1);
            if (eager != batched) {
                fprintf(stderr, "program=%u seed=%u eager=%08x batched=%08x\n", p, seed, eager,
                        batched);
                return 1;
            }
        }
    const long events[] = {-1, 0, 1, 41, 42, 43, 84, 711, 1364, LONG_MIN};
    unsigned schedules = 0;
    for (unsigned p = 0; p < 5; ++p)
        for (unsigned e = 0; e < sizeof(events) / sizeof(events[0]); ++e)
            for (unsigned offset = 0; offset < 4; ++offset)
                for (unsigned seed = 0; seed < 8; ++seed) {
                    long event = events[e];
                    long start = event == LONG_MIN || !offset ? 0 : event + ((long)offset - 2) * 42;
                    uint32_t eager = shutdown_run(programs[p], event, start, seed, 0);
                    uint32_t batch = shutdown_run(programs[p], event, start, seed, 1);
                    if (eager != batch) {
                        fprintf(stderr, "shutdown program=%u event=%ld start=%ld seed=%u %08x != %08x\n",
                                p, event, start, seed, eager, batch);
                        return 1;
                    }
                    ++schedules;
                }
    printf("%u shutdown schedules match strict event boundaries\n", schedules);
    emu_core_close();
    free(r);
    puts("160 SPC schedules match: ports, timer wakeups, bounded queue, idle, SLEEP and STOP");
    return 0;
}
