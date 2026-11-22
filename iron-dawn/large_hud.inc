// SPDX-License-Identifier: MIT
// App members. The sidebar owns its entire touch region, including empty slots.
 void large_topbar(Raster& r,const LargeHudLayout& l){
  r.rect(0,0,w,l.top,navy);r.rect(0,l.top-2,w,2,edge);char text[80];int pause_w=std::max(70,l.row*2),slot=(w-pause_w-32)/3;
  for(int i=0;i<3;++i)Skin::panel_frame(r,{8+i*(slot+5),4,slot,l.top-10},rgb(22,35,38));
  Skin::icon(r,UiIcon::ore,16,l.top/2-7,gold);std::snprintf(text,sizeof text,"矿石  %d",world.credits);r.label({34,4,slot-32,l.top-10},text,gold,14,0);
  Skin::icon(r,UiIcon::power,21+slot,l.top/2-7,world.power_net<0?red:cyan);std::snprintf(text,sizeof text,"电力  %+d",world.power_net);r.label({39+slot,4,slot-32,l.top-10},text,world.power_net<0?red:cyan,14,0);
  std::snprintf(text,sizeof text,save_flash>0?"%02u:%02u  已保存":"%02u:%02u",unsigned(world.time)/60,unsigned(world.time)%60);r.label({22+2*slot,4,slot-8,l.top-10},text,cream,14);
  button(r,{w-pause_w-8,4,pause_w,l.top-10},pausing,online()?"菜单":"暂停");
 }
 void large_hud(Raster& r){
  auto l=large_layout();char text[160];large_topbar(r,l);Skin::panel_frame(r,l.sidebar(),navy,edge);r.rect(l.world_width,l.top,2,h-l.top,edge);draw_minimap(r);
  Rect info=l.selection_info(),squad=l.squad_button();
  const Entity* chosen=nullptr;int selected=world.selected();for(auto& a:world.e)if(a.hp>0&&a.selected){chosen=&a;break;}
  int portrait_height=std::max(0,info.h-l.line-l.row-6);
  if(chosen&&portrait_height>12){Skin::well(r,{info.x,info.y,info.w,portrait_height});Skin::picture(r,chosen->kind,{info.x+4,info.y+4,info.w-8,portrait_height-8},0,world.faction(chosen->team));}
  if(info.h>=l.line+l.row){std::snprintf(text,sizeof text,"已选 %d",selected);r.label({info.x,squad.y-l.line,info.w,l.line},text,cyan,12);}
  button(r,squad,squads_open,dock.panel==DockPanel::squads?"收起":"编队",dock.panel==DockPanel::squads);
  if(dock.panel==DockPanel::squads){
   for(int i=0;i<squad_count;++i)squad_cards(r,i,l.card(i*2),l.card(i*2+1));
   button(r,l.foot(0),base,"回基地");button(r,l.foot(1),dock_close,"收起");
   large_commands(r,l,selected);return;
  }
  button(r,l.tab(0),units,"生产",tab==0);
  button(r,l.tab(1),placing>=0?cancel:buildings,placing>=0?"取消":construction_label(),tab==1);
  if(!tab&&world.selected_producer()<0)r.label({l.world_width+8,l.grid,l.side-16,l.row},production_label(),muted,12,0);
  for(int n=0;n<5;++n){
   int k=production_card(tab==1,n);if(k<0)continue;
   auto a=l.card(n);auto state=production_state(k);
   if(button_count<int(buttons.size()))buttons[button_count++]={a,produce0+k};
   Skin::card_frame(r,a,state.active);
   std::snprintf(text,sizeof text,"%d",world.spec(k).cost);
   const char* price=state.text?state.text:text;
   int text_width=std::max(r.ink_width(short_title(k),12),r.ink_width(price,12));
   int picture=std::clamp(a.w-text_width-14,0,std::max(0,std::min(a.h-8,a.w/2)));
   if(picture<14)picture=0;
   if(picture)production_picture(r,k,{a.x+4,a.y+4,picture,a.h-8},state);
   int text_left=picture?picture+8:5,label_line=std::min(l.line,(a.h-6)/2);
   Rect name={a.x+text_left,a.y+(a.h-label_line*2)/2,a.w-text_left-5,label_line};
   r.label(name,short_title(k),state.enabled?cream:muted,12,picture?0:1);
   name.y+=label_line;
   r.label(name,price,state.enabled?(state.active||world.credits>=world.spec(k).cost?gold:red):muted,12,picture?0:1);
   if(state.queued){std::snprintf(text,sizeof text,"%d",state.queued);r.label({a.x+a.w-17,a.y+3,14,l.line},text,cyan,12);}
   if(state.active)Skin::progress(r,{a.x+3,a.y+a.h-3,a.w-6,2},state.progress,tab&&world.build_ready()?gold:cyan);
  }
  if(tab)button(r,l.card(5),construction_page,combined_page?"基础设施":"海空设施",combined_page);
  else {
   auto q=l.card(5);Skin::well(r,q);
   int line=std::min(l.line,(q.h-6)/2),y=q.y+(q.h-2*line)/2;
   r.label({q.x+4,y,q.w-8,line},"生产队列",muted,12);
   if(world.job_count){auto& job=world.jobs[0];std::snprintf(text,sizeof text,"%s %us",short_title(job.kind),unsigned(std::ceil(job.left)));r.label({q.x+4,y+line,q.w-8,line},text,cream,12);Skin::progress(r,{q.x+4,q.y+q.h-4,q.w-8,2},1-job.left/job.total,cyan);}else r.label({q.x+4,y+line,q.w-8,line},"空闲",muted,12);
  }
  auto status=l.status();if(status.h>=l.line*3+12){Skin::well(r,status);std::snprintf(text,sizeof text,"火力 %u  装甲 %u",world.extra.damage,world.extra.armor);r.label({status.x+8,status.y+8,status.w-16,l.line},text,cyan,12,0);std::snprintf(text,sizeof text,"采矿 %d",world.earned);r.label({status.x+8,status.y+8+l.line,status.w-16,l.line},text,muted,12,0);std::snprintf(text,sizeof text,"%s %u秒",world.wave_direction(world.wave+1),unsigned(std::max(0.f,std::ceil(world.next_wave-world.time))));r.label({status.x+8,status.y+8+l.line*2,status.w-16,l.line},text,gold,12,0);}
  button(r,l.foot(0),research_open,"科研");button(r,l.foot(1),tab&&placing<0?cancel_build:cancel,placing>=0?"取消":tab?"撤建":"撤单",false,placing>=0||(tab?world.build_active():world.job_count>0));
  large_commands(r,l,selected);
 }
 void large_commands(Raster& r,const LargeHudLayout& l,int selected){
  r.rect(0,l.bottom,l.world_width,h-l.bottom,navy);r.rect(0,l.bottom,l.world_width,1,edge);button(r,l.command(0),select_army,"全军",true);button(r,l.command(1),box_select,selection.armed?"框选中":"框选",selection.armed);button(r,l.command(2),stop_army,"停止");button(r,l.command(3),repair,"维修",false,selected>0);button(r,l.command(4),zooming,"缩放");button(r,l.command(5),strike,"空袭",false,world.credits>=180&&world.strike_cd<=0);
  battle_overlays(r,false);
 }
