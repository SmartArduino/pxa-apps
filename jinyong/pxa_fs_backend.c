/* SPDX-License-Identifier: MIT
 * Streaming PXA backend. No WASI preopens or Host modifications.
 * One128KiB logical/base page plus bounded compressed scratch lives in RAM.
 * Persistent blocks are private FS files; groups use XOR + zlib differences.
 * Metadata commits last; interrupted rename is replayed from a bounded journal.
 * Private files avoid board-specific KV value/key limits. Host ENOSPC is
 * propagated, never treated as queued success.
 */
#include "pxa_fs_backend.h"
#include "deps/zlib/zlib.h"
#include "fs/jy_asset_index.h"
#include "fs/jy_fs.h"
#include "protected_bridge.h"
#include "pxa_assets.h"
#include "pxa_fs.h"
#include <errno.h>
#include <limits.h>
#include <string.h>
#define BLOCK 2048u
#define META_BYTES 640u
#define GROUP 131072u
#define FIRST (BLOCK - 28u)
#define COMP_CAP (GROUP + GROUP / 8u + GROUP / 64u + 64u)
#define TIMEOUT_US UINT64_C(15000000)
#define MAX_FILE_BYTES UINT64_C(67108864)
#define TOMB 3u
typedef struct {
  char path[256], base[256];
  uint8_t kind;
  uint64_t id, epoch, size, gcid, gcepoch;
  uint32_t gcfirst, gcend;
} meta;
typedef struct {
  int valid, error;
  char path[256];
  meta value;
} cached;
static cached cache[8];
static unsigned cache_next;
static struct {
  int valid;
  uint64_t id, epoch;
  uint32_t index, generation, old_generation;
  uint16_t extra, old_extra;
  uint8_t bytes[GROUP], base[GROUP];
} page;
static uint8_t compressed[COMP_CAP];
/* Deflate's default allocator needs roughly 256 KiB transient heap. A small
 * upstream zlib window/memLevel and a bounded arena leave room for the Lua
 * world state on devices with a 4 MiB linear memory. Streams remain ordinary
 * zlib data and existing saves still decode through inflate. */
static _Alignas(16) uint8_t deflate_arena[65536];
static size_t deflate_used;
static voidpf deflate_allocate(voidpf opaque,uInt items,uInt bytes) {
  (void)opaque;size_t n=(size_t)items*bytes;
  if(bytes && n/bytes!=items)return NULL;
  if(n>sizeof deflate_arena-deflate_used)return NULL;
  void *p=deflate_arena+deflate_used;deflate_used+=(n+15u)&~(size_t)15u;
  return p;
}
static void deflate_release(voidpf opaque,voidpf address){(void)opaque;(void)address;}
static int compress_group(uLongf *count) {
  z_stream stream;memset(&stream,0,sizeof stream);deflate_used=0;
  stream.zalloc=deflate_allocate;stream.zfree=deflate_release;
  int z=deflateInit2(&stream,6,Z_DEFLATED,13,4,Z_DEFAULT_STRATEGY);
  if(z!=Z_OK)return z;
  stream.next_in=page.bytes;stream.avail_in=GROUP;
  stream.next_out=compressed;stream.avail_out=COMP_CAP;
  z=deflate(&stream,Z_FINISH);*count=stream.total_out;
  int end=deflateEnd(&stream);
  return z==Z_STREAM_END?end:(z==Z_OK?Z_BUF_ERROR:z);
}
__attribute__((weak)) const char *jy_pxa_base_alias(const char *p) { return p; }
__attribute__((weak)) const char *jy_pxa_base_revision(void) {
  return "unversioned";
}
static int erase_group(uint64_t, uint64_t, uint32_t);
static int recover_staged(void);
static struct {
  uint64_t token, deadline;
  uint16_t service, opcode;
  int active, done, error;
  uint32_t offset, total;
  uint64_t handle;
  size_t bytes;
  uint8_t data[4064];
} request;
static uint8_t packet[PXA_FS_MAX_RENAME_PACKET];
static int files_ready;
static uint64_t now_us, sequence;
static int cancelled, recovered, recovering, clock_seen;
static jy_pxa_fs_stats stats;
static int status_errno(int32_t s) {
  switch (s) {
  case 0:
    return 0;
  case -1:
    return EINVAL;
  case -2:
    return EBUSY;
  case -3:
    return ENOSYS;
  case -4:
    return EACCES;
  case -5:
    return ENOENT;
  case -6:
    return EBUSY;
  case -7:
    return EAGAIN;
  case -8:
    return ENOSPC;
  case -9:
    return ENOMEM;
  case -10:
    return ECANCELED;
  case -12:
    return ETIMEDOUT;
  case -13:
    return ENODEV;
  case -15:
    return EPROTO;
  case -16:
    return EFBIG;
  default:
    return EIO;
  }
}
static int begin(uint16_t service, uint16_t opcode) {
  if (cancelled)
    return ECANCELED;
  if (request.active)
    return EBUSY;
  memset(&request, 0, sizeof request);
  request.active = 1;
  request.service = service;
  request.opcode = opcode;
  sequence = (sequence + 1) & UINT64_C(0x0000ffffffffffff);
  if (!sequence)
    sequence = 1;
  request.token = UINT64_C(0x4a46000000000000) | sequence;
  request.deadline = !clock_seen                        ? 0
                     : now_us > UINT64_MAX - TIMEOUT_US ? UINT64_MAX
                                                        : now_us + TIMEOUT_US;
  stats.requests++;
  return 0;
}
static int finish(int32_t submitted) {
  if (submitted) {
    request.active = 0;
    stats.last_error = status_errno(submitted);
    return stats.last_error;
  }
  while (!request.done)
    jy_suspend(0);
  int e = request.error;
  request.active = 0;
  stats.last_error = e;
  return e;
}
int jy_pxa_fs_pending(void) { return request.active && !request.done; }
void jy_pxa_fs_tick(uint64_t us) {
  if (us > now_us)
    now_us = us;
  clock_seen = 1;
  if (jy_pxa_fs_pending()) {
    if (!request.deadline)
      request.deadline =
          now_us > UINT64_MAX - TIMEOUT_US ? UINT64_MAX : now_us + TIMEOUT_US;
    else if (now_us >= request.deadline) {
      (void)pxa_cancel(request.token);
      request.error = ETIMEDOUT;
      request.done = 1;
      stats.timeouts++;
    }
  }
}
void jy_pxa_fs_cancel(void) {
  cancelled = 1;
  if (jy_pxa_fs_pending()) {
    (void)pxa_cancel(request.token);
    request.error = ECANCELED;
    request.done = 1;
    stats.cancels++;
  }
}
int jy_pxa_fs_handle_event(const pxa_event_t *event) {
  if (!event || !jy_pxa_fs_pending() || event->token != request.token)
    return 0;
  if (event->service != request.service || event->opcode != request.opcode) {
    request.error = EPROTO;
    request.done = 1;
    return 1;
  }
  if (request.service == PXA_ASSETS_SERVICE) {
    pxa_asset_read_result_t r;
    if (!pxa_assets_parse_read(event, request.token, &r))
      request.error = EPROTO;
    else {
      request.error = status_errno(r.status);
      if (!request.error) {
        request.offset = r.offset;
        request.total = r.total_bytes;
        request.bytes = r.bytes;
        if (r.bytes)
          memcpy(request.data, r.data, r.bytes);
        stats.asset_bytes += r.bytes;
      }
    }
  } else if (request.opcode == PXA_FS_OPEN) {
    pxa_fs_open_result_t r;
    if (!pxa_fs_parse_open(event,request.token,&r)) request.error=EPROTO;
    else {request.error=status_errno(r.status);request.handle=r.handle;}
  } else {
    int32_t status;
    if (!pxa_fs_parse_status(event,request.token,request.opcode,&status)) request.error=EPROTO;
    else request.error=status_errno(status);
  }
  if (request.bytes > stats.peak_reply_bytes)
    stats.peak_reply_bytes = (uint32_t)request.bytes;
  request.done = 1;
  return 1;
}
/* Compressed delta blocks stay bounded, but no longer consume the tiny
 * board KV quota. Each value is an ordinary private file. Replacing a complete
 * value is atomic; the existing metadata-last/journal protocol is preserved. */
static int object_path(const char *key,char path[PXA_FS_MAX_PATH+1]) {
  static const char digits[]="0123456789abcdef";
  size_t n=strlen(key);const char prefix[]="jy-files-v1/k-";
  if(n>(PXA_FS_MAX_PATH-(sizeof prefix-1))/2)return ENAMETOOLONG;
  memcpy(path,prefix,sizeof prefix-1);
  for(size_t i=0;i<n;i++){unsigned c=(unsigned char)key[i];path[sizeof prefix-1+i*2]=digits[c>>4];path[sizeof prefix+i*2]=digits[c&15];}
  path[sizeof prefix-1+n*2]=0;return 0;
}
static int file_path(uint16_t op,const char *path) {
  int e=begin(PXA_FS_SERVICE,op);if(e)return e;
  return finish(pxa_fs_request_path(op,request.token,path,strlen(path)));
}
static int file_open(const char *path,uint32_t flags,uint64_t *handle) {
  int e=begin(PXA_FS_SERVICE,PXA_FS_OPEN);if(e)return e;
  e=finish(pxa_fs_request_open(request.token,path,strlen(path),flags));
  if(!e)*handle=request.handle;return e;
}
static int file_replace(const char *from,const char *to) {
  int e=begin(PXA_FS_SERVICE,PXA_FS_REPLACE);if(e)return e;uint32_t n;
  if(!pxa_fs_build_replace(packet,sizeof packet,request.token,from,strlen(from),to,strlen(to),&n)){request.active=0;return EINVAL;}
  return finish(pxa_control_packet(packet,n));
}
static int file_io(uint64_t handle,int writing,uint8_t *data,size_t n,size_t *done) {
  *done=0;
  while(*done<n){
    int32_t r=writing?pxa_fs_write(handle,data+*done,(uint32_t)(n-*done)):pxa_fs_read(handle,data+*done,(uint32_t)(n-*done));
    if(r==PXA_STATUS_WOULD_BLOCK){jy_suspend(0);continue;}
    if(r<0)return status_errno(r);
    if((size_t)r>n-*done)return EPROTO;
    if(!r)return writing?EIO:0;
    *done+=(size_t)r;
  }
  return 0;
}
static int get(const char *key,void *dst,size_t cap,size_t *n) {
  char path[PXA_FS_MAX_PATH+1];uint64_t handle=0;*n=0;
  int e=object_path(key,path);if(e)return e;
  e=file_open(path,PXA_FS_OPEN_READ,&handle);if(e)return e;
  e=file_io(handle,0,dst,cap,n);
  if(!e&&*n==cap){uint8_t extra;size_t more;e=file_io(handle,0,&extra,1,&more);if(!e&&more)e=EFBIG;}
  int closed=status_errno(pxa_fs_close(handle));if(!e)e=closed;
  if(!e)stats.storage_read_bytes+=*n;stats.last_error=e;return e;
}
static int set(const char *key,const void *data,size_t n) {
  char path[PXA_FS_MAX_PATH+1];uint64_t handle=0;uint8_t copy[BLOCK];size_t done;
  if(n>sizeof copy)return EFBIG;
  memcpy(copy,data,n);int e=object_path(key,path);if(e)return e;
  if(!files_ready){
    e=file_path(PXA_FS_MAKE_DIRECTORY,"jy-files-v1");
    /* MKDIR already existing is BAD_STATE on the public FS service. Verify
       the directory by opening it, so unrelated failures are not hidden. */
    if(e){uint64_t dir;e=file_open("jy-files-v1",PXA_FS_OPEN_READ|PXA_FS_OPEN_DIRECTORY,&dir);if(e)return e;e=status_errno(pxa_fs_close(dir));if(e)return e;}
    files_ready=1;
  }
  const char *stage="jy-files-v1/staging";
  e=file_open(stage,PXA_FS_OPEN_WRITE|PXA_FS_OPEN_CREATE|PXA_FS_OPEN_TRUNCATE,&handle);if(e)return e;
  e=file_io(handle,1,copy,n,&done);int closed=status_errno(pxa_fs_close(handle));if(!e)e=closed;
  if(!e)e=file_replace(stage,path);
  if(!e)stats.storage_write_bytes+=n;stats.last_error=e;return e;
}
static int del(const char *key) {
  char path[PXA_FS_MAX_PATH+1];int e=object_path(key,path);if(e)return e;
  e=file_path(PXA_FS_REMOVE,path);return e==ENOENT?0:e;
}
static void hex(char *out, uint64_t x, unsigned digits) {
  static const char h[] = "0123456789abcdef";
  for (unsigned i = 0; i < digits; i++)
    out[digits - 1 - i] = h[(x >> (i * 4)) & 15];
}
static uint32_t checksum(const uint8_t *p, size_t n) {
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < n; i++)
    h = (h ^ p[i]) * 16777619u;
  return h;
}
static void metakey(const char *path, char key[48]) {
  uint64_t a = UINT64_C(1469598103934665603), b = UINT64_C(7809847782465536322);
  for (const unsigned char *p = (const unsigned char *)path; *p; p++) {
    a = (a ^ *p) * UINT64_C(1099511628211);
    b = (b + *p) * UINT64_C(14029467366897019727);
    b ^= b >> 29;
  }
  memcpy(key, "jyf.m.", 6);
  hex(key + 6, a, 16);
  hex(key + 22, b, 16);
  key[38] = 0;
}
static void groupkey(uint64_t id, uint64_t epoch, uint32_t group,
                     char key[64]) {
  memcpy(key, "jyf.g.", 6);
  hex(key + 6, id, 16);
  key[22] = '.';
  hex(key + 23, epoch, 16);
  key[39] = '.';
  hex(key + 40, group, 8);
  key[48] = 0;
}
static void shardkey(uint64_t id, uint64_t epoch, uint32_t group,
                     uint32_t generation, unsigned shard, char key[64]) {
  memcpy(key, "jyf.c.", 6);
  hex(key + 6, id, 16);
  key[22] = '.';
  hex(key + 23, epoch, 16);
  key[39] = '.';
  hex(key + 40, group, 8);
  key[48] = '.';
  hex(key + 49, generation, 8);
  key[57] = '.';
  hex(key + 58, shard, 2);
  key[60] = 0;
}
static size_t encode(const meta *m, uint8_t out[META_BYTES]) {
  size_t n = strlen(m->path), b = strlen(m->base);
  memcpy(out, "JYF2", 4);
  out[4] = m->kind;
  out[5] = 0;
  pxa_store_u16(out + 6, (uint16_t)n);
  pxa_store_u64(out + 8, m->id);
  pxa_store_u64(out + 16, m->epoch);
  pxa_store_u64(out + 24, m->size);
  pxa_store_u64(out + 32, m->gcid);
  pxa_store_u64(out + 40, m->gcepoch);
  pxa_store_u32(out + 48, m->gcfirst);
  pxa_store_u32(out + 52, m->gcend);
  pxa_store_u16(out + 56, (uint16_t)b);
  out[58] = out[59] = 0;
  memcpy(out + 60, m->path, n);
  memcpy(out + 60 + n, m->base, b);
  pxa_store_u32(out + 60 + n + b, checksum(out, 60 + n + b));
  return 64 + n + b;
}
static int decode(const uint8_t *b, size_t n, meta *m) {
  if (n < 64 || memcmp(b, "JYF2", 4) || b[5] || b[58] || b[59] || b[4] < 1 ||
      b[4] > TOMB)
    return EIO;
  unsigned len = pxa_load_u16(b + 6), blen = pxa_load_u16(b + 56);
  if (len > 255 || blen > 255 || n != 64u + len + blen ||
      pxa_load_u32(b + 60 + len + blen) != checksum(b, 60 + len + blen))
    return EIO;
  memset(m, 0, sizeof *m);
  memcpy(m->path, b + 60, len);
  memcpy(m->base, b + 60 + len, blen);
  char normal[256];
  if (jy_fs_normalize(m->path, normal) || strcmp(normal, m->path) ||
      jy_fs_normalize(m->base, normal) || strcmp(normal, m->base))
    return EIO;
  m->kind = b[4];
  m->id = pxa_load_u64(b + 8);
  m->epoch = pxa_load_u64(b + 16);
  m->size = pxa_load_u64(b + 24);
  m->gcid = pxa_load_u64(b + 32);
  m->gcepoch = pxa_load_u64(b + 40);
  m->gcfirst = pxa_load_u32(b + 48);
  m->gcend = pxa_load_u32(b + 52);
  if (!m->id || !m->epoch || m->size > MAX_FILE_BYTES ||
      m->gcfirst > m->gcend || m->gcend > MAX_FILE_BYTES / GROUP + 1)
    return EIO;
  return 0;
}
static void cache_drop(const char *p) {
  for (unsigned i = 0; i < 8; i++)
    if (cache[i].valid && !strcmp(cache[i].path, p))
      cache[i].valid = 0;
}
static void cache_put(const char *p, const meta *m, int e) {
  cache_drop(p);
  cached *c = &cache[cache_next++ % 8];
  memset(c, 0, sizeof *c);
  c->valid = 1;
  c->error = e;
  strcpy(c->path, p);
  if (!e)
    c->value = *m;
}
static int load(const char *p, meta *m) {
  for (unsigned i = 0; i < 8; i++)
    if (cache[i].valid && !strcmp(cache[i].path, p)) {
      if (!cache[i].error)
        *m = cache[i].value;
      return cache[i].error;
    }
  char key[48];
  uint8_t data[META_BYTES];
  size_t n;
  metakey(p, key);
  int e = get(key, data, sizeof data, &n);
  if (!e) {
    e = decode(data, n, m);
    if (!e && strcmp(p, m->path))
      e = EEXIST;
  }
  if (!e || e == ENOENT)
    cache_put(p, m, e);
  return e;
}
static int store(const meta *m) {
  char key[48];
  uint8_t data[META_BYTES];
  metakey(m->path, key);
  size_t n = encode(m, data);
  int e = set(key, data, n);
  if (!e)
    cache_put(m->path, m, 0);
  return e;
}
static int erase_meta(const char *p) {
  char key[48];
  metakey(p, key);
  int e = del(key);
  if (!e)
    cache_put(p, NULL, ENOENT);
  return e;
}
static uint32_t blocks(uint64_t size) {
  return (uint32_t)((size + GROUP - 1) / GROUP);
}
static int gc(meta *m) {
  if (!m->gcid)
    return 0;
  for (uint32_t i = m->gcfirst; i < m->gcend; i++) {
    int e = erase_group(m->gcid, m->gcepoch, i);
    if (e)
      return e;
  }
  m->gcid = m->gcepoch = 0;
  m->gcfirst = m->gcend = 0;
  return store(m);
}
static int allocate_id(uint64_t *id) {
  uint8_t b[8];
  size_t n;
  int e = get("jyf.counter", b, sizeof b, &n);
  if (e == ENOENT)
    *id = 1;
  else if (e)
    return e;
  else {
    if (n != 8)
      return EIO;
    *id = pxa_load_u64(b) + 1;
    if (!*id)
      return EFBIG;
  }
  pxa_store_u64(b, *id);
  return set("jyf.counter", b, sizeof b);
}
static int recover(void) {
  if (recovered || recovering)
    return 0;
  recovering = 1;
  uint8_t rev[128];
  size_t revn = 0;
  const char *expected = jy_pxa_base_revision();
  if (!expected || strlen(expected) > sizeof rev) {
    recovering = 0;
    return EINVAL;
  }
  size_t explen = strlen(expected);
  int re = get("jyf.rev", rev, sizeof rev, &revn);
  if (re == ENOENT) {
    uint8_t counter[8];
    size_t bytes;
    int found = get("jyf.counter", counter, sizeof counter, &bytes);
    if (!found)
      re = ESTALE;
    else if (found != ENOENT)
      re = found;
    else
      re = set("jyf.rev", expected, explen);
  } else if (!re && (revn != explen || memcmp(rev, expected, revn)))
    re = ESTALE;
  if (re) {
    recovering = 0;
    return re;
  }
  if ((re = recover_staged())) {
    recovering = 0;
    return re;
  }
  uint8_t journal[2 * META_BYTES + 4];
  size_t n;
  int e = get("jyf.tx", journal, sizeof journal, &n);
  if (e == ENOENT)
    e = 0;
  else if (!e) {
    if (n < 4)
      e = EIO;
    else {
      unsigned a = pxa_load_u16(journal), b = pxa_load_u16(journal + 2);
      meta dest, old;
      if (a > META_BYTES || b > META_BYTES || n != 4u + a + b)
        e = EIO;
      else if ((e = decode(journal + 4, a, &dest)) == 0 &&
               (e = decode(journal + 4 + a, b, &old)) == 0) {
        if (old.kind != TOMB || !strcmp(dest.path, old.path))
          e = EIO;
        else if (!(e = store(&dest)) && !(e = store(&old)))
          e = del("jyf.tx");
      }
    }
  }
  recovering = 0;
  if (!e)
    recovered = 1;
  return e;
}
static int meta_for(const char *p, meta *m) {
  int e = recover();
  return e ? e : load(p, m);
}
static int base_stat(void *ignored, const char *p, jy_fs_info *info) {
  (void)ignored;
  meta m;
  int e = meta_for(p, &m);
  if (!e && m.kind == TOMB)
    return ENOENT;
  if (e && e != ENOENT)
    return e;
  return jy_asset_stat(NULL, p, info);
}
static int base_read(void *ignored, const char *p, uint64_t off, void *dst,
                     size_t n, size_t *got) {
  (void)ignored;
  *got = 0;
  const jy_asset_entry *a = jy_asset_lookup(p);
  if (!a)
    return ENOENT;
  uint32_t total = a->resource_total_bytes ? a->resource_total_bytes : a->bytes;
  /* Widen before addition: malformed metadata must not wrap into a valid
   * physical offset or expose bytes belonging to an adjacent logical file. */
  if ((uint64_t)a->resource_offset + a->bytes > total)
    return EINVAL;
  if (off >= a->bytes || !n)
    return 0;
  if (n > JY_FS_CHUNK_BYTES)
    return EINVAL;
  if (n > a->bytes - off)
    n = (size_t)(a->bytes - off);
  uint32_t physical = (uint32_t)((uint64_t)a->resource_offset + off);
  int e = begin(PXA_ASSETS_SERVICE, PXA_ASSETS_READ);
  if (e)
    return e;
  e = finish(pxa_assets_read(request.token, a->resource_path, physical,
                             (uint32_t)n));
  if (e)
    return e;
  if (request.offset != physical || request.total != total || request.bytes != n)
    return EIO;
  memcpy(dst, request.data, n);
  *got = n;
  return 0;
}
static int overlay_stat(void *ignored, const char *p, jy_fs_info *info) {
  (void)ignored;
  meta m;
  int e = meta_for(p, &m);
  if (e)
    return e;
  if (m.kind == TOMB)
    return ENOENT;
  info->size = m.size;
  info->kind = m.kind;
  return 0;
}
static int header(const uint8_t *b, size_t n, uint32_t *length,
                  uint32_t *generation, uint16_t *extra, uint32_t *oldgen,
                  uint16_t *oldextra) {
  if (n < 28 || memcmp(b, "JYG2", 4) || pxa_load_u32(b + 20) ||
      pxa_load_u32(b + 24) != checksum(b, 24))
    return EIO;
  *length = pxa_load_u32(b + 4);
  *generation = pxa_load_u32(b + 8);
  *extra = pxa_load_u16(b + 12);
  *oldextra = pxa_load_u16(b + 14);
  *oldgen = pxa_load_u32(b + 16);
  if (!*generation || *length > COMP_CAP || *extra > 65 || *oldextra > 65 ||
      n != 28u + (*length < FIRST ? *length : FIRST) ||
      *extra != (*length > FIRST ? (*length - FIRST + BLOCK - 1) / BLOCK : 0))
    return EIO;
  return 0;
}
static int delete_shards(uint64_t id, uint64_t epoch, uint32_t group,
                         uint32_t generation, unsigned count) {
  char key[64];
  for (unsigned i = 0; i < count; i++) {
    shardkey(id, epoch, group, generation, i, key);
    int e = del(key);
    if (e)
      return e;
  }
  return 0;
}
static int erase_group(uint64_t id, uint64_t epoch, uint32_t group) {
  char key[64];
  uint8_t data[BLOCK];
  size_t n;
  groupkey(id, epoch, group, key);
  int e = get(key, data, sizeof data, &n);
  if (e == ENOENT)
    return 0;
  if (e)
    return e;
  uint32_t len, gen, oldgen;
  uint16_t extra, oldextra;
  if ((e = header(data, n, &len, &gen, &extra, &oldgen, &oldextra)))
    return e;
  if ((e = delete_shards(id, epoch, group, gen, extra)) ||
      (e = delete_shards(id, epoch, group, oldgen, oldextra)))
    return e;
  return del(key);
}
static int recover_staged(void) {
  uint8_t pending[36];
  size_t n;
  int e = get("jyf.pending", pending, sizeof pending, &n);
  if (e == ENOENT)
    return 0;
  if (e)
    return e;
  if (n != 36 || memcmp(pending, "JYP2", 4) || pxa_load_u16(pending + 30) ||
      pxa_load_u32(pending + 32) != checksum(pending, 32))
    return EIO;
  uint64_t id = pxa_load_u64(pending + 4), epoch = pxa_load_u64(pending + 12);
  uint32_t group = pxa_load_u32(pending + 20),
           generation = pxa_load_u32(pending + 24);
  uint16_t extra = pxa_load_u16(pending + 28);
  if (!id || !epoch || !generation || !extra || extra > 65 ||
      group > MAX_FILE_BYTES / GROUP)
    return EIO;
  char key[64];
  uint8_t data[BLOCK];
  groupkey(id, epoch, group, key);
  e = get(key, data, sizeof data, &n);
  int committed = 0;
  if (!e) {
    uint32_t len, gen, old;
    uint16_t count, oldcount;
    if ((e = header(data, n, &len, &gen, &count, &old, &oldcount)))
      return e;
    if (gen == generation) {
      if (count != extra)
        return EIO;
      committed = 1;
    }
  } else if (e != ENOENT)
    return e;
  if (!committed && (e = delete_shards(id, epoch, group, generation, extra)))
    return e;
  return del("jyf.pending");
}
static int load_page(const meta *m, uint32_t group) {
  if (page.valid && page.id == m->id && page.epoch == m->epoch &&
      page.index == group)
    return 0;
  page.valid = 0;
  memset(page.base, 0, GROUP);
  const jy_asset_entry *asset = jy_asset_lookup(m->base);
  uint64_t start = (uint64_t)group * GROUP;
  if (asset && start < asset->bytes) {
    size_t total =
        (asset->bytes - start) > GROUP ? GROUP : (size_t)(asset->bytes - start);
    for (size_t done = 0; done < total;) {
      size_t n = total - done;
      if (n > JY_FS_CHUNK_BYTES)
        n = JY_FS_CHUNK_BYTES;
      size_t got;
      int e = base_read(NULL, m->base, start + done, page.base + done, n, &got);
      if (e)
        return e;
      if (got != n)
        return EIO;
      done += got;
    }
  }
  memcpy(page.bytes, page.base, GROUP);
  char key[64];
  uint8_t first[BLOCK];
  size_t n;
  groupkey(m->id, m->epoch, group, key);
  int e = get(key, first, sizeof first, &n);
  page.generation = page.old_generation = 0;
  page.extra = page.old_extra = 0;
  if (e != ENOENT) {
    if (e)
      return e;
    uint32_t len;
    if ((e = header(first, n, &len, &page.generation, &page.extra,
                    &page.old_generation, &page.old_extra)))
      return e;
    size_t copied = len < FIRST ? len : FIRST;
    memcpy(compressed, first + 28, copied);
    for (unsigned i = 0; i < page.extra; i++) {
      size_t got;
      shardkey(m->id, m->epoch, group, page.generation, i, key);
      e = get(key, compressed + copied, COMP_CAP - copied, &got);
      size_t expected = len - copied;
      if (expected > BLOCK)
        expected = BLOCK;
      if (e)
        return e;
      if (got != expected)
        return EIO;
      copied += got;
    }
    if (copied != len)
      return EIO;
    if (len) {
      uLongf full =
          GROUP; /* Inflate XOR differences in bounded output slices. */
      z_stream stream;
      memset(&stream, 0, sizeof stream);
      if (inflateInit(&stream) != Z_OK)
        return ENOMEM;
      stream.next_in = compressed;
      stream.avail_in = (uInt)len;
      uint8_t out[4064];
      size_t at = 0;
      int z;
      do {
        stream.next_out = out;
        stream.avail_out = sizeof out;
        z = inflate(&stream, Z_NO_FLUSH);
        size_t produced = sizeof out - stream.avail_out;
        if (at + produced > GROUP) {
          inflateEnd(&stream);
          return EIO;
        }
        for (size_t j = 0; j < produced; j++)
          page.bytes[at + j] ^= out[j];
        at += produced;
      } while (z == Z_OK);
      full = (uLongf)at;
      int trailing = stream.avail_in;
      inflateEnd(&stream);
      if (z != Z_STREAM_END || full != GROUP || trailing)
        return EIO;
    }
  }
  size_t valid =
      m->size > start
          ? (m->size - start > GROUP ? GROUP : (size_t)(m->size - start))
          : 0;
  if (valid < GROUP)
    memset(page.bytes + valid, 0, GROUP - valid);
  page.valid = 1;
  page.id = m->id;
  page.epoch = m->epoch;
  page.index = group;
  return 0;
}
static int commit_page(const meta *m, uint64_t visible_size) {
  if (!page.valid || page.id != m->id || page.epoch != m->epoch)
    return EIO;
  int e = delete_shards(m->id, m->epoch, page.index, page.old_generation,
                        page.old_extra);
  if (e)
    return e;
  page.old_extra = 0;
  uint64_t start = (uint64_t)page.index * GROUP;
  size_t valid = visible_size > start ? (visible_size - start > GROUP
                                             ? GROUP
                                             : (size_t)(visible_size - start))
                                      : 0;
  int nonzero = 0;
  for (size_t i = 0; i < valid; i++)
    if (page.bytes[i] != page.base[i]) {
      nonzero = 1;
      break;
    }
  uLongf count = 0;
  if (nonzero) {
    for (size_t i = 0; i < valid; i++)
      page.bytes[i] ^= page.base[i];
    if (valid < GROUP)
      memset(page.bytes + valid, 0, GROUP - valid);
    count = COMP_CAP;
    int z = compress_group(&count);
    for (size_t i = 0; i < valid; i++)
      page.bytes[i] ^= page.base[i];
    if (z != Z_OK)
      return z == Z_MEM_ERROR ? ENOMEM : EIO;
  }
  if (!count && !page.generation)
    return 0;
  uint32_t generation = page.generation + 1;
  if (!generation)
    return EFBIG;
  uint16_t extra =
      count > FIRST ? (uint16_t)((count - FIRST + BLOCK - 1) / BLOCK) : 0;
  char key[64];
  size_t copied = count < FIRST ? count : FIRST;
  if (extra) {
    uint8_t pending[36];
    memcpy(pending, "JYP2", 4);
    pxa_store_u64(pending + 4, m->id);
    pxa_store_u64(pending + 12, m->epoch);
    pxa_store_u32(pending + 20, page.index);
    pxa_store_u32(pending + 24, generation);
    pxa_store_u16(pending + 28, extra);
    pxa_store_u16(pending + 30, 0);
    pxa_store_u32(pending + 32, checksum(pending, 32));
    if ((e = set("jyf.pending", pending, sizeof pending)))
      return e;
    recovered = 0;
  }
  for (unsigned i = 0; i < extra; i++) {
    size_t n = count - copied;
    if (n > BLOCK)
      n = BLOCK;
    shardkey(m->id, m->epoch, page.index, generation, i, key);
    if ((e = set(key, compressed + copied, n)))
      return e;
    copied += n;
  }
  uint8_t first[BLOCK];
  memcpy(first, "JYG2", 4);
  pxa_store_u32(first + 4, (uint32_t)count);
  pxa_store_u32(first + 8, generation);
  pxa_store_u16(first + 12, extra);
  pxa_store_u16(first + 14, page.extra);
  pxa_store_u32(first + 16, page.generation);
  pxa_store_u32(first + 20, 0);
  pxa_store_u32(first + 24, checksum(first, 24));
  size_t take = count < FIRST ? count : FIRST;
  memcpy(first + 28, compressed, take);
  groupkey(m->id, m->epoch, page.index, key);
  if ((e = set(key, first, 28 + take)))
    return e;
  uint32_t previous = page.generation;
  uint16_t oldextra = page.extra;
  page.generation = generation;
  page.extra = extra;
  page.old_generation = previous;
  page.old_extra = oldextra;
  if ((e = delete_shards(m->id, m->epoch, page.index, previous, oldextra)))
    return e;
  page.old_extra = 0;
  if (!count) {
    if ((e = del(key)))
      return e;
    page.generation = 0;
    page.extra = 0;
  }
  if (extra) {
    if ((e = del("jyf.pending")))
      return e;
    recovered = 1;
  }
  return 0;
}
static int overlay_read(void *ignored, const char *p, uint64_t off, void *dst,
                        size_t n, size_t *got) {
  (void)ignored;
  *got = 0;
  meta m;
  int e = meta_for(p, &m);
  if (e)
    return e;
  if (m.kind != JY_FS_FILE)
    return m.kind == TOMB ? ENOENT : EISDIR;
  if (off >= m.size)
    return 0;
  if (n > JY_FS_CHUNK_BYTES)
    return EINVAL;
  if (n > m.size - off)
    n = (size_t)(m.size - off);
  while (*got < n) {
    uint64_t at = off + *got;
    size_t within = (size_t)(at % GROUP), take = GROUP - within;
    if (take > n - *got)
      take = n - *got;
    if ((e = load_page(&m, (uint32_t)(at / GROUP))))
      return *got ? 0 : e;
    memcpy((uint8_t *)dst + *got, page.bytes + within, take);
    *got += take;
  }
  return 0;
}
static int grow_zero(meta *m, uint64_t size) {
  while (m->size < size) {
    int e = load_page(m, (uint32_t)(m->size / GROUP));
    if (e)
      return e;
    uint64_t next = ((m->size / GROUP) + 1) * GROUP;
    if (next > size)
      next = size;
    memset(page.bytes + m->size % GROUP, 0, (size_t)(next - m->size));
    if ((e = commit_page(m, next))) {
      page.valid = 0;
      return e;
    }
    meta candidate = *m;
    candidate.size = next;
    if ((e = store(&candidate))) {
      page.valid = 0;
      return e;
    }
    *m = candidate;
  }
  return 0;
}
static int overlay_write(void *ignored, const char *p, uint64_t off,
                         const void *src, size_t n, size_t *put) {
  (void)ignored;
  *put = 0;
  if (n > GROUP)
    return EINVAL;
  if (off > MAX_FILE_BYTES || n > MAX_FILE_BYTES - off)
    return EFBIG;
  meta m;
  int e = meta_for(p, &m);
  if (e)
    return e;
  if (m.kind != JY_FS_FILE)
    return m.kind == TOMB ? ENOENT : EISDIR;
  if ((e = gc(&m)))
    return e;
  if (off > m.size && (e = grow_zero(&m, off)))
    return e;
  while (*put < n) {
    uint64_t at = off + *put;
    size_t within = (size_t)(at % GROUP), take = GROUP - within;
    if (take > n - *put)
      take = n - *put;
    if ((e = load_page(&m, (uint32_t)(at / GROUP))))
      return *put ? 0 : e;
    memcpy(page.bytes + within, (const uint8_t *)src + *put, take);
    uint64_t visible = m.size > at + take ? m.size : at + take;
    if ((e = commit_page(&m, visible))) {
      page.valid = 0;
      return *put ? 0 : e;
    }
    if (visible > m.size) {
      meta next = m;
      next.size = visible;
      if ((e = store(&next))) {
        page.valid = 0;
        return *put ? 0 : e;
      }
      m = next;
    }
    *put += take;
  }
  return 0;
}
static int parent_exists(const char *p) {
  char parent[256];
  strcpy(parent, p);
  char *slash = strrchr(parent, '/');
  if (!slash)
    return 0;
  *slash = 0;
  meta m;
  int e = meta_for(parent, &m);
  if (!e)
    return m.kind == JY_FS_DIRECTORY ? 0 : m.kind == TOMB ? ENOENT : ENOTDIR;
  if (e != ENOENT)
    return e;
  jy_fs_info i;
  e = jy_asset_stat(NULL, parent, &i);
  return e ? e : i.kind == JY_FS_DIRECTORY ? 0 : ENOTDIR;
}
static int resize(void *ignored, const char *p, uint64_t size) {
  (void)ignored;
  if (size > MAX_FILE_BYTES)
    return EFBIG;
  meta m;
  int e = meta_for(p, &m);
  if (e && e != ENOENT)
    return e;
  if (!e && m.kind == JY_FS_DIRECTORY)
    return EISDIR;
  if (!e && m.kind == TOMB && (e = gc(&m)))
    return e;
  if (e == ENOENT || m.kind == TOMB) {
    if ((e = parent_exists(p)))
      return e;
    memset(&m, 0, sizeof m);
    strcpy(m.path, p);
    const char *alias = jy_pxa_base_alias(p);
    if (!alias || jy_fs_normalize(alias, m.base))
      return EINVAL;
    m.kind = JY_FS_FILE;
    m.epoch = 1;
    if ((e = allocate_id(&m.id)))
      return e;
    if ((e = store(&m)))
      return e;
    return size ? grow_zero(&m, size) : 0;
  }
  if ((e = gc(&m)))
    return e;
  if (size == m.size)
    return 0;
  if (size > m.size)
    return grow_zero(&m, size);
  meta next = m;
  next.size = size;
  next.gcid = m.id;
  next.gcepoch = m.epoch;
  next.gcfirst = blocks(size);
  next.gcend = blocks(m.size);
  if (!size) {
    if (++next.epoch == 0)
      return EFBIG;
    next.gcfirst = 0;
  }
  page.valid = 0;
  if ((e = store(&next)))
    return e;
  return gc(&next);
}
static int remove_file(void *ignored, const char *p) {
  (void)ignored;
  meta m;
  int e = meta_for(p, &m);
  if (e == ENOENT) {
    jy_fs_info i;
    e = jy_asset_stat(NULL, p, &i);
    if (e)
      return e;
    if (i.kind != JY_FS_FILE)
      return EISDIR;
    memset(&m, 0, sizeof m);
    strcpy(m.path, p);
    m.epoch = 1;
    if ((e = allocate_id(&m.id)))
      return e;
  } else if (e)
    return e;
  else if (m.kind == TOMB)
    return ENOENT;
  else if (m.kind != JY_FS_FILE)
    return EISDIR;
  if ((e = gc(&m)))
    return e;
  m.kind = TOMB;
  m.gcid = m.id;
  m.gcepoch = m.epoch;
  m.gcfirst = 0;
  m.gcend = blocks(m.size);
  m.size = 0;
  if ((e = store(&m)) || (e = gc(&m)))
    return e;
  return jy_asset_lookup(p) ? 0 : erase_meta(p);
}
static int rename_file(void *ignored, const char *from, const char *to) {
  (void)ignored;
  if (!strcmp(from, to))
    return 0;
  meta src, dst;
  int e = meta_for(from, &src);
  if (e == ENOENT) {
    jy_fs_info info;
    e = jy_asset_stat(NULL, from, &info);
    if (e)
      return e;
    if (info.kind != JY_FS_FILE)
      return EISDIR;
    memset(&src, 0, sizeof src);
    strcpy(src.path, from);
    strcpy(src.base, from);
    src.kind = JY_FS_FILE;
    src.epoch = 1;
    src.size = info.size;
    if ((e = allocate_id(&src.id)) || (e = store(&src)))
      return e;
  } else if (e)
    return e;
  if (src.kind != JY_FS_FILE)
    return src.kind == TOMB ? ENOENT : EISDIR;
  if ((e = parent_exists(to)) || (e = gc(&src)))
    return e;
  e = load(to, &dst);
  if (e && e != ENOENT)
    return e;
  if (!e && dst.kind == JY_FS_DIRECTORY)
    return EISDIR;
  if (!e && (e = gc(&dst)))
    return e;
  meta dest = src, old = src;
  strcpy(dest.path, to);
  if (!e && dst.kind == JY_FS_FILE) {
    dest.gcid = dst.id;
    dest.gcepoch = dst.epoch;
    dest.gcfirst = 0;
    dest.gcend = blocks(dst.size);
  }
  old.kind = TOMB;
  old.size = 0;
  old.gcid = old.gcepoch = 0;
  old.gcfirst = old.gcend = 0;
  uint8_t journal[2 * META_BYTES + 4];
  size_t a = encode(&dest, journal + 4), b = encode(&old, journal + 4 + a);
  pxa_store_u16(journal, (uint16_t)a);
  pxa_store_u16(journal + 2, (uint16_t)b);
  if ((e = set("jyf.tx", journal, 4 + a + b)))
    return e;
  recovered = 0;
  cache_drop(from);
  cache_drop(to);
  if ((e = recover()))
    return e;
  if ((e = gc(&dest)))
    return e;
  if (!jy_asset_lookup(from))
    return erase_meta(from);
  return 0;
}
static int mkdir_file(void *ignored, const char *p) {
  (void)ignored;
  meta m;
  int e = meta_for(p, &m);
  if (!e && m.kind != TOMB)
    return EEXIST;
  if (e && e != ENOENT)
    return e;
  if ((e = parent_exists(p)))
    return e;
  memset(&m, 0, sizeof m);
  strcpy(m.path, p);
  m.kind = JY_FS_DIRECTORY;
  m.epoch = 1;
  if ((e = allocate_id(&m.id)))
    return e;
  return store(&m);
}
/* Publish a complete immutable-base clone in one metadata SET. A failed
 * materialize therefore cannot leave a partial copy or whiteout the resource.
 */
static int clone_base(void *ignored, const char *from, const char *to) {
  (void)ignored;
  meta source, previous;
  int e = meta_for(from, &source);
  if (!e) {
    if (source.kind == TOMB)
      return ENOENT;
    if (source.kind == JY_FS_FILE && !strcmp(from, to))
      return 0;
    return source.kind == JY_FS_DIRECTORY ? EISDIR : EEXIST;
  }
  if (e != ENOENT)
    return e;
  const jy_asset_entry *asset = jy_asset_lookup(from);
  if (!asset)
    return ENOENT;
  if (asset->bytes > MAX_FILE_BYTES)
    return EFBIG;
  if ((e = parent_exists(to)))
    return e;
  e = load(to, &previous);
  if (!e) {
    if (previous.kind != TOMB)
      return previous.kind == JY_FS_DIRECTORY ? EISDIR : EEXIST;
    if ((e = gc(&previous)))
      return e;
  } else if (e != ENOENT)
    return e;
  meta copy;
  memset(&copy, 0, sizeof copy);
  strcpy(copy.path, to);
  strcpy(copy.base, from);
  copy.kind = JY_FS_FILE;
  copy.epoch = 1;
  copy.size = asset->bytes;
  if ((e = allocate_id(&copy.id)))
    return e;
  return store(&copy);
}
int jy_pxa_fs_bind(void) {
  jy_fs_stats previous;
  jy_fs_get_stats(&previous);
  if (previous.open_files || jy_pxa_fs_pending()) {
    errno = EBUSY;
    return -1;
  }
  memset(cache, 0, sizeof cache);
  memset(&page, 0, sizeof page);
  memset(&request, 0, sizeof request);
  memset(&stats, 0, sizeof stats);
  cache_next = 0;
  cancelled = recovered = recovering = clock_seen = files_ready = 0;
  now_us = 0;
  jy_fs_ops assets = {NULL, base_stat, base_read, NULL, NULL,
                      NULL, NULL,      NULL,      NULL};
  jy_fs_ops overlay = {NULL,          overlay_stat, overlay_read,
                       overlay_write, resize,       remove_file,
                       rename_file,   mkdir_file,   clone_base, GROUP};
  return jy_fs_bind(&assets, &overlay);
}
void jy_pxa_fs_get_stats(jy_pxa_fs_stats *out) {
  if (out)
    *out = stats;
}
