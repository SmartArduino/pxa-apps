// SPDX-License-Identifier: MIT
#pragma once
#include "art_layout.hpp"
#include "pxa_assets.h"
#include <cstdio>
#include <cstring>
namespace carrot {
// Prepare small PNGs once, outside drawing. Large landscapes remain on the
// ordinary Canvas path because they exceed the prepared-PNG pixel limit.
class ImageCache {
  static constexpr unsigned Capacity = 6;
  static constexpr uint64_t Token = 9000;
  struct Entry {
    char path[64]{};
    uint64_t handle = 0;
    uint8_t state = 0;
  };
  Entry entries[Capacity];
  unsigned count = 0, pending = 0;
  unsigned reserved = 0;
  static int indexOf(const char *p) {
    if (*p < '0' || *p > '9')
      return -1;
    unsigned n = 0;
    while (*p >= '0' && *p <= '9') {
      n = n * 10 + unsigned(*p++ - '0');
      if (n > 16)
        return -1;
    }
    return std::strcmp(p, ".png") ? -1 : int(n);
  }

public:
  uint64_t lookup(const char *path) {
    for (unsigned i = 0; i < count; ++i)
      if (!std::strcmp(entries[i].path, path))
        return entries[i].handle;
    // The device's prepared-assets pool is shared with all 14 sounds.
    // Reserve at most 80 KiB and six handles for frequently repeated UI art;
    // large actors and direction strips already use the separate PNG cache.
    unsigned cost = 0;
    int index = -1;
    if (!std::strncmp(path, "assets/ui/s", 11) &&
        (index = indexOf(path + 11)) >= 0 && (index == 0 || index == 4))
      cost = unsigned(artSkins[index].w * artSkins[index].h * 4) + 512;
    else if (!std::strncmp(path, "assets/n", 8) &&
             (index = indexOf(path + 8)) >= 0 && index < 16)
      cost = unsigned(digitWidths[index] * digitHeights[index] * 4) + 512;
    if (!cost || reserved + cost > 80 * 1024)
      return 0;
    if (count < Capacity && std::strlen(path) < 64) {
      reserved += cost;
      std::snprintf(entries[count++].path, 64, "%s", path);
    }
    return 0;
  }
  void pump() {
    if (pending >= 2)
      return;
    for (unsigned i = 0; i < count; ++i)
      if (!entries[i].state) {
        int status = pxa_assets_load_image(Token + i, entries[i].path);
        if (status == PXA_STATUS_OK) {
          entries[i].state = 1;
          ++pending;
        } else if (status != PXA_STATUS_BUSY &&
                   status != PXA_STATUS_WOULD_BLOCK)
          entries[i].state = 3;
        return;
      }
  }
  bool handle(const pxa_event_t &e) {
    if (e.token < Token || e.token >= Token + count)
      return false;
    unsigned i = unsigned(e.token - Token);
    pxa_asset_result_t result;
    if (entries[i].state != 1 ||
        !pxa_assets_parse_result(&e, e.token, PXA_ASSETS_LOAD, &result))
      return false;
    --pending;
    if (result.status == PXA_STATUS_OK && result.info.kind == PXA_ASSET_IMAGE) {
      entries[i].handle = result.handle;
      entries[i].state = 2;
    } else {
      char message[128];
      std::snprintf(message, sizeof(message),
                    "carrot image fallback status=%d path=%s", result.status,
                    entries[i].path);
      pxa_log_write(2, message);
      if (result.status == PXA_STATUS_OK && result.handle)
        pxa_close_handle(result.handle);
      entries[i].state = 3;
    }
    return true;
  }
  void stop() {
    for (unsigned i = 0; i < count; ++i) {
      if (entries[i].state == 1)
        pxa_cancel(Token + i);
      if (entries[i].handle)
        pxa_close_handle(entries[i].handle);
    }
  }
};
} // namespace carrot
