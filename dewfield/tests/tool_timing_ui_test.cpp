// SPDX-License-Identifier: MIT
#define main existing_ui_suite
#include "ui_test.cpp"
#undef main
static void tree(int d,int grade=0) {
  game::init();title_screen=false;page=Page::Farm;cancel_tool();stop_motion();
  std::memset(state.cleared,1,sizeof(state.cleared));state.cleared[7]=0;state.tree_age[7]=5;
  int dx=(d==1)-(d==3),dy=(d==2)-(d==0);
  state.px=(19-dx)*16+8;state.py=(14-dy)*16+8;dir=d;clock_acc=0;
  std::fill(std::begin(held_keys),std::end(held_keys),false);
  farm::select_slot(state,4);state.tool_grade[4]=grade;
  assert(farm::walkable(state,state.px/16,state.py/16));
}
int main() {
 WIDTH=HEIGHT=240;
 for(int d=0;d<4;++d)for(int grade=0;grade<3;++grade) {
  tree(d,grade);state.px+=d&1?3:0;int px=state.px,py=state.py;
  moving=walking=true;action(19,14);
  assert(pending_tool.active&&swing&&!moving&&!walking&&state.energy==100&&!state.damage[7]);
  int before=art::tool_anim::duration(4,grade)-art::tool_anim::impact_remaining(4,grade)-1;
  game::tick(before);assert(!state.damage[7]&&state.energy==100);
  game::key(pocket::Key::LEFT);assert(dir==d&&action_direction==d);
  game::key(pocket::Key::R);assert(state.tool==4);
  auto slot=layout().tool(0);tap(slot.center_x(),slot.center_y());assert(state.tool==4&&!route);
  game::tick(1);assert(!pending_tool.active&&state.damage[7]==1+grade&&hit_ms);
  assert(state.energy==unsigned(grade?98:97)&&state.px==px&&state.py==py);
  game::tick(art::tool_anim::duration(4,grade));assert(state.damage[7]==1+grade&&!swing);
 }
 tree(1);action(19,14);game::tick(80);open_page(Page::Bag);
 assert(!swing&&!pending_tool.active);game::tick(1000);assert(state.energy==100&&!state.damage[7]);
 // A long press produces one hit per completed swing, with no extra hit on release.
 tree(1);auto use=layout().action();game::pointer(pocket::Phase::DOWN,use.center_x(),use.center_y());
 game::tick(art::tool_anim::duration(4)-art::tool_anim::impact_remaining(4));assert(state.damage[7]==1);
 game::pointer(pocket::Phase::UP,use.center_x(),use.center_y());game::tick(800);
 assert(state.damage[7]==1&&state.energy==97&&!pending_tool.active);
 // Finishing a tree and dropping its resources also occur on the actual contact.
 tree(1,2);state.damage[7]=3;action(19,14);assert(!state.cleared[7]&&!state.wood);
 game::tick(art::tool_anim::duration(4,2)-art::tool_anim::impact_remaining(4,2));
 assert(state.cleared[7]&&state.wood==4);
 // Water and hoe share the same contact clock, rather than changing soil on button-down.
 for(int tool:{0,2}) {
  tree(0);state.cleared[7]=1;state.px=13*16+8;state.py=15*16+8;dir=0;
  auto &plot=state.plots[farm::plot_index(13,14)];plot={uint8_t(tool==2),0,0,0};farm::select_slot(state,tool);
  action(13,14);assert(pending_tool.active&&!plot.wet&&state.water==30);
  if(tool==0)assert(!plot.tilled);
  game::tick(art::tool_anim::duration(tool)-art::tool_anim::impact_remaining(tool));
  assert(plot.tilled);if(tool==2)assert(plot.wet&&state.water==29);
 }
 state.story=5;state.rain=false;
 for(int minute=1020;minute<1200;++minute)for(int fraction:{0,250,499}) {
  state.minute=minute;
  for(int a=0;a<4;++a)for(int b=a+1;b<4;++b) {
   auto first=farm::npc_pose(state,a,fraction),second=farm::npc_pose(state,b,fraction);
   assert(first.region==farm::Region::Town&&second.region==farm::Region::Town);
   assert(std::abs(first.x-second.x)>=20);
  }
 }
 puts("PASS: delayed four-direction strikes, exact contact resources, heading/equipment locks, cancellation/release safety, hoe/water timing and separated village gathering");
}
