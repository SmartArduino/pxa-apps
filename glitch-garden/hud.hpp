// SPDX-License-Identifier: MIT
#pragma once
#include "scene.hpp"
namespace garden {
inline void draw_hud_base(Paint& p,const Game& game,const Layout& layout){using namespace garden_art;
 int rw=layout.w<260?40:48;
 p.image(ui_tray,layout.tray());p.image(ui_resource,{layout.x,layout.y+1,rw,48});
 p.image(ui_pause,{layout.x+layout.w-29,layout.y+13,28,37});
 p.image(ui_wave,layout.wave(),false);
 p.image(glyph_ordinal,{layout.x+49,layout.y+layout.h-24,13,17});p.number({layout.x+64,layout.y+layout.h-24,14,17},game.level+1);p.image(glyph_stage,{layout.x+80,layout.y+layout.h-24,13,17});
}
inline void draw_hud_cards(Paint& p,const Game& game,const Layout& layout,int selection){using namespace garden_art;
 for(int i=0;i<plant_count;++i){auto r=layout.card(i);if(r.w<=0)continue;bool selected=selection==i+1,affordable=game.stars>=cost[i+1];
  p.image(selected?ui_seed_selected:affordable?ui_seed:ui_seed_disabled,r,false);
  p.actor(plant_clip(Defender(i+1)),0,r.x+r.w/2,r.y+34,23);p.number({r.x+3,r.y+35,r.w-6,11},cost[i+1]);
 }
 if(layout.scroll_seeds()){p.arrow(layout.seed_arrow(false),false,layout.seed_offset>0);p.arrow(layout.seed_arrow(true),true,layout.seed_offset+layout.seed_visible()<plant_count);}
 auto shovel=layout.card(plant_count);if(!selection)p.image(ui_seed_selected,shovel,false);p.image(ui_trowel,shovel);
}
inline void draw_hud_dynamic(Paint& p,const Game& game,const Layout& layout){int rw=layout.w<260?40:48;p.number({layout.x+6,layout.y+25,rw-12,17},game.stars);auto wave=layout.wave();p.image(garden_art::ui_dew,{wave.x+10+int(std::min(1.f,game.elapsed/game.duration())*55),wave.y+13,9,11});}
}
