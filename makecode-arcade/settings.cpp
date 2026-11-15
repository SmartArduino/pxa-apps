// MIT. Bounded per-cartridge settings with transactional, checksummed saves.
#include "pxt.h"
#include <string.h>
extern "C" uint32_t emu_port_crc32(uint32_t, const void *, size_t);
namespace {
struct Entry { char key[128]; uint32_t size; uint8_t value[1024]; };
struct Save { uint32_t magic; Entry entries[32]; };
Save save;
constexpr uint32_t legacy_magic = 0x3158434d, file_magic = 0x3258434d;
constexpr size_t payload_size = sizeof(save.entries), file_size = 16 + payload_size;
uint32_t read32(const uint8_t *p) { return p[0] | uint32_t(p[1])<<8 | uint32_t(p[2])<<16 | uint32_t(p[3])<<24; }
void write32(uint8_t *p, uint32_t x) { for (unsigned i=0;i<4;++i) p[i]=x>>(8*i); }
const char *key_data(String key, bool allow_empty=false) {
    if (!key) return nullptr;
    auto size=key->getUTF8Size();
    const auto *text=key->getUTF8Data();
    if ((!size && !allow_empty) || size>=sizeof(Entry::key) || memchr(text,0,size)) return nullptr;
    return text;
}
Entry *find(const char *key, bool create=false) {
    if (!key) return nullptr;
    for (auto &e:save.entries) if (e.key[0] && !strcmp(e.key,key)) return &e;
    if (create) for (auto &e:save.entries) if (!e.key[0]) { strcpy(e.key,key); return &e; }
    return nullptr;
}
bool validate(Save &candidate) {
    for (unsigned i=0;i<32;++i) {
        auto &e=candidate.entries[i];
        const auto *end=static_cast<const char *>(memchr(e.key,0,sizeof(e.key)));
        if (!end || e.size>sizeof(e.value) || (!e.key[0] && e.size)) return false;
        for (unsigned j=0;j<i && e.key[0];++j)
            if (!strcmp(e.key,candidate.entries[j].key)) return false;
        memset(e.key+(end-e.key),0,sizeof(e.key)-(end-e.key));
        memset(e.value+e.size,0,sizeof(e.value)-e.size);
    }
    return true;
}
}
namespace settings {
int _set(String key, Buffer data) {
    auto text=key_data(key);
    if (!text || !data || data->length<0 || size_t(data->length)>sizeof(Entry::value)) return -1;
    auto e=find(text,true); if (!e) return -1;
    e->size=data->length; memcpy(e->value,data->data,e->size);
    memset(e->value+e->size,0,sizeof(e->value)-e->size);
    return 0;
}
int _remove(String key) { auto e=find(key_data(key)); if (!e) return -1; memset(e,0,sizeof(*e)); return 0; }
bool _exists(String key) { return find(key_data(key))!=nullptr; }
Buffer _get(String key) { auto e=find(key_data(key)); return e?mkBuffer(e->value,e->size):nullptr; }
void _userClean() { for (auto &e:save.entries) if (e.key[0]!='#') memset(&e,0,sizeof(e)); }
RefCollection *_list(String prefix) {
    auto list=Array_::mk(); registerGCObj(list);
    auto text=key_data(prefix,true);
    if (text) {
        auto length=strlen(text);
        for (auto &e:save.entries) if (e.key[0] && (text[0]=='#' || e.key[0]!='#') && !strncmp(e.key,text,length)) {
            auto name=mkString(e.key,-1); registerGCObj(name);
            list->head.push((TValue)name); unregisterGCObj(name);
        }
    }
    unregisterGCObj(list); return list;
}
}
extern "C" {
void arcade_settings_reset() { memset(&save,0,sizeof(save)); save.magic=legacy_magic; }
size_t arcade_settings_size() { return file_size; }
bool arcade_settings_export(void *data,size_t size) {
    if (!data || size!=file_size) return false;
    auto bytes=static_cast<uint8_t *>(data);
    write32(bytes,file_magic);write32(bytes+4,2);write32(bytes+8,payload_size);
    memcpy(bytes+16,save.entries,payload_size);
    write32(bytes+12,emu_port_crc32(0,bytes+16,payload_size));
    return true;
}
bool arcade_settings_import(const void *data,size_t size) {
    if (!data) return false;
    auto bytes=static_cast<const uint8_t *>(data);
    Save candidate{};
    if (size==sizeof(Save) && read32(bytes)==legacy_magic) {
        memcpy(&candidate,data,size);
    } else if (size==file_size && read32(bytes)==file_magic && read32(bytes+4)==2 &&
               read32(bytes+8)==payload_size && read32(bytes+12)==emu_port_crc32(0,bytes+16,payload_size)) {
        candidate.magic=legacy_magic;
        memcpy(candidate.entries,bytes+16,payload_size);
    } else return false;
    if (!validate(candidate)) return false;
    save=candidate;
    return true;
}
}
