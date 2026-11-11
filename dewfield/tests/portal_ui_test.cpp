// SPDX-License-Identifier: MIT
#define main previous_ui_tests
#include "ui_test.cpp"
#undef main
#include "../pacing.hpp"
static void place(Region region,int x,int y){
 game::init();fresh(state);state.character_chosen=true;state.intro=IntroScenes;state.story=5;state.minute=600;
 state.region=unsigned(region);state.visited|=1u<<state.region;state.px=x*16+8;state.py=y*16+8;
 std::memset(state.cleared,1,sizeof state.cleared);std::fill(std::begin(held_keys),std::end(held_keys),false);
 title_screen=false;page=Page::Farm;WIDTH=HEIGHT=240;state.show_joystick=state.show_action=false;
 cancel_tool();stop_motion();clock_acc=move_acc=0;toast_ms=0;following_npc=following_animal=journey_region=-1;
 scene_targets_dirty=true;base_dirty=true;game::draw();clock_resets=0;
}
static void finish(){for(int i=0;i<120&&(route||moving);++i)game::tick(33);assert(!route&&!moving);}
int main(){
 // A target beyond an exit is ordinary local travel, not permission to leave.
 place(Region::Farm,28,7);begin_route(30,7,false);finish();assert(state.region==unsigned(Region::Farm)&&state.px==30*16+8);
 place(Region::SeedShop,22,14);begin_route(24,14,false);finish();assert(state.region==unsigned(Region::SeedShop)&&state.px==24*16+8);
 // Explicit door destinations retain the approach/threshold sequence.
 place(Region::Farm,28,7);begin_route(29,7,true);
 for(int i=0;i<40&&state.region==unsigned(Region::Farm);++i)game::tick(33);
 assert(state.region==unsigned(Region::Town)&&state.px==2*16+8&&clock_resets>0&&arrival_frame_pending);
 // Picking and camera are valid before the first new-map draw, too. Stale
 // farm coordinates would turn this into a target hundreds of pixels away.
 toast_ms=0;int x=6*16+8-camx,y=8*16+8-camy;
 game::pointer(Phase::DOWN,x,y);assert(world_touch.active&&world_touch.tx==6&&world_touch.ty==8&&targetx==6&&targety==8);
 game::pointer(Phase::UP,x,y);int px=state.px,py=state.py;
 game::tick(33);assert(std::abs(int(state.px)-px)+std::abs(int(state.py)-py)<=4);
 int resets=clock_resets;game::draw();assert(!arrival_frame_pending&&clock_resets==resets+1);
 // The UP belonging to the old map is consumed; it cannot become a new tap,
 // click a control or start another route at the arrival point.
 place(Region::Farm,6,8);touch_down=true;world_touch.active=true;world_touch.region=state.region;
 action(6,7);assert(state.region==unsigned(Region::Home)&&!touch_down&&!world_touch.active);
 game::pointer(Phase::UP,180,145);assert(!route&&!moving&&page==Page::Farm);
 // A held controller direction still intentionally crosses an exit.
 place(Region::Farm,28,7);held_keys[1]=true;
 for(int i=0;i<40&&state.region==unsigned(Region::Farm);++i)game::tick(33);
 held_keys[1]=false;assert(state.region==unsigned(Region::Town));
 // A held finger reaches its dead zone before the step finishes. Intent must
 // follow the step's origin, rather than the last frame's direction value.
 place(Region::Farm,28,7);x=29*16+8-camx;y=7*16+8-camy;
 game::pointer(Phase::DOWN,x,y);
 for(int i=0;i<50&&state.region==unsigned(Region::Farm);++i){game::tick(33);game::draw();}
 assert(state.region==unsigned(Region::Town)&&state.px==40&&!touch_down);
 game::pointer(Phase::UP,x,y);assert(!route&&!moving);
 // Short/long presses and ordinary fingertip jitter all retain entrance intent.
 for(int kind=0;kind<3;++kind)for(int duration:{0,80,240,450})for(auto drift:{farm::Point{0,0},farm::Point{6,0},farm::Point{6,6}}){
  place(kind==2?Region::SeedShop:Region::Farm,kind==0?28:kind==1?6:22,kind==2?14:kind==1?8:7);
  int gx=kind==0?29:kind==1?6:23,gy=kind==2?14:7,destination=kind==0?1:kind==1?4:9;
  x=gx*16+8-camx;y=(kind==2?gy*16-8:gy*16+8)-camy;game::pointer(Phase::DOWN,x,y);assert(world_touch.portal&&route_portal>=0);
  game::pointer(Phase::MOVE,x+drift.x,y+drift.y);
  for(int t=0;t<duration&&state.region!=destination;t+=16)game::tick(16);
  game::pointer(Phase::UP,x+drift.x,y+drift.y);
  for(int i=0;i<80&&state.region!=destination;++i)game::tick(33);
  assert(state.region==destination);
 }
 // A long item-name chip must leave the visible door/mat touchable.
 for(bool food:{false,true}){
  place(Region::Home,16,19);
  if(food){state.produce[0]=1;sync_inventory(state);equip_item(state,12);game::draw();}
  if(food)assert(caption_over_entry());x=264-camx;y=322-camy;
  tap(x,y);assert(route_portal>=0);
  for(int i=0;i<80&&state.region==unsigned(Region::Home);++i)game::tick(33);
  assert(state.region==unsigned(Region::Farm)&&(!food||state.produce[0]==1));
 }
 // A deliberate drag still cancels the entrance.
 place(Region::Farm,28,7);x=29*16+8-camx;y=7*16+8-camy;
 game::pointer(Phase::DOWN,x,y);game::pointer(Phase::MOVE,x-48,y+30);assert(world_touch.following&&route_portal<0);
 game::pointer(Phase::UP,x-48,y+30);for(int i=0;i<30;++i)game::tick(33);assert(state.region==0);
 // Clicking the visible outward road lane selects its single physical exit.
 place(Region::Farm,28,7);x=30*16+8-camx;y=7*16+8-camy;tap(x,y);
 for(int i=0;i<80&&state.region==0;++i)game::tick(33);assert(state.region==1);
 // A 500-ms catch-up batch must stop on the map change. The next leg of an
 // explicit multi-map journey starts only on a later simulation interval.
 place(Region::Farm,28,7);go_to(Region::Town,15,12,false);resets=clock_resets;int ticks=0;
 dewfield_pacing::advance(500,[&](uint32_t ms){++ticks;game::tick(ms);},[&]{return clock_resets==resets;});
 assert(ticks<16&&state.region==unsigned(Region::Town)&&state.px==2*16+8&&state.py==7*16+8&&route);
 game::draw();assert(!arrival_frame_pending);
 puts("PASS: local routes ignore incidental exits; explicit/controller warps, immediate arrival taps, old release cancellation and interrupted catch-up");
}
