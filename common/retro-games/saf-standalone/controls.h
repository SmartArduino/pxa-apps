/* SPDX-License-Identifier: MIT. User touch masks; no synthetic rule state. */
#ifndef PXA_SAF_STANDALONE_CONTROLS_H
#define PXA_SAF_STANDALONE_CONTROLS_H
static inline unsigned saf_touch_mask(int x,int y) {
#if SAF_ENTRY_ID == 1
    if(y>=316&&y<336){if(x>=184&&x<278)return 36; if(x>=282&&x<380)return 40;}
#endif
    if(x>=58&&x<100&&y>=316&&y<346)return 1;
    if(x>=58&&x<100&&y>=376&&y<406)return 2;
    if(x>=16&&x<58&&y>=346&&y<376)return 4;
    if(x>=100&&x<142&&y>=346&&y<376)return 8;
    if(y>=340&&y<404){if(x>=184&&x<248)return 16;if(x>=250&&x<314)return 32;if(x>=316&&x<380)return 64;}
    return 0;
}
#endif
