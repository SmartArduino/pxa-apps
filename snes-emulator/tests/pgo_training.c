/* Native-only profile collection. Reads ROMs, never writes emulator saves.
 * GPL-2.0-or-later. Input is deterministic so profiles can be reproduced. */
#include "core.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

static unsigned positive(const char *text)
{
    if (*text < '1' || *text > '9') return 0;
    char *end;
    errno = 0;
    unsigned long value = strtoul(text, &end, 10);
    return errno || !*text || *end || value == 0 || value >= UINT_MAX
        ? 0 : (unsigned)value;
}

static uint16_t training_pad(unsigned frame)
{
    static const unsigned starts[] = {900, 1200, 1380};
    for (unsigned i = 0; i < sizeof(starts) / sizeof(starts[0]); ++i)
        if (frame >= starts[i] && frame < starts[i] + 6)
            return EMU_PAD_START;
    if (frame >= 1620 && frame < 1626) return EMU_PAD_B;
    if (frame >= 1860 && frame < 1866) return EMU_PAD_A;
    if (frame >= 2460 && frame < 2466) return EMU_PAD_RIGHT;
    if (frame >= 2580 && frame < 2586) return EMU_PAD_B;
    if (frame < 2820) return 0;
    return EMU_PAD_RIGHT | (frame % 61 < 10 ? EMU_PAD_B : 0)
        | (frame % 503 < 6 ? EMU_PAD_A : 0);
}

int main(int argc, char **argv)
{
    if (argc < 2 || argc > 4) {
        fprintf(stderr, "Usage: %s ROM [frames=12000] [draw_interval=3]\n", argv[0]);
        return 2;
    }
    unsigned frames = argc > 2 ? positive(argv[2]) : 12000;
    unsigned interval = argc > 3 ? positive(argv[3]) : 3;
    if (!frames || !interval) return 2;
    FILE *file = fopen(argv[1], "rb");
    if (!file) { perror(argv[1]); return 2; }
    if (fseek(file, 0, SEEK_END)) { fclose(file); return 2; }
    long length = ftell(file);
    if (length <= 0 || length > 8 * 1024 * 1024 + 512) {
        fclose(file);
        return 2;
    }
    rewind(file);
    uint8_t *rom = calloc(1, EMU_ROM_CAPACITY((size_t)length));
    if (!rom || fread(rom, 1, (size_t)length, file) != (size_t)length) {
        fclose(file); free(rom); return 2;
    }
    fclose(file);
    const char *error = emu_core_load(rom, (size_t)length);
    if (error) {
        fprintf(stderr, "%s\n", error);
        emu_core_close(); free(rom); return 1;
    }
    uint32_t pcm_crc = 0, video_crc = 0;
    uint64_t samples = 0;
    for (unsigned frame = 0; frame < frames; ++frame) {
        emu_core_frame_render(training_pad(frame), 0,
                              frame % interval == 0 || frame + 1 == frames);
        size_t count;
        const int16_t *pcm = emu_core_audio(&count);
        samples += count;
        pcm_crc = emu_port_crc32(pcm_crc, pcm, count * 2 * sizeof(*pcm));
    }
    const uint16_t *pixels = emu_core_video();
    for (unsigned y = 0; y < EMU_VIDEO_HEIGHT; ++y)
        video_crc = emu_port_crc32(video_crc, pixels + y * emu_core_info.stride,
                                  EMU_VIDEO_WIDTH * sizeof(*pixels));
    printf("{\"frames\":%u,\"samples\":%llu,\"pcm_crc\":\"%08x\","
           "\"video_crc\":\"%08x\"}\n", frames,
           (unsigned long long)samples, pcm_crc, video_crc);
    emu_core_close(); free(rom);
    return 0;
}
