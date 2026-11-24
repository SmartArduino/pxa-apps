#include "rgb565_blend.hpp"

#include <cassert>

using namespace tide;

static Color reference(Color background,Color foreground,int alpha) {
    const auto channel=[&](int shift,int mask) {
        return ((((background>>shift)&mask)*(15-alpha)+((foreground>>shift)&mask)*alpha+7)/15)<<shift;
    };
    return Color(channel(11,31)|channel(5,63)|channel(0,31));
}

int main() {
    for (int fg=0;fg<64;++fg) {
        const Color foreground=Color((fg%32)<<11 | fg<<5 | (31-fg%32));
        const GlyphInk ink(foreground);
        for (int alpha=0;alpha<=15;++alpha) {
            for (int bg=0;bg<64;++bg) {
                const Color background=Color((31-bg%32)<<11 | bg<<5 | (bg%32));
                const Color expected=reference(background,foreground,alpha);
                assert(Rgb565Blend::mix(background,foreground,alpha)==expected);
                assert(ink.over(background,alpha)==expected);
            }
        }
    }
}
