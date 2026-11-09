// SPDX-License-Identifier: MIT
// Included after the shared pixel UI helpers. All controls use the farm palette.
bool multiplayer_from_title=false,multiplayer_edit=false,multiplayer_port=false;
int multiplayer_focus=0,multiplayer_digit=0,multiplayer_list=0;
char multiplayer_ip[24]="192.168.1.10",multiplayer_port_text[6]="31889";
farm_ui::Rect multiplayer_title_button(){auto r=layout().title_enter();return {r.x,r.y-37,r.w,30};}
void open_multiplayer(){
  multiplayer_from_title=title_screen;title_screen=false;multiplayer_edit=false;
  open_page(Page::Multiplayer);multiplayer_focus=0;
}
void close_multiplayer(){
  multiplayer_edit=false;page=Page::Farm;
  if(multiplayer_from_title&&!dewfield_net::room.active()){title_screen=true;scene_surface(false);}
  else scene_surface(true);
  redraw();
}
farm_ui::Rect multiplayer_control(int index){
  auto m=layout().modal();
  if(index==0)return {m.x+12,m.y+56,128,24};
  if(index==1)return {m.x+146,m.y+56,69,24};
  if(index==2)return dewfield_net::room.browser?farm_ui::Rect{0,0,0,0}:farm_ui::Rect{m.x+12,m.y+86,98,25};
  if(index==3)return dewfield_net::room.browser?farm_ui::Rect{m.x+12,m.y+86,203,25}:farm_ui::Rect{m.x+117,m.y+86,98,25};
  if(index==4)return {m.x+12,m.y+117,154,24};
  if(index==5)return {m.x+171,m.y+117,44,24};
  return {m.x+12,m.y+148+(index-6)*25,203,22};
}
void multiplayer_action(int index){
  using namespace dewfield_net;auto &r=room;uint16_t port;
  if(index==0||index==1){multiplayer_edit=true;multiplayer_port=index==1;multiplayer_digit=0;redraw();return;}
  if(index==2||index==3){
    if(!wire::address(index==2?"127.0.0.1":multiplayer_ip,multiplayer_port_text,port)){
      msg("请输入有效的IP和端口号","Enter a valid IP and port");return;
    }
    if(index==2){if(!r.can_host){msg("此设备不能主持，请更新固件或加入房间","This Host cannot listen. Join a farm instead");return;}r.host(port);}
    else r.join(multiplayer_ip,port);
  }else if(index==4){
    if(r.browser){r.secure=!r.secure;}else if(r.discover)r.discover();else r.message("Use IP and port to join");
  }else if(index==5){multiplayer_list=r.found_count>2?(multiplayer_list?0:2):0;}
  else{
    unsigned selected=unsigned(multiplayer_list+index-6);
    if(selected<r.found_count){auto f=r.found[selected];std::snprintf(multiplayer_ip,sizeof multiplayer_ip,"%s",f.ip);
      std::snprintf(multiplayer_port_text,sizeof multiplayer_port_text,"%u",f.port);r.join(f.ip,f.port);}
  }
  redraw();
}
void multiplayer_digit_press(int digit){
  if(digit<0||digit>=12)return;
  char *value=multiplayer_port?multiplayer_port_text:multiplayer_ip;
  unsigned n=unsigned(std::strlen(value)),capacity=multiplayer_port?5:15;
  const char keys[]="123456789.0<";
  char c=keys[digit];
  if(c=='<'){if(n)value[n-1]=0;}
  else if(n<capacity&&(!multiplayer_port||c!='.')){value[n]=c;value[n+1]=0;}
  redraw();
}
void draw_multiplayer(int x,int y){
  using namespace dewfield_net;auto &r=room;
  if(multiplayer_edit){
    const char *value=multiplayer_port?multiplayer_port_text:multiplayer_ip;
    button(x+12,y+37,148,25,value,true);button(x+167,y+37,48,25,tr("DONE","完成"));
    const char *keys[]={"1","2","3","4","5","6","7","8","9",".","0","<"};
    for(int i=0;i<12;++i)button(x+12+(i%3)*69,y+68+(i/3)*32,64,27,keys[i],using_controller()&&i==multiplayer_digit);
    return;
  }
  if(r.active()||r.mode==Mode::Joining){
    center_text(x+114,y+41,r.mode==Mode::Hosting?tr("YOUR SHARED FARM","你的合作农场"):r.ready?tr("VISITING A FARM","正在一起种田"):tr("CONNECTING...","正在连接…"),Sage);
    char value[80];std::snprintf(value,sizeof value,"%s:%u",r.address,r.port);fit_text(x+12,y+61,value,202,Text);
    std::snprintf(value,sizeof value,english()?"Farmers %u / 4":"农友 %u / 4",r.players());text(x+12,y+83,value,Sage);
    for(unsigned i=0;i<MaxPlayers;++i){auto&f=r.farmers[i];if(!f.pose.active)continue;
      std::snprintf(value,sizeof value,english()?"P%u %s%s":"P%u %s%s",i+1,i==0?tr("Host","主持"):tr("Farmer","农友"),i==r.my_id?tr(" / you"," · 你"):"");
      text(x+16,y+103+int(i)*15,value,Text);
    }
    fit_text(x+12,y+163,tr("Shared goods. The host saves farm progress.","共享物资，农场进度由主持设备保存"),202,Muted);
    button(x+12,y+179,98,24,tr("PLAY","回到农场"),multiplayer_focus==0);
    button(x+117,y+179,98,24,r.mode==Mode::Hosting?tr("CLOSE FARM","结束联机"):tr("LEAVE","离开房间"),multiplayer_focus==1);
    return;
  }
  fit_text(x+12,y+37,r.status[0]?r.status:tr("One host, up to three friends","一台设备主持，最多三位农友加入"),202,Sage);
  for(int i=0;i<6;++i){auto q=multiplayer_control(i);if(!q.w)continue;const char *label=i==0?multiplayer_ip:i==1?multiplayer_port_text:
    i==2?tr("HOST","创建农场"):i==3?tr("JOIN","加入农场"):i==4?(r.browser?(r.secure?tr("WSS: ON","加密连接：开"):tr("WSS: OFF","加密连接：关")):tr("FIND LOCAL FARMS","寻找附近农场")):tr("NEXT","更多");
    button(q.x,q.y,q.w,q.h,label,using_controller()&&multiplayer_focus==i);
  }
  for(int row=0;row<2;++row){unsigned index=unsigned(multiplayer_list+row);if(index>=r.found_count)continue;
    auto q=multiplayer_control(6+row);button(q.x,q.y,q.w,q.h,"",using_controller()&&multiplayer_focus==6+row);
    char label[96];std::snprintf(label,sizeof label,"%s:%u",r.found[index].ip,r.found[index].port);fit_text(q.x+7,q.y+5,label,q.w-14,Text);
  }
  if(!r.found_count)fit_text(x+12,y+158,r.browser?tr("Web: join with IP and port","网页端请用IP和端口加入"):tr("Same Wi-Fi, or enter the host address","连接同一Wi-Fi，或输入主持设备地址"),202,Muted);
}
void multiplayer_tap(int px,int py){
  auto m=layout().modal();using namespace dewfield_net;
  if(layout().close().contains(px,py)){close_multiplayer();return;}
  if(multiplayer_edit){
    if(farm_ui::Rect{m.x+167,m.y+37,48,25}.contains(px,py)){multiplayer_edit=false;redraw();return;}
    for(int i=0;i<12;++i)if(farm_ui::Rect{m.x+12+(i%3)*69,m.y+68+(i/3)*32,64,27}.contains(px,py))multiplayer_digit_press(i);
    return;
  }
  if(room.active()||room.mode==Mode::Joining){
    if(farm_ui::Rect{m.x+12,m.y+179,98,24}.contains(px,py))close_multiplayer();
    else if(farm_ui::Rect{m.x+117,m.y+179,98,24}.contains(px,py)){room.leave();stop_motion();base_dirty=true;save();close_multiplayer();}
    redraw();return;
  }
  for(int i=0;i<8;++i)if(multiplayer_control(i).contains(px,py)){multiplayer_action(i);return;}
}
void multiplayer_key(Key k){
  using namespace dewfield_net;
  if(k==Key::B){if(multiplayer_edit){multiplayer_edit=false;redraw();}else close_multiplayer();return;}
  if(multiplayer_edit){
    if(k==Key::Y){multiplayer_edit=false;redraw();return;}
    if(k==Key::X){(multiplayer_port?multiplayer_port_text:multiplayer_ip)[0]=0;redraw();return;}
    if(k==Key::LEFT)multiplayer_digit=(multiplayer_digit+11)%12;
    if(k==Key::RIGHT)multiplayer_digit=(multiplayer_digit+1)%12;
    if(k==Key::UP)multiplayer_digit=(multiplayer_digit+9)%12;
    if(k==Key::DOWN)multiplayer_digit=(multiplayer_digit+3)%12;
    if(k==Key::A)multiplayer_digit_press(multiplayer_digit);
  }else if(room.active()||room.mode==Mode::Joining){
    if(k==Key::LEFT||k==Key::RIGHT)multiplayer_focus=1-multiplayer_focus;
    if(k==Key::A){if(multiplayer_focus){room.leave();base_dirty=true;save();}close_multiplayer();}
  }else{
    if(k==Key::LEFT||k==Key::UP)multiplayer_focus=(multiplayer_focus+7)%8;
    if(k==Key::RIGHT||k==Key::DOWN)multiplayer_focus=(multiplayer_focus+1)%8;
    if(k==Key::A)multiplayer_action(multiplayer_focus);
  }
  redraw();
}
