/* Single-owner SQLite VFS on PXA's app-private stdio bridge. SQLite is public domain. */
#include "sqlite3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
typedef struct {sqlite3_file base;FILE *fp;} KysFile;
static int io_close(sqlite3_file *f){return fclose(((KysFile*)f)->fp)?SQLITE_IOERR_CLOSE:SQLITE_OK;}
static int io_read(sqlite3_file *f,void *buf,int n,sqlite3_int64 off){
    FILE *fp=((KysFile*)f)->fp;if(off<0 || off>0x7fffffff || fseek(fp,(long)off,SEEK_SET))return SQLITE_IOERR_SEEK;
    size_t got=fread(buf,1,n,fp);if(got==(size_t)n)return SQLITE_OK;
    memset((char*)buf+got,0,n-got);return ferror(fp)?SQLITE_IOERR_READ:SQLITE_IOERR_SHORT_READ;
}
static int io_write(sqlite3_file *f,const void *buf,int n,sqlite3_int64 off){
    FILE *fp=((KysFile*)f)->fp;if(off<0 || off>0x7fffffff || fseek(fp,(long)off,SEEK_SET))return SQLITE_IOERR_SEEK;
    return fwrite(buf,1,n,fp)==(size_t)n?SQLITE_OK:SQLITE_IOERR_WRITE;
}
static int io_truncate(sqlite3_file *f,sqlite3_int64 n){
    /* SQLite's normal rollback-journal mode deletes completed journals. Never
       claim a shrinking truncate succeeded; a caller can recover the journal. */
    FILE *fp=((KysFile*)f)->fp;long p=ftell(fp);fseek(fp,0,SEEK_END);long size=ftell(fp);fseek(fp,p,SEEK_SET);
    return size==n?SQLITE_OK:SQLITE_IOERR_TRUNCATE;
}
static int io_sync(sqlite3_file *f,int flags){(void)flags;return fflush(((KysFile*)f)->fp)?SQLITE_IOERR_FSYNC:SQLITE_OK;}
static int io_size(sqlite3_file *f,sqlite3_int64 *n){FILE *fp=((KysFile*)f)->fp;long p=ftell(fp);if(fseek(fp,0,SEEK_END))return SQLITE_IOERR_FSTAT;*n=ftell(fp);return fseek(fp,p,SEEK_SET)?SQLITE_IOERR_FSTAT:SQLITE_OK;}
static int io_lock(sqlite3_file *f,int n){(void)f;(void)n;return SQLITE_OK;}
static int io_check(sqlite3_file *f,int *n){(void)f;*n=0;return SQLITE_OK;}
static int io_control(sqlite3_file *f,int op,void *arg){(void)f;(void)op;(void)arg;return SQLITE_NOTFOUND;}
static int io_sector(sqlite3_file *f){(void)f;return 512;}
static int io_device(sqlite3_file *f){(void)f;return 0;}
static const sqlite3_io_methods methods={1,io_close,io_read,io_write,io_truncate,io_sync,io_size,io_lock,io_lock,io_check,io_control,io_sector,io_device};
static int v_open(sqlite3_vfs *v,const char *name,sqlite3_file *out,int flags,int *actual){
    (void)v;KysFile *f=(KysFile*)out;memset(f,0,sizeof(*f));if(!name)return SQLITE_CANTOPEN;
    int write=!!(flags&SQLITE_OPEN_READWRITE);f->fp=fopen(name,write?"r+b":"rb");
    if(!f->fp && (flags&SQLITE_OPEN_CREATE))f->fp=fopen(name,"w+b");
    if(!f->fp)return SQLITE_CANTOPEN;f->base.pMethods=&methods;if(actual)*actual=flags;return SQLITE_OK;
}
static int v_delete(sqlite3_vfs *v,const char *p,int sync){(void)v;(void)sync;struct stat st;if(stat(p,&st))return SQLITE_OK;return remove(p)?SQLITE_IOERR_DELETE:SQLITE_OK;}
static int v_access(sqlite3_vfs *v,const char *p,int flags,int *yes){(void)v;(void)flags;struct stat st;*yes=stat(p,&st)==0;return SQLITE_OK;}
static int v_path(sqlite3_vfs *v,const char *p,int n,char *out){(void)v;if((int)strlen(p)>=n)return SQLITE_CANTOPEN;strcpy(out,p);return SQLITE_OK;}
static int v_random(sqlite3_vfs *v,int n,char *out){(void)v;for(int i=0;i<n;++i)out[i]=rand();return n;}
static int v_sleep(sqlite3_vfs *v,int us){(void)v;usleep(us);return us;}
static int v_time(sqlite3_vfs *v,double *d){(void)v;*d=2440587.5+time(NULL)/86400.;return SQLITE_OK;}
static sqlite3_vfs vfs={1,sizeof(KysFile),255,0,"pxa",0,v_open,v_delete,v_access,v_path,0,0,0,0,v_random,v_sleep,v_time,0};
int sqlite3_os_init(void){return sqlite3_vfs_register(&vfs,1);}
int sqlite3_os_end(void){return SQLITE_OK;}
