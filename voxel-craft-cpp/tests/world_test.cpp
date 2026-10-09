#include "../voxel_world.hpp"
#include <array>
#include <cassert>
#include <cstdio>

using namespace voxel;
static World world;
static constexpr int N = kChunkSize;
static constexpr int directions[6][3] = {
    {0,1,0},{1,0,0},{0,0,-1},{0,-1,0},{-1,0,0},{0,0,1}};

// Expand merged faces and compare every cell with independent voxel adjacency.
static void check_chunk(int cx, int cy, int cz) {
    std::array<unsigned char, 6 * N * N * N> faces{};
    auto visit = [&](const MeshQuad& q) noexcept {
        assert(q.face < 6 && q.plane < N && q.width && q.height);
        assert(q.u + q.width <= N && q.v + q.height <= N && q.slot < 45);
        for (int v = q.v; v < q.v + q.height; ++v)
            for (int u = q.u; u < q.u + q.width; ++u) {
                auto& cell = faces[((q.face * N + q.plane) * N + v) * N + u];
                assert(cell == 0); cell = q.slot + 1 + (q.cutout ? 128 : 0);
            }
        return true;
    };
    assert(world.visit_chunk(cx, cy, cz, visit));
    for (int f = 0; f < 6; ++f) for (int p = 0; p < N; ++p)
        for (int v = 0; v < N; ++v) for (int u = 0; u < N; ++u) {
            int x=cx*N, y=cy*N, z=cz*N;
            if (f == 0 || f == 3) {y+=p; x+=u; z+=v;}
            else if (f == 1 || f == 4) {x+=p; z+=u; y+=v;}
            else {z+=p; x+=u; y+=v;}
            const auto id=world.at(x,y,z);
            const auto neighbor=world.at(x+directions[f][0],y+directions[f][1],z+directions[f][2]);
            const auto& info=block_info(id);
            const bool visible=id!=kAir && (info.solid || info.cutout) &&
                !block_info(neighbor).opaque && !(neighbor==id && (id==kWater || info.cutout));
            const unsigned expected=visible ? 1+info.texture[f==0?0:f==3?2:1]+(info.cutout?128:0) : 0;
            assert(faces[((f*N+p)*N+v)*N+u] == expected);
        }
}

int main() {
    for (int block=1; block<=15; ++block)
        for (int face=0; face<3; ++face)
            assert(block_info(block).texture[face] == (block-1)*3+face);
    unsigned trees=0, overflows=0, max_quads=0;
    for (unsigned i=0; i<32; ++i) {
        world.generate(i==0 ? 0x5ae1 : i*7919);
        for (int x=0; x<kWorldX; ++x) for (int z=0; z<kWorldZ; ++z)
            for (int y=1; y<kWorldY; ++y) {
                if (world.at(x,y,z)!=kWood || world.at(x,y-1,z)==kWood) continue;
                ++trees;
                const auto ground=world.at(x,y-1,z);
                assert(ground==kGrass || ground==kSand); // Grassland or beach, never a bush.
                int top=y;
                while (world.at(x,top+1,z)==kWood) ++top;
                assert(world.at(x,top+1,z)==kLeaves || world.at(x,top+1,z)==kWood);
            }
        for (int cx=0; cx<kChunksX; ++cx) for (int cy=0; cy<kChunksY; ++cy)
            for (int cz=0; cz<kChunksZ; ++cz) {
                overflows += world.chunk_mesh(cx,cy,cz).overflow;
                unsigned quads=0;
                auto count=[&](const MeshQuad&) noexcept {++quads; return true;};
                assert(world.visit_chunk(cx,cy,cz,count));
                if (quads>max_quads) max_quads=quads;
                check_chunk(cx,cy,cz);
            }
    }
    // User edits can exceed the fixed cache. Every face still has to be visited.
    for (int x=0; x<N*2; ++x) for (int y=0; y<N*2; ++y) for (int z=0; z<N*2; ++z)
        world.set(x,y,z, x<N && y<N && z<N && ((x+y+z)&1) ? kStone : kAir);
    assert(world.chunk_mesh(0,0,0).overflow);
    check_chunk(0,0,0);
    unsigned paged=0, streamed=0;
    auto count_all=[&](const MeshQuad&) noexcept {++streamed; return true;};
    assert(world.visit_chunk(0,0,0,count_all));
    for (const ChunkMesh* page=&world.chunk_mesh(0,0,0);;) {
        paged+=page->count;
        if (!page->overflow) break;
        page=&world.chunk_mesh_page(0,0,0,paged);
    }
    assert(paged==streamed && paged>kMaxQuadsPerChunk);
    check_chunk(0,0,0); // First page restored after overflow pagination.
    unsigned count=0;
    auto bounded=[&](const MeshQuad&) noexcept {return ++count < 7;};
    assert(!world.visit_chunk(0,0,0,bounded) && count==7);
    world.set(0,0,0,kGrass);
    check_chunk(0,0,0); // Dirty overflow cache rebuild after editing.
    for (const auto& xyz : std::array<std::array<int,3>,7>{{
        {N-1,7,7},{N,7,7},{7,N-1,7},{7,N,7},
        {7,7,N-1},{7,7,N},{7,kWorldY-1,7}}}) {
        world.set(xyz[0],xyz[1],xyz[2],kWood);
        for (int cx=0;cx<2;++cx) for (int cy=0;cy<kChunksY;++cy)
            for (int cz=0;cz<2;++cz) check_chunk(cx,cy,cz);
    }
    // Non-colliding vegetation in front of dirt is a selectable block. The
    // DDA must return its cell/entry normal in both horizontal and down rays.
    for (int y=0;y<4;++y) for (int z=40;z<45;++z)
        world.set(40,y,z,kAir);
    world.set(40,0,42,kDirt);
    for (auto plant:{kLeaves,kBush,kFlower}) {
        world.set(40,1,42,plant);
        auto h=raycast(world,40.5f,3.f,42.5f,0,-1,0,5.5f);
        assert(h.hit&&h.y==1&&h.ny==1&&world.at(h.x,h.y,h.z)==plant);
        h=raycast(world,40.5f,1.5f,40.5f,0,0,1,5.5f);
        assert(h.hit&&h.z==42&&h.nz==-1);
        assert(plant==kLeaves||!world.solid(40,1,42));
    }
    world.set(40,1,42,kWater);
    auto through_water=raycast(world,40.5f,3.f,42.5f,0,-1,0,5.5f);
    assert(through_water.hit&&through_water.y==0);
    assert(!raycast(world,-2.f,40.f,-2.f,0,0,1,5.5f).hit);
    std::printf("World bytes=%zu, chunks=%d, max terrain quads=%u; ", sizeof(World), kChunksX*kChunksY*kChunksZ, max_quads);
    std::printf("World: 32 seeds, %u rooted trees, %u terrain cache overflows; overflow coverage OK\n",trees,overflows);
}
