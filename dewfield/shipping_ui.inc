// SPDX-License-Identifier: MIT
// Included inside game after the shared drawing helpers.
farm_ui::Rect shipping_cell(int index) {auto m=layout().modal();return {m.x+12+(index%7)*29,m.y+64+(index/7)*26,27,24};}
farm_ui::Rect shipping_button(int index) {auto m=layout().modal();return {m.x+12+index*69,m.y+179,65,24};}
void shipping_action(int choice) {
  int item=state.slots[shipping_selection];
  if(choice==2) {
    if(unship(state)){sound(dewfield_audio::Sfx::Confirm);changed();}
    else msg("无法取回：检查背包空位和物品堆叠","Cannot retrieve: check bag space and stack capacity");
    return;
  }
  unsigned count=choice==0?item_count(state,item):1;
  if(ship_items(state,item,count)) {
    deposited_item=item;deposit_ms=360;sound(dewfield_audio::Sfx::Ship);changed();
  } else msg("选择可出售物品；待结算金额不能超上限","Choose a sellable stack; the pending total must fit");
}
void shipping_draw(int x,int y) {
  char b[96];std::snprintf(b,sizeof(b),english()?"Pending %uG / %d of 3":"待结算%uG · 第%d/3页",state.shipping,shipping_page+1);fit_text(x+12,y+36,b,202,Sage);
  fit_text(x+12,y+50,tr("A stack / X one / Y retrieve","A整组 · X单个 · Y取回"),202,Muted);
  button(x+12,y+7,20,20,"<");button(x+169,y+7,20,20,">");
  for(int i=0;i<21;++i) {
    int slot=shipping_page*21+i;if(slot>=InventorySize)break;int item=state.slots[slot];auto r=shipping_cell(i);
    rounded_rect(r.x,r.y,r.w,r.h,slot==shipping_selection?Sage:farm_palette::Frame,3);
    rounded_rect(r.x+1,r.y+1,r.w-2,r.h-2,slot==shipping_selection?farm_palette::Selected:Soft,2);
    if(item){item_icon(r.x+r.w/2,r.y+12,item);
      if(item>=8){char count[8];std::snprintf(count,sizeof(count),"%u",item_count(state,item));int w=text_width(count);rect(r.x+r.w-w-1,r.y+12,w,12,Paper);text(r.x+r.w-w-1,r.y+12,count,Text);}if(!item_price(item))rect(r.x+3,r.y+r.h-3,r.w-6,1,Muted);}

  }
  int item=state.slots[shipping_selection];char label[64];item_label(label,sizeof(label),item,false);
  std::snprintf(b,sizeof(b),"%s x%u / %uG",label,item_count(state,item),unsigned(item_price(item))*item_count(state,item));fit_text(x+12,y+149,b,202,Sage);
  fit_text(x+12,y+164,tr("B close; only the last deposit can be retrieved","B关闭 · 仅可取回最后一组"),202,Muted);
  const char *labels[]={tr("STACK","整组"),tr("ONE","单个"),tr("RETRIEVE","取回")};
  for(int i=0;i<3;++i){auto r=shipping_button(i);button(r.x,r.y,r.w,r.h,labels[i]);}
}
void shipping_key(Key k) {
  if(k==Key::B){page=Page::Farm;redraw();return;}
  if(k==Key::L||k==Key::R){shipping_page=(shipping_page+(k==Key::L?2:1))%3;shipping_selection=shipping_page*21;}
  else if(k==Key::LEFT&&shipping_selection%7)--shipping_selection;
  else if(k==Key::RIGHT&&shipping_selection%7<6&&shipping_selection+1<InventorySize)++shipping_selection;
  else if(k==Key::UP&&shipping_selection>=7)shipping_selection-=7;
  else if(k==Key::DOWN&&shipping_selection+7<InventorySize)shipping_selection+=7;
  else if(k==Key::A)shipping_action(0);
  else if(k==Key::X)shipping_action(1);
  else if(k==Key::Y)shipping_action(2);
  shipping_page=shipping_selection/21;redraw();
}
void shipping_tap(int px,int py) {
  auto m=layout().modal();
  if(farm_ui::Rect{m.x+12,m.y+7,20,20}.contains(px,py)){shipping_key(Key::L);return;}
  if(farm_ui::Rect{m.x+169,m.y+7,20,20}.contains(px,py)){shipping_key(Key::R);return;}
  for(int i=0;i<21;++i)if(shipping_page*21+i<InventorySize&&shipping_cell(i).contains(px,py)){shipping_selection=shipping_page*21+i;redraw();return;}
  for(int i=0;i<3;++i)if(shipping_button(i).contains(px,py)){shipping_action(i);return;}
}
struct FoodPreview {
  int x,y;
  void rect(int xx,int yy,int w,int h,uint16_t color){pocket::rect(x+xx*2,y+yy*2,w*2,h*2,color);}
  void line(int x0,int y0,int x1,int y1,uint16_t color){int dx=std::abs(x1-x0),dy=-std::abs(y1-y0),sx=x0<x1?1:-1,sy=y0<y1?1:-1,error=dx+dy;for(;;){rect(x0,y0,1,1,color);if(x0==x1&&y0==y1)break;int e=error*2;if(e>=dy){error+=dy;x0+=sx;}if(e<=dx){error+=dx;y0+=sy;}}}
};
void food_preview(int x,int y,int item){
  rounded_rect(x-26,y-16,52,24,Border,8);rounded_rect(x-25,y-15,50,21,farm_palette::Brass,8);rounded_rect(x-22,y-13,44,16,Paper,7);
  FoodPreview c{x,y};if(item>=12&&item<=15)art::produce(c,0,0,item-12);else art::food(c,0,0,item);
}
void eat_action(bool confirm) {
  page=Page::Farm;
  if(confirm&&food_item==held_item()) {
    if(eat(state,food_item)){sound(dewfield_audio::Sfx::Eat);changed();msg("吃好了，体力和生命恢复了","A good meal. Energy and health restored");}
    else msg("体力充足，留着稍后享用吧","You feel well. Save it for later");
  }
  food_item=0;redraw();
}
