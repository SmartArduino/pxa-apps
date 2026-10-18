/* Keep Vanilla-Conquer's case resolution, with bounded reuse of directory
 * scans. No heap allocation and no persistent negative lookup cache. */
#include <cstdint>
#include <cstring>
extern "C" uint64_t port_filesystem_revision(void);
extern "C" uint64_t port_time_us(void);
void Pxa_Resolve_File_Uncached(char *);
static struct { uint64_t expires; char path[256]; } cache[16];
static uint64_t revision;
static unsigned next_slot;
static bool same_path(const char *a,const char *b) {
    for(;*a && *b;++a,++b) {
        unsigned char x=*a,y=*b;
        if(x>='A' && x<='Z')x+='a'-'A';
        if(y>='A' && y<='Z')y+='a'-'A';
        if(x!=y)return false;
    }
    return *a==*b;
}
void Resolve_File(char *name) {
    if(!name)return;
#ifdef PORT_CNC_CANONICAL_PATHS
    /* Both PXA private paths and the fixed SD importer are canonical. There
     * is no directory scan to perform for every archived SHP/FNT/AUD name. */
    for(char *p=name;*p;++p)if(*p>='A'&&*p<='Z')*p+='a'-'A';
#else
    uint64_t now=port_time_us(),current=port_filesystem_revision();
    if(current!=revision) { memset(cache,0,sizeof(cache));revision=current; }
    if(strlen(name)>=sizeof(cache[0].path)) { Pxa_Resolve_File_Uncached(name);return; }
    for(auto &entry:cache) if(entry.expires>now && same_path(name,entry.path)) {
        strcpy(name,entry.path);return;
    }
    Pxa_Resolve_File_Uncached(name);
    auto &entry=cache[next_slot++%16];
    strcpy(entry.path,name);entry.expires=now+500000;
#endif
}
