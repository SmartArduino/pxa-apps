// SPDX-License-Identifier: MIT
#define main old_ui_main
#include "ui_test.cpp"
#undef main
#include "../wilderness.hpp"
#include <string>
static void capture(const std::string&path){FILE*f=std::fopen(path.c_str(),"wb");assert(f);std::fprintf(f,"P6\n%d %d\n255\n",WIDTH,HEIGHT);for(int i=0;i<WIDTH*HEIGHT;++i){auto c=framebuffer()[i];unsigned char b[]={uint8_t((c>>11)*255/31),uint8_t(((c>>5)&63)*255/63),uint8_t((c&31)*255/31)};std::fwrite(b,1,3,f);}std::fclose(f);}
int main(int argc,char **argv) {
 WIDTH=HEIGHT=240;game::init();title_screen=false;page=Page::Farm;
 for(int d=0;d<4;++d) {
  int dx=(d==1)-(d==3),dy=(d==2)-(d==0),remaining=art::tool_anim::impact_remaining(7);
  auto motion=art::tool_anim::motion(d,7,remaining,0,dx*16,dy*16-10,true);
  const auto &blade=art::tool_anim::held_frame(7,0,motion.angle,motion.mirrored);
  assert(motion.x+blade.tip_x==dx*16&&motion.y+blade.tip_y==dy*16-10);
  assert(blade.face_x*dx+blade.face_y*dy==16);
 }
 for(int angle=0;angle<16;++angle) {
  const auto&a=art::tool_anim::frame(7,0,angle),&b=art::tool_anim::frame(7,0,(-angle)&15,true);
  std::vector<uint16_t> left(44*44),right(44*44);art::Raster lr{left.data(),44,44},rr{right.data(),44,44};art::grounded(lr,a,22,22);art::grounded(rr,b,22,22);
  for(int y=0;y<44;++y)for(int x=1;x<44;++x)assert(left[y*44+x]==right[y*44+44-x]);
 }
 // A real field action defers a forest tree's damage and cost until impact.
 state.region=unsigned(Region::Forest);state.visited|=1u<<state.region;auto tree=WildTrees[1];state.px=(tree.x-1)*16+8;state.py=tree.y*16+8;dir=1;equip_item(state,5);stop_motion();
 auto energy=state.energy;facing_action();assert(pending_tool.active&&state.wild_damage[1]==0&&state.energy==energy);
 for(int i=0;i<20&&!state.wild_damage[1];++i)game::tick(33);
 assert(state.wild_damage[1]==1&&state.energy==energy-3);cancel_tool();
 state.region=unsigned(Region::Forest);state.px=22*16+8;state.py=4*16+8;dir=0;page=Page::Farm;facing_action();
 assert(in_mine(state)&&state.sword_owned&&page==Page::Letter&&held_item()==37&&mine_letter&&dir==2);
 game::draw();if(argc>1)capture(std::string(argv[1])+"/mine-letter.ppm");
 auto mob=state.mobs[1];game::tick(1000);assert(state.mobs[1].x==mob.x&&state.mobs[1].y==mob.y);
 game::key(Key::B);game::key(Key::SELECT);assert(held_item()==6);game::key(Key::SELECT);assert(held_item()==37);
 // The hit, enemy death and loot all occur at the blade contact phase.
 state.px=16*16+8;state.py=6*16+8;dir=2;state.sword_grade=2;state.mine_defeated[0]=61;
 for(auto &m:state.mobs)m.hp=0;state.mobs[1]={state.px,uint16_t(7*16+8),500,0,0,10,0};state.invulnerable=0;state.attack_cooldown=0;
 facing_action();assert(pending_tool.active&&state.mobs[1].hp==10);
 for(int i=0;i<20&&state.mobs[1].hp;++i)game::tick(16);
 assert(!state.mobs[1].hp&&state.monsters_slain==1&&item_count(state,45)==1);
 cancel_tool();state.sword_grade=0;state.gold=1000;change_item(state,39,5);change_item(state,38,2);sync_inventory(state);
 open_page(Page::MineJournal);mine_ui_action(2);assert(state.sword_grade==1&&state.gold==920);
 // Descending resets cached geometry, and an unvisited lift cannot teleport.
 state.mine_cleared[0]|=7|MineLadder;open_page(Page::MineStairs);mine_ui_action(0);assert(state.mine_depth==2&&page==Page::Farm&&base_dirty);
 open_page(Page::MineLift);mine_ui_action(3);assert(state.mine_depth==2&&page==Page::MineLift);mine_ui_action(0);assert(state.region==unsigned(Region::Forest)&&page==Page::Farm);
 // Touching a walkable cave crop performs pickup instead of only routing.
 enter_mine(state,1);page=Page::Farm;state.px=5*16+8;state.py=7*16+8;equip_item(state,4);stop_motion();base_dirty=true;game::draw();
 tap(5*16+8-camx,8*16+11-camy);assert(route&&route_action);
 for(int i=0;i<30&&!state.cave_gathered;++i)game::tick(33);
 assert(state.cave_gathered==1&&item_count(state,43)==1);
 // A controller can use a revealed ladder while standing on its walkable tile.
 cancel_tool();stop_motion();state.mine_cleared[0]|=MineLadder;state.px=25*16+8;state.py=17*16+8;page=Page::Farm;dir=0;equip_item(state,37);facing_action();
 assert(page==Page::MineStairs);game::key(Key::B);
 // An enemy on the ladder keeps the blade action instead of opening a modal.
 state.px=25*16+8;state.py=18*16+8;state.mine_defeated[0]=61;for(auto &m:state.mobs)m.hp=0;state.mobs[1]={state.px,uint16_t(17*16+8),1000,0,0,10,2};state.sword_grade=2;state.attack_cooldown=0;
 assert(mine_act(state,25,17)==Result::Attacked&&!state.mobs[1].hp);
 // World clicks and HUD controls use the shared safe coordinates.
 enter_mine(state,1);state.invulnerable=0;page=Page::Farm;base_dirty=true;game::draw();
 tap(mine_quick(0).center_x(),mine_quick(0).center_y());assert(held_item()==6);
 tap(mine_quick(1).center_x(),mine_quick(1).center_y());assert(held_item()==37);
 tap(mine_quick(2).center_x(),mine_quick(2).center_y());assert(page==Page::MineJournal);
 // A sword strike stays an attack if its target leaves a forage tile before impact.
 fresh(state);enter_mine(state);page=Page::Farm;stop_motion();state.px=5*16+8;state.py=7*16+8;dir=2;state.sword_grade=2;
 for(auto &m:state.mobs)m.hp=0;state.mine_defeated[0]=62;state.mobs[0]={uint16_t(5*16+8),uint16_t(8*16+8),1000,0,0,10,0};
 action(5,8);assert(pending_tool.active&&state.cave_gathered==0);state.mobs[0].x+=40;
 for(int i=0;i<30&&swing;++i)game::tick(16);
 assert(state.cave_gathered==0&&cave_forage_at(state,5,8)==0&&state.mobs[0].hp==10);
 // Quick supplies heal from the bag while preserving the equipped sword.
 cancel_tool();stop_motion();state.energy=50;state.health=60;change_item(state,43,2);sync_inventory(state);equip_item(state,37);base_dirty=true;game::draw();
 tap(mine_quick(4).center_x(),mine_quick(4).center_y());assert(state.energy==70&&state.health==70&&item_count(state,43)==1&&held_item()==37);
 state.energy=state.health=100;tap(mine_quick(4).center_x(),mine_quick(4).center_y());assert(item_count(state,43)==1);
 state.energy=50;state.health=60;change_item(state,12,1);sync_inventory(state);equip_item(state,12);
 tap(mine_quick(4).center_x(),mine_quick(4).center_y());assert(!item_count(state,12)&&item_count(state,43)==1&&state.energy==60&&state.health==65);
 // Switching quick tools cancels an uncommitted hit; no hidden attack follows.
 equip_item(state,37);state.px=16*16+8;state.py=6*16+8;state.attack_cooldown=0;dir=2;facing_action();assert(pending_tool.active);
 tap(mine_quick(0).center_x(),mine_quick(0).center_y());assert(!pending_tool.active&&!swing&&held_item()==6);
 // The ladder shortcut refuses locked stairs, then routes to a real confirmation.
 state.mine_cleared[0]=0;state.mine_defeated[0]=0;enter_mine(state);page=Page::Farm;tap(mine_quick(3).center_x(),mine_quick(3).center_y());assert(!route&&page==Page::Farm);
 state.mine_cleared[0]|=7|MineLadder;state.px=25*16+8;state.py=18*16+8;stop_motion();tap(mine_quick(3).center_x(),mine_quick(3).center_y());
 for(int i=0;i<30&&page==Page::Farm;++i)game::tick(16);assert(page==Page::MineStairs);
 for(auto dims:{std::pair<int,int>{240,240},{400,240},{240,427}})for(bool en:{false,true}) {
  WIDTH=dims.first;HEIGHT=dims.second;set_english(en);
  for(int floor:{1,11,21,30}) {
   load_mine_floor(state,floor);base_dirty=true;
   for(Page p:{Page::Farm,Page::MineJournal,Page::MineLift,Page::MineStairs,Page::MineChest,Page::World}) {
    page=p;std::fill(pixels,pixels+WIDTH*HEIGHT+32,0x1ace);game::draw();
    for(int i=0;i<16;++i)assert(pixels[i]==0x1ace&&pixels[16+WIDTH*HEIGHT+i]==0x1ace);
    if(argc>1&&WIDTH==240&&HEIGHT==240&&!en&&(p==Page::Farm||floor==1))capture(std::string(argv[1])+"/mine-"+std::to_string(floor)+"-"+std::to_string(int(p))+".ppm");
   }
  }
 }
 puts("PASS: four-heading sword contact and exact mirrors; forest contact timing; mine tutorial/quick controls; delayed combat loot; forging, stairs/lift gates; three biome and bilingual page bounds");
}
