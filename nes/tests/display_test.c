/* GPL-2.0-or-later */
#include "../display.h"
#include <assert.h>
int main(void) {
    const unsigned screens[][2]={{240,240},{320,240},{480,480},{800,480},{480,854},{854,480},{640,640}};
    for(unsigned i=0;i<sizeof(screens)/sizeof(screens[0]);++i){
        unsigned w=screens[i][0],h=screens[i][1];
        assert(nes_display_canvas(&w,&h)==1);
        assert(w==screens[i][0]&&h==screens[i][1]);
    }
    unsigned w=854,h=854;assert(!nes_display_canvas(&w,&h));
    w=855;h=480;assert(!nes_display_canvas(&w,&h));
    w=480;h=855;assert(!nes_display_canvas(&w,&h));
}
