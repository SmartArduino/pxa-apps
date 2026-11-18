// SPDX-License-Identifier: MIT
#pragma once
#include "paint.hpp"
#include "layout.hpp"
#include "frame_restore.hpp"
namespace garden {
// Compute once per layout. All poses (including jumps/defeats), shadows,
// shots, the grid cursor and transient notices stay inside this native band.
inline Rect board_damage(const Layout& layout,float zoom,int width,int height){
 int unit=layout.cell;auto pixel=[&](int n){return int(std::lround(n*zoom));};DamageBounds damage;
 for(int row:{0,rows-1}){int ground=layout.by+(row+1)*unit-1;
  for(unsigned i=0;i<garden_art::frost_shot;++i){auto r=actor_bounds(garden_art::frames[i],0,ground,unit,zoom);r.x=0;r.w=width;damage.include(r);}
  damage.include({0,pixel(ground-3),width,pixel(6)});
  damage.include({0,pixel(layout.by+row*unit+unit/2-4),width,pixel(10)+1});
 }
 damage.include({0,pixel(layout.by),width,pixel(layout.by+rows*unit)-pixel(layout.by)});
 damage.include({0,pixel(layout.by+unit*2-14),width,pixel(layout.by+unit*2+16)-pixel(layout.by+unit*2-14)});
 int top=std::clamp(damage.rect.y,0,height),bottom=std::clamp(damage.rect.y+damage.rect.h,0,height);return {0,top,width,bottom-top};
}
inline garden_art::Clip plant_clip(Defender type,unsigned state=0){using namespace garden_art;
 switch(type){
 case Defender::trophy:return state==3?dewflower_defeat:state==2?dewflower_hurt:state==1?dewflower_action:dewflower_idle;
 case Defender::cactus:return state==3?coralflower_defeat:state==2?coralflower_hurt:state==1?coralflower_action:coralflower_idle;
 case Defender::frost:return state==3?frostbell_defeat:state==2?frostbell_hurt:state==1?frostbell_action:frostbell_idle;
 case Defender::ember:return state==3?emberpod_defeat:state==2?emberpod_hurt:state==1?emberpod_action:emberpod_idle;
 case Defender::clover:return state==3?cloverling_defeat:state==2?cloverling_hurt:state==1?cloverling_action:cloverling_idle;
 case Defender::stone:return state==3?leafshield_defeat:state==2?leafshield_hurt:state==1?leafshield_action:leafshield_idle;
 default:return state==3?pinwheel_defeat:state==2?pinwheel_hurt:state==1?pinwheel_action:pinwheel_idle;
 }
}
inline garden_art::Clip enemy_clip(EnemyKind kind,unsigned state=0){using namespace garden_art;
 switch(kind){case EnemyKind::snail:return state==3?armorsnail_defeat:state==2?armorsnail_hurt:state==1?armorsnail_action:armorsnail_idle;
 case EnemyKind::moth:return state==3?swiftmoth_defeat:state==2?swiftmoth_hurt:state==1?swiftmoth_action:swiftmoth_idle;
 case EnemyKind::beetle:return state==3?copperbeetle_defeat:state==2?copperbeetle_hurt:state==1?copperbeetle_action:copperbeetle_idle;
 default:return state==3?dustimp_defeat:state==2?dustimp_hurt:state==1?dustimp_action:dustimp_idle;}}
inline void draw_board(Paint& p,const Game& game,const Layout& layout,bool using_keys=false,int row=0,int col=0,bool plant_shadows=true){
 int unit=layout.cell;
 for(int r=0;r<rows;++r){
  for(int c=0;c<columns;++c){const auto& plant=game.plants[r*columns+c];if(plant.kind==Defender::none)continue;
   bool producer=plant.kind==Defender::trophy,healer=plant.kind==Defender::clover;
   bool action=producer?plant.cycle>7.4f:healer?plant.cycle>3.4f:plant.attacking&&(plant.kind==Defender::stone||plant.cycle<.6f);
   unsigned state=plant.flash>0&&!producer?2:action?1:0;
   float t=state==2?(.15f-plant.flash)*4:state==1?(producer?(plant.cycle-7.4f)/.6f:healer?(plant.cycle-3.4f)/.6f:plant.cycle/.6f):plant.animation;
   auto tile=layout.tile(r,c);int center=tile.x+tile.w/2,ground=tile.y+tile.h-1;if(plant_shadows)p.shadow(center,ground,unit);p.actor(plant_clip(plant.kind,state),t,center,ground,unit);
  }
  for(const auto& e:game.enemies)if(e.active&&e.row==r){using namespace garden_art;
   auto clip=e.jump>0?copperbeetle_jump:enemy_clip(e.kind,e.flash>0?2:e.attacking?1:0);
   float t=e.jump>0?std::min(.599f,.666667f-e.jump):e.flash>0?(.12f-e.flash)*5:e.attacking?e.cycle*1.666667f:e.animation*1.25f;
   float center=layout.bx+e.x*unit;int ground=layout.by+(r+1)*unit-1;p.shadow(center,ground,unit,true);p.actor(clip,t,center,ground,unit,true);
  }
  for(const auto& e:game.effects)if(e.active&&e.row==r){using namespace garden_art;auto clip=e.enemy?enemy_clip(e.enemy_kind,3):plant_clip(e.kind,3);
   p.actor(clip,std::min(.599f,e.time),layout.bx+e.x*unit,layout.by+(r+1)*unit-1,unit);
  }
  for(const auto& s:game.shots)if(s.active&&s.row==r)p.moving_image(s.kind==ShotKind::frost?garden_art::frost_shot:s.kind==ShotKind::ember?garden_art::ember_shot:s.axe?garden_art::ui_leaf_shot:garden_art::ui_seed_shot,layout.bx+s.x*unit-6,layout.by+r*unit+unit/2-4,13,10);
 }
 if(using_keys){constexpr auto gold=rgb(239,196,90);int x=layout.bx+col*unit,y=layout.by+row*unit;p.rect(x,y,unit,1,gold);p.rect(x,y+unit-1,unit,1,gold);p.rect(x,y,1,unit,gold);p.rect(x+unit-1,y,1,unit,gold);}
}
}
