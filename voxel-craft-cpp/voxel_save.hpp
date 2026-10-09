#pragma once
#include "voxel_world.hpp"
#include "voxel_mesher.hpp"
#include <pxa/core.hpp>
#include <bit>
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
inline SaveHeader encode_save(const SaveState& state)noexcept {
    SaveHeader h{};auto* p=h.data();pxa::wire::put32(p,0x31584356u); // VCX1
    pxa::wire::put16(p+4,1);pxa::wire::put16(p+6,kWorldX);pxa::wire::put16(p+8,kWorldY);pxa::wire::put16(p+10,kWorldZ);
    pxa::wire::put32(p+12,kWorldX*kWorldY*kWorldZ);pxa::wire::put32(p+16,state.generation);
    pxa::wire::put32(p+20,state.seed);pxa::wire::put32(p+24,state.blocks_hash);
    const float f[]={state.camera.x,state.camera.y,state.camera.z,state.camera.yaw,state.camera.pitch,state.velocity};
    for(int i=0;i<6;++i)pxa::wire::put32(p+28+4*i,std::bit_cast<std::uint32_t>(f[i]));
    for(int i=0;i<kQuickCount;++i)p[52+i]=std::byte(state.quick[i]);
    p[60]=std::byte(state.selected);p[61]=std::byte(state.flying);
    pxa::wire::put32(p+76,checksum(std::span{h}.first(76)));return h;
}
inline pxa::Result<SaveState> decode_save(std::span<const std::byte> h)noexcept {
    if(h.size()!=kSaveHeaderBytes)return std::unexpected(pxa::Error::protocol_error);auto* p=h.data();
    if(pxa::wire::get32(p)!=0x31584356u||pxa::wire::get16(p+4)!=1||pxa::wire::get16(p+6)!=kWorldX||pxa::wire::get16(p+8)!=kWorldY||pxa::wire::get16(p+10)!=kWorldZ||pxa::wire::get32(p+12)!=kWorldX*kWorldY*kWorldZ||pxa::wire::get32(p+76)!=checksum(h.first(76)))return std::unexpected(pxa::Error::protocol_error);
    SaveState s;s.generation=pxa::wire::get32(p+16);s.seed=pxa::wire::get32(p+20);s.blocks_hash=pxa::wire::get32(p+24);
    float f[6];for(int i=0;i<6;++i){f[i]=std::bit_cast<float>(pxa::wire::get32(p+28+4*i));if(!std::isfinite(f[i]))return std::unexpected(pxa::Error::protocol_error);}
    // Flying and walking can leave the finite block field. Preserve those
    // legitimate poses while bounding floor-to-int collision/raycast inputs.
    if(std::fabs(f[0])>1048576||std::fabs(f[1])>1048576||std::fabs(f[2])>1048576||std::fabs(f[3])>3.142f||std::fabs(f[4])>1.35f||std::fabs(f[5])>32||!s.generation)return std::unexpected(pxa::Error::protocol_error);
    s.camera={f[0],f[1],f[2],f[3],f[4]};s.velocity=f[5];
    for(int i=0;i<kQuickCount;++i){s.quick[i]=std::to_integer<unsigned>(p[52+i]);if(!s.quick[i]||s.quick[i]>=kBedrock)return std::unexpected(pxa::Error::protocol_error);}
    s.selected=std::to_integer<unsigned>(p[60]);s.flying=std::to_integer<unsigned>(p[61])!=0;
    if(s.selected>=kQuickCount||std::to_integer<unsigned>(p[61])>1)return std::unexpected(pxa::Error::protocol_error);return s;
}
inline bool valid_saved_blocks(std::span<const std::byte> blocks)noexcept {
    if(blocks.size()!=kWorldX*kWorldY*kWorldZ)return false;
    for(auto b:blocks)if(std::to_integer<unsigned>(b)>=kBlockCount)return false;return true;
}
} // namespace voxel
