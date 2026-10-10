// Generate the same normal new-game save for both signed AOT Guests.
#include "game.hpp"
#include <array>
#include <cassert>
#include <cstdio>
int main(int argc, char** argv) {
    assert(argc==2);
    dungeon::pd_game_t game{};
    dungeon::pd_game_reset(&game,0x51ed270b);
    dungeon::pd_game_start_run(&game,0);
    std::array<uint8_t,1024> bytes{};
    const int size=dungeon::pd_game_serialize(&game,bytes.data(),bytes.size());
    assert(size>0);
    auto* file=std::fopen(argv[1],"wb");assert(file);
    assert(std::fwrite(bytes.data(),1,size,file)==static_cast<size_t>(size));
    assert(!std::fclose(file));
}
