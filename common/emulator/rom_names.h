/* GPL-2.0-or-later. Menu-only, owned UTF-8 names; no per-name or row limit. */
#ifndef EMU_ROM_NAMES_H
#define EMU_ROM_NAMES_H
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static inline void emu_rom_names_clear(char ***items, unsigned *count,
                                       unsigned *capacity) {
    for (unsigned i = 0; i < *count; ++i) free((*items)[i]);
    free(*items);
    *items = NULL;
    *count = *capacity = 0;
}

/* On failure the existing list remains valid and unchanged. */
static inline int emu_rom_names_append(char ***items, unsigned *count,
    unsigned *capacity, const void *name, size_t bytes) {
    if (!name || !bytes || bytes == SIZE_MAX || *count == UINT_MAX) return 0;
    char *copy = malloc(bytes + 1);
    if (!copy) return 0;
    memcpy(copy, name, bytes);
    copy[bytes] = 0;
    if (*count == *capacity) {
        unsigned next = *capacity ? *capacity + *capacity / 2 + 1 : 16;
        if (next <= *capacity || (size_t)next > SIZE_MAX / sizeof(char *)) {
            free(copy); return 0;
        }
        char **grown = realloc(*items, (size_t)next * sizeof(*grown));
        if (!grown) { free(copy); return 0; }
        *items = grown;
        *capacity = next;
    }
    (*items)[(*count)++] = copy;
    return 1;
}

static inline int emu_rom_name_compare(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}
static inline void emu_rom_names_sort(char **items, unsigned count) {
    if (count > 1) qsort(items, count, sizeof(*items), emu_rom_name_compare);
}

/* Flash keeps its legacy roms/ namespace. SD opens the name relative to its
 * directory capability. Neither path is truncated into a fixed local array. */
static inline int emu_rom_open(uint64_t token, uint64_t media, int sd,
                               const char *name) {
    size_t bytes = strlen(name);
    if (sd) return pxa_fs_request_open_at(token, media, name, bytes, PXA_FS_OPEN_READ);
    if (bytes > SIZE_MAX - 6) return -1;
    char *path = malloc(bytes + 6);
    if (!path) return -1;
    memcpy(path, "roms/", 5);
    memcpy(path + 5, name, bytes + 1);
    int result = pxa_fs_request_open(token, path, bytes + 5, PXA_FS_OPEN_READ);
    free(path);
    return result;
}
#endif
