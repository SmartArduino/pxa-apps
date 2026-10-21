/* Host side regression for the gnuboy Game Boy core. */
#include "core.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void log_sink(const char *line) { fprintf(stderr, "core: %s\n", line); }

static uint8_t *read_file(const char *path, size_t *size) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (length <= 0) {
        fclose(file);
        return NULL;
    }
    uint8_t *bytes = malloc((size_t) length);
    if (!bytes) {
        fclose(file);
        return NULL;
    }
    if (fread(bytes, 1, (size_t) length, file) != (size_t) length) {
        free(bytes);
        fclose(file);
        return NULL;
    }
    fclose(file);
    *size = (size_t) length;
    return bytes;
}

static uint32_t checksum(const uint16_t *pixels, size_t count) {
    uint32_t sum = 0x12345678u;
    for (size_t i = 0; i < count; ++i) {
        sum = (sum ^ pixels[i]) * 16777619u;
    }
    return sum;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <rom> [frames]\n", argv[0]);
        return 2;
    }
    int frames = argc > 2 ? atoi(argv[2]) : 120;
    gb_core_set_log_sink(log_sink);

    size_t size = 0;
    uint8_t *bytes = read_file(argv[1], &size);
    if (!bytes) {
        fprintf(stderr, "cannot read %s\n", argv[1]);
        return 2;
    }
    const char *error = gb_core_validate(bytes, size);
    if (error) {
        fprintf(stderr, "validation failed: %s\n", error);
        free(bytes);
        return 1;
    }
    error = gb_core_load(bytes, size);
    if (error) {
        fprintf(stderr, "load failed: %s\n", error);
        free(bytes);
        return 1;
    }
    printf("title=%s model=%u crc=%08x sram=%zu\n", gb_core_title(),
           gb_core_model(), gb_core_checksum(), gb_core_sram_size());

    struct timespec start, stop;
    clock_gettime(CLOCK_MONOTONIC, &start);
    size_t total_audio = 0;
    unsigned peak=0;
    uint32_t before = checksum(gb_core_video(), GB_VIDEO_WIDTH * GB_VIDEO_HEIGHT);
    uint32_t video_sum = before;
    for (int i = 0; i < frames; ++i) {
        gb_core_frame((uint16_t)(i & 0xFF));
        size_t samples = 0;
        const int16_t *pcm=gb_core_audio(&samples);
        for(size_t j=0;j<samples*2;++j){unsigned v=pcm[j]<0?-(int)pcm[j]:pcm[j];if(v>peak)peak=v;}
        total_audio += samples;
        video_sum = checksum(gb_core_video(), GB_VIDEO_WIDTH * GB_VIDEO_HEIGHT);
    }
    clock_gettime(CLOCK_MONOTONIC, &stop);
    double elapsed = (double)(stop.tv_sec - start.tv_sec) +
                     (double)(stop.tv_nsec - start.tv_nsec) / 1e9;
    double per_frame = elapsed / (double) frames;

    const uint8_t *sram = gb_core_sram();
    printf("frames=%d elapsed=%.3fs per_frame=%.2fms audio_frames=%zu "
           "video_sum=%08x sram=%02x%02x\n",
           frames, elapsed, per_frame * 1000.0, total_audio, video_sum,
           sram ? sram[0] : 0, sram ? sram[1] : 0);

    if (total_audio == 0 || peak<100) {
        fprintf(stderr, "FAIL: no audio produced\n");
        gb_core_close();
        free(bytes);
        return 1;
    }
    if (video_sum == before) {
        fprintf(stderr, "FAIL: video buffer never changed\n");
        gb_core_close();
        free(bytes);
        return 1;
    }
    if (!sram || sram[0] != 0x42 || sram[1] != 0x43) {
        fprintf(stderr, "FAIL: cartridge RAM marker missing\n");
        gb_core_close();
        free(bytes);
        return 1;
    }
    printf("PASS\n");
    gb_core_close();
    free(bytes);
    return 0;
}
