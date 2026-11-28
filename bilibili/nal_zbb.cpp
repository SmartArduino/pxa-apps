// SPDX-License-Identifier: MIT
#include "mp4.hpp"
#include "pxa.h"

extern "C" PXA_RV32_ZBB_VARIANT(scan_nal4)
int billy_scan_nal4_zbb(const uint8_t *data,size_t size,uint8_t *output) {
    return bili::scan_nal4(data,size,output);
}
