// SPDX-License-Identifier: MIT
// Included after the shared UI helpers, inside the application's namespace.
farm_ui::Rect ranch_row(int i) {auto m=layout().modal();return {m.x+12,m.y+66+i*30,202,28};}
farm_ui::Rect ranch_button(int i) {auto m=layout().modal();return {m.x+12,m.y+100+i*26,202,24};}
void open_ranch() {
  if(!ranch_here(state)){go_to(Region::Farm,RanchX,RanchY,true);return;}
  if(!state.ranch){begin_ranch(state);base_dirty=true;changed();msg("旧棚打开了！两只小鸡和20份草料等着你","Two young chickens and 20 hay welcome you");}
  ranch_page=0;open_page(Page::Ranch);
}
int ranch_slot(int row) {
  int n=0;for(int i=0;i<AnimalMax;++i)if(state.animals[i].kind) {if(n++==ranch_page*3+row)return i;}return -1;
}
void ranch_error() {msg("条件不足：检查金币、草料、背包与牧场空位","Check gold, hay, bag space and reserved stalls");}
void animal_action(int choice) {
  if(ranch_index<0||ranch_index>=AnimalMax||!state.animals[ranch_index].kind){page=Page::Ranch;return;}
  if(choice==3) {
    if(state.animals[ranch_index].kind!=5){open_page(Page::Ranch);return;}
    auto p=animal_pose(state,ranch_index);
    if(!ranch_here(state)&&(state.region||!near(state,p.x/16,p.y/16))) {
      int id=ranch_index;go_to(Region::Farm,p.x/16,p.y/16,true);butcher_pending=id;return;
    }
    open_page(Page::Butcher);return;
  }
  if(choice==2){state.breeding=!state.breeding;changed();return;}
  auto p=animal_pose(state,ranch_index);
  if(!state.region&&!near(state,p.x/16,p.y/16)){go_to(Region::Farm,p.x/16,p.y/16,true);return;}
  if(state.region){go_to(Region::Farm,p.x/16,p.y/16,true);return;}
  if(choice==0) {
    if(pet_animal(state,ranch_index))msg("轻轻摸了摸，今天更亲近你了","A gentle pat brings you closer");
    else msg("今天已经摸过啦","Already petted today");
  } else {
    int item=collect_animal(state,ranch_index);
    if(item){gather_effect(p.x/16,p.y/16,item,1);msg("产物已放进背包，可拿起出货或食用","Collected into your bag for shipping or food");}
    else msg("尚未产出，或背包没有空位","No produce ready, or the bag is full");
  }
  changed();
}
void ranch_action(int choice) {
  if(!ranch_here(state)){go_to(Region::Farm,RanchX,RanchY,true);return;}
  if(choice==0)open_page(Page::Supply);
  else if(choice==1)open_page(Page::Incubate);
  else {if(!fill_trough(state))ranch_error();else msg("食槽已添好，两天的草料更安心","Trough filled for up to two nights");changed();}
}
void supply_action(int choice) {
  if(choice==0)supply_male=!supply_male;
  else if(choice==1){if(!buy_animal(state,supply_kind,supply_male))ranch_error();else msg("新伙伴已到牧区，记得添草料","A new youngster arrived in the pasture");changed();}
  else if(choice==2){if(!buy_hay(state))ranch_error();changed();}
  else open_page(Page::Ranch);
}
void animal_portrait(int x,int y,const farm::Animal&a) {
  IconCanvas c;art::animal_pixels(c,art::animal_sprite(a,1,0),x,y);
}
void ranch_draw(int x,int y) {
  char b[128];
  if(page==Page::Ranch) {
    std::snprintf(b,sizeof(b),english()?"Animals %d/12 | hay %u | feed %u":"动物%d/12 · 草料%u · 槽内%u",animal_count(state),state.hay,state.trough);
    fit_text(x+12,y+36,b,202,Sage);fit_text(x+12,y+51,ranch_goal(state,english()),202,Muted);
    for(int i=0;i<3;++i) {
      int id=ranch_slot(i);if(id<0)continue;auto &a=state.animals[id];auto r=ranch_row(i);
      button(r.x,r.y,r.w,r.h,"",selected_row==i);
      animal_portrait(r.x+17,r.y+25,a);
      std::snprintf(b,sizeof(b),english()?"%s %s #%d":"%s · %s %d号",english()?Breeds[a.kind-1].en:Breeds[a.kind-1].cn,a.male?tr("male","公"):tr("female","母"),id+1);
      fit_text(r.x+36,r.y+2,b,160);
      const char *stage=animal_stage(a)==0?tr("Baby","幼年"):animal_stage(a)==1?tr("Young","成长期"):tr("Adult","成年");
      std::snprintf(b,sizeof(b),"%s / %s",stage,animal_product(a)?tr("Produce ready","可以收取"):a.pregnancy?tr("Expecting","孕育中"):tr("Growing peacefully","安心生活"));fit_text(r.x+36,r.y+15,b,160,Sage);
    }
    button(x+12,y+160,35,20,"<");button(x+181,y+160,35,20,">");
    std::snprintf(b,sizeof(b),"%d / %d",ranch_page+1,std::max(1,(animal_count(state)+2)/3));center_text(x+114,y+164,b,Muted);
    const char *labels[]={tr("SUPPLY","补给"),tr("HATCH","孵化"),tr("FEED","添草料")};
    for(int i=0;i<3;++i)button(x+12+i*69,y+184,65,21,labels[i],selected_row==i+3);
  } else if(page==Page::Animal||page==Page::Butcher) {
    auto &a=state.animals[ranch_index];if(!a.kind)return;
    std::snprintf(b,sizeof(b),"%s #%d · %s",english()?Breeds[a.kind-1].en:Breeds[a.kind-1].cn,ranch_index+1,a.male?tr("Male","公"):tr("Female","母"));
    center_text(x+114,y+37,b,Sage);animal_portrait(x+39,y+91,a);
    std::snprintf(b,sizeof(b),english()?"Age %u | bond %u":"年龄%u天 · 亲密%u",a.age,a.heart);fit_text(x+65,y+55,b,145);
    std::snprintf(b,sizeof(b),a.pregnancy?(english()?"Birth in %u fed nights":"再喂%u夜迎接幼崽"):(english()?"Fed on day %u":"上次进食：第%u天"),a.pregnancy?a.pregnancy:a.fed_day);fit_text(x+65,y+71,b,145,Muted);
    if(page==Page::Butcher) {
      paragraph(x+12,y+104,tr("An adult pig yields 4 pork. This removes this animal. Pregnant pigs cannot be selected.","成年猪可取4份猪肉，该动物会离开牧场。孕期不能取肉。确认后无法撤回。"),202,4,Text);
      button(x+12,y+175,98,27,tr("CANCEL","取消"),selected_row==0);
      button(x+117,y+175,99,27,tr("CONFIRM","确认取肉"),selected_row==1);
    } else {
      const char *labels[]={tr("PET","摸一摸"),tr("COLLECT","收取产物"),state.breeding?tr("Breeding: ON","自然繁殖：开"):tr("Breeding: OFF","自然繁殖：关"),a.kind==5?tr("PROCESS PIG...","屠宰取肉…"):tr("BACK TO HERD","返回牧场")};
      for(int i=0;i<4;++i){auto r=ranch_button(i);button(r.x,r.y,r.w,r.h,labels[i],selected_row==i);}
    }
  } else if(page==Page::Supply) {
    button(x+12,y+38,30,44,"<");button(x+186,y+38,30,44,">");
    Animal a{uint8_t(supply_kind),uint8_t(supply_male),0,50};animal_portrait(x+72,y+80,a);
    std::snprintf(b,sizeof(b),"%s %uG",english()?Breeds[supply_kind-1].en:Breeds[supply_kind-1].cn,Breeds[supply_kind-1].price);fit_text(x+98,y+48,b,86,Sage);
    fit_text(x+48,y+83,tr("L/R: choose species","左右选择动物种类"),166,Muted);
    const char *labels[]={supply_male?tr("Sex: male","性别：公"):tr("Sex: female","性别：母"),tr("BUY YOUNG ANIMAL","购买幼崽"),tr("10 HAY / 20G","草料10份 / 20G"),tr("BACK","返回牧场")};
    for(int i=0;i<4;++i){auto r=ranch_button(i);button(r.x,r.y,r.w,r.h,labels[i],selected_row==i);}
  } else {
    for(int i=0;i<IncubatorMax;++i) {
      auto &e=state.incubators[i];std::snprintf(b,sizeof(b),english()?"Nest %d: %s / %u nights":"孵化位%d：%s · 还需%u夜",i+1,e.kind?(english()?Breeds[e.kind-1].en:Breeds[e.kind-1].cn):tr("Empty","空置"),e.days);fit_text(x+12,y+38+i*19,b,202,Sage);
    }
    fit_text(x+12,y+98,state.ranch_chapter?tr("A fertile egg and a free stall are needed","需受精蛋与一个空位，睡觉后孵化"):tr("Xiaohe can repair this after the first egg","先送一枚鸡蛋给小禾，她会修好孵化器"),202,Muted);
    const char *labels[]={tr("INCUBATE CHICKEN EGG","放入受精鸡蛋"),tr("INCUBATE DUCK EGG","放入受精鸭蛋"),tr("BACK","返回牧场")};
    for(int i=0;i<3;++i)button(x+12,y+122+i*27,202,24,labels[i],selected_row==i);
  }
}
void butcher_action(bool confirm) {
  if(!confirm){open_page(Page::Animal);return;}
  if(slaughter(state,ranch_index)){changed();open_page(Page::Ranch);msg("获得4份猪肉，可出货","Collected 4 pork for shipping");}
  else {ranch_error();open_page(Page::Animal);}
}
void ranch_key(Key k) {
  if(k==Key::B){open_page(page==Page::Ranch?Page::Farm:page==Page::Butcher?Page::Animal:Page::Ranch);return;}
  if(page==Page::Ranch&&(k==Key::L||k==Key::R)) {int count=std::max(1,(animal_count(state)+2)/3);ranch_page=(ranch_page+(k==Key::L?count-1:1))%count;selected_row=0;return;}
  if(page==Page::Supply&&(k==Key::L||k==Key::LEFT||k==Key::R||k==Key::RIGHT)){supply_kind=1+(supply_kind+(k==Key::L||k==Key::LEFT?3:0))%5;return;}
  int count=page==Page::Ranch?6:page==Page::Butcher?2:page==Page::Incubate?3:4;
  if(k==Key::UP||k==Key::LEFT)selected_row=(selected_row+count-1)%count;
  if(k==Key::DOWN||k==Key::RIGHT)selected_row=(selected_row+1)%count;
  if(k!=Key::A)return;
  if(page==Page::Ranch){if(selected_row<3){int id=ranch_slot(selected_row);if(id>=0){ranch_index=id;open_page(Page::Animal);}}else ranch_action(selected_row-3);}
  else if(page==Page::Animal)animal_action(selected_row);
  else if(page==Page::Supply)supply_action(selected_row);
  else if(page==Page::Butcher)butcher_action(selected_row==1);
  else if(selected_row==2)open_page(Page::Ranch);
  else {if(!incubate(state,selected_row+1))ranch_error();changed();}
}
void ranch_tap(int px,int py) {
  auto m=layout().modal();int x=m.x,y=m.y;
  if(page==Page::Ranch) {
    for(int i=0;i<3;++i)if(ranch_row(i).contains(px,py)){int id=ranch_slot(i);if(id>=0){ranch_index=id;open_page(Page::Animal);}return;}
    if(farm_ui::Rect{x+12,y+160,35,20}.contains(px,py))ranch_key(Key::L);
    if(farm_ui::Rect{x+181,y+160,35,20}.contains(px,py))ranch_key(Key::R);
    for(int i=0;i<3;++i)if(farm_ui::Rect{x+12+i*69,y+184,65,21}.contains(px,py))ranch_action(i);
  } else if(page==Page::Butcher) {
    if(farm_ui::Rect{x+12,y+175,98,27}.contains(px,py))butcher_action(false);
    if(farm_ui::Rect{x+117,y+175,99,27}.contains(px,py))butcher_action(true);
  } else if(page==Page::Incubate) {
    for(int i=0;i<3;++i)if(farm_ui::Rect{x+12,y+122+i*27,202,24}.contains(px,py)){selected_row=i;ranch_key(Key::A);return;}
  } else {
    if(page==Page::Supply&&farm_ui::Rect{x+12,y+38,30,44}.contains(px,py))ranch_key(Key::L);
    if(page==Page::Supply&&farm_ui::Rect{x+186,y+38,30,44}.contains(px,py))ranch_key(Key::R);
    for(int i=0;i<4;++i)if(ranch_button(i).contains(px,py)){if(page==Page::Supply)supply_action(i);else animal_action(i);return;}
  }
}
