#pragma once
#include "voxel_world.hpp"
namespace voxel {
enum ItemId : std::uint8_t {
    kStick=32,kWoodPickaxe,kWoodAxe,kWoodShovel,kStonePickaxe,kStoneAxe,kStoneShovel,kItemEnd
};
enum class Tool : std::uint8_t { hand,pickaxe,axe,shovel };
inline constexpr Tool tool_kind(std::uint8_t item)noexcept {
    if(item<kWoodPickaxe||item>=kItemEnd)return Tool::hand;
    return Tool(1+(item-kWoodPickaxe)%3);
}
inline constexpr unsigned tool_speed(std::uint8_t item)noexcept {
    return tool_kind(item)==Tool::hand?1:item>=kStonePickaxe?4:2;
}
inline constexpr std::uint8_t tool_life(std::uint8_t item)noexcept {
    return tool_kind(item)==Tool::hand?0:item>=kStonePickaxe?132:60;
}
inline constexpr unsigned stack_limit(std::uint8_t item)noexcept {
    return tool_kind(item)==Tool::hand?64:1;
}
inline constexpr bool valid_item(std::uint8_t item)noexcept {
    return (item>kAir&&item<kBedrock&&item!=kWater)||(item>=kStick&&item<kItemEnd);
}
inline constexpr const char* item_name(std::uint8_t item)noexcept {
    switch(item){
    case kStick:return "木棍";
    case kWoodPickaxe:return "木镐";case kWoodAxe:return "木斧";case kWoodShovel:return "木铲";
    case kStonePickaxe:return "石镐";case kStoneAxe:return "石斧";case kStoneShovel:return "石铲";
    default:return "";
    }
}
inline constexpr Tool preferred_tool(std::uint8_t block)noexcept {
    switch(block){
    case kStone:case kCobble:case kBrick:case kGlass:return Tool::pickaxe;
    case kWood:case kPlank:case kTable:case kCactus:return Tool::axe;
    case kGrass:case kDirt:case kSand:case kSnow:case kGravel:return Tool::shovel;
    default:return Tool::hand;
    }
}
} // namespace voxel
