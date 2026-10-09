#pragma once
#include "voxel_items.hpp"
namespace voxel {
// Bare-hand durations in seconds. Bedrock and liquid have no mining action.
inline constexpr float mining_seconds(std::uint8_t block)noexcept {
    switch(block){
    case kBush:case kFlower:return .20f;
    case kLeaves:return .45f;
    case kSnow:return .40f;
    case kSand:return .55f;
    case kDirt:case kGrass:return .65f;
    case kGravel:return .80f;
    case kGlass:return .60f;
    case kWool:return .90f;
    case kCactus:return 1.f;
    case kWood:case kPlank:case kTable:return 1.6f;
    case kCobble:case kBrick:return 2.4f;
    case kStone:return 3.2f;
    default:return 0;
    }
}
struct MiningProgress {
    float elapsed=0;
    std::uint32_t key=0;
    std::uint8_t stage=0;
    void reset()noexcept {elapsed=0;key=0;stage=0;}
    float progress()const noexcept {
        auto duration=duration_seconds(std::uint8_t((key>>17)&31),std::uint8_t(key>>22));
        return duration>0?std::min(1.f,elapsed/duration):0;
    }
    static constexpr float duration_seconds(std::uint8_t block,std::uint8_t item=0)noexcept {
        return mining_seconds(block)/(preferred_tool(block)!=Tool::hand&&preferred_tool(block)==tool_kind(item)?tool_speed(item):1);
    }
    static std::uint32_t target_key(const RayHit& hit,std::uint8_t block,std::uint8_t item=0)noexcept {
        if(tool_kind(item)==Tool::hand)item=0;
        return unsigned(hit.x)|(unsigned(hit.y)<<6)|(unsigned(hit.z)<<11)|(unsigned(block)<<17)|(unsigned(item)<<22);
    }
    bool advance(const RayHit& hit,std::uint8_t block,float dt,std::uint8_t item=0)noexcept {
        if(!hit.hit||mining_seconds(block)==0){reset();return false;}
        auto next=target_key(hit,block,item);
        if(next!=key){reset();key=next;}
        elapsed+=dt;return progress()>=1.f;
    }
};
} // namespace voxel
