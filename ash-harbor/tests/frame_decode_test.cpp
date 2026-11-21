// SPDX-License-Identifier: MIT
#include "../frame_decode.hpp"
#include <cassert>
#include <concepts>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <vector>
#include <cstdio>

static_assert(std::same_as<decltype(ash::decodeFramePage(0,std::span<std::byte>{})),bool>);

int main(int argc,char** argv){
    assert(argc==2);
    std::vector<std::byte> pixels;
    for(unsigned page=0;page<ash::art::pages;++page){
        pixels.resize(ash::art::page_bytes[page]);assert(ash::decodeFramePage(page,pixels));
        std::ifstream input(std::filesystem::path(argv[1])/"resources"/
            ("atlas-"+std::to_string(page)+".index8"),std::ios::binary);
        assert(input);
        std::vector<char> expected((std::istreambuf_iterator<char>(input)),{});
        const auto& actual=pixels;
        assert(actual.size()==expected.size());
        for(unsigned i=0;i<actual.size();++i)
            assert(std::to_integer<unsigned char>(actual[i])==static_cast<unsigned char>(expected[i]));
        assert(!ash::decodeFramePage(page,std::span<std::byte>(pixels).first(pixels.size()-1)));
    }
    assert(!ash::decodeFramePage(ash::art::pages,pixels));
    std::printf("%u atlases decoded losslessly with a 64 KiB native stack; bounds passed.\n",ash::art::pages);
}
