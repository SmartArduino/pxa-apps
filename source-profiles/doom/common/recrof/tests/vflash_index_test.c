#include "vflash.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PAGE 8192u
#define CAPACITY PORT_VFLASH_CACHE_PAGES
#define STRIDE (CAPACITY * 2u + 1u)
#define ROM_PAGES (STRIDE * 6u)
static char directory[] = "/tmp/pxa-rom-index-XXXXXX";
static unsigned order[CAPACITY], count;
static uint64_t hits, misses;
const char *port_data_path(void) { return directory; }
uint64_t port_time_us(void) { static uint64_t time; return ++time; }
void port_error(const char *message) { fprintf(stderr, "%s\n", message); abort(); }
int port_replace(const char *from, const char *to) { return rename(from, to); }
static uint8_t pixel(unsigned page, unsigned offset) {
    return (uint8_t)(page * 43 + offset * 17 + (offset >> 8));
}
/* Independent MRU list: verifies the original cache capacity and LRU policy,
 * including hits in long collision chains and replacement of their members. */
static void reference(unsigned page) {
    unsigned at = 0;
    while (at < count && order[at] != page) ++at;
    if (at == count) {
        ++misses;
        if (count < CAPACITY) ++count;
        at = count - 1;
    } else ++hits;
    while (at) { order[at] = order[at - 1]; --at; }
    order[0] = page;
}
static void read_range(unsigned offset, unsigned bytes) {
    uint8_t output[256];
    assert(bytes && bytes <= sizeof(output));
    unsigned remaining = bytes, cursor = offset;
    while (remaining) {
        unsigned n = PAGE - cursor % PAGE;
        if (n > remaining) n = remaining;
        reference(cursor / PAGE);
        cursor += n; remaining -= n;
    }
    vflash_read(offset + 4, output, bytes);
    for (unsigned i = 0; i < bytes; ++i)
        assert(output[i] == pixel((offset + i) / PAGE, (offset + i) % PAGE));
}
int main(void) {
    assert(mkdtemp(directory));
    char archive[256], save[256];
    snprintf(archive, sizeof(archive), "%s/archive", directory);
    snprintf(save, sizeof(save), "%s/state.nvm", directory);
    FILE *file = fopen(archive, "wb"); assert(file);
    uint8_t page[PAGE];
    for (unsigned tag = 0; tag < ROM_PAGES; ++tag) {
        for (unsigned i = 0; i < PAGE; ++i) page[i] = pixel(tag, i);
        assert(fwrite(page, 1, PAGE, file) == PAGE);
    }
    assert(!fclose(file));
    uint32_t random = 0x519b21;
    for (unsigned run = 0; run < 2; ++run) {
        vflash_init(archive, 32u * 1024 * 1024, 4u * PAGE);
        vflash_stats(NULL, 1); count = 0; hits = misses = 0;
        for (unsigned i = 0; i < 5000; ++i) {
            random ^= random << 13; random ^= random >> 17; random ^= random << 5;
            unsigned tag = i < 1000 ? (i % (CAPACITY + 2u)) * STRIDE : random % ROM_PAGES;
            /* Wrap deliberately colliding tags into the valid archive. */
            tag %= ROM_PAGES;
            unsigned offset = tag * PAGE + (i % 2 ? PAGE - 32 : random % PAGE);
            unsigned bytes = 1 + (random >> 16) % 193;
            if (bytes > ROM_PAGES * PAGE - offset) bytes = ROM_PAGES * PAGE - offset;
            read_range(offset, bytes);
            read_range(offset, 1);
        }
        vflash_stats_t actual; vflash_stats(&actual, 0);
        assert(actual.hits == hits && actual.misses == misses);
        assert(actual.cache_bytes == CAPACITY * PAGE);
    }
    assert(!unlink(archive) && !unlink(save) && !rmdir(directory));
    puts("ROM index, LRU, collisions, eviction, cross-page and reopen checks passed");
}
