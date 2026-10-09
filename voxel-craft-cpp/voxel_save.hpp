#pragma once
#include "voxel_world.hpp"
#include "voxel_mesher.hpp"
#include <pxa/binary.hpp>
#include <cmath>

namespace voxel {
inline constexpr int kSaveSlots=3,kQuickCount=8;
inline constexpr std::size_t kSaveHeaderBytes=80;
using SaveHeader=std::array<std::byte,kSaveHeaderBytes>;
inline std::uint32_t checksum(std::span<const std::byte> data)noexcept {
    std::uint32_t h=2166136261u;for(auto b:data){h^=std::to_integer<unsigned>(b);h*=16777619u;}return h;
}
struct SaveState {
    Camera camera;float velocity=0;std::uint32_t seed=0x5ae1,generation=0,blocks_hash=0;
    std::array<std::uint8_t,kQuickCount> quick{1,2,3,4,5,6,8,12};
    std::uint8_t selected=0;bool flying=false;
};
inline SaveHeader encode_save(const SaveState& state) noexcept {
    SaveHeader header{};
    pxa::binary::Writer out(header);
    // Preserve the existing 80-byte VCX1 layout.
    (void)out.write(std::uint32_t{0x31584356u});
    (void)out.write(std::uint16_t{1});
    (void)out.write(std::uint16_t{kWorldX});
    (void)out.write(std::uint16_t{kWorldY});
    (void)out.write(std::uint16_t{kWorldZ});
    (void)out.write(std::uint32_t{kWorldX*kWorldY*kWorldZ});
    (void)out.write(state.generation);
    (void)out.write(state.seed);
    (void)out.write(state.blocks_hash);
    for (float value : {state.camera.x,state.camera.y,state.camera.z,
                        state.camera.yaw,state.camera.pitch,state.velocity})
        (void)out.write(value);
    for (int i=0;i<kQuickCount;++i) (void)out.write(state.quick[i]);
    (void)out.write(state.selected);
    (void)out.write(state.flying);
    (void)out.bytes(std::span{header}.subspan(62,14)); // Reserved zeros.
    (void)out.write(checksum(std::span{header}.first(76)));
    return header;
}
inline pxa::Result<SaveState> decode_save(std::span<const std::byte> header) noexcept {
    if (header.size()!=kSaveHeaderBytes ||
        *pxa::binary::read<std::uint32_t>(header,76)!=checksum(header.first(76)))
        return std::unexpected(pxa::Error::protocol_error);
    pxa::binary::Reader in(header);
    const auto magic=*in.read<std::uint32_t>();
    const auto version=*in.read<std::uint16_t>();
    const auto x=*in.read<std::uint16_t>(),y=*in.read<std::uint16_t>(),z=*in.read<std::uint16_t>();
    const auto blocks=*in.read<std::uint32_t>();
    if (magic!=0x31584356u || version!=1 ||
        x!=kWorldX || y!=kWorldY || z!=kWorldZ || blocks!=kWorldX*kWorldY*kWorldZ)
        return std::unexpected(pxa::Error::protocol_error);
    SaveState state;
    state.generation=*in.read<std::uint32_t>();
    state.seed=*in.read<std::uint32_t>();
    state.blocks_hash=*in.read<std::uint32_t>();
    float pose[6];
    for (auto& value : pose) {
        value=*in.read<float>();
        if (!std::isfinite(value)) return std::unexpected(pxa::Error::protocol_error);
    }
    // Flying/walking may leave the finite field. Bound collision/raycast inputs.
    if (std::fabs(pose[0])>1048576 || std::fabs(pose[1])>1048576 ||
        std::fabs(pose[2])>1048576 || std::fabs(pose[3])>3.142f ||
        std::fabs(pose[4])>1.35f || std::fabs(pose[5])>32 || !state.generation)
        return std::unexpected(pxa::Error::protocol_error);
    state.camera={pose[0],pose[1],pose[2],pose[3],pose[4]};
    state.velocity=pose[5];
    for (auto& block : state.quick) {
        block=*in.read<std::uint8_t>();
        if (!block || block>=kBedrock) return std::unexpected(pxa::Error::protocol_error);
    }
    state.selected=*in.read<std::uint8_t>();
    auto flying=in.read<bool>();
    if (!flying) return std::unexpected(flying.error());
    state.flying=*flying;
    if (state.selected>=kQuickCount) return std::unexpected(pxa::Error::protocol_error);
    return state;
}
inline bool valid_saved_blocks(std::span<const std::byte> blocks)noexcept {
    if(blocks.size()!=kWorldX*kWorldY*kWorldZ)return false;
    for(auto b:blocks)if(std::to_integer<unsigned>(b)>=kBlockCount)return false;return true;
}
} // namespace voxel
