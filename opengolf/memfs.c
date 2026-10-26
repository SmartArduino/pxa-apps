/* SPDX-License-Identifier: MIT
 * Port-local volatile filesystem at wasi-libc's Preview 1 boundary.
 * This keeps upstream stdio/recording code intact without requesting Host FS
 * or stdio capabilities. Files live only until this Guest is unloaded.
 */
/* SDK 34 puts real WASI calls in one libc object. Wrap instead of defining
 * duplicate strong symbols, so unneeded Host imports can be collected. */
#define __wasi_fd_prestat_get __wrap___wasi_fd_prestat_get
#define __wasi_fd_prestat_dir_name __wrap___wasi_fd_prestat_dir_name
#define __wasi_path_open __wrap___wasi_path_open
#define __wasi_fd_close __wrap___wasi_fd_close
#define __wasi_fd_fdstat_get __wrap___wasi_fd_fdstat_get
#define __wasi_fd_fdstat_set_flags __wrap___wasi_fd_fdstat_set_flags
#define __wasi_fd_read __wrap___wasi_fd_read
#define __wasi_fd_write __wrap___wasi_fd_write
#define __wasi_fd_seek __wrap___wasi_fd_seek
#define __wasi_fd_filestat_get __wrap___wasi_fd_filestat_get
#define __wasi_path_filestat_get __wrap___wasi_path_filestat_get
#define __wasi_path_unlink_file __wrap___wasi_path_unlink_file
#define __wasi_path_remove_directory __wrap___wasi_path_remove_directory
#define __wasi_path_rename __wrap___wasi_path_rename
#define __wasi_fd_readdir __wrap___wasi_fd_readdir
#define __wasi_proc_exit __wrap___wasi_proc_exit
#include <wasi/api.h>
#include <stdlib.h>
#include <string.h>
#include "pxa_log.h"

#define MF_FILES 32
#define MF_FDS 24
#define MF_NAME 256
#define MF_MAX_BYTES (4u * 1024u * 1024u)
typedef struct { char name[MF_NAME]; unsigned char *data; size_t size, capacity; } mf_file;
typedef struct { int used, file; size_t pos; __wasi_fdflags_t flags; } mf_fd;
static mf_file files[MF_FILES];
static mf_fd descriptors[MF_FDS];
static char log_line[2][257];
static size_t log_used[2];

void __wasi_proc_exit(__wasi_exitcode_t code) {
    (void)code;
    pxa_log_write(4,"GOLF libc requested fatal exit");
    __builtin_trap();
}

static const char *normalize(const char *path) {
    while (path[0]=='.' && path[1]=='/') path+=2;
    return path;
}
static int find_file(const char *path) {
    path=normalize(path);
    for(int i=0;i<MF_FILES;i++) if(files[i].name[0] && !strcmp(files[i].name,path)) return i;
    return -1;
}
static int is_dir(const char *path) {
    path=normalize(path); return !*path || !strcmp(path,".") || !strcmp(path,"/");
}
static mf_fd *get_fd(__wasi_fd_t fd) {
    return fd>=4 && fd<MF_FDS && descriptors[fd].used ? &descriptors[fd] : NULL;
}
static int grow(mf_file *file,size_t n) {
    if(n>MF_MAX_BYTES) return 0;
    if(n<=file->capacity) return 1;
    size_t cap=file->capacity ? file->capacity : 1024;
    while(cap<n) cap*=2;
    unsigned char *bytes=realloc(file->data,cap);
    if(!bytes) return 0;
    memset(bytes+file->capacity,0,cap-file->capacity);
    file->data=bytes; file->capacity=cap; return 1;
}
static void file_stat(int index,__wasi_filestat_t *out) {
    memset(out,0,sizeof(*out)); out->nlink=1;
    out->filetype=index<0 ? __WASI_FILETYPE_DIRECTORY : __WASI_FILETYPE_REGULAR_FILE;
    out->ino=index+2; if(index>=0) out->size=files[index].size;
    /* Stable valid date for ephemeral recordings, independent of wall time. */
    out->atim=out->mtim=out->ctim=UINT64_C(1767225600000000000);
}

__wasi_errno_t __wasi_fd_prestat_get(__wasi_fd_t fd,__wasi_prestat_t *out) {
    if(fd!=3) return __WASI_ERRNO_BADF;
    memset(out,0,sizeof(*out)); out->tag=__WASI_PREOPENTYPE_DIR; out->u.dir.pr_name_len=1;
    return 0;
}
__wasi_errno_t __wasi_fd_prestat_dir_name(__wasi_fd_t fd,uint8_t *path,__wasi_size_t n) {
    if(fd!=3) return __WASI_ERRNO_BADF;
    if(n<1) return __WASI_ERRNO_NAMETOOLONG;
    path[0]='.'; return 0;
}
__wasi_errno_t __wasi_path_open(__wasi_fd_t parent,__wasi_lookupflags_t lookup,
    const char *path,__wasi_oflags_t oflags,__wasi_rights_t rights,
    __wasi_rights_t inheriting,__wasi_fdflags_t flags,__wasi_fd_t *out) {
    (void)parent;(void)lookup;(void)rights;(void)inheriting;
    path=normalize(path); int index=find_file(path);
    if(is_dir(path)) index=-2;
    else if(oflags & __WASI_OFLAGS_DIRECTORY) return __WASI_ERRNO_NOTDIR;
    else if(index<0) {
        if(!(oflags & __WASI_OFLAGS_CREAT)) return __WASI_ERRNO_NOENT;
        if(strlen(path)>=MF_NAME) return __WASI_ERRNO_NAMETOOLONG;
        for(int i=0;i<MF_FILES;i++) if(!files[i].name[0]) { index=i; break; }
        if(index<0) return __WASI_ERRNO_NOSPC;
        strcpy(files[index].name,path);
    } else if((oflags & __WASI_OFLAGS_CREAT) && (oflags & __WASI_OFLAGS_EXCL)) return __WASI_ERRNO_EXIST;
    for(unsigned fd=4;fd<MF_FDS;fd++) if(!descriptors[fd].used) {
        if(index>=0 && (oflags & __WASI_OFLAGS_TRUNC)) files[index].size=0;
        descriptors[fd]=(mf_fd){1,index,index>=0 && (flags & __WASI_FDFLAGS_APPEND) ? files[index].size : 0,flags};
        *out=fd; return 0;
    }
    return __WASI_ERRNO_MFILE;
}
__wasi_errno_t __wasi_fd_close(__wasi_fd_t fd) {
    if(fd<=3) return 0;
    mf_fd *entry=get_fd(fd); if(!entry) return __WASI_ERRNO_BADF;
    entry->used=0; return 0;
}
__wasi_errno_t __wasi_fd_fdstat_get(__wasi_fd_t fd,__wasi_fdstat_t *out) {
    mf_fd *entry=get_fd(fd);
    if(fd>3 && !entry) return __WASI_ERRNO_BADF;
    memset(out,0,sizeof(*out));
    out->fs_filetype=fd<3 ? __WASI_FILETYPE_CHARACTER_DEVICE :
        fd==3 || entry->file<0 ? __WASI_FILETYPE_DIRECTORY : __WASI_FILETYPE_REGULAR_FILE;
    out->fs_rights_base=out->fs_rights_inheriting=UINT64_MAX;
    if(entry) out->fs_flags=entry->flags;
    return 0;
}
__wasi_errno_t __wasi_fd_fdstat_set_flags(__wasi_fd_t fd,__wasi_fdflags_t flags) {
    if(fd<3) return 0;
    mf_fd *entry=get_fd(fd); if(!entry) return __WASI_ERRNO_BADF;
    entry->flags=flags; return 0;
}
__wasi_errno_t __wasi_fd_read(__wasi_fd_t fd,const __wasi_iovec_t *vectors,size_t count,__wasi_size_t *out) {
    *out=0; if(fd==0) return 0;
    mf_fd *entry=get_fd(fd); if(!entry || entry->file<0) return __WASI_ERRNO_BADF;
    mf_file *file=&files[entry->file];
    for(size_t i=0;i<count;i++) {
        size_t n=entry->pos<file->size ? file->size-entry->pos : 0;
        if(n>vectors[i].buf_len) n=vectors[i].buf_len;
        if(n) memcpy(vectors[i].buf,file->data+entry->pos,n);
        entry->pos+=n; *out+=n;
        if(n<vectors[i].buf_len) break;
    }
    return 0;
}
__wasi_errno_t __wasi_fd_write(__wasi_fd_t fd,const __wasi_ciovec_t *vectors,size_t count,__wasi_size_t *out) {
    *out=0;
    if(fd==1 || fd==2) {
        char *line=log_line[fd-1];size_t *used=&log_used[fd-1];
        for(size_t i=0;i<count;i++) for(size_t j=0;j<vectors[i].buf_len;j++) {
            char c=((const char *)vectors[i].buf)[j]; (*out)++;
            if(c=='\n' || *used==256) { if(*used) { line[*used]=0; (void)pxa_log_write(2,line); *used=0; } }
            if(c!='\n') line[(*used)++]=c;
        }
        return 0;
    }
    mf_fd *entry=get_fd(fd); if(!entry || entry->file<0) return __WASI_ERRNO_BADF;
    mf_file *file=&files[entry->file];
    if(entry->flags & __WASI_FDFLAGS_APPEND) entry->pos=file->size;
    for(size_t i=0;i<count;i++) {
        size_t n=vectors[i].buf_len;
        if(n>MF_MAX_BYTES || entry->pos>MF_MAX_BYTES-n || !grow(file,entry->pos+n)) return __WASI_ERRNO_NOSPC;
        memcpy(file->data+entry->pos,vectors[i].buf,n);
        entry->pos+=n; *out+=n;
        if(entry->pos>file->size) file->size=entry->pos;
    }
    return 0;
}
__wasi_errno_t __wasi_fd_seek(__wasi_fd_t fd,__wasi_filedelta_t offset,__wasi_whence_t whence,__wasi_filesize_t *out) {
    mf_fd *entry=get_fd(fd); if(!entry || entry->file<0) return __WASI_ERRNO_BADF;
    int64_t base=whence==__WASI_WHENCE_SET ? 0 : whence==__WASI_WHENCE_CUR ? entry->pos :
        whence==__WASI_WHENCE_END ? files[entry->file].size : -1;
    if(base<0 || offset < -base || offset > MF_MAX_BYTES-base) return __WASI_ERRNO_INVAL;
    entry->pos=base+offset; *out=entry->pos; return 0;
}
__wasi_errno_t __wasi_fd_filestat_get(__wasi_fd_t fd,__wasi_filestat_t *out) {
    mf_fd *entry=get_fd(fd); if(fd!=3 && !entry) return __WASI_ERRNO_BADF;
    file_stat(fd==3 ? -2 : entry->file,out); return 0;
}
__wasi_errno_t __wasi_path_filestat_get(__wasi_fd_t fd,__wasi_lookupflags_t flags,const char *path,__wasi_filestat_t *out) {
    (void)fd;(void)flags; int index=find_file(path);
    if(is_dir(path)) index=-2; else if(index<0) return __WASI_ERRNO_NOENT;
    file_stat(index,out); return 0;
}
__wasi_errno_t __wasi_path_unlink_file(__wasi_fd_t fd,const char *path) {
    (void)fd; int index=find_file(path); if(index<0) return __WASI_ERRNO_NOENT;
    free(files[index].data); memset(&files[index],0,sizeof(files[index])); return 0;
}
__wasi_errno_t __wasi_path_remove_directory(__wasi_fd_t fd,const char *path) {
    (void)fd;(void)path; return __WASI_ERRNO_NOTDIR;
}
__wasi_errno_t __wasi_path_rename(__wasi_fd_t from,const char *old,__wasi_fd_t to,const char *name) {
    (void)from;(void)to; int index=find_file(old); if(index<0) return __WASI_ERRNO_NOENT;
    name=normalize(name); if(strlen(name)>=MF_NAME) return __WASI_ERRNO_NAMETOOLONG;
    int dest=find_file(name);
    if(dest>=0 && dest!=index) {free(files[dest].data);memset(&files[dest],0,sizeof(files[dest]));}
    strcpy(files[index].name,name); return 0;
}
__wasi_errno_t __wasi_fd_readdir(__wasi_fd_t fd,uint8_t *buf,__wasi_size_t capacity,__wasi_dircookie_t cookie,__wasi_size_t *out) {
    mf_fd *entry=get_fd(fd); if(fd!=3 && (!entry || entry->file>=0)) return __WASI_ERRNO_BADF;
    *out=0;
    for(unsigned i=(unsigned)cookie;i<MF_FILES;i++) if(files[i].name[0]) {
        __wasi_dirent_t dir={0}; dir.d_next=i+1;dir.d_ino=i+2;
        dir.d_namlen=strlen(files[i].name);dir.d_type=__WASI_FILETYPE_REGULAR_FILE;
        size_t n=sizeof(dir)+dir.d_namlen;
        if(n>capacity-*out) break;
        memcpy(buf+*out,&dir,sizeof(dir));memcpy(buf+*out+sizeof(dir),files[i].name,dir.d_namlen);
        *out+=n;
    }
    return 0;
}
