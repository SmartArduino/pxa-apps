/* Dumps one emulated frame as a PPM so the video path can be checked on the
 * host without a display. */
#include "core.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <rom> [frames] [out.ppm]\n", argv[0]);
        return 2;
    }
    int frames = argc > 2 ? atoi(argv[2]) : 60;
    const char *out = argc > 3 ? argv[3] : "/tmp/opencode/host_frame.ppm";
    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        fprintf(stderr, "cannot read %s\n", argv[1]);
        return 2;
    }
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    uint8_t *rom = malloc((size_t) length);
    if (!rom || fread(rom, 1, (size_t) length, file) != (size_t) length) {
        fprintf(stderr, "cannot read %s\n", argv[1]);
        return 2;
    }
    fclose(file);
    const char *error = gb_core_load(rom, (size_t) length);
    if (error) {
        fprintf(stderr, "load failed: %s\n", error);
        return 1;
    }
    for (int i = 0; i < frames; ++i) gb_core_frame(0);
    const uint16_t *video = gb_core_video();
    printf("row  0:");
    for (int i = 0; i < 8; ++i) printf(" %04x", video[i]);
    printf("\nrow  1:");
    for (int i = 0; i < 8; ++i) printf(" %04x", video[160 + i]);
    printf("\nrow 72:");
    for (int i = 0; i < 8; ++i) printf(" %04x", video[72 * 160 + i]);
    printf("\n");
    FILE *ppm = fopen(out, "wb");
    if (!ppm) return 2;
    fprintf(ppm, "P6\n%d %d\n255\n", GB_VIDEO_WIDTH, GB_VIDEO_HEIGHT);
    for (unsigned i = 0; i < GB_VIDEO_WIDTH * GB_VIDEO_HEIGHT; ++i) {
        unsigned r = (video[i] >> 11) & 0x1Fu;
        unsigned g = (video[i] >> 5) & 0x3Fu;
        unsigned b = video[i] & 0x1Fu;
        fputc((int) (r * 255 / 31), ppm);
        fputc((int) (g * 255 / 63), ppm);
        fputc((int) (b * 255 / 31), ppm);
    }
    fclose(ppm);
    printf("wrote %s\n", out);
    return 0;
}
