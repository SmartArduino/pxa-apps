/* SPDX-License-Identifier: MIT
 * Native independent host-file oracle; not an emulated game or PXA Guest.
 */
#define _GNU_SOURCE 1
#include "jy_fs.h"
#include "jy_asset_index.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
typedef struct {const char *root;size_t read_cap,write_cap;int fail_read,fail_write;} backing;
static backing *active_overlay;
static backing *active_assets;
static unsigned long checks;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"CHECK failed %s:%d: %s errno=%d\n",__FILE__,__LINE__,#x,errno);abort();}}while(0)
static void path_for(backing *b,const char *name,char path[PATH_MAX]){CHECK(snprintf(path,PATH_MAX,"%s/%s",b->root,name)<PATH_MAX);}
static int parents(const char *path){
 char p[PATH_MAX];if(strlen(path)>=sizeof p)return ENAMETOOLONG;strcpy(p,path);
 for(char *x=p+1;*x;++x)if(*x=='/'){*x=0;if(mkdir(p,0700)&&errno!=EEXIST)return errno;*x='/';}
 return 0;
}
static int overlay_stat(void *ctx,const char *name,jy_fs_info *out){char path[PATH_MAX];path_for(ctx,name,path);struct stat st;if(stat(path,&st))return errno;out->kind=S_ISDIR(st.st_mode)?JY_FS_DIRECTORY:JY_FS_FILE;out->size=st.st_size;return 0;}
static int asset_stat(void *ctx,const char *name,jy_fs_info *out){
 (void)ctx;char marker[PATH_MAX];if(active_overlay){CHECK(snprintf(marker,sizeof marker,"%s/.deleted/%s",active_overlay->root,name)<(int)sizeof marker);if(!access(marker,F_OK))return ENOENT;}return jy_asset_stat(NULL,name,out);
}
static int asset_read(void *ctx,const char *name,uint64_t offset,void *dst,size_t wanted,size_t *got){
 backing *b=ctx;CHECK(wanted<=JY_FS_CHUNK_BYTES);if(b->fail_read)return EIO;const jy_asset_entry *entry=jy_asset_lookup(name);if(!entry)return ENOENT;
 if(b->read_cap&&wanted>b->read_cap)wanted=b->read_cap;
 char path[PATH_MAX];path_for(b,entry->resource_path,path);FILE *f=fopen(path,"rb");if(!f)return errno;if(fseeko(f,(off_t)offset,SEEK_SET)){int e=errno;fclose(f);return e;}*got=fread(dst,1,wanted,f);int e=ferror(f)?EIO:0;fclose(f);return e;
}
static int overlay_read(void *ctx,const char *name,uint64_t offset,void *dst,size_t wanted,size_t *got){
 backing *b=ctx;CHECK(wanted<=JY_FS_CHUNK_BYTES);if(b->fail_read)return EIO;if(b->read_cap&&wanted>b->read_cap)wanted=b->read_cap;char path[PATH_MAX];path_for(b,name,path);FILE *f=fopen(path,"rb");if(!f)return errno;if(fseeko(f,(off_t)offset,SEEK_SET)){int e=errno;fclose(f);return e;}*got=fread(dst,1,wanted,f);int e=ferror(f)?EIO:0;fclose(f);return e;
}
static int overlay_write(void *ctx,const char *name,uint64_t offset,const void *src,size_t wanted,size_t *put){
 backing *b=ctx;CHECK(wanted<=JY_FS_CHUNK_BYTES);if(b->fail_write)return ENOSPC;if(b->write_cap&&wanted>b->write_cap)wanted=b->write_cap;char path[PATH_MAX];path_for(b,name,path);FILE *f=fopen(path,"r+b");if(!f)return errno;if(fseeko(f,(off_t)offset,SEEK_SET)){int e=errno;fclose(f);return e;}*put=fwrite(src,1,wanted,f);int e=ferror(f)||fflush(f)?EIO:0;if(fclose(f)&&!e)e=EIO;return e;
}
static int overlay_resize(void *ctx,const char *name,uint64_t size){char path[PATH_MAX];path_for(ctx,name,path);int e=parents(path);if(e)return e;FILE *f=fopen(path,"ab");if(!f)return errno;e=ftruncate(fileno(f),(off_t)size)?errno:0;if(fclose(f)&&!e)e=errno;return e;}
static int overlay_clone(void *ctx,const char *from,const char *to){
 backing *output=ctx;if(output->fail_write)return ENOSPC;const jy_asset_entry *asset=jy_asset_lookup(from);if(!asset)return ENOENT;
 char path[PATH_MAX],temporary[PATH_MAX];path_for(output,to,path);CHECK(snprintf(temporary,sizeof temporary,"%s.jy-clone",path)<(int)sizeof temporary);int e=parents(path);if(e)return e;FILE *file=fopen(temporary,"wb");if(!file)return errno;
 unsigned char buffer[JY_FS_CHUNK_BYTES];for(uint64_t offset=0;offset<asset->bytes;){size_t n=asset->bytes-offset>sizeof buffer?sizeof buffer:(size_t)(asset->bytes-offset),got=0;e=asset_read(active_assets,from,offset,buffer,n,&got);if(e||!got){e=e?e:EIO;break;}if(fwrite(buffer,1,got,file)!=got){e=EIO;break;}offset+=got;}
 if(fclose(file)&&!e)e=EIO;if(e){remove(temporary);return e;}if(rename(temporary,path)){e=errno;remove(temporary);return e;}return 0;
}
static int overlay_remove(void *ctx,const char *name){
 char path[PATH_MAX];path_for(ctx,name,path);int removed=!remove(path);int error=errno;if(!removed&&error!=ENOENT)return error;
 if(jy_asset_lookup(name)){CHECK(snprintf(path,sizeof path,"%s/.deleted/%s",((backing *)ctx)->root,name)<(int)sizeof path);int e=parents(path);if(e)return e;FILE *marker=fopen(path,"wb");if(!marker)return errno;return fclose(marker)?errno:0;}return removed?0:ENOENT;
}
static int overlay_rename(void *ctx,const char *from,const char *to){
 char a[PATH_MAX],b[PATH_MAX];path_for(ctx,from,a);path_for(ctx,to,b);int e=parents(b);if(e)return e;
 if(!access(a,F_OK)){if(rename(a,b))return errno;return jy_asset_lookup(from)?overlay_remove(ctx,from):0;}
 const jy_asset_entry *asset=jy_asset_lookup(from);if(!asset)return ENOENT;
 char temporary[PATH_MAX];CHECK(snprintf(temporary,sizeof temporary,"%s.jy-rename",b)<(int)sizeof temporary);FILE *file=fopen(temporary,"wb");if(!file)return errno;
 unsigned char buffer[JY_FS_CHUNK_BYTES];for(uint64_t offset=0;offset<asset->bytes;){size_t n=asset->bytes-offset>sizeof buffer?sizeof buffer:(size_t)(asset->bytes-offset),got=0;e=asset_read(active_assets,from,offset,buffer,n,&got);if(e||!got){e=e?e:EIO;break;}if(fwrite(buffer,1,got,file)!=got){e=EIO;break;}offset+=got;}
 if(fclose(file)&&!e)e=EIO;if(e){remove(temporary);return e;}if(rename(temporary,b)){e=errno;remove(temporary);return e;}return overlay_remove(ctx,from);
}
static int overlay_mkdir(void *ctx,const char *name){char path[PATH_MAX];path_for(ctx,name,path);return mkdir(path,0700)?errno:0;}
static void compare_stream(FILE *adapted,FILE *oracle,uint64_t *total){
 unsigned char a[16384],b[16384];for(;;){size_t n=fread(a,1,sizeof a,adapted),m=fread(b,1,sizeof b,oracle);CHECK(n==m);CHECK(!memcmp(a,b,n));*total+=n;if(!n)break;}CHECK(!ferror(adapted));CHECK(!ferror(oracle));CHECK(feof(adapted));
}
int jy_redirect_fixture(void);
int main(int argc,char **argv){
 CHECK(argc==3||argc==4);backing assets={argv[1],0,0,0,0},overlay={argv[2],0,0,0,0};
 CHECK(!mkdir(overlay.root,0700)||errno==EEXIST);
 active_overlay=&overlay;active_assets=&assets;jy_fs_ops base={.context=&assets,.stat=asset_stat,.read_at=asset_read};
 jy_fs_ops writes={.context=&overlay,.stat=overlay_stat,.read_at=overlay_read,.write_at=overlay_write,.resize=overlay_resize,.remove=overlay_remove,.rename=overlay_rename,.mkdir=overlay_mkdir,.clone_base=overlay_clone};
 CHECK(!jy_fs_bind(&base,&writes));char normalized[JY_FS_PATH_MAX+1];CHECK(!jy_fs_normalize("./data\\./ranger.grp",normalized)&&!strcmp(normalized,"data/ranger.grp"));
 CHECK(jy_fs_normalize("../escape",normalized)==-1&&errno==EACCES);CHECK(jy_fs_normalize("C:\\data",normalized)==-1&&errno==EACCES);CHECK(jy_fs_normalize("/etc/passwd",normalized)==-1&&errno==EACCES);
 if(argc==4){
  CHECK(!strcmp(argv[3],"--clone-only"));assets.fail_read=1;CHECK(jy_fs_fopen("data/ranger.grp","r+")==NULL&&errno==EIO);CHECK(jy_fs_fopen("data/ranger.grp","a")==NULL&&errno==EIO);assets.fail_read=0;
  overlay.fail_write=1;CHECK(jy_fs_fopen("data/ranger.grp","r+")==NULL&&errno==ENOSPC);CHECK(jy_fs_fopen("data/ranger.grp","a")==NULL&&errno==ENOSPC);overlay.fail_write=0;
  jy_fs_reset();CHECK(!jy_fs_bind(&base,&writes));FILE *unchanged=jy_fs_fopen("data/ranger.grp","rb");CHECK(unchanged);const jy_asset_entry *entry=jy_asset_lookup("data/ranger.grp");char physical[PATH_MAX];path_for(&assets,entry->resource_path,physical);FILE *oracle=fopen(physical,"rb");CHECK(oracle);uint64_t compared=0;compare_stream(unchanged,oracle,&compared);CHECK(compared==entry->bytes);CHECK(!jy_fs_fclose(unchanged)&&!fclose(oracle));
  FILE *modified=jy_fs_fopen("data/ranger.grp","r+");CHECK(modified);CHECK(!fseek(modified,3,SEEK_SET));CHECK(fputs("COW",modified)>=0);CHECK(!jy_fs_fclose(modified));jy_fs_reset();CHECK(!jy_fs_bind(&base,&writes));modified=jy_fs_fopen("data/ranger.grp","rb");CHECK(modified);CHECK(!fseek(modified,3,SEEK_SET));char edited[3];CHECK(fread(edited,1,3,modified)==3&&!memcmp(edited,"COW",3));CHECK(!jy_fs_fclose(modified));jy_fs_reset();
  printf("{\"passed\":true,\"checks\":%lu,\"clone_read_and_append_errors_preserve_base\":true,\"clone_quota_errors_preserve_base\":true,\"cold_bind_original_bytes_compared\":%" PRIu64 ",\"successful_clone_edit_persisted\":true,\"guest_run\":false}\n",checks,compared);return 0;
 }
 uint64_t streamed=0;
 for(size_t i=0;i<jy_asset_count;++i){
  const jy_asset_entry *entry=&jy_assets[i];FILE *f=jy_fs_fopen(entry->path,"rb");CHECK(f);char path[PATH_MAX];path_for(&assets,entry->resource_path,path);FILE *o=fopen(path,"rb");CHECK(o);struct stat st;CHECK(!jy_fs_stat(entry->path,&st)&&st.st_size==entry->bytes);CHECK(!fseek(f,0,SEEK_END)&&ftell(f)==(long)entry->bytes);CHECK(!fseek(f,0,SEEK_SET));compare_stream(f,o,&streamed);CHECK(!fclose(o));CHECK(!jy_fs_fclose(f));
 }
 const jy_asset_entry *large=jy_asset_lookup("font/bkai00mp.ttf");CHECK(large&&large->bytes==10417860);
 assets.read_cap=113;FILE *f=jy_fs_fopen(large->path,"rb");CHECK(f);char original[PATH_MAX];path_for(&assets,large->resource_path,original);FILE *o=fopen(original,"rb");CHECK(o);
 const long offsets[]={0,1,4063,4064,4065,8191,8192,10417859,10417860};
 unsigned char a[257],b[257];for(size_t i=0;i<sizeof offsets/sizeof offsets[0];++i){CHECK(!fseek(f,offsets[i],SEEK_SET)&&!fseek(o,offsets[i],SEEK_SET));size_t n=fread(a,1,sizeof a,f),m=fread(b,1,sizeof b,o);CHECK(n==m&&!memcmp(a,b,n));CHECK(ftell(f)==ftell(o));}
 CHECK(!fseek(f,-1,SEEK_END)&&!fseek(o,-1,SEEK_END));CHECK(fgetc(f)==fgetc(o));CHECK(!fclose(o));CHECK(!jy_fs_fclose(f));assets.read_cap=0;
 /* The original low-memory scene path copies real allsin.grp and then mutates
  * the separate backing file. Guest-side memory remains one bounded chunk. */
 f=jy_fs_fopen("data/allsin.grp","rb");FILE *out=jy_fs_fopen("data/allsinbk.grp","wb");CHECK(f&&out);uint64_t scene_bytes=0;unsigned char block[8192];size_t n;
 while((n=fread(block,1,sizeof block,f))){CHECK(fwrite(block,1,n,out)==n);scene_bytes+=n;}CHECK(!ferror(f));CHECK(!jy_fs_fclose(f)&&!jy_fs_fclose(out));CHECK(scene_bytes==4915200);
 f=jy_fs_fopen("data/allsinbk.grp","r+b");CHECK(f);CHECK(!fseek(f,4063,SEEK_SET));const unsigned char marker[]={1,2,3,4,5,6,7};CHECK(fwrite(marker,1,sizeof marker,f)==sizeof marker);CHECK(!fseek(f,4063,SEEK_SET));CHECK(fread(a,1,sizeof marker,f)==sizeof marker&&!memcmp(a,marker,sizeof marker));CHECK(!jy_fs_fclose(f));
 /* Original saved R record must be copied before a random edit; verify all
  * surrounding bytes against the independent immutable original. */
 const jy_asset_entry *r=jy_asset_lookup("data/ranger.grp");CHECK(r);path_for(&assets,r->resource_path,original);o=fopen(original,"rb");CHECK(o);f=jy_fs_fopen(r->path,"r+b");CHECK(f);overlay.write_cap=97;CHECK(!fseek(f,4060,SEEK_SET));unsigned char edit[500];for(size_t i=0;i<sizeof edit;++i)edit[i]=(unsigned char)(i*17);CHECK(fwrite(edit,1,sizeof edit,f)==sizeof edit);CHECK(!fseek(f,0,SEEK_SET));
 uint64_t position=0;for(;;){size_t na=fread(a,1,sizeof a,f),nb=fread(b,1,sizeof b,o);CHECK(na==nb);for(size_t k=0;k<na;++k){unsigned char expected=position+k>=4060&&position+k<4560?edit[position+k-4060]:b[k];CHECK(a[k]==expected);}position+=na;if(!na)break;}CHECK(position==r->bytes);CHECK(!jy_fs_fclose(f)&&!fclose(o));overlay.write_cap=0;
 CHECK(!jy_fs_mkdir("save",0700));CHECK(!jy_fs_rename("data/ranger.idx","save/renamed.idx"));CHECK(jy_fs_fopen("data/ranger.idx","rb")==NULL&&errno==ENOENT);f=jy_fs_fopen("save/renamed.idx","rb");CHECK(f);CHECK(jy_fs_remove("save/renamed.idx")==-1&&errno==EBUSY);CHECK(!jy_fs_fclose(f));CHECK(!jy_fs_remove("save/renamed.idx"));CHECK(jy_fs_access("save/renamed.idx",F_OK)==-1&&errno==ENOENT);
 f=jy_fs_tmpfile();CHECK(f);CHECK(fputs("123 45\n",f)>=0);CHECK(!fseek(f,0,SEEK_SET));int x=0,y=0;CHECK(fscanf(f,"%d %d",&x,&y)==2&&x==123&&y==45);CHECK(!jy_fs_fclose(f));
 CHECK(!jy_redirect_fixture());
 assets.fail_read=1;f=jy_fs_fopen("data/mmap.col","rb");CHECK(f);CHECK(fread(a,1,sizeof a,f)==0&&ferror(f)&&errno==EIO);CHECK(!jy_fs_fclose(f));assets.fail_read=0;
 f=jy_fs_fopen("save/full","wb");CHECK(f);overlay.fail_write=1;CHECK(fwrite(a,1,sizeof a,f)<sizeof a&&ferror(f)&&errno==ENOSPC);overlay.fail_write=0;CHECK(!jy_fs_fclose(f));
 jy_fs_stats result;jy_fs_get_stats(&result);CHECK(result.max_callback_request<=4064);CHECK(result.open_files==1); /* redirected stderr is deliberately open */
 jy_fs_reset();CHECK(!jy_fs_bind(&base,NULL));CHECK(jy_fs_fopen("data/ranger.grp","wb")==NULL&&errno==EROFS);CHECK(jy_fs_remove("data/ranger.grp")==-1&&errno==EROFS);CHECK(jy_fs_access("data/ranger.grp",W_OK)==-1&&errno==EROFS);CHECK(jy_fs_popen("ignored","r")==NULL&&errno==ENOSYS);CHECK(jy_fs_system(NULL)==0);CHECK(jy_fs_system("ignored")==-1&&errno==ENOSYS);jy_fs_reset();
 printf("{\"passed\":true,\"checks\":%lu,\"official_resources_streamed\":%zu,\"official_bytes_compared\":%" PRIu64 ",\"real_scene_copy_bytes\":%" PRIu64 ",\"max_callback_request\":%u,\"base_read_calls\":%" PRIu64 ",\"base_read_bytes\":%" PRIu64 ",\"overlay_write_bytes\":%" PRIu64 ",\"copy_on_write_bytes\":%" PRIu64 ",\"cache_hits\":%" PRIu64 ",\"peak_open_files\":%u,\"guest_run\":false}\n",checks,jy_asset_count,streamed,scene_bytes,result.max_callback_request,result.base_read_calls,result.base_read_bytes,result.overlay_write_bytes,result.copy_on_write_bytes,result.cache_hits,result.peak_open_files);
 return 0;
}
