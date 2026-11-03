/* SPDX-License-Identifier: MIT. PXA assets + persistent chunked Storage
 * backend. */
#ifndef JY_PXA_FS_BACKEND_H
#define JY_PXA_FS_BACKEND_H
#include "pxa_core.h"
#include <stdint.h>
/* bind does no I/O. Call tick on wall-clock events, handle_event before any
 * coroutine resume. A pending() request must not be resumed until a matching
 * result or tick timeout clears it. All borrowed event bytes are copied here.
 * Storage operations suspend via jy_suspend(0), so run them on the game stack.
 */
int jy_pxa_fs_bind(void);
int jy_pxa_fs_handle_event(const pxa_event_t *);
void jy_pxa_fs_tick(uint64_t wall_us);
void jy_pxa_fs_cancel(void);
int jy_pxa_fs_pending(void);
typedef struct {
  uint64_t requests, asset_bytes, storage_read_bytes, storage_write_bytes,
      timeouts, cancels;
  uint32_t peak_reply_bytes;
  int last_error;
} jy_pxa_fs_stats;
void jy_pxa_fs_get_stats(jy_pxa_fs_stats *);
/* Override these with the package's immutable base aliases and revision.
 * Changing the revision rejects existing deltas with ESTALE. */
const char *jy_pxa_base_alias(const char *normalized_path);
const char *jy_pxa_base_revision(void);
#endif
