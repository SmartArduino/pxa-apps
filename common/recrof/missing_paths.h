/* Short-lived absent-file reuse within one Guest namespace revision. */
#pragma once
#include <stdint.h>
#include <string.h>
#ifndef PORT_MISSING_PATHS_CAPACITY
#define PORT_MISSING_PATHS_CAPACITY 16
#endif
typedef struct {
  struct { char path[256]; uint64_t expires; } entries[PORT_MISSING_PATHS_CAPACITY];
  uint64_t revision;
  unsigned next;
} port_missing_paths_t;
static inline int port_missing_path_hit(port_missing_paths_t *cache,
    const char *path, uint64_t now, uint64_t revision) {
  if(cache->revision!=revision) {
    memset(cache,0,sizeof(*cache));cache->revision=revision;
  }
  for(unsigned i=0;i<PORT_MISSING_PATHS_CAPACITY;++i)
    if(cache->entries[i].expires>now && !strcmp(cache->entries[i].path,path))return 1;
  return 0;
}
static inline void port_missing_path_store_for(port_missing_paths_t *cache,
    const char *path, uint64_t now, uint64_t revision, uint64_t lifetime) {
  if(strlen(path)>=sizeof(cache->entries[0].path))return;
  (void)port_missing_path_hit(cache,path,now,revision);
  unsigned slot=cache->next++%PORT_MISSING_PATHS_CAPACITY;
  strcpy(cache->entries[slot].path,path);
  cache->entries[slot].expires=now+lifetime;
}
static inline void port_missing_path_store(port_missing_paths_t *cache,
    const char *path, uint64_t now, uint64_t revision) {
  port_missing_path_store_for(cache,path,now,revision,1000000);
}
