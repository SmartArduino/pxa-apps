#pragma once
#include <stddef.h>
#include <stdint.h>

// Decode an original 8-bit, single-plane PCX into the existing indexed title
// buffer. Only final screen pixels are stored; there is no full-size RGB copy.
bool ol_decode_title_pcx(const uint8_t *source, size_t bytes,
                         uint8_t *indices, unsigned width, unsigned height,
                         uint8_t palette_rgb565[512]);
