/* PXA has no memory-mapped spare flash partition. Use Vanilla-Conquer's file
 * and heap fallback instead of exposing hardware flash to an app. */
#include <stdint.h>
extern "C" const void *cncFlashCacheFind(const char *, int32_t, int32_t) {
  return nullptr;
}
extern "C" const void *cncFlashCacheStore(const char *, int32_t, int32_t,
                                          int (*)(void *, void *, int),
                                          void *) {
  return nullptr;
}
extern "C" bool cncFlashCacheStandBy() { return false; }
extern "C" void cncFlashCacheDefer(const char *) {}
extern "C" bool cncFlashCachePending(char *, int) { return false; }
extern "C" void cncFlashCacheDone(const char *) {}
