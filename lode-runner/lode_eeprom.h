/* SPDX-License-Identifier: MIT. AVR EEPROM addresses map to bounded session bytes. */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <EEPROM.h>
inline uint8_t eeprom_read_byte(const uint8_t *address) {
    return EEPROM.read(static_cast<int>(reinterpret_cast<uintptr_t>(address)));
}
inline uint16_t eeprom_read_word(const uint16_t *address) {
    const uintptr_t i=reinterpret_cast<uintptr_t>(address);
    return static_cast<uint16_t>(EEPROM.read(static_cast<int>(i)) |
                                (static_cast<uint16_t>(EEPROM.read(static_cast<int>(i+1)))<<8));
}
inline void eeprom_update_byte(uint8_t *address,uint8_t value) {
    EEPROM.update(static_cast<int>(reinterpret_cast<uintptr_t>(address)),value);
}
inline void eeprom_update_word(uint16_t *address,uint16_t value) {
    const uintptr_t i=reinterpret_cast<uintptr_t>(address);
    EEPROM.update(static_cast<int>(i),static_cast<uint8_t>(value));
    EEPROM.update(static_cast<int>(i+1),static_cast<uint8_t>(value>>8));
}
