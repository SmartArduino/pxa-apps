/* Optional desktop diagnostic, linked against the simulator's WAMR library.
 * Counts allocator requests while loading, optionally instantiating Wasm.
 * No application entry point is called. Input storage, allocator overhead,
 * memory obtained outside this allocator (including mmap) and UI are separate.
 */
#include "wasm_export.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef union { max_align_t alignment; size_t bytes; } Header;
static size_t current, peak;
static size_t allocation_size(unsigned bytes) {
    size_t total = sizeof(Header) + (size_t)bytes;
    return total < bytes ? 0 : total;
}
static void *allocate(unsigned bytes) {
    size_t total = allocation_size(bytes);
    if (!total) return NULL;
    Header *header = malloc(total);
    if (!header) return NULL;
    header->bytes = bytes;
    current += bytes;
    if (current > peak) peak = current;
    return header + 1;
}
static void release(void *pointer) {
    if (!pointer) return;
    Header *header = (Header *)pointer - 1;
    current -= header->bytes;
    free(header);
}
static void *resize(void *pointer, unsigned bytes) {
    if (!pointer) return allocate(bytes);
    if (!bytes) { release(pointer); return NULL; }
    size_t total = allocation_size(bytes);
    if (!total) return NULL;
    Header *header = (Header *)pointer - 1;
    size_t old = header->bytes;
    header = realloc(header, total);
    if (!header) return NULL;
    header->bytes = bytes;
    current = current - old + bytes;
    if (current > peak) peak = current;
    return header + 1;
}
int main(int argc, char **argv) {
    int instantiate = argc == 3 && strcmp(argv[2], "--instantiate") == 0;
    if (argc != 2 && !instantiate) {
        fprintf(stderr, "usage: %s script.wasm [--instantiate]\n", argv[0]); return 2;
    }
    FILE *file = fopen(argv[1], "rb");
    if (!file) return 2;
    if (fseek(file, 0, SEEK_END)) { fclose(file); return 2; }
    long length = ftell(file);
    if (length <= 0 || (uint64_t)length > UINT32_MAX || fseek(file, 0, SEEK_SET)) {
        fclose(file); return 2;
    }
    uint8_t *input = malloc((size_t)length);
    if (!input) { fclose(file); return 2; }
    size_t read = fread(input, 1, (size_t)length, file);
    fclose(file);
    if (read != (size_t)length) { free(input); return 2; }
    RuntimeInitArgs init = {0};
    init.mem_alloc_type = Alloc_With_Allocator;
    init.mem_alloc_option.allocator.malloc_func = allocate;
    init.mem_alloc_option.allocator.realloc_func = resize;
    init.mem_alloc_option.allocator.free_func = release;
    if (!wasm_runtime_full_init(&init)) { free(input); return 2; }
    size_t baseline = current;
    peak = current;
    char error[256] = {0};
    wasm_module_t module = wasm_runtime_load(input, (uint32_t)length, error, sizeof(error));
    if (!module) {
        fprintf(stderr, "%s\n", error);
        wasm_runtime_destroy(); free(input); return 1;
    }
    size_t resident = current - baseline, loading_peak = peak - baseline;
    size_t instance_resident = 0, instance_peak = 0;
    int failed = 0;
    if (instantiate) {
        /* Match the product's 16 KiB operand stack, without enlarging it. */
        peak = current;
        wasm_module_inst_t instance = wasm_runtime_instantiate(module, 16384, 0, error, sizeof(error));
        wasm_exec_env_t env = instance ? wasm_runtime_create_exec_env(instance, 16384) : NULL;
        if (!env) {
            fprintf(stderr, "instantiate/exec environment: %s\n", error);
            failed = 1;
        } else {
            instance_resident = current - baseline;
            instance_peak = peak - baseline;
            wasm_runtime_destroy_exec_env(env);
        }
        if (instance) wasm_runtime_deinstantiate(instance);
    }
    wasm_runtime_unload(module);
    wasm_runtime_destroy();
    free(input);
    if (failed) return 1;
    printf("{\"input_bytes\":%ld,\"runtime_requested_bytes\":%zu,"
           "\"runtime_requested_peak_bytes\":%zu,\"remaining_bytes\":%zu",
           length, resident, loading_peak, current);
    if (instantiate)
        printf(",\"instance_requested_bytes\":%zu,\"instance_requested_peak_bytes\":%zu",
               instance_resident, instance_peak);
    puts("}");
    return current != 0;
}
