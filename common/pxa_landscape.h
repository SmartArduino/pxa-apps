// SPDX-License-Identifier: MIT
#ifndef PXA_APP_LANDSCAPE_H
#define PXA_APP_LANDSCAPE_H
#include "pxa_window.h"
/* Optional Window 0.5 preference: unsupported/native-only panels keep their
 * actual geometry; square panels keep their angle. Never swap sizes locally. */
static inline int32_t pxa_app_prefer_landscape(void) {
    const uint8_t payload[]={9,0x80,1,0,2};
    return pxa_control_call(PXA_WINDOW_SERVICE,PXA_WINDOW_CONFIGURE,0,payload,sizeof(payload));
}
#endif
