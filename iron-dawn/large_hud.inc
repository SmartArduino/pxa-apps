// SPDX-License-Identifier: MIT
// App members. The sidebar owns its entire touch region, including empty slots.
 void large_topbar(Raster& r,const LargeHudLayout& l){
  r.rect(0,0,w,l.top,navy);r.rect(0,l.top-2,w,2,edge);char text[80];int pause_w=std::max(70,l.row*2),slot=(w-pause_w-32)/3;
  for(int i=0;i<3;++i)Skin::panel_frame(r,{8+i*(slot+5),4,slot,l.top-10},rgb(22,35,38));
  Skin::icon(r,UiIcon::ore,16,l.top/2-6,gold);std::snprintf(text,sizeof text,"矿石  %d",world.credits);r.label({34,4,slot-30,l.top-10},text,gold,14,0);
  Skin::icon(r,UiIcon::power,21+slot,l.top/2-6,world.power_net<0?red:cyan);std::snprintf(text,sizeof text,"电力  %+d",world.power_net);r.label({39+slot,4,slot-30,l.top-10},text,world.power_net<0?red:cyan,14,0);
  std::snprintf(text,sizeof text,save_flash>0?"%02u:%02u  已保存":"%02u:%02u",unsigned(world.time)/60,unsigned(world.time)%60);r.label({18+2*slot,4,slot,l.top-10},text,cream,14);
  button(r,{w-pause_w-8,4,pause_w,l.top-10},pausing,"暂停");
 }
 void large_hud(Raster& r){
  auto l=large_layout();char text[160];large_topbar(r,l);Skin::panel_frame(r,l.sidebar(),navy,edge);r.rect(l.world_width,l.top,2,h-l.top,edge);draw_minimap(r);
  int info_x=l.map().x+l.map_size+10,info_w=w-info_x-10;const Entity* chosen=nullptr;int selected=world.selected();for(auto& a:world.e)if(a.hp>0&&a.selected){chosen=&a;break;}
  int portrait_height=std::max(0,l.map_size-l.line*2-4);
  if(chosen&&portrait_height>12){Skin::well(r,{info_x,l.top+10,info_w,portrait_height});unsigned id=chosen->kind>=hq?art::buildings_first+chosen->kind-hq:Sprites::unit_frame(chosen->kind,1.5707963f,chosen->kind==harvester);Sprites::fit(r,id,{info_x+4,l.top+14,info_w-8,portrait_height-8});}
  std::snprintf(text,sizeof text,"已选 %d",selected);r.label({info_x,l.top+12+portrait_height,info_w,l.line},text,cyan,12);
  if(l.map_size>=l.line*2){if(selected==1&&chosen)std::snprintf(text,sizeof text,"耐久 %u",unsigned(std::ceil(chosen->hp)));else std::snprintf(text,sizeof text,"部队 %d",world.army());r.label({info_x,l.top+12+portrait_height+l.line,info_w,l.line},text,muted,12);}
  button(r,l.tab(0),units,"部队",tab==0);button(r,l.tab(1),placing>=0?cancel:buildings,placing>=0?"取消":"基地",tab==1);
  constexpr int units_k[]={rifle,rocket,tank,artillery,harvester},build_k[]={power,refinery,barracks,factory,turret};const char* short_unit[]={"游骑","破甲","坦克","火炮","矿车"};const char* short_build[]={"电站","矿厂","兵营","工厂","炮塔"};
  for(int n=0;n<5;++n){int k=tab?build_k[n]:units_k[n];bool unlocked=world.unlocked(k),afford=world.credits>=specs[k].cost;auto a=l.card(n);bool active=placing==k||(world.job_count&&world.jobs[0].kind==k);if(button_count<int(buttons.size()))buttons[button_count++]={a,produce0+k};Skin::card_frame(r,a,active);
   unsigned id=tab?art::buildings_first+k-hq:Sprites::unit_frame(k,1.5707963f,k==harvester);int picture=std::min({a.h-8,a.w/2,std::max(12,a.w-2*(l.line-4)-12)});Sprites::fit(r,id,{a.x+4,a.y+4,picture,a.h-8},unlocked?0:4);
   Rect name={a.x+picture+8,a.y+(a.h-l.line*2)/2,a.w-picture-12,l.line};r.label(name,tab?short_build[n]:short_unit[n],unlocked?cream:muted,12,0);std::snprintf(text,sizeof text,"%d",specs[k].cost);name.y+=l.line;r.label(name,unlocked?text:"锁定",unlocked?(afford?gold:red):muted,12,0);
   int queued=0;for(int i=0;i<world.job_count;++i)queued+=world.jobs[i].kind==k;if(queued){std::snprintf(text,sizeof text,"%d",queued);r.box({a.x+a.w-17,a.y+3,14,l.line},navy,cyan);r.label({a.x+a.w-17,a.y+3,14,l.line},text,cyan,12);}
   if(active&&world.job_count&&world.jobs[0].kind==k)Skin::progress(r,{a.x+3,a.y+a.h-3,a.w-6,2},1-world.jobs[0].left/world.jobs[0].total,cyan);
  }
  auto q=l.card(5);Skin::well(r,q);r.label({q.x+4,q.y+2,q.w-8,l.line},"生产队列",muted,12);if(world.job_count){auto& job=world.jobs[0];std::snprintf(text,sizeof text,"%s %us",short_unit[job.kind],unsigned(std::ceil(job.left)));r.label({q.x+4,q.y+l.line,q.w-8,l.line},text,cream,12);Skin::progress(r,{q.x+4,q.y+q.h-4,q.w-8,2},1-job.left/job.total,cyan);}else r.label({q.x+4,q.y+l.line,q.w-8,l.line},"空闲",muted,12);
  auto status=l.status();if(status.h>=l.line*3+12){Skin::well(r,status);std::snprintf(text,sizeof text,"火力 %u  装甲 %u",world.extra.damage,world.extra.armor);r.label({status.x+8,status.y+8,status.w-16,l.line},text,cyan,12,0);std::snprintf(text,sizeof text,"采矿 %d",world.earned);r.label({status.x+8,status.y+8+l.line,status.w-16,l.line},text,muted,12,0);std::snprintf(text,sizeof text,"%s %u秒",world.wave_direction(world.wave+1),unsigned(std::max(0.f,std::ceil(world.next_wave-world.time))));r.label({status.x+8,status.y+8+l.line*2,status.w-16,l.line},text,gold,12,0);}
  button(r,l.foot(0),research_open,"科研");button(r,l.foot(1),cancel,placing>=0?"取消":"撤单",false,placing>=0||world.job_count>0);
  r.rect(0,l.bottom,l.world_width,h-l.bottom,navy);r.rect(0,l.bottom,l.world_width,1,edge);button(r,l.command(0),select_army,"全军",true);button(r,l.command(1),box_select,selection.armed?"框选中":"框选",selection.armed);button(r,l.command(2),stop_army,"停止");button(r,l.command(3),repair,"维修",false,selected>0);button(r,l.command(4),zooming,"缩放");button(r,l.command(5),strike,"空袭",false,world.credits>=180&&world.strike_cd<=0);
  battle_overlays(r,false);
 }
