/* GPL-2.0-or-later */
#include "../display.h"
#include <assert.h>

int main(void) {
    const unsigned screens[][2] = {
        {240, 240}, {320, 240}, {480, 480}, {800, 480},
        {480, 854}, {854, 480}, {640, 640},
    };
    for (unsigned i = 0; i < sizeof(screens) / sizeof(screens[0]); ++i) {
        unsigned width = screens[i][0], height = screens[i][1];
        assert(emu_display_canvas(&width, &height) == 1);
        assert(width == screens[i][0] && height == screens[i][1]);
    }
    const unsigned invalid[][2] = {{854, 854}, {855, 480}, {480, 855}, {199, 240}, {240, 159}};
    for (unsigned i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        unsigned width = invalid[i][0], height = invalid[i][1];
        assert(emu_display_canvas(&width, &height) == 0);
    }
}
