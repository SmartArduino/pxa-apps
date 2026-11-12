// MIT. Bounded per-cartridge settings; serialized by the PXA save API.
#include "pxt.h"
#include <string.h>
namespace {
struct Entry { char key[128]; unsigned size; uint8_t value[1024]; };
struct Save { uint32_t magic; Entry entries[32]; };
Save save;
Entry *find(const char *key, bool create) {
    for (auto &e : save.entries) if (e.key[0] && !strcmp(e.key, key)) return &e;
    if (create && strlen(key) < sizeof(Entry::key)) {
        for (auto &e : save.entries) if (!e.key[0]) { strcpy(e.key, key); return &e; }
    }
    return nullptr;
}
}
namespace settings {
int _set(String key, Buffer data) {
    if (data->length > 1024) return -1;
    auto e = find(key->getUTF8Data(), true); if (!e) return -1;
    e->size = data->length; memcpy(e->value, data->data, e->size); return 0;
}
int _remove(String key) { auto e = find(key->getUTF8Data(), false); if (!e) return -1; memset(e, 0, sizeof(*e)); return 0; }
bool _exists(String key) { return find(key->getUTF8Data(), false) != nullptr; }
Buffer _get(String key) { auto e = find(key->getUTF8Data(), false); return e ? mkBuffer(e->value, e->size) : nullptr; }
void _userClean() { for (auto &e : save.entries) if (e.key[0] != '#') memset(&e, 0, sizeof(e)); }
RefCollection *_list(String prefix) {
    auto list = Array_::mk(); registerGCObj(list);
    const char *p = prefix->getUTF8Data(); auto n = strlen(p);
    for (auto &e : save.entries) if (e.key[0] && (p[0] == '#' || e.key[0] != '#') && !strncmp(e.key, p, n)) {
        auto name = mkString(e.key, -1); registerGCObj(name);
        list->head.push((TValue)name); unregisterGCObj(name);
    }
    unregisterGCObj(list); return list;
}
}
extern "C" {
void arcade_settings_reset() { memset(&save, 0, sizeof(save)); save.magic = 0x3158434d; }
size_t arcade_settings_size() { return sizeof(save); }
bool arcade_settings_export(void *data, size_t size) { if (size != sizeof(save)) return false; memcpy(data, &save, size); return true; }
bool arcade_settings_import(const void *data, size_t size) {
    if (size != sizeof(save)) return false;
    const auto *in = static_cast<const Save *>(data); if (in->magic != 0x3158434d) return false;
    for (const auto &e : in->entries) if (!memchr(e.key, 0, sizeof(e.key)) || e.size > sizeof(e.value)) return false;
    memcpy(&save, data, size); return true;
}
}
