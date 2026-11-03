/* SPDX-License-Identifier: MIT */
#include "jy_fs_redirect.h"
#include <assert.h>
#include <errno.h>
int jy_redirect_fixture(void){
 FILE *f=fopen("script/oldtalk.idx","wt");assert(f);assert(fprintf(f,"0 37 98\n")==8);assert(!fclose(f));
 f=fopen("script/oldtalk.idx","r");assert(f);int a,b,c;assert(fscanf(f,"%d %d %d",&a,&b,&c)==3&&a==0&&b==37&&c==98);assert(!fclose(f));
 assert(freopen("error.txt","wt",stderr));assert(fprintf(stderr,"fs redirect preserved\n")==22);assert(!fflush(stderr));
 f=fopen("error.txt","rb");assert(f);char line[64];assert(fgets(line,sizeof line,f));assert(!strcmp(line,"fs redirect preserved\n"));assert(!fclose(f));
 struct stat st;assert(!stat("script/oldtalk.idx",&st)&&st.st_size==8);assert(!access("error.txt",R_OK));
 f=tmpfile();assert(f);assert(fputs("temporary",f)>=0);assert(!fclose(f));return 0;
}
