#pragma once
#include "scene.hpp"

namespace sky {

enum class Paint { player, enemy, wingman, wreck, rescue, deck };
void aircraft_shadow(Scene& scene,const World& world,const Craft& craft);
void draw_aircraft(Scene& scene,const Craft& craft,Paint paint,float heat=0,float hatch=0,float time=0);

} // namespace sky
