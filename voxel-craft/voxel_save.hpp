#pragma once
#include "voxel_world.hpp"
#include "voxel_mesher.hpp"
#include "voxel_inventory.hpp"
#include <pxa/binary.hpp>
#include <cmath>

namespace voxel {
inline constexpr int kSaveSlots=3;
inline constexpr int kHeaderQuickCount=8; // Existing header layout; slot nine lives in the v2 payload.
inline constexpr std::size_t kSaveHeaderBytes=80;
using SaveHeader=std::array<std::byte,kSaveHeaderBytes>;
inline std::uint32_t checksum(std::span<const std::byte> data)noexcept {
    std::uint32_t h=2166136261u;for(auto b:data){h^=std::to_integer<unsigned>(b);h*=16777619u;}return h;
}
struct SaveState {
    Camera camera;float velocity=0;std::uint32_t seed=0x5ae1,generation=0,blocks_hash=0;
    std::array<std::uint8_t,kHeaderQuickCount> quick{1,2,3,4,5,6,8,12};
    std::uint8_t selected=0;bool flying=false;
    Inventory inventory;
    std::uint32_t inventory_hash=0;
    std::uint16_t inventory_bytes=sizeof(Inventory);
    bool legacy=false;
};
inline SaveHeader encode_save(const SaveState& state) noexcept {
    SaveHeader header{};
    pxa::binary::Writer out(header);
    // The v3 header stays 80 bytes; the payload adds one wear byte per slot.
    (void)out.write(std::uint32_t{0x31584356u});
    (void)out.write(std::uint16_t{3});
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
    for (int i=0;i<kHeaderQuickCount;++i) (void)out.write(state.inventory.slots[i].block);
    (void)out.write(state.selected);
    (void)out.write(state.flying);
    (void)out.write(checksum(std::as_bytes(std::span{state.inventory.slots})));
    (void)out.write(std::uint16_t{sizeof(Inventory)});
    (void)out.bytes(std::span{header}.subspan(68,8)); // Reserved zeros.
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
    if (magic!=0x31584356u || version<1 || version>3 ||
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
    state.legacy=version==1;
    for (auto& block : state.quick) block=*in.read<std::uint8_t>();
    state.selected=*in.read<std::uint8_t>();
    auto flying=in.read<bool>();
    if (!flying) return std::unexpected(flying.error());
    state.flying=*flying;
    state.inventory_hash=*in.read<std::uint32_t>();
    state.inventory_bytes=version==2?kInventorySlots*2:sizeof(Inventory);
    if (const auto bytes=*in.read<std::uint16_t>(); !state.legacy&&bytes!=state.inventory_bytes)
        return std::unexpected(pxa::Error::protocol_error);
    for (int i=0;i<kHeaderQuickCount;++i) {
        const auto block=state.quick[i];
        if (version<3?((state.legacy&&!block) || block>=kBedrock || (!state.legacy&&block==kWater)):
            (block&&!valid_item(block)))
            return std::unexpected(pxa::Error::protocol_error);
        if (state.legacy&&block!=kWater) state.inventory.slots[i]={block,1};
    }
    if (state.selected>=(state.legacy?kHeaderQuickCount:kQuickCount))
        return std::unexpected(pxa::Error::protocol_error);
    return state;
}
inline bool decode_inventory(SaveState& state,std::span<const std::byte> data)noexcept {
    if(state.legacy)return data.empty();
    if(data.size()!=state.inventory_bytes||checksum(data)!=state.inventory_hash)return false;
    Inventory inventory;
    const unsigned stride=state.inventory_bytes/kInventorySlots;
    for(int i=0;i<kInventorySlots;++i){auto& s=inventory.slots[i];s={std::to_integer<std::uint8_t>(data[i*stride]),std::to_integer<std::uint8_t>(data[i*stride+1]),stride==3?std::to_integer<std::uint8_t>(data[i*stride+2]):std::uint8_t(0)};if(!Inventory::valid(s)||(i<kHeaderQuickCount&&s.block!=state.quick[i]))return false;}
    state.inventory=inventory;return true;
}
inline bool valid_saved_blocks(std::span<const std::byte> blocks)noexcept {
    if(blocks.size()!=kWorldX*kWorldY*kWorldZ)return false;
    for(auto b:blocks)if(std::to_integer<unsigned>(b)>=kBlockCount)return false;return true;
}
} // namespace voxel
