/* SPDX-License-Identifier: MIT */
#ifndef JY_ASSET_INDEX_H
#define JY_ASSET_INDEX_H
#include "jy_fs.h"
/* Trailing zero fields retain legacy {logical, resource, bytes} initializers.
 * A bundled entry identifies a logical byte range of the physical resource.
 * resource_total_bytes==0 means a legacy resource whose total equals bytes. */
typedef struct {
  const char *path, *resource_path;
  uint32_t bytes;
  uint32_t resource_offset, resource_total_bytes;
} jy_asset_entry;
extern const jy_asset_entry jy_assets[];
extern const size_t jy_asset_count;
const jy_asset_entry *jy_asset_lookup(const char *normalized_path);
int jy_asset_stat(void *ignored,const char *normalized_path,jy_fs_info *);
#endif
