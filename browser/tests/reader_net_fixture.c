/* Linux simulator tests only. Preserve Guest origin checks and signed modules
 * while directing this child process's HTTP to synthetic responses. Never use
 * this library for real account/site testing. No Host code is modified. */
#define _GNU_SOURCE
#include <curl/curl.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *fixture_server;
__attribute__((constructor)) static void reader_fixture_init(void) {
    const char *value=getenv("PXA_READER_TEST_SERVER");
    if(value)fixture_server=strdup(value);
}
static CURLcode reader_fixture_redirect(CURL *handle) {
    char *url=NULL;
    if(curl_easy_getinfo(handle,CURLINFO_EFFECTIVE_URL,&url) || !url) return CURLE_URL_MALFORMAT;
    const char *origins[]={"https://www.gmanhua.com","https://weread.qq.com","https://fixture.cdndm5.com"};
    const char *path=NULL;
    unsigned site;
    for(site=0;site<3;++site) {
        size_t n=strlen(origins[site]);
        if(!strncmp(url,origins[site],n) && url[n]=='/') {path=url+n;break;}
    }
    // Fail closed rather than carrying synthetic values to a real service.
    if(!path)return CURLE_COULDNT_RESOLVE_HOST;
    char target[2048];
    if(snprintf(target,sizeof(target),"%s/%u%s",fixture_server,site,path)>=(int)sizeof(target)) return CURLE_URL_MALFORMAT;
    return curl_easy_setopt(handle,CURLOPT_URL,target);
}
CURLcode curl_easy_perform(CURL *handle) {
    CURLcode (*perform)(CURL *)=dlsym(RTLD_NEXT,"curl_easy_perform");
    if(!perform)return CURLE_FAILED_INIT;
    if(fixture_server) {CURLcode error=reader_fixture_redirect(handle);if(error)return error;}
    return perform(handle);
}
CURLMcode curl_multi_add_handle(CURLM *multi,CURL *handle) {
    CURLMcode (*add)(CURLM *,CURL *)=dlsym(RTLD_NEXT,"curl_multi_add_handle");
    if(!add)return CURLM_INTERNAL_ERROR;
    if(fixture_server && reader_fixture_redirect(handle)!=CURLE_OK)return CURLM_BAD_EASY_HANDLE;
    return add(multi,handle);
}
