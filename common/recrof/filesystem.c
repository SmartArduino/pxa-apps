/* stdio/POSIX bridge to the app-private PXA FS service. GPL-3.0-or-later. */
#include "port.h"
#include "pxa_assets.h"
#include "pxa_fs.h"
#include "pxa_fs_roots.h"
#include "rpc.h"
#include "external.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define SLOT_COUNT 64
#define FD_BASE 100
#ifdef PORT_LOADING_TRACE
static unsigned loading_opens,loading_reads,loading_seeks,loading_stats,loading_missing_hits;
static char loading_path[96];
void port_filesystem_loading_stats(void) {
  port_log("PXA LOAD FS opens=%u reads=%u seeks=%u stats=%u miss_cache=%u last_open=%s",
    loading_opens,loading_reads,loading_seeks,loading_stats,loading_missing_hits,loading_path);
}
#endif
/* A single Guest owns its private namespace. RTS filename resolution may
 * cache a spelling only until the next possible namespace mutation. */
#ifdef PORT_RTS
#include "missing_paths.h"
static port_missing_paths_t missing_paths;
static int loading_missing_cache;
void port_filesystem_loading_cache(int active) {
  active=!!active;
  if(loading_missing_cache==active)return;
  loading_missing_cache=active;
  memset(&missing_paths,0,sizeof(missing_paths));
}
static uint64_t namespace_revision;
uint64_t port_filesystem_revision(void) { return namespace_revision; }
#define NAMESPACE_CHANGED() (++namespace_revision)
#else
#define NAMESPACE_CHANGED() ((void)0)
#endif
static struct File {
  uint64_t handle,features,read_cookie;
  int used, eof, error, asset, external;
  uint32_t size;
  long position;
  union {
    char asset_path[256];
    struct dirent entry;
    char entry_storage[512];
  };
} files[SLOT_COUNT];
/* Two pages amortize small INI/header reads without per-file buffers. Signed
 * assets reuse pages across close/reopen; read-only SD pages belong to one
 * open handle and are invalidated on close. Private reads are uncached. */
static struct {
  uint64_t external_handle;
  char path[256];
  uint32_t offset, bytes, age;
  uint8_t data[PXA_FS_READ_AT_MAX_BYTES>PXA_ASSETS_READ_MAX_BYTES ?
               PXA_FS_READ_AT_MAX_BYTES:PXA_ASSETS_READ_MAX_BYTES];
} read_cache[2];
static uint32_t cache_age;
static unsigned cached_reads;
static void cached_read_cooperate(void) {
  if((++cached_reads&255u)==0 &&
      port_cooperate_due(port_time_us(),PORT_COOPERATE_BUDGET_US))port_sleep(0);
}
static void invalidate_external_page(uint64_t handle) {
  for(unsigned i=0;i<2;++i)
    if(read_cache[i].external_handle==handle)read_cache[i].bytes=0;
}
#ifdef PORT_CNC_MEDIA_HANDLES
#ifndef PORT_RTS
#error "RTS archive reuse requires tracked private namespace mutations"
#endif
/* MIX archives are read-only resources. Retain at most two idle Host handles
 * for half a second, preserving cookies and resetting only the Guest cursor.
 * Private writes invalidate the overlay decision before the next reuse. */
static struct {
  uint64_t handle,features,cookie,revision,expires;
  uint32_t size;
  char path[256];
} media_handles[2];
static void expire_media_handles(uint64_t now) {
  for(unsigned i=0;i<2;++i)if(media_handles[i].handle &&
      (media_handles[i].revision!=namespace_revision || media_handles[i].expires<=now)) {
    invalidate_external_page(media_handles[i].handle);
    pxa_close_handle(media_handles[i].handle);media_handles[i].handle=0;
  }
}
void port_filesystem_tick(uint64_t now) { expire_media_handles(now); }
static int reuse_media_handle(const char *path,struct File *file) {
  expire_media_handles(port_time_us());
  for(unsigned i=0;i<2;++i)if(media_handles[i].handle && !strcmp(media_handles[i].path,path)) {
    memset(file,0,sizeof(*file));file->used=file->external=1;
    file->handle=media_handles[i].handle;file->features=media_handles[i].features;
    file->read_cookie=media_handles[i].cookie;file->size=media_handles[i].size;
    strcpy(file->asset_path,path);media_handles[i].handle=0;return 1;
  }
  return 0;
}
static int retain_media_handle(struct File *file) {
  size_t length=strlen(file->asset_path);
  if(file->error || !(file->features&PXA_FS_FILE_FEATURE_READ_AT) ||
      length<4 || strcmp(file->asset_path+length-4,".mix"))return 0;
  uint64_t now=port_time_us();expire_media_handles(now);
  unsigned victim=0;
  for(unsigned i=0;i<2;++i)
    if(!media_handles[i].handle || (media_handles[victim].handle &&
        media_handles[i].expires<media_handles[victim].expires))victim=i;
  if(media_handles[victim].handle) {
    invalidate_external_page(media_handles[victim].handle);
    pxa_close_handle(media_handles[victim].handle);
  }
  media_handles[victim].handle=file->handle;media_handles[victim].features=file->features;
  media_handles[victim].cookie=file->read_cookie;media_handles[victim].size=file->size;
  media_handles[victim].revision=namespace_revision;media_handles[victim].expires=now+500000;
  strcpy(media_handles[victim].path,file->asset_path);return 1;
}
#endif
static char cwd[256] = "/data";
static struct File *slot(int fd) {
  return fd >= FD_BASE && fd < FD_BASE + SLOT_COUNT && files[fd - FD_BASE].used
             ? &files[fd - FD_BASE]
             : NULL;
}
static int filefd(FILE *f) {
  for (int i = 0; i < SLOT_COUNT; i++)
    if (f == (FILE *)&files[i] && files[i].used)
      return FD_BASE + i;
  return -1;
}
static int path(const char *s, char *out) {
  if (!s || !*s) {
    errno = ENOENT;
    return 0;
  }
  size_t n = strlen(s);
  if (n > 230) {
    errno = ENAMETOOLONG;
    return 0;
  }
  if (s[0] == '/')
    snprintf(out, 256, "%s", s);
  else
    snprintf(out, 256, "%s/%s", cwd, s);
#ifdef PORT_CNC_CANONICAL_PATHS
  /* The DOS-facing RTS namespace uses the same lowercase spelling as the
   * resource importer. Apply it to private writes as well as reads. */
  for(char *p=out;*p;++p)if(*p>='A'&&*p<='Z')*p+='a'-'A';
#endif
  /* Engines are confined to /data; strip redundant ./ segments. */
  for (char *p; (p = strstr(out, "/./"));)
    memmove(p, p + 2, strlen(p + 2) + 1);
  for (char *p; (p = strstr(out, "//"));)
    memmove(p, p + 1, strlen(p + 1) + 1);
  size_t len = strlen(out);
  if (len > 2 && out[len - 1] == '.' && out[len - 2] == '/')
    out[len -= 2] = 0;
  if (len > 1 && out[len - 1] == '/')
    out[len - 1] = 0;
  if (strstr(out, "/../") || strstr(out, "/..\0") ||
      (strncmp(out, "/data", 5) != 0) || (out[5] && out[5] != '/')) {
    errno = EACCES;
    return 0;
  }
  memmove(out, out + 1, strlen(out));
  return 1;
}
static int simple(uint16_t op, const char *s) {
  NAMESPACE_CHANGED();
  char p[256];
  uint8_t packet[320];
  uint32_t n;
  if (!path(s, p) ||
      !pxa_fs_build_path(packet, sizeof(packet), op, 1, p, strlen(p), &n))
    return -1;
  const pxa_event_t *r = port_rpc_packet(packet, n);
  if (!r || r->payload_size < 4 || pxa_load_u32(r->payload)) {
    errno = EIO;
    return -1;
  }
  return 0;
}
static int asset_info(const char *p, char *name, pxa_asset_result_t *info) {
#ifdef PORT_EXTERNAL_GAME_DATA
  /* SD variants contain code only. A missing SD resource is not a request
   * to consult a second, absent package archive. */
  (void)p;(void)name;(void)info;return 0;
#else
  if (strncmp(p, "data/", 5))
    return 0;
  snprintf(name, 256, "assets/game/%s", p + 5);
  for (char *q = name; *q; q++)
    *q = tolower((unsigned char)*q);
  uint8_t packet[320];
  uint32_t n;
  if (!pxa_assets_build(packet, sizeof(packet), PXA_ASSETS_QUERY, 1, name, 0,
                        &n))
    return 0;
  const pxa_event_t *e = port_rpc_packet(packet, n);
  return e && pxa_assets_parse_result(e, e->token, PXA_ASSETS_QUERY, info) &&
         !info->status && info->info.kind == PXA_ASSET_BLOB;
#endif
}
static int openfile(const char *name, uint32_t flags) {
#ifdef PORT_LOADING_TRACE
  ++loading_opens;snprintf(loading_path,sizeof(loading_path),"%s",name?name:"");
#endif
  if(flags & (PXA_FS_OPEN_CREATE|PXA_FS_OPEN_TRUNCATE)) NAMESPACE_CHANGED();
  char p[256];
  if (!path(name, p))
    return -1;
#ifdef PORT_RTS
  if(flags==PXA_FS_OPEN_READ && port_missing_path_hit(&missing_paths,p,
      port_time_us(),namespace_revision)) {
#ifdef PORT_LOADING_TRACE
    ++loading_missing_hits;
#endif
    errno=ENOENT;return -1;
  }
#endif
  int i;
  for (i = 0; i < SLOT_COUNT && files[i].used; i++)
    ;
  if (i == SLOT_COUNT) {
    errno = EMFILE;
    return -1;
  }
#ifdef PORT_CNC_MEDIA_HANDLES
  if(flags==PXA_FS_OPEN_READ && reuse_media_handle(p,&files[i]))return i+FD_BASE;
#endif
  uint8_t packet[320];
  uint32_t n;
  if (!pxa_fs_build_open(packet, sizeof(packet), 1, p, strlen(p), flags, &n)) {
    errno = EINVAL;
    return -1;
  }
  const pxa_event_t *e = port_rpc_packet(packet, n);
  pxa_fs_open_result_t r={0};
  if (!e || !pxa_fs_parse_open(e, e->token, &r) || r.status) {
#ifdef PORT_RTS
    const int private_missing=e && r.status==PXA_STATUS_NOT_FOUND;
#endif
    if (flags == PXA_FS_OPEN_READ) {
      uint64_t external_handle,features;uint32_t external_size;
      if(port_external_open(p,&external_handle,&external_size,&features)) {
        memset(&files[i],0,sizeof(files[i]));files[i].used=files[i].external=1;
        files[i].handle=external_handle;files[i].size=external_size;files[i].features=features;
        strcpy(files[i].asset_path,p);
        return i+FD_BASE;
      }
      pxa_asset_result_t info;
      char name[256];
      if (asset_info(p, name, &info)) {
        memset(&files[i], 0, sizeof(files[i]));
        files[i].used = files[i].asset = 1;
        files[i].size = info.info.decoded_bytes;
        strcpy(files[i].asset_path, name);
        return i + FD_BASE;
      }
#ifdef PORT_RTS
      /* Only explicit NOT_FOUND from both roots is reusable. I/O errors,
       * denied access and unmounted media must be retried normally. */
      if(private_missing && port_external_open_status()==PXA_STATUS_NOT_FOUND)
        /* Startup probes span many seconds of SD RPCs. The first real frame
         * discards this longer-lived working set; private writes invalidate it. */
        port_missing_path_store_for(&missing_paths,p,port_time_us(),namespace_revision,
            loading_missing_cache?UINT64_C(300000000):UINT64_C(1000000));
#endif
    }
    errno = ENOENT;
    return -1;
  }
  memset(&files[i], 0, sizeof(files[i]));
  files[i].used = 1;
  files[i].handle = r.handle;
  return i + FD_BASE;
}
int __wrap_open(const char *s, int flags, ...) {
  uint32_t f = (flags & O_RDWR) ? 3 : (flags & O_WRONLY) ? 2 : 1;
  if (flags & O_CREAT)
    f |= PXA_FS_OPEN_CREATE;
  if (flags & O_TRUNC)
    f |= PXA_FS_OPEN_TRUNCATE;
  if (flags & O_APPEND)
    f |= PXA_FS_OPEN_APPEND;
  return openfile(s, f);
}
int __wrap_close(int fd) {
  struct File *f = slot(fd);
  if (!f) {
    errno = EBADF;
    return -1;
  }
  int retained=0;
#ifdef PORT_CNC_MEDIA_HANDLES
  if(f->external)retained=retain_media_handle(f);
#endif
  if (!f->asset && !retained)
    pxa_close_handle(f->handle);
  if(f->external && !retained)invalidate_external_page(f->handle);
  f->used = 0;
  return 0;
}
ssize_t __wrap_read(int fd, void *p, size_t n) {
#ifdef PORT_LOADING_TRACE
  ++loading_reads;
#endif
  struct File *f = slot(fd);
  if (!f) {
    errno = EBADF;
    return -1;
  }
  size_t done = 0;
  if (f->asset) {
    while (done < n && (uint32_t)f->position < f->size) {
      unsigned victim=0;
      int hit=-1;
      for(unsigned i=0;i<2;++i) {
        if(read_cache[i].bytes && !read_cache[i].external_handle &&
            !strcmp(read_cache[i].path,f->asset_path) &&
            (uint32_t)f->position>=read_cache[i].offset &&
            (uint32_t)f->position-read_cache[i].offset<read_cache[i].bytes) hit=i;
        if(read_cache[i].age<read_cache[victim].age)victim=i;
      }
      if(hit>=0) {
        uint32_t within=(uint32_t)f->position-read_cache[hit].offset;
        size_t count=read_cache[hit].bytes-within;
        if(count>n-done)count=n-done;
        port_copy_read((uint8_t *)p+done,read_cache[hit].data+within,count);
        f->position+=count;done+=count;read_cache[hit].age=++cache_age;
        cached_read_cooperate();
        continue;
      }
      uint32_t count = n-done > PXA_ASSETS_READ_MAX_BYTES
                            ? PXA_ASSETS_READ_MAX_BYTES : n-done;
      uint32_t offset=f->position;
      int prefetch=count<512;
      if(prefetch) {
        read_cache[victim].external_handle=0;
        offset=offset/PXA_ASSETS_READ_MAX_BYTES*PXA_ASSETS_READ_MAX_BYTES;
        count=f->size-offset;
      }
      if (count > PXA_ASSETS_READ_MAX_BYTES)
        count = PXA_ASSETS_READ_MAX_BYTES;
      uint8_t packet[320];
      uint32_t size;
      pxa_assets_build_read(packet, sizeof(packet), 1, f->asset_path,
                            offset, count, &size);
      const pxa_event_t *e = port_rpc_packet(packet, size);
      pxa_asset_read_result_t r;
      if (!e || !pxa_assets_parse_read(e, e->token, &r) || r.status ||
          r.offset!=offset || r.total_bytes!=f->size || r.bytes>count) {
        f->error = 1;
        errno = EIO;
        return done ? (ssize_t)done : -1;
      }
      if(prefetch) {
        strcpy(read_cache[victim].path,f->asset_path);
        read_cache[victim].offset=offset;read_cache[victim].bytes=r.bytes;
        read_cache[victim].age=++cache_age;
        port_copy_read(read_cache[victim].data,r.data,r.bytes);
        if (!r.bytes || (uint32_t)f->position-offset>=r.bytes) {
          f->error=1;errno=EIO;return done ? (ssize_t)done : -1;
        }
        continue;
      }
      port_copy_read((uint8_t *)p + done, r.data, r.bytes);
      done += r.bytes;
      f->position += r.bytes;
      if (!r.bytes)
        break;
    }
    f->eof = (uint32_t)f->position == f->size;
    return done;
  }
  if(f->external && (f->features & PXA_FS_FILE_FEATURE_READ_AT)) {
    uint8_t packet[24+PXA_FS_READ_AT_MAX_BYTES];
    while(done<n) {
      if((uint32_t)f->position>=f->size){f->eof=1;break;}
      if(n-done<512) {
        unsigned victim=0;int hit=-1;
        for(unsigned i=0;i<2;++i) {
          if(read_cache[i].bytes && read_cache[i].external_handle==f->handle &&
             (uint32_t)f->position>=read_cache[i].offset &&
             (uint32_t)f->position-read_cache[i].offset<read_cache[i].bytes)hit=i;
          if(read_cache[i].age<read_cache[victim].age)victim=i;
        }
        if(hit>=0) {
          uint32_t within=(uint32_t)f->position-read_cache[hit].offset;
          size_t count=read_cache[hit].bytes-within;if(count>n-done)count=n-done;
          port_copy_read((uint8_t *)p+done,read_cache[hit].data+within,count);
          f->position+=count;done+=count;read_cache[hit].age=++cache_age;
          cached_read_cooperate();continue;
        }
        uint32_t offset=(uint32_t)f->position/PXA_FS_READ_AT_MAX_BYTES*PXA_FS_READ_AT_MAX_BYTES;
        uint32_t count=f->size-offset;
        if(count>PXA_FS_READ_AT_MAX_BYTES)count=PXA_FS_READ_AT_MAX_BYTES;
        int r=pxa_fs_read_at(f->handle,offset,++f->read_cookie,packet,24+count);
        if(r<=0 || (uint32_t)r>count || (uint32_t)f->position-offset>=(uint32_t)r) {
          f->error=1;errno=EIO;return done?(ssize_t)done:-1;
        }
        read_cache[victim].external_handle=f->handle;
        read_cache[victim].offset=offset;read_cache[victim].bytes=r;
        read_cache[victim].age=++cache_age;
        port_copy_read(read_cache[victim].data,packet+24,r);
        if(port_cooperate_due(port_time_us(),PORT_COOPERATE_BUDGET_US))port_sleep(0);
        continue;
      }
      uint32_t count=n-done>4096 ? 4096 : n-done;
      int r=pxa_fs_read_at(f->handle,f->position,++f->read_cookie,packet,24+count);
      if(r<0) {f->error=1;errno=EIO;return done?(ssize_t)done:-1;}
      port_copy_read((uint8_t *)p+done,packet+24,r);done+=r;f->position+=r;
      if(port_cooperate_due(port_time_us(),PORT_COOPERATE_BUDGET_US))port_sleep(0);
      if(!r) {f->eof=1;break;}
    }
    return done;
  }
  while (done < n) {
    size_t maximum=f->external?4096:32768;
    uint32_t chunk = n - done > maximum ? maximum : n - done;
    int r = pxa_fs_read(f->handle, (uint8_t *)p + done, chunk);
    if (r < 0) {
      f->error = 1;
      errno = EIO;
      return done ? (ssize_t)done : -1;
    }
    done += r;
    f->position += r;
    /* A synchronous Host read can consume most of a callback deadline.
     * Return to the event loop before the engine issues its next seek. */
    if(port_cooperate_due(port_time_us(),PORT_COOPERATE_BUDGET_US)) port_sleep(0);
    if (r == 0) {
      f->eof = 1;
      break;
    }
  }
  return done;
}
ssize_t __wrap_write(int fd, const void *p, size_t n) {
  struct File *f = slot(fd);
  if (!f) {
    errno = EBADF;
    return -1;
  }
  size_t done = 0;
  while (done < n) {
    uint32_t chunk = n - done > 32768 ? 32768 : n - done;
    int r = pxa_fs_write(f->handle, (uint8_t *)p + done, chunk);
    if (r <= 0) {
      f->error = 1;
      errno = EIO;
      return done ? (ssize_t)done : -1;
    }
    done += r;
    f->position += r;
    if(port_cooperate_due(port_time_us(),PORT_COOPERATE_BUDGET_US)) port_sleep(0);
  }
  return done;
}
off_t __wrap_lseek(int fd, off_t off, int whence) {
#ifdef PORT_LOADING_TRACE
  ++loading_seeks;
#endif
  struct File *f = slot(fd);
  if (!f) {
    errno = EBADF;
    return -1;
  }
  if (f->asset || (f->external && (f->features & PXA_FS_FILE_FEATURE_READ_AT))) {
    int64_t pos = (whence == SEEK_SET   ? 0
                   : whence == SEEK_CUR ? f->position
                   : whence == SEEK_END ? f->size
                                        : -INT64_MAX) +
                  off;
    if (pos < 0 || pos > INT32_MAX) {
      errno = EINVAL;
      return -1;
    }
    f->position = pos;
    f->eof = 0;
    return pos;
  }
  uint8_t packet[64];
  uint32_t n;
  if (!pxa_fs_build_seek(packet, sizeof(packet), 1, f->handle, off, whence, &n))
    return -1;
  const pxa_event_t *e = port_rpc_packet(packet, n);
  pxa_fs_seek_result_t r;
  if (!e || !pxa_fs_parse_seek(e, e->token, &r) || r.status) {
    errno = EIO;
    return -1;
  }
  f->position = r.position;
  f->eof = 0;
  return r.position;
}
FILE *__wrap_fopen(const char *p, const char *mode) {
  uint32_t flags = mode[0] == 'r' ? 1 : 2;
  if (strchr(mode, '+'))
    flags = 3;
  if (mode[0] == 'w')
    flags |= 4 | 16;
  if (mode[0] == 'a')
    flags |= 4 | 32;
  int fd = openfile(p, flags);
  return fd < 0 ? NULL : (FILE *)slot(fd);
}
int __wrap_fclose(FILE *f) { return __wrap_close(filefd(f)); }
size_t __wrap_fread(void *p, size_t size, size_t n, FILE *f) {
  if (!size || n > SIZE_MAX / size)
    return 0;
  ssize_t r = __wrap_read(filefd(f), p, size * n);
  return r < 0 ? 0 : (size_t)r / size;
}
size_t __wrap_fwrite(const void *p, size_t size, size_t n, FILE *f) {
  if (!size || n > SIZE_MAX / size)
    return 0;
  if (f == stdout || f == stderr) {
    char line[256];
    size_t len = size * n;
    if (len > 255)
      len = 255;
    memcpy(line, p, len);
    line[len] = 0;
    port_log("%s", line);
    return n;
  }
  ssize_t r = __wrap_write(filefd(f), p, size * n);
  return r < 0 ? 0 : (size_t)r / size;
}
int __wrap_fseek(FILE *f, long off, int w) {
  return __wrap_lseek(filefd(f), off, w) < 0 ? -1 : 0;
}
long __wrap_ftell(FILE *f) {
  struct File *s = slot(filefd(f));
  return s ? s->position : -1;
}
void __wrap_rewind(FILE *f) { __wrap_fseek(f, 0, SEEK_SET); }
int __wrap_fflush(FILE *f) {
  (void)f;
  return 0;
}
int __wrap_feof(FILE *f) {
  struct File *s = slot(filefd(f));
  return s ? s->eof : 1;
}
int __wrap_ferror(FILE *f) {
  struct File *s = slot(filefd(f));
  return s ? s->error : 1;
}
void __wrap_clearerr(FILE *f) {
  struct File *s = slot(filefd(f));
  if (s)
    s->eof = s->error = 0;
}
int __wrap_fgetc(FILE *f) {
  unsigned char c;
  return __wrap_fread(&c, 1, 1, f) == 1 ? c : EOF;
}
int __wrap_getc(FILE *f) { return __wrap_fgetc(f); }
int __wrap_fputc(int c, FILE *f) {
  unsigned char b = c;
  return __wrap_fwrite(&b, 1, 1, f) == 1 ? b : EOF;
}
int __wrap_putc(int c, FILE *f) { return __wrap_fputc(c, f); }
char *__wrap_fgets(char *s, int n, FILE *f) {
  int i = 0, c;
  if (n <= 1)
    return NULL;
  while (i < n - 1 && (c = __wrap_fgetc(f)) != EOF) {
    s[i++] = c;
    if (c == '\n')
      break;
  }
  s[i] = 0;
  return i ? s : NULL;
}
int __wrap_fputs(const char *s, FILE *f) {
  return __wrap_fwrite(s, 1, strlen(s), f) == strlen(s) ? 0 : EOF;
}
int __wrap_fprintf(FILE *f, const char *fmt, ...) {
  char buf[2048];
  va_list a;
  va_start(a, fmt);
  int n = vsnprintf(buf, sizeof(buf), fmt, a);
  va_end(a);
  if (n < 0 || n >= (int)sizeof(buf)) {
    errno = EOVERFLOW;
    return -1;
  }
  return __wrap_fwrite(buf, 1, n, f) == (size_t)n ? n : -1;
}
int __wrap_printf(const char *fmt, ...) {
  char buf[256];
  va_list a;
  va_start(a, fmt);
  int n = vsnprintf(buf, sizeof(buf), fmt, a);
  va_end(a);
  port_log("%s", buf);
  return n;
}
int __wrap_puts(const char *s) {
  port_log("%s", s);
  return 0;
}
int __wrap_stat(const char *s, struct stat *st) {
#ifdef PORT_LOADING_TRACE
  ++loading_stats;
#endif
  char p[256];
  uint8_t packet[320];
  uint32_t n;
  if (!path(s, p) || !pxa_fs_build_path(packet, sizeof(packet), PXA_FS_STAT, 1,
                                        p, strlen(p), &n))
    return -1;
  const pxa_event_t *e = port_rpc_packet(packet, n);
  pxa_fs_stat_result_t r;
  if (!e || !pxa_fs_parse_stat(e, e->token, &r) || r.status) {
    uint8_t kind;uint64_t external_size;
    if(port_external_stat(p,&kind,&external_size)) {
      memset(st,0,sizeof(*st));st->st_size=external_size;
      st->st_mode=(kind==PXA_FS_KIND_DIRECTORY?S_IFDIR:S_IFREG)|0500;
      return 0;
    }
    pxa_asset_result_t info;
    char name[256];
    if (!asset_info(p, name, &info)) {
      errno = ENOENT;
      return -1;
    }
    r.size = info.info.decoded_bytes;
    r.kind = PXA_FS_KIND_REGULAR;
  }
  memset(st, 0, sizeof(*st));
  st->st_size = r.size;
  st->st_mode = (r.kind == PXA_FS_KIND_DIRECTORY ? S_IFDIR : S_IFREG) | 0700;
  return 0;
}
int __wrap_access(const char *s, int mode) {
  (void)mode;
  struct stat st;
  return __wrap_stat(s, &st);
}
int __wrap_mkdir(const char *s, mode_t mode) {
  (void)mode;
  /* Resource directories on read-only SD do not create the corresponding
   * private save directory. Query only the writable layer for mkdir. */
  char p[256];uint8_t packet[320];uint32_t n;
  if(!path(s,p) || !pxa_fs_build_path(packet,sizeof(packet),PXA_FS_STAT,1,
                                      p,strlen(p),&n))return -1;
  const pxa_event_t *e=port_rpc_packet(packet,n);
  pxa_fs_stat_result_t info;
  if(!e || !pxa_fs_parse_stat(e,e->token,&info)) {errno=EIO;return -1;}
  if(!info.status) {errno=EEXIST;return -1;}
  if(info.status!=PXA_STATUS_NOT_FOUND) {errno=EIO;return -1;}
  return simple(PXA_FS_MAKE_DIRECTORY,s);
}
int __wrap_unlink(const char *s) { return simple(PXA_FS_REMOVE, s); }
int __wrap_remove(const char *s) { return __wrap_unlink(s); }
static int move_file(const char *a, const char *b, unsigned opcode) {
  NAMESPACE_CHANGED();
  char p[256], q[256];
  uint8_t packet[600];
  uint32_t n;
  if (!path(a, p) || !path(b, q) ||
      !pxa_fs_build_move(packet, sizeof(packet), opcode, 1, p, strlen(p), q,
                           strlen(q), &n))
    return -1;
  const pxa_event_t *e = port_rpc(PXA_FS_SERVICE, opcode,
      packet + PXA_HEADER_BYTES, n - PXA_HEADER_BYTES);
  return !e || e->payload_size < 4 || (int32_t)pxa_load_u32(e->payload) ? -1
                                                                        : 0;
}
int __wrap_rename(const char *a, const char *b) {
  return move_file(a,b,PXA_FS_RENAME);
}
int port_replace(const char *a, const char *b) {
  /* FS 0.2 replace (8) has the same path records as rename. */
  return move_file(a,b,PXA_FS_REPLACE);
}
DIR *__wrap_opendir(const char *s) {
  int fd = openfile(s, PXA_FS_OPEN_READ | PXA_FS_OPEN_DIRECTORY);
  return fd < 0 ? NULL : (DIR *)slot(fd);
}
int __wrap_closedir(DIR *d) { return __wrap_close(filefd((FILE *)d)); }
struct dirent *__wrap_readdir(DIR *d) {
  struct File *f = (struct File *)d;
  uint8_t p[8];
  pxa_store_u64(p, f->handle);
  const pxa_event_t *e = port_rpc(PXA_FS_SERVICE, PXA_FS_READ_DIRECTORY, p, 8);
  pxa_fs_directory_result_t r;
  if (!e || !pxa_fs_parse_directory(e, e->token, &r) || r.status) {
    errno=EIO;
    return NULL;
  }
  if(r.end) return NULL;
  memset(&f->entry, 0, sizeof(f->entry));
  size_t n = r.name_size;
  if (n >= 256)
    n = 256 - 1;
  memcpy(f->entry.d_name, r.name, n);
  f->entry.d_name[n] = 0;
  f->entry.d_type = r.kind == PXA_FS_KIND_DIRECTORY ? DT_DIR : DT_REG;
  return &f->entry;
}
char *__wrap_getcwd(char *s, size_t n) {
  if (!s || strlen(cwd) >= n) {
    errno = ERANGE;
    return NULL;
  }
  strcpy(s, cwd);
  return s;
}
int __wrap_chdir(const char *s) {
  NAMESPACE_CHANGED();
  char p[256];
  struct stat st;
  if (!path(s, p) || __wrap_stat(s, &st) || !S_ISDIR(st.st_mode))
    return -1;
  snprintf(cwd, sizeof(cwd), "/%s", p);
  return 0;
}
int __wrap_fileno(FILE *f) { return filefd(f); }
int __wrap_setvbuf(FILE *f, char *b, int mode, size_t n) {
  (void)f;
  (void)b;
  (void)mode;
  (void)n;
  return 0;
}
int __wrap_nanosleep(const struct timespec *req, struct timespec *rem) {
  port_sleep((unsigned)(req->tv_sec * 1000 + req->tv_nsec / 1000000));
  if (rem)
    memset(rem, 0, sizeof(*rem));
  return 0;
}
int __wrap_usleep(useconds_t us) {
  port_sleep((us + 999) / 1000);
  return 0;
}
unsigned __wrap_sleep(unsigned s) {
  port_sleep(s * 1000);
  return 0;
}
_Noreturn void __wrap_exit(int status) {
  char message[96];
  snprintf(message, sizeof(message),
           "Game exited with status %d. Reopen it to restart.", status);
  port_error(message);
  __builtin_unreachable();
}
_Noreturn void __wrap__Exit(int s) { __wrap_exit(s); }
_Noreturn void __wrap_abort(void) {
  port_error("The game stopped after an engine error.");
  __builtin_unreachable();
}

int __wrap_vprintf(const char *fmt, va_list a) {
  char buf[256];
  int n = vsnprintf(buf, sizeof(buf), fmt, a);
  port_log("%s", buf);
  return n;
}

int __wrap_putchar(int c) {
  char s[2] = {(char)c, 0};
  port_log("%s", s);
  return c;
}

char *__wrap_getenv(const char *name) {
  (void)name;
  return NULL;
}
void __wrap_perror(const char *s) { port_log("%s: I/O error", s); }
/* libc's null stdio bookkeeping and locale initialization must not request
 * host stdio or environment capabilities. These resources are absent. */
#include <wasi/api.h>
__wasi_errno_t __wrap___wasi_fd_fdstat_get(__wasi_fd_t fd,
                                           __wasi_fdstat_t *st) {
  (void)fd;
  (void)st;
  return __WASI_ERRNO_BADF;
}
__wasi_errno_t __wrap___wasi_environ_sizes_get(__wasi_size_t *count,
                                               __wasi_size_t *size) {
  *count = *size = 0;
  return 0;
}
__wasi_errno_t __wrap___wasi_environ_get(uint8_t **env, uint8_t *buf) {
  (void)buf;
  env[0] = NULL;
  return 0;
}

int __wrap_system(const char *s) {
  (void)s;
  errno = ENOSYS;
  return -1;
}
