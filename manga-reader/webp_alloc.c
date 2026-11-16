#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
// Used only by the app's libwebp decoder build; vendor sources stay unchanged.
typedef union { max_align_t alignment; size_t bytes; } Header;
static size_t live,peak,ceiling;
static int denied;
int manga_webp_budget(size_t limit) { if(live)return 0;peak=0;denied=0;ceiling=limit;return 1; }
size_t manga_webp_peak(void) { return peak; }
size_t manga_webp_live(void) { return live; }
int manga_webp_denied(void) { return denied; }
void* manga_webp_malloc(size_t bytes) {
    if(bytes>ceiling-live||bytes>SIZE_MAX-sizeof(Header)){denied=1;return NULL;}
    Header* p=(Header*)malloc(sizeof(Header)+bytes);if(!p){denied=1;return NULL;}
    p->bytes=bytes;live+=bytes;if(live>peak)peak=live;return p+1;
}
void* manga_webp_calloc(size_t count,size_t bytes) {
    if(bytes&&count>SIZE_MAX/bytes){denied=1;return NULL;}
    size_t size=count*bytes;void* p=manga_webp_malloc(size);if(p)memset(p,0,size);return p;
}
void manga_webp_free(void* value) { if(value){Header* p=(Header*)value-1;live-=p->bytes;free(p);} }
