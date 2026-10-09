#include "../voxel_world.hpp"
#include "../../common/voxel_benchmark.h"
extern "C" {
#include "../../voxel-craft/game.h"
}
#include <cassert>
#include <cstdio>
static voxel::World world;
int main() {
    static_assert(VOXEL_BENCH_SCENE);
    game_generate(VOXEL_BENCH_SEED);
    player_t player{};
    player.x=VOXEL_BENCH_X; player.z=VOXEL_BENCH_Z;
    game_ensure_chunks(&player);
    world.generate(VOXEL_BENCH_SEED);
    unsigned hash=2166136261u;
    for(int y=0;y<24;++y) for(int z=0;z<64;++z) for(int x=0;x<64;++x) {
        const unsigned id=world.at(x,y,z);
        assert(id==voxel_benchmark_block(x,y,z));
        assert(id==game_block(x,y,z));
        hash=(hash^id)*16777619u;
    }
    for(int z=-16;z<80;++z) for(int x=-16;x<80;++x)
        if(x<0 || z<0 || x>=64 || z>=64) assert(game_block(x,7,z)==0);
    std::printf("Canonical C/C++: 98304 equal voxels, FNV1a=%08x, outside=air\n",hash);
    unsigned maximum=0, overflows=0;
    for(int cx=0;cx<voxel::kChunksX;++cx) for(int cy=0;cy<voxel::kChunksY;++cy)
        for(int cz=0;cz<voxel::kChunksZ;++cz) {
            overflows+=world.chunk_mesh(cx,cy,cz).overflow;
            unsigned count=0;
            auto visit=[&](const voxel::MeshQuad&) noexcept {++count;return true;};
            assert(world.visit_chunk(cx,cy,cz,visit));
            if(count>maximum)maximum=count;
        }
    std::printf("Canonical cache: max quads=%u overflow chunks=%u\n",maximum,overflows);
    assert(overflows==0); // Steady fixture timing must not rebuild overflow pages.
}
