// MIT. Structural preflight runs before in-place VM relocation touches memory.
#pragma once
#include <stddef.h>
#include <stdint.h>
struct ArcadeCartridge { size_t image_size; uint32_t checksum; };
const char *arcade_cartridge_inspect(const uint8_t *,size_t,ArcadeCartridge *);
