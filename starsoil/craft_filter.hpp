// SPDX-License-Identifier: MIT
#pragma once
#include "items.hpp"
namespace starsoil {
enum class CraftGroup : uint8_t { All, Building, Tools, Gear, Supplies, Count };
constexpr CraftGroup craft_group(int item) {
  const auto k=items[item].kind;
  if(k==Block || k==Furniture || k==Wall)return CraftGroup::Building;
  if(k==Pick || k==Axe || k==Hammer || k==Sword || k==Bow || k==Wand)return CraftGroup::Tools;
  if(k>=Head && k<=Charm)return CraftGroup::Gear;
  return CraftGroup::Supplies;
}
constexpr bool craft_matches(int recipe,CraftGroup group) {
  return recipe>=0 && recipe<RecipeCount &&
         (group==CraftGroup::All || craft_group(recipes[recipe].out)==group);
}
} // namespace starsoil
