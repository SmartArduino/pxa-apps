#pragma once

#include "draw.hpp"

namespace sky {

struct CloudField {
    static constexpr int width=40, height=24, length=24, bank_count=17;
    static constexpr float cell=5.2f, layer=4.f;
    static constexpr Vec pivot{width*.5f,7.f,length*.5f};

    struct Bank {
        Vec center;
        float scale=1;
        Vec lower,upper;
        // The low nibble stores density, the high nibble stores incident light.
        std::array<std::uint8_t,width*height*length> voxels{};
        std::array<std::uint16_t,width*length> columns{};
        static constexpr int index(int x,int y,int z) { return (z*height+y)*width+x; }
        std::uint8_t sample(int x,int y,int z) const { return voxels[index(x,y,z)]; }
    };
    std::array<Bank,bank_count> banks{};
    std::array<std::uint8_t,World::map_size*World::map_size> shadows{};

    bool ready() const { return generated==banks.size(); }
    void generate_next(std::uint32_t seed);
    float density_at(Vec position,float time) const;
    void draw(Scene& scene,float time) const;

private:
    unsigned generated=0;
};

} // namespace sky
