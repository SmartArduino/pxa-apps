#pragma once
#include <stdint.h>
static inline uint32_t esp_rom_crc32_le(uint32_t crc, const uint8_t *p,
                                        uint32_t n) {
  crc = ~crc;
  while (n--) {
    crc ^= *p++;
    for (int i = 0; i < 8; i++)
      crc = (crc >> 1) ^ (0xedb88320u & -(crc & 1));
  }
  return ~crc;
}
static inline uint16_t esp_rom_crc16_le(uint16_t crc, const uint8_t *p,
                                        uint32_t n) {
  crc = ~crc;
  while (n--) {
    crc ^= *p++;
    for (int i = 0; i < 8; i++)
      crc = (crc >> 1) ^ (0xa001u & -(crc & 1));
  }
  return ~crc;
}
