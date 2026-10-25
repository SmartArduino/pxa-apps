// SPDX-License-Identifier: CC0-1.0
#pragma once
#include "i18n.hpp"
namespace pocket_ui {
// Independent, versioned four-byte preference; game save formats stay untouched.
inline void encode_locale(uint8_t out[4], pocket::Locale locale) {
    out[0] = 'P'; out[1] = 'L'; out[2] = 1; out[3] = uint8_t(locale);
}
inline bool decode_locale(const uint8_t *data, size_t size, pocket::Locale &locale) {
    if (!data || size != 4 || data[0] != 'P' || data[1] != 'L' || data[2] != 1 || data[3] > 1) return false;
    locale = pocket::Locale(data[3]); return true;
}
}
