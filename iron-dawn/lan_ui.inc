// SPDX-License-Identifier: MIT
 enum class LanStage {off,opening,waiting,connecting,ready,playing,failed};
 struct LanSession {
  LanStage stage=LanStage::off;
  bool host=false,connected=false;
  uint8_t cue=0;
  unsigned ticket=0;
  uint64_t deadline=0,next_send=0;
  char room[22]{},address[22]{};
  size_t typed=0;
  const char* status="";
  pxa::Listener listener;
  // Keep the grant alive for as long as its network resources are in use.
  pxa::Permission permission;
  std::unique_ptr<lan::Link> link;
  std::unique_ptr<World> solo,projection;
  Vec solo_view{};
  float solo_zoom=1;
 } net;

 bool online()const{return bool(net.solo);}
 bool guest()const{return online()&&!net.host;}
 bool net_setup()const{return net.host&&net.stage!=LanStage::off&&!online();}
 bool guest_action(int a)const{
  switch(a){
   case pausing:case resume:case net_leave:case select_army:case base:case stop_army:
   case objective_focus:case zooming:case box_select:case tactics_open:case squads_open:
   case dock_close:case sound_toggle:return true;
   default:return (a>=squad_recall0&&a<squad_recall0+squad_count)||
                  (a>=squad_assign0&&a<squad_assign0+squad_count);
  }
 }
 void close_lan(const char* error=nullptr){
  ++net.ticket;
  (void)ctx->window().keep_screen_on(false);
  net.link.reset();(void)net.listener.close();(void)net.permission.close();
  net.projection.reset();
  net.connected=false;net.next_send=0;net.cue=0;
  if(online()){
   world=*net.solo;net.solo.reset();
   zoom_level=net.solo_zoom;focus_with_zoom(net.solo_view);
   field.ground.release();minimap_cache.release();
  }
  dock.close();selection.cancel();tactical=TacticalMode::none;placing=-1;
  pressing=dragging=false;last_selected_id=-1;
  net.stage=error?LanStage::failed:LanStage::off;
  net.status=error?error:"";
  net.host=false;
  screen=Screen::lan_menu;
  focus_action=net_host;
  if(error)log("Iron Dawn LAN ended");
 }

 void open_lan(bool host){
  close_lan();
  net.host=host;
  net.stage=host?LanStage::opening:LanStage::connecting;
  (void)ctx->window().keep_screen_on(true);
  if(!ctx->tasks().start(connect_lan(host,net.ticket)))
   close_lan("网络任务启动失败。");
 }
 pxa::Task<void> connect_lan(bool host,unsigned ticket){
  auto caps=co_await ctx->network().capabilities();
  if(ticket!=net.ticket)co_return pxa::Result<void>{};
  if(!caps||!(host?caps->listen()&&caps->ipv4():caps->connect())){
   close_lan("此设备暂不支持局域网联机。");co_return pxa::Result<void>{};
  }
  auto grant=co_await ctx->permissions().acquire(host?"net.listen":"net.socket",host?"local-network":"outbound");
  if(ticket!=net.ticket)co_return pxa::Result<void>{};
  if(!grant){close_lan("请允许游戏使用局域网。");co_return pxa::Result<void>{};}
  net.permission=std::move(*grant);
  if(host){
   pxa::NetworkOptions options;options.buffer_bytes=8192;options.max_items=1;
   auto listener=co_await ctx->network().listen(net.permission,options);
   if(ticket!=net.ticket)co_return pxa::Result<void>{};
   if(!listener){close_lan("建房失败，请检查网络后重试。");co_return pxa::Result<void>{};}
   auto endpoint=listener->local_endpoint();
   if(!endpoint||endpoint->family!=4||!endpoint->port){
    close_lan("暂时无法读取房间地址。");co_return pxa::Result<void>{};
   }
   const auto& ip=endpoint->address;
   std::snprintf(net.room,sizeof net.room,"%u.%u.%u.%u:%u",std::to_integer<unsigned>(ip[0]),
    std::to_integer<unsigned>(ip[1]),std::to_integer<unsigned>(ip[2]),std::to_integer<unsigned>(ip[3]),endpoint->port);
   if(!lan::valid_address(net.room)){close_lan("设备尚未取得局域网地址。");co_return pxa::Result<void>{};}
   net.listener=std::move(*listener);net.stage=LanStage::waiting;
   char message[64];std::snprintf(message,sizeof message,"Iron Dawn LAN room %s",net.room);log(message);
   auto peer=co_await net.listener.accept();
   if(ticket!=net.ticket)co_return pxa::Result<void>{};
   if(!peer){close_lan("房间已关闭。");co_return pxa::Result<void>{};}
   (void)net.listener.close();
   net.link=std::make_unique<lan::Link>();net.link->socket=std::move(*peer);
   net.deadline=now+8'000'000;
  }else{
   char endpoint[32];std::snprintf(endpoint,sizeof endpoint,"tcp://%s",net.address);
   auto peer=co_await ctx->network().connect(endpoint,net.permission,{.buffer_bytes=8192,.connect_timeout_ms=8000});
   if(ticket!=net.ticket)co_return pxa::Result<void>{};
   if(!peer){close_lan("连接失败，请核对房间地址。");co_return pxa::Result<void>{};}
   net.link=std::make_unique<lan::Link>();net.link->socket=std::move(*peer);
   net.link->signal(lan::Message::hello);
   net.deadline=now+8'000'000;net.next_send=now+1'000'000;
  }
  co_return pxa::Result<void>{};
 }

 void prepare_lan_world(){
  net.solo=std::make_unique<World>(world);
  net.solo_view=view_center();
  net.solo_zoom=zoom_level;save_flash=0;
 }
 void enter_lan_battle(){
  dock.close();selection.cancel();minimap_cache.release();field.ground.release();
  combined_page=false;tab=0;placing=-1;tactical=TacticalMode::none;
  last_selected_id=-1;zoom_level=1;focus_with_zoom(world.deployment_view());world.select_all();
  world.say(net.host?"基地由你管理，队友会协助防守。":"房主管理基地，你可以选兵协防。",6);
  screen=Screen::battle;net.stage=LanStage::playing;set_clock(33);
  log(net.host?"Iron Dawn LAN battle host":"Iron Dawn LAN battle guest");
 }
 void launch_lan(){
  if(!net.host||net.stage!=LanStage::ready)return;
  prepare_lan_world();
  world.init_defense(uint32_t(now)^0x51d39f17u,lan_faction,chosen_map);
  world.next_wave+=10;
  enter_lan_battle();
  net.next_send=0;
 }
 bool receive_lan(lan::Message type,std::span<const std::byte> bytes){
  if(net.host){
   if(!net.connected&&type==lan::Message::hello&&bytes.empty()){
    net.connected=true;net.stage=LanStage::ready;focus_action=net_start;net.link->signal(lan::Message::hello);
    log("Iron Dawn LAN teammate ready");
   }else if(net.connected&&type==lan::Message::ping&&bytes.empty()){
    // The guest can wait for the host to approve the start.
   }else if(net.connected&&online()&&type==lan::Message::order){
    lan::Order order;
    if(!lan::decode_order(bytes,order))return false;
    if(lan::apply_order(world,order))log("Iron Dawn LAN teammate order");
   }else return false;
  }else{
   if(!net.connected&&type==lan::Message::hello&&bytes.empty()){
    net.connected=true;net.stage=LanStage::ready;
   }else if(net.connected&&type==lan::Message::ping&&bytes.empty()){
   }else if(net.connected&&type==lan::Message::state){
    if(!net.projection)net.projection=std::make_unique<World>();
    if(!lan::decode_snapshot(bytes,*net.projection))return false;
    auto before=online()?UnitVoice::roster(world):decltype(UnitVoice::roster(world)){};
    if(!online()){
     prepare_lan_world();world=*net.projection;
     enter_lan_battle();
    }else{
     if(net.projection->extra.seed!=world.extra.seed||net.projection->extra.mode!=world.extra.mode||
        net.projection->extra.map_rules!=world.extra.map_rules||net.projection->time<world.time)return false;
     lan::merge_snapshot(world,*net.projection);
     for(int i=0;i<MAX;++i)if(!before[i]&&world.e[i].hp>0&&world.e[i].team==0&&unit_kind(world.e[i].kind))
      sounds.unit_ready(world.e[i].kind,world.faction());
    }
    if(world.sound)sounds.play_event(world.sound);
    if((world.won||world.defeated)&&screen!=Screen::result)finish();
   }else return false;
  }
  net.deadline=now+8'000'000;
  return true;
 }
 void update_lan(){
  if(!net.link)return;
  if(now>net.deadline||!net.link->pump([&](auto type,auto bytes){return receive_lan(type,bytes);})){
   if(screen==Screen::result){
    (void)ctx->window().keep_screen_on(false);
    net.link.reset();(void)net.permission.close();
   }else close_lan(net.connected?"与队友的连接已断开。":"未能连上队友，请核对地址和游戏版本。");
   return;
  }
  if(now<net.next_send||!net.link->idle())return;
  if(net.host&&online()){
   if(!net.link->send(lan::Message::state,[&](auto bytes){return lan::encode_snapshot(bytes,world,net.cue);})){
    close_lan("战场同步失败。");return;
   }
   net.cue=0;net.next_send=now+100'000;
  }else if(net.connected){
   net.link->signal(lan::Message::ping);net.next_send=now+1'000'000;
  }
 }
 bool send_lan_order(Vec point,int target=-1,bool stop=false){
  auto order=lan::selected_order(world,point,target,stop);
  if(!order.count){world.say("请先选择作战部队。");return false;}
  if(!net.link->send(lan::Message::order,[&](auto bytes){return lan::encode_order(bytes,order);})){
   world.say("正在发送上一条指令，请稍候。",2);return false;
  }
  world.say(stop?"已下令停止。":target>=0?"已下令集火。":"已下令前进。",2);
  return true;
 }

 bool lan_action(int a){
  if(a>=net_key0&&a<net_key0+12&&screen==Screen::lan_join){
   char key="123456789.0:"[a-net_key0];
   if(net.typed+1<sizeof net.address){net.address[net.typed++]=key;net.address[net.typed]='\0';}
   return true;
  }
  switch(a){
   case net_open:close_lan();break;
   case net_host:net.host=true;net.stage=LanStage::opening;screen=Screen::defense_maps;focus_action=defense_map0+int(chosen_map);break;
   case net_join:screen=Screen::lan_join;focus_action=net_key0;break;
   case net_delete:if(net.typed)net.address[--net.typed]='\0';break;
   case net_connect:if(lan::valid_address(net.address))open_lan(false);break;
   case net_start:launch_lan();break;
   case net_leave:close_lan();break;
   default:return false;
  }
  return true;
 }
 void lan_screen(Raster& r){
  r.native_text=&native_text;
  auto column=page_column(w,h);
  int row=display.width<400?small_dialog_row():page_row(r),footer=h-row-12;
  int top=page_top(r)+6,line=std::max(16,r.text_height("中",13)+4);
  if(screen==Screen::lan_join){
   page_header(r,"加入房间","输入房主屏幕上的房间地址");
   int begin=top+row+4,step=(footer-begin-6)/3;
   Skin::well(r,{column.x,begin-row-4,column.w,row});
   r.label({column.x+8,begin-row-4,column.w-16,row},net.typed?net.address:"例如 192.168.1.20:31888",cream,15);
   const char* keys[]={"1","2","3","4","5","6","7","8","9",".","0",":"};
   for(int i=0;i<12;++i){
    int x=column.x+column.w*(i%4)/4;
    button(r,{x+2,begin+(i/4)*step,column.w/4-4,step-4},net_key0+i,keys[i]);
   }
   int width=(column.w-12)/3;
   button(r,{column.x,footer,width,row},net_open,"返回");
   button(r,{column.x+width+6,footer,width,row},net_delete,"退格",false,net.typed>0);
   button(r,{column.x+2*(width+6),footer,width,row},net_connect,"连接",true,lan::valid_address(net.address));
   return;
  }
  const bool idle=net.stage==LanStage::off||net.stage==LanStage::failed;
  page_header(r,idle?"局域网联机":net.host?"守备房间":"加入守备","双人合作守备");
  if(idle){
   r.label({column.x,top,column.w,line},net.status[0]?net.status:"两台设备需接入同一局域网。",net.status[0]?gold:cream,13,0);
   r.label({column.x,top+line+5,column.w,line},"房主管基地和生产，队友指挥部队。",muted,13,0);
   int y=std::min(top+2*line+20,footer-row-10),width=(column.w-10)/2;
   button(r,{column.x,y,width,row},net_host,"创建房间",true);
   button(r,{column.x+width+10,y,column.w-width-10,row},net_join,"加入房间");
   button(r,{column.x,footer,column.w,row},endless,"返回无尽守备");
   return;
  }
  const char* state=net.host?(net.stage==LanStage::ready?"队友已到达，可以开始守备。":
   net.stage==LanStage::waiting?"把房间地址告诉队友。":"正在创建房间……"):
   net.stage==LanStage::ready?"已加入，等待房主开始。":"正在连接房主……";
  r.label({column.x,top,column.w,line},state,cream,13,0);
  if(net.host&&(net.stage==LanStage::waiting||net.stage==LanStage::ready)){
   Skin::well(r,{column.x,top+line+14,column.w,row});
   r.label({column.x+8,top+line+14,column.w-16,row},net.room,cyan,20);
   r.label({column.x,top+line+row+24,column.w,line},defense_arenas[unsigned(chosen_map)].title,muted,13,0);
  }
  int width=(column.w-10)/2;
  button(r,{column.x,footer,net.host?width:column.w,row},net_leave,"取消");
  if(net.host)button(r,{column.x+width+10,footer,column.w-width-10,row},net_start,"开始守备",true,net.stage==LanStage::ready);
 }
