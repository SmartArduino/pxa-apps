#pragma once

#include <array>
#include <cstdint>

namespace tide {

using Color = std::uint16_t;

struct Rgb565Blend {
    static constexpr std::uint32_t premultiply(Color color, int alpha) {
        // Each channel has a 10-bit lane. Even 15 * 63 fits without carrying.
        const std::uint32_t lanes=(std::uint32_t(color&0xf800)<<9) |
                                  (std::uint32_t(color&0x07e0)<<5) | (color&0x001f);
        return lanes*alpha;
    }

    static constexpr Color over(Color background, std::uint32_t foreground, int inverse_alpha) {
        constexpr std::uint32_t one=(1U<<20)|(1U<<10)|1U;
        auto sum=premultiply(background,inverse_alpha)+foreground+8*one;
        // For each 10-bit lane, (n + (n >> 4) + ((n + (n >> 4)) >> 8)) >> 4
        // equals (n-1)/15 over the range used here. The extra 1 in the sum
        // gives exact division, while the other 7 preserves round-to-nearest.
        sum+=(sum>>4)&(63*one);
        sum+=(sum>>8)&(3*one);
        const auto rounded=sum>>4;
        return Color(((rounded>>9)&0xf800) | ((rounded>>5)&0x07e0) | (rounded&0x001f));
    }

    static constexpr Color mix(Color a, Color b, int alpha) {
        return over(a,premultiply(b,alpha),15-alpha);
    }
};

class GlyphInk {
public:
    explicit GlyphInk(Color color) : solid(color) {
        const auto step=Rgb565Blend::premultiply(color,1);
        for (int i=1;i<15;++i) weighted[i]=step*i;
    }

    Color over(Color background, int alpha) const {
        return alpha==15?solid:Rgb565Blend::over(background,weighted[alpha],15-alpha);
    }

private:
    Color solid;
    std::array<std::uint32_t,15> weighted{};
};

} // namespace tide
