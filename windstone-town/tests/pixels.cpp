#include "../draw.hpp"

#include <cassert>

using namespace wind;

int main() {
    constexpr Color red=rgb(0xff0000),green=rgb(0x00ff00),blue=rgb(0x0000ff),white=rgb(0xffffff);
    std::array<unsigned char,522> sprite{};
    sprite[0]=3;
    sprite[2]=2;
    constexpr Color palette[]{red,green,blue};
    for (int i=0;i<3;++i) {
        sprite[4+2*i]=uint8_t(palette[i]);
        sprite[5+2*i]=uint8_t(palette[i]>>8);
    }
    constexpr uint8_t indices[]{0,1,255,2,0,1};
    std::copy(std::begin(indices),std::end(indices),sprite.begin()+516);
    std::array<std::byte,8*6*2> buffer{};
    auto surface=*pxa::Rgb565Pixels::from_bytes(buffer,8,6,16);
    auto at=[&](int x,int y){return Color((*surface.row(y))[x]);};
    const Rect source{0,0,3,2};

    surface.fill(white);
    Draw draw(surface);
    draw.blit(sprite.data(),source,{2,1});
    assert(at(2,1)==red && at(3,1)==green && at(4,1)==white);
    assert(at(2,2)==blue && at(3,2)==red && at(4,2)==green);
    assert(at(1,1)==white && at(5,2)==white);

    surface.fill(white);
    draw.blit(sprite.data(),source,{-1,1},true);
    assert(at(0,1)==green && at(1,1)==red);
    assert(at(0,2)==red && at(1,2)==blue);

    surface.fill(white);
    draw.blit_double(sprite.data(),source,{1,1});
    for (int y=0;y<4;++y)
        for (int x=0;x<6;++x) {
            const unsigned index=indices[(y/2)*3+x/2];
            assert(at(x+1,y+1)==(index==255?white:palette[index]));
        }
    assert(at(0,1)==white && at(7,4)==white);

    surface.fill(white);
    draw.clip={1,1,5,4};
    draw.blit_double(sprite.data(),source,{-1,0});
    assert(at(1,1)==green && at(2,1)==green && at(3,1)==white);
    assert(at(1,2)==red && at(2,2)==red && at(3,2)==green && at(4,2)==green);
    assert(at(0,1)==white && at(1,0)==white);
}
