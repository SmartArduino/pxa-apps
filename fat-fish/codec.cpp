// SPDX-License-Identifier: MIT
#include "core.hpp"
#include "vendor/zlib/zlib.h"

namespace fish {
bool unpack(const uint8_t *input, size_t bytes, uint8_t *output, size_t pixels, const uint8_t *previous) {
    if (!input || !output || previous == output || !bytes || bytes > 32768 || !pixels || pixels > 32768) return false;
    struct Memory {
        alignas(max_align_t) uint8_t data[8192]{};
        size_t used = 0;
    } memory;
    z_stream stream{};
    stream.opaque = &memory;
    stream.zalloc = [](voidpf context, unsigned count, unsigned size) -> voidpf {
        auto &arena = *static_cast<Memory *>(context);
        const size_t alignment = alignof(max_align_t);
        const size_t at = (arena.used + alignment - 1) / alignment * alignment;
        if (!size || at > sizeof(arena.data) || count > (sizeof(arena.data) - at) / size) return nullptr;
        arena.used = at + size_t(count) * size;
        return arena.data + at;
    };
    // One complete frame is decoded in one call, so zlib uses its output as history.
    stream.zfree = [](voidpf, voidpf) {};
    stream.next_in = const_cast<Bytef *>(input);
    stream.avail_in = uInt(bytes);
    stream.next_out = output;
    stream.avail_out = uInt(pixels);
    if (inflateInit(&stream) != Z_OK) return false;
    const int status = inflate(&stream, Z_FINISH);
    const bool complete = status == Z_STREAM_END && stream.total_out == pixels && !stream.avail_in;
    inflateEnd(&stream);
    if (complete && previous)
        for (size_t i = 0; i < pixels; ++i) output[i] = uint8_t(output[i] + previous[i]);
    return complete;
}
} // namespace fish
