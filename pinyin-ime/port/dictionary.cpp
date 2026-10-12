#include "dictionary.hpp"
#include "guest_stdio.h"
#include <pxa/core.hpp>
#include <algorithm>
#include <cstring>
#include <limits>

struct ImeFile { std::uint64_t handle, size, position; bool active, failed, eof; };
static ImeFile dictionary;
static pxa::Transport* transport;
void ime_bind_dictionary(pxa::Transport& owner, std::uint64_t handle, std::uint64_t bytes) noexcept {
    transport = &owner;
    dictionary = {handle, bytes, 0, false, false, false};
}
ImeFile* ime_fopen(const char* path, const char* mode) {
    if (!path || !mode || std::strcmp(path,"system.dict") ||
        std::strcmp(mode,"rb") || !dictionary.handle || dictionary.active) return nullptr;
    dictionary.position = 0; dictionary.active = true;
    dictionary.failed = dictionary.eof = false;
    return &dictionary;
}
ImeFile* ime_fdopen(int, const char*) { return nullptr; }
int ime_fclose(ImeFile* f) {
    if (f != &dictionary || !f->active) return -1;
    // The decoder has loaded its final tables; release the Host file/cache now.
    auto result = transport->close(f->handle);
    if (!result) return -1;
    f->active = false; f->handle = 0; return 0;
}
int ime_fseek(ImeFile* f, long offset, int origin) {
    if (f != &dictionary || !f->active) return -1;
    std::int64_t base = origin == SEEK_SET ? 0 : origin == SEEK_CUR ? f->position :
        origin == SEEK_END ? f->size : -1;
    if (base < 0 || offset < -base || (offset > 0 &&
        std::uint64_t(offset) > f->size - std::uint64_t(base))) return -1;
    auto position = std::uint64_t(base + offset);
    std::uint8_t payload[8]; pxa::wire::put64(reinterpret_cast<std::byte*>(payload), position);
    if (pxa_io(f->handle, 3, payload, sizeof payload) < 0) { f->failed = true; return -1; }
    f->position = position; f->eof = false; return 0;
}
long ime_ftell(ImeFile* f) { return f && f->position <= LONG_MAX ? long(f->position) : -1; }
size_t ime_fread(void* output, size_t size, size_t count, ImeFile* f) {
    if (f != &dictionary || !f->active || !size || count > SIZE_MAX / size) return 0;
    auto requested = size * count;
    auto bytes = std::min<std::uint64_t>(requested, f->size - f->position);
    std::size_t done = 0;
    while (done < bytes) {
        auto chunk = std::min<std::uint64_t>(bytes - done, INT32_MAX);
        auto n = pxa_io(f->handle, 1, static_cast<std::uint8_t*>(output) + done, chunk);
        if (n < 0 || std::uint32_t(n) > chunk) { f->failed = true; break; }
        if (!n) break;
        done += n; f->position += n;
    }
    if (done < requested) f->eof = true;
    return done / size;
}
size_t ime_fwrite(const void*, size_t, size_t, ImeFile* f) { if(f) f->failed = true; return 0; }
int ime_ferror(ImeFile* f) { return !f || f->failed; }
int ime_feof(ImeFile* f) { return !f || f->eof; }
int ime_no_open(const char*, int, ...) { return -1; }
off_t ime_no_lseek(int, off_t, int) { return -1; }
ssize_t ime_no_write(int, const void*, size_t) { return -1; }
int ime_no_truncate(int, off_t) { return -1; }
int ime_no_close(int) { return -1; }
int ime_no_unlink(const char*) { return -1; }
