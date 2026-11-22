#pragma once
#include "scene.hpp"

namespace sky {

void render_flight_effects(Scene& scene,const Game& game,bool show_trails);
void render_speed_streaks(Canvas& canvas,const Game& game,View view);

} // namespace sky
