#pragma once
#include <stdlib.h>
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_INTERNAL 2
#define MALLOC_CAP_8BIT 4
#define MALLOC_CAP_DMA 8
#define MALLOC_CAP_32BIT 16
static inline void *heap_caps_malloc(size_t n, unsigned c) {
  (void)c;
  return malloc(n);
}
static inline void *heap_caps_calloc(size_t n, size_t s, unsigned c) {
  (void)c;
  return calloc(n, s);
}
static inline void *heap_caps_realloc(void *p, size_t n, unsigned c) {
  (void)c;
  return realloc(p, n);
}
static inline void heap_caps_free(void *p) { free(p); }
static inline size_t heap_caps_get_largest_free_block(unsigned c) {
  (void)c;
  return 12 * 1024 * 1024;
}
static inline size_t heap_caps_get_free_size(unsigned c) {
  return heap_caps_get_largest_free_block(c);
}

static inline void *heap_caps_aligned_alloc(size_t a, size_t n, unsigned c) {
  (void)c;
  return aligned_alloc(a, (n + a - 1) & ~(a - 1));
}
