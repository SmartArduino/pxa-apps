// SPDX-License-Identifier: MIT
farm_ui::Rect mine_quick(int i){return {6+i*29,64,26,24};}
void mine_quick_action(int choice) {
 cancel_tool();stop_motion();
 if(choice==2){open_page(Page::MineJournal);return;}
 if(choice<2){equip_item(state,choice?37:6);changed();return;}
 if(choice==3) {
  if(state.mine_depth==30?bool(state.mobs[5].hp):!ladder_ready(state)) {
   msg(state.mine_depth==30?"先击败守灯石偶":"碎3处矿石或击败2只怪物，寻找梯子",state.mine_depth==30?"Defeat the lamp guardian first":"Break 3 ore nodes or defeat 2 foes to find stairs");return;
  }
  begin_route(25,17,true);return;
 }
 int food=mine_food_item(state),energy=state.energy,health=state.health;
 if(!food){msg("背包中没有可以食用的补给","No edible supplies in your bag");return;}
 if(!dewfield_actions::eat(state,food)){msg("体力与生命已满，先留着食物","Energy and health are full. Save your food");return;}
 char name[64],cn[96],en[96];item_label(name,sizeof(name),food,false);
 std::snprintf(cn,sizeof(cn),"%s · 体力+%d 生命+%d",name,int(state.energy)-energy,int(state.health)-health);
 std::snprintf(en,sizeof(en),"%s: energy +%d, health +%d",name,int(state.energy)-energy,int(state.health)-health);
 msg(cn,en);changed();
}
void refresh_mine_scene() {
 cancel_tool();stop_motion();dir=2;base_dirty=true;cached_geometry=-1;clock_acc=0;page=Page::Farm;prepare_arrival();changed();
}
void mine_ui_action(int choice) {
 if(page==Page::MineLift) {
  constexpr int floors[]={0,1,5,10,15,20,25,30};int floor=floors[std::clamp(choice,0,7)];
  if(!floor){dewfield_actions::leave_mine(state);refresh_mine_scene();return;}
  if(dewfield_actions::enter_mine(state,floor)){refresh_mine_scene();return;}
  msg("先亲自到达这一层，才能使用升降梯","Visit this floor first to unlock its lift");return;
 }
 if(page==Page::MineStairs) {
  if(choice==1){page=Page::Farm;return;}
  if(dewfield_actions::descend_mine(state)){refresh_mine_scene();msg("继续深入，留意攻击前的红光","Deeper still. Watch the red attack warning");}
  return;
 }
 if(page==Page::MineChest) {
  if(choice==1){page=Page::Farm;return;}
  if(dewfield_actions::mine_treasure(state)) {
   std::snprintf(letter,sizeof(letter),"%s",tr("You found the old lamp's crystal heart! 800G, 3 crystals and 3 glowroots. Alan's lost light can shine again.","你找到了旧矿灯的晶心！获得800金币、3块晶石和3份荧根菜。阿岚说，失去的光也能被重新点亮。"));open_page(Page::Letter);changed();
  } else msg(state.mine_rewards&1?"宝箱已经领过了":"先击败守灯石偶，并留出物品堆叠空间",state.mine_rewards&1?"This chest has been claimed":"Defeat the lamp guardian and leave stack space");
  return;
 }
 if(choice<2){equip_item(state,choice?37:6);changed();page=Page::Farm;return;}
 if(dewfield_actions::forge_sword(state)){msg("剑已强化，下一次命中会更有力","Sword strengthened. The next strike is stronger");changed();}
 else msg("需铜矿5或铁矿8、煤炭2，以及80或180金币","Need 5 copper or 8 iron, 2 coal, and 80 or 180G");
}
void mine_ui_draw(int x,int y) {
 char b[128];
 if(page==Page::MineLift) {
  constexpr int floors[]={0,1,5,10,15,20,25,30};
  for(int i=0;i<8;++i) {
   auto r=layout().destination(i);int f=floors[i];bool ready=f<=state.mine_best||f==1;
   if(!f)std::snprintf(b,sizeof(b),"%s",tr("SURFACE","返回林间"));else std::snprintf(b,sizeof(b),english()?"Floor %d%s":"第%d层%s",f,ready?"":tr(" locked"," 未到达"));
   button(r.x,r.y,r.w,r.h,b,selected_row==i);
  }
  paragraph(x+12,y+163,tr("Every fifth floor is a permanent checkpoint. Rocks and monsters return after a new day.","每五层设一个永久检查点。睡醒后矿石、怪物和洞穴作物刷新，背包与检查点会保留。"),202,3,Muted);
 } else if(page==Page::MineStairs) {
  std::snprintf(b,sizeof(b),english()?"Continue to floor %d?":"继续前往第%d层？",state.mine_depth+1);center_text(x+114,y+52,b,Sage);
  IconCanvas c;art::mine_ladder(c,x+114,y+113,true);
  paragraph(x+12,y+125,tr("Bring food. Three broken ore nodes or two defeated monsters reveal the next ladder.","带好食物。打碎三处矿石或击败两只怪物，就能找到下一层的梯子。"),202,3,Muted);
  button(x+12,y+174,98,26,tr("DESCEND","继续下潜"),selected_row==0);button(x+117,y+174,99,26,tr("STAY","留在本层"),selected_row==1);
 } else if(page==Page::MineChest) {
  IconCanvas c;art::mine_monster(c,x+114,y+86,3,0,2,false);
  paragraph(x+12,y+99,tr("The old lamp's crystal heart rests here. Defeat its stone guardian to recover the light.","矿灯的晶心沉睡在这里。击败守灯石偶，带着新的光回到晴露谷。奖励只能领取一次。"),202,4,Text);
  button(x+12,y+174,98,26,tr("CLAIM","打开宝箱"),selected_row==0);button(x+117,y+174,99,26,tr("BACK","暂时离开"),selected_row==1);
 } else {
  std::snprintf(b,sizeof(b),english()?"Depth %u / best %u | HP %u":"深度%u / 最深%u · 生命%u",state.mine_depth,state.mine_best,state.health);fit_text(x+12,y+38,b,202,Sage);
  if(state.mine_depth==30)std::snprintf(b,sizeof(b),"%s",state.mobs[5].hp?tr("Guardian: still guarding the chest","守灯石偶仍守护着宝箱"):tr("Guardian defeated / chest ahead","守灯石偶已击败，前往宝箱"));
  else if(ladder_ready(state))std::snprintf(b,sizeof(b),"%s",tr("Stairs found / ladder icon to walk","已找到楼梯，点梯子图标前往"));
  else std::snprintf(b,sizeof(b),english()?"Stairs: ore %u/3 or foes %u/2":"开梯：碎矿%u/3 或击败%u/2",unsigned(__builtin_popcount(state.mine_cleared[state.mine_depth-1]&0xffffffu)),unsigned(__builtin_popcount(state.mine_defeated[state.mine_depth-1])));
  fit_text(x+12,y+54,b,202);
  paragraph(x+12,y+72,state.mine_rewards?tr("The lamp is shining again.\nFresh supplies return each day.\nSELECT swaps pick and sword.","阿岚的矿灯已重新点亮。\n矿层每日更新，可继续采集。\nSELECT切换镐和剑。"):tr("Find the lost lamp heart on 30F.\nSELECT swaps your sword and pick.\nFood heals both energy and HP.","阿岚的矿灯丢了晶心。\n探至三十层，找回那束光。\nSELECT换镐与剑；食物可回血。"),202,3,Muted);
  auto grade=state.sword_grade;std::snprintf(b,sizeof(b),english()?"Blade %u / damage %u":"剑等级%u · 伤害%u",grade,8+grade*5);fit_text(x+12,y+118,b,202,Sage);
  const char *labels[]={tr("EQUIP PICK","拿起镐子"),tr("EQUIP SWORD","拿起短剑"),grade>=2?tr("SWORD MAXED","短剑已强化至最高"):grade?tr("8 IRON + 2 COAL / 180G","8铁矿+2煤炭 / 180G"):tr("5 COPPER + 2 COAL / 80G","5铜矿+2煤炭 / 80G")};
  for(int i=0;i<3;++i)button(x+12,y+137+i*22,202,21,labels[i],selected_row==i);
 }
}
void mine_ui_key(Key k) {
 int count=page==Page::MineLift?8:page==Page::MineJournal?3:2;
 if(k==Key::B){page=Page::Farm;return;}
 if(k==Key::UP)selected_row=(selected_row+count-(page==Page::MineLift?2:1))%count;
 if(k==Key::DOWN)selected_row=(selected_row+(page==Page::MineLift?2:1))%count;
 if(k==Key::LEFT)selected_row=(selected_row+count-1)%count;
 if(k==Key::RIGHT)selected_row=(selected_row+1)%count;
 if(k==Key::A)mine_ui_action(selected_row);
}
void mine_ui_tap(int px,int py) {
 auto m=layout().modal();
 if(page==Page::MineLift){for(int i=0;i<8;++i)if(layout().destination(i).contains(px,py)){mine_ui_action(i);return;}}
 else if(page==Page::MineJournal){for(int i=0;i<3;++i)if(farm_ui::Rect{m.x+12,m.y+137+i*22,202,21}.contains(px,py)){mine_ui_action(i);return;}}
 else for(int i=0;i<2;++i)if(farm_ui::Rect{m.x+12+i*105,m.y+174,i?99:98,26}.contains(px,py)){mine_ui_action(i);return;}
}
