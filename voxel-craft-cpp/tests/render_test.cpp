#include "../voxel_draw_path.hpp"
#include "../voxel_mesher.hpp"
#include <pxa/raster.h>

#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

static unsigned allocations;
void* operator new(std::size_t size) {
    ++allocations;
    if (auto* p = std::malloc(size)) return p;
    std::abort();
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
static std::array<std::uint8_t, 49152> commands{};
static std::size_t command_size;
extern "C" std::int32_t pxa_submit(const std::uint8_t*, std::uint32_t) { return 0; }
extern "C" std::int32_t pxa_io(std::uint64_t handle, std::uint32_t op,
                                  std::uint8_t* bytes, std::uint32_t size) {
    assert(handle == 77 && op == 0x101 && size <= commands.size());
    std::memcpy(commands.data(), bytes, size);
    command_size = size;
    return size;
}

static voxel::World world;
static pxa::game::DrawBuffer<49152> buffer;
static std::array<std::uint16_t, 296 * 240> pixels{}, depth{};

int main() {
    using voxel::DrawPath;
    std::array<pxa::game::Vertex, 4> quad{{
        {.x_q4=0,.y_q4=0,.depth_q8=4096},
        {.x_q4=64,.y_q4=0,.depth_q8=4096},
        {.x_q4=64,.y_q4=64,.depth_q8=4096},
        {.x_q4=0,.y_q4=64,.depth_q8=4096}}};
    assert(voxel::choose_draw_path(quad, 1, false) == DrawPath::affine);
    quad[1].x_q4 = quad[2].x_q4 = 63;
    assert(voxel::choose_draw_path(quad, 1, false) == DrawPath::solid);
    assert(voxel::choose_draw_path(quad, 1, true) == DrawPath::affine);
    quad[1].x_q4 = quad[2].x_q4 = 64;
    quad[1].depth_q8 = quad[2].depth_q8 = 4606;
    assert(voxel::choose_draw_path(quad, 1, false) == DrawPath::affine);
    assert(voxel::choose_draw_path(quad, 2, false) == DrawPath::perspective);
    ++quad[1].depth_q8;
    assert(voxel::choose_draw_path(quad, 1, false) == DrawPath::perspective);
    quad[0].depth_q8 = 64; // A clipped near-plane crossing is not an affine face.
    assert(voxel::choose_draw_path(quad, 8, true) == DrawPath::perspective);
    quad[0].depth_q8 = 0;
    assert(voxel::choose_draw_path(quad, 1, false) == DrawPath::perspective);

    world.generate(0x5ae1);
    auto projector = pxa::game3d::Projector::create(296, 240, 1.15f, 0.25f, 64);
    assert(projector);
    pxa::Transport transport;
    transport.phase(pxa::Phase::event);
    pxa::game::Renderer renderer(transport, 77, PXA_RASTER_CAP_KNOWN_MASK);
    std::array<std::uint16_t, 16 * 256> palette{};
    for (int level = 0; level < 16; ++level) palette[level * 256 + 1] = 0x07e0;
    const std::array<std::uint8_t, 4> texture{1, 0, 0, 1};
    pxa_raster_resources_t resources{};
    resources.capabilities = renderer.capabilities();
    resources.palette = palette.data();
    resources.palette_light_levels = 16;
    for (auto& slot : resources.textures) slot = {texture.data(), 2, 2};
    pxa_raster_target_t target{};
    target.pixels = pixels.data();
    target.depth_pixels = depth.data();
    target.width = target.stride_pixels = target.depth_stride_pixels = 296;
    target.height = 240;
    target.scratch_mode = PXA_RASTER_SCRATCH_DEPTH16;
    voxel::Camera camera{32.5f, float(world.highest_block(32,32)) + 2.6f, 32.5f};
    voxel::DrawBudget budget;
    budget.focal = 240 / (2 * std::tan(1.15f * 0.5f));
    budget.max_distance = 24;
    unsigned affine = 0, solid = 0;
    for (const float yaw : {0.0f, 1.0f, 2.4f, -1.1f}) {
        camera.yaw = yaw;
        camera.pitch = yaw == 1.0f ? -0.4f : 0.0f;
        std::uint32_t original_faces = 0;
        for (bool cheap : {false, true}) {
            budget.cheap_paths = cheap;
            const auto before = allocations;
            auto frame = renderer.frame(buffer);
            frame.clear({0x001f});
            const auto stats = voxel::draw_world(frame, *projector, world, camera, budget);
            assert(!stats.exhausted && stats.faces_drawn > 0);
            assert(frame.submit());
            assert(allocations == before);
            if (!cheap) original_faces = stats.faces_drawn;
            else assert(stats.faces_drawn == original_faces);
            affine += stats.affine_faces;
            solid += stats.solid_faces;
            pxa_raster_draw_list_view_t view{};
            assert(pxa_raster_validate_draw_list(commands.data(), command_size, &target,
                                                  &resources, &view) == PXA_STATUS_OK);
            pxa_raster_execute_draw_list(commands.data(), &view, &target, &resources, nullptr);
#if VOXEL_PROFILE
            const auto previous=commands;
            const auto previous_size=command_size;
            auto staged=renderer.frame(buffer);
            staged.clear({0x001f});
            budget.staging=buffer.bytes().subspan(staged.bytes_used());
            const auto staged_stats=voxel::draw_world(staged,*projector,world,camera,budget);
            assert(!staged_stats.exhausted && staged_stats.faces_drawn==stats.faces_drawn);
            assert(staged.submit() && command_size==previous_size && allocations==before);
            budget.staging={};
            for(std::size_t i=0;i<command_size;++i)
                if(i<20 || i>=28) assert(commands[i]==previous[i]);
#endif
            // Every textured cutout retains index-0 transparency in both paths.
            for (std::size_t offset = 32; offset < command_size;) {
                const auto* record = commands.data() + offset;
                const auto flags = record[1];
                if ((record[0] == 3 || record[0] == 6) && !(flags & 1)) {
                    const auto& info = voxel::block_info(record[4] / 3 + 1);
                    assert(bool(flags & 8) == info.cutout);
                }
                offset += std::uint16_t(record[2]) | std::uint16_t(record[3]) << 8;
            }
        }
    }
    assert(affine && solid);
    // A fixed pool has nonempty storage even with no live effects. An idle
    // pool must emit no commands; live debris must share terrain depth.
    std::array<voxel::Particle, 64> effects{};
    auto idle = renderer.frame(buffer);
    idle.clear({0});
    const auto allocations_before_effects = allocations;
    voxel::draw_particles(idle, *projector, {}, effects);
    assert(idle.bytes_used() == 40 && idle.submit());
    effects[0].z = 10;
    effects[0].life = 1;
    effects[0].color = 0xffff;
    auto live = renderer.frame(buffer);
    live.clear({0});
    voxel::draw_particles(live, *projector, {}, effects);
    assert(live.bytes_used() > 40 && live.submit());
    assert(allocations == allocations_before_effects);
    pxa_raster_draw_list_view_t effects_view{};
    assert(pxa_raster_validate_draw_list(commands.data(), command_size, &target,
                                       &resources, &effects_view) == PXA_STATUS_OK);
    assert(effects_view.uses_depth);
    pxa_raster_execute_draw_list(commands.data(), &effects_view, &target, &resources, nullptr);
    assert(std::find(pixels.begin(), pixels.end(), 0xffff) != pixels.end());
    // A tiny list budget must stop cleanly, including pending triangle bytes.
    budget.max_bytes = 512;
    auto bounded = renderer.frame(buffer);
    bounded.clear({0});
    assert(voxel::draw_world(bounded, *projector, world, camera, budget).exhausted);
    assert(bounded.bytes_used() <= budget.max_bytes);
    assert(bounded.submit());
    // One isolated cube viewed along all six face normals catches rotated,
    // flipped and shifted UVs. Compare encoded corners against the independent
    // scalar camera transform, allowing only one fixed-point rounding unit.
    for (int x=0; x<voxel::kWorldX; ++x)
        for (int y=0; y<voxel::kWorldY; ++y)
            for (int z=0; z<voxel::kWorldZ; ++z) world.set(x,y,z,voxel::kAir);
    world.set(32,12,32,voxel::kStone);
    const std::array<voxel::Camera, 6> views{{
        {32.5f,20,32.5f,0,1.570796327f},
        {40,12.5f,32.5f,-1.570796327f,0},
        {32.5f,12.5f,25,0,0},
        {32.5f,6,32.5f,0,-1.570796327f},
        {25,12.5f,32.5f,1.570796327f,0},
        {32.5f,12.5f,40,3.141592654f,0}}};
    const float corners[6][4][3] = {
        {{32,13,32},{33,13,32},{33,13,33},{32,13,33}},
        {{33,12,32},{33,12,33},{33,13,33},{33,13,32}},
        {{32,12,32},{33,12,32},{33,13,32},{32,13,32}},
        {{32,12,32},{33,12,32},{33,12,33},{32,12,33}},
        {{32,12,32},{32,12,33},{32,13,33},{32,13,32}},
        {{32,12,33},{33,12,33},{33,13,33},{32,13,33}}};
    budget.max_bytes = 44000;
    budget.cheap_paths = false;
    for (unsigned face = 0; face < 6; ++face) {
        std::array<pxa::game3d::MeshVertex, 4> source{};
                for (unsigned corner = 0; corner < 4; ++corner) {
            const auto& p = corners[face][corner];
            views[face].to_camera(p[0],p[1],p[2],source[corner].position);
            source[corner].u = corner == 1 || corner == 2 ? 16 : 0;
            source[corner].v = corner >= 2 ? 0 : 16;
        }
        std::array<pxa::game::Vertex, 10> expected{};
        const auto count = projector->project_polygon(source,expected);
        assert(count && *count == 4);
        const auto before = allocations;
        auto cube = renderer.frame(buffer);
        cube.clear({0});
        const auto stats = voxel::draw_world(cube,*projector,world,views[face],budget);
        assert(stats.faces_drawn == 1 && cube.submit());
#if VOXEL_PROFILE
        const auto previous_size=command_size;
        auto previous=commands;
        auto staged=renderer.frame(buffer);
        staged.clear({0});
        budget.staging=buffer.bytes().subspan(staged.bytes_used());
        const auto staged_stats=voxel::draw_world(staged,*projector,world,views[face],budget);
        assert(staged_stats.faces_drawn==stats.faces_drawn && staged.submit());
        budget.staging={};
        assert(command_size==previous_size);
        // Frame ids necessarily differ; every command and all other header
        // bytes must match the ordinary path's validated packet exactly.
        for(std::size_t i=0;i<command_size;++i)
            if(i<20 || i>=28) assert(commands[i]==previous[i]);
#endif
        assert(allocations == before && command_size == 32 + 8 + 56);
        const auto* record = reinterpret_cast<const std::byte*>(commands.data()) + 40;
        assert(record[0] == std::byte{3});
        const auto slot = face == 0 ? 6 : face == 3 ? 8 : 7;
        assert(record[4] == std::byte(slot));
        for (unsigned corner = 0; corner < 4; ++corner) {
            const auto* encoded = record + 8 + corner * 12;
            const auto& v = expected[corner];
            assert(std::abs(int(std::int16_t(pxa::wire::get16(encoded))) - v.x_q4) <= 1);
            assert(std::abs(int(std::int16_t(pxa::wire::get16(encoded+2))) - v.y_q4) <= 1);
            assert(std::int16_t(pxa::wire::get16(encoded+4)) == v.u_q4);
            assert(std::int16_t(pxa::wire::get16(encoded+6)) == v.v_q4);
            assert(std::abs(int(pxa::wire::get16(encoded+10)) - v.depth_q8) <= 1);
        }
    }
    // This chunk centre lies outside the old radial reach, while its nearest
    // corner intersects the 24-block far plane. It must remain visible.
    world.set(32,12,32,voxel::kAir);
    world.set(40,8,32,voxel::kWood);
    auto corner_projector=pxa::game3d::Projector::create(296,240,1.221451928f,0.25f,24);
    assert(corner_projector);
    const voxel::Camera corner_camera{18.5f,9.6f,18.5f,0.7f,-0.15f};
    assert(25.5f*25.5f+2.4f*2.4f+17.5f*17.5f>
           (24+voxel::kChunkSize*.87f)*(24+voxel::kChunkSize*.87f));
    budget.focal=corner_projector->focal_length();
    budget.max_distance=24;
    auto corner_frame=renderer.frame(buffer);
    corner_frame.clear({0x001f});
    const auto corner_stats=voxel::draw_world(corner_frame,*corner_projector,world,corner_camera,budget);
    assert(corner_stats.faces_drawn>0 && corner_frame.submit());
    assert(pxa_raster_validate_draw_list(commands.data(),command_size,&target,&resources,&effects_view)==PXA_STATUS_OK);
    pxa_raster_execute_draw_list(commands.data(),&effects_view,&target,&resources,nullptr);
    assert(std::any_of(depth.begin(),depth.end(),[](auto z){return z!=0;}));
    world.set(40,8,32,voxel::kAir);
    world.set(56,11,48,voxel::kWood);
    auto beyond=renderer.frame(buffer);
    beyond.clear({0x001f});
    assert(voxel::draw_world(beyond,*corner_projector,world,corner_camera,budget).faces_drawn==0);
    assert(beyond.submit() && command_size==40);
    std::printf("Voxel render: affine=%u solid=%u; same faces, frustum corners, zero frame allocations OK\n", affine, solid);
}
