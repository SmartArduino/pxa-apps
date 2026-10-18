#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
void Resolve_File(char *);
static uint64_t revision,now;
static unsigned scans;
extern "C" uint64_t port_time_us(void) { return now; }
extern "C" uint64_t port_filesystem_revision(void) { return revision; }
void Pxa_Resolve_File_Uncached(char *name) {
    ++scans;
    for(char *p=name;*p;++p) if(*p>='A' && *p<='Z') *p+='a'-'A';
}
int main() {
    char a[256]="/DATA/SOUNDS.MIX";Resolve_File(a);
    assert(!strcmp(a,"/data/sounds.mix") && scans==1);
    strcpy(a,"/Data/Sounds.Mix");Resolve_File(a);assert(scans==1);
    ++revision;strcpy(a,"/DATA/SOUNDS.MIX");Resolve_File(a);assert(scans==2);
    now=500000;Resolve_File(a);assert(scans==3);
    now+=1;Resolve_File(a);assert(scans==3);
    std::string longer(300,'A');Resolve_File(longer.data());assert(scans==4);
    for(unsigned i=0;i<40;++i) {
        char name[20];snprintf(name,sizeof(name),"/DATA/%u.MIX",i);Resolve_File(name);
    }
    assert(scans==44);
    strcpy(a,"/DATA/SOUNDS.MIX");Resolve_File(a);assert(scans==45);
    Resolve_File(nullptr);
}
