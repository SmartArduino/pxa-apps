/* Deterministic per-frame comparison and draw/skip timing for arbitrary ROMs.
 * Signatures include CPU, RAM, PPU state, video and PCM, not wall-clock time. */
#include "core.h"
#include "nes/nes.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void nes_port_log(const char *format, ...) { (void)format; }
int main(int argc, char **argv)
{
    if (argc < 2) return 2;
    FILE *file = fopen(argv[1], "rb");
    if (!file) return 2;
    fseek(file, 0, SEEK_END); size_t size = ftell(file); rewind(file);
    uint8_t *rom = malloc(size);
    if (!rom || fread(rom, 1, size, file) != size) return 2;
    fclose(file);
    const char *error = nes_core_load(rom, size);
    if (error) { fprintf(stderr, "%s\n", error); return 2; }
    unsigned frames = argc > 2 ? atoi(argv[2]) : 1200;
    unsigned interval = argc > 3 ? atoi(argv[3]) : 1;
    if (!interval) return 2;
    uint64_t draw_us = 0, skip_us = 0;
    unsigned draws = 0, skips = 0;
    for (unsigned frame = 0; frame < frames; ++frame)
    {
        uint8_t pad = 0;
        if (frame == 120 || frame == 300) pad = 8; /* START */
        if (frame >= 360) pad = 0x80 | ((frame % 63) < 12 ? 1 : 0);
        bool draw = frame % interval == 0;
        uint64_t began = nes_port_time();
        nes_core_frame(draw, pad, 0);
        uint64_t elapsed = nes_port_time() - began;
        if (draw) { draw_us += elapsed; ++draws; }
        else { skip_us += elapsed; ++skips; }
        nes6502_t cpu; nes6502_getcontext(&cpu);
        ppu_t *p = nes_getptr()->ppu;
        uint32_t state[] = {cpu.pc_reg, cpu.a_reg, cpu.x_reg, cpu.y_reg,
            cpu.s_reg, cpu.p_reg, (uint32_t)cpu.total_cycles, p->ctrl0,
            p->ctrl1, p->stat, p->vaddr, p->vaddr_latch, p->tile_xofs,
            p->strikeflag, p->strike_cycle, p->oam_addr, p->left_bg_counter};
        uint32_t crc = nes_port_crc32(0, state, sizeof(state));
        crc = nes_port_crc32(crc, nes_getptr()->mem->ram, 2048);
        rom_t *cart = nes_getptr()->cart;
        crc = nes_port_crc32(crc, cart->prg_ram, cart->prg_ram_banks * 8192u);
        crc = nes_port_crc32(crc, p->oam, sizeof(p->oam));
        crc = nes_port_crc32(crc, p->palette, sizeof(p->palette));
        crc = nes_port_crc32(crc, p->nametab, 4096);
        crc = nes_port_crc32(crc, nes_core_video() - 8, NES_SCREEN_PITCH * 240);
        size_t samples; const int16_t *pcm = nes_core_audio(&samples);
        crc = nes_port_crc32(crc, pcm, samples * sizeof(*pcm));
        printf("%u %08x\n", frame, crc);
    }
    fprintf(stderr, "frames=%u draws=%u draw_us=%llu skips=%u skip_us=%llu\n",
        frames, draws, (unsigned long long)draw_us, skips, (unsigned long long)skip_us);
    nes_core_close(); free(rom); return 0;
}
