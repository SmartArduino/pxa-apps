/* Offline only. The original generator is the source of truth for pixels. */
#include "../block_textures.h"
#include "../voxel_font_data.h"
#include <assert.h>
#include <stdio.h>

static void emit(const void *data, size_t size) {
    assert(fwrite(data, 1, size, stdout) == size);
}
int main(void) {
    const uint16_t *palette = block_texture_palette();
    const block_index_set_t *indices = block_texture_indices();
    for (unsigned i = 0; i < 256; ++i) {
        unsigned char bytes[2] = {(unsigned char)palette[i], (unsigned char)(palette[i] >> 8)};
        emit(bytes, 2);
    }
    for (unsigned block = 1; block <= 15; ++block)
        for (unsigned face = 0; face < 3; ++face) emit(indices[block][face], 256);
    emit(voxel_font_small_pixels, sizeof(voxel_font_small_pixels));
    emit(voxel_font_medium_pixels, sizeof(voxel_font_medium_pixels));
    emit(voxel_font_large_pixels, sizeof(voxel_font_large_pixels));
    return 0;
}
