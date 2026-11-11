// SPDX-License-Identifier: MIT
#pragma once
#include "multiplayer_model.hpp"
#include <cstdio>

namespace dewfield_net {
struct Room {
  using Send=bool(*)(unsigned,Message,const uint8_t*,unsigned);
  using Start=void(*)(bool,const char*,uint16_t);
  using Close=void(*)();
  struct Floor {bool loaded=false;uint8_t damage[farm::MineRocks]{};farm::MineMob mobs[farm::MineMobs]{};};
  struct Pending {Command command;Pose pose;bool sent=false;};
  farm::State *state=nullptr,original{};
  Farmer farmers[MaxPlayers]{};
  Floor floors[farm::MineFloors]{};
  Pending pending[8]{};
  unsigned first=0,count=0,my_id=0;
  Mode mode=Mode::Offline;
  Send send=nullptr;Start start=nullptr;Close close=nullptr;void(*discover)()=nullptr;
  bool ready=false,restore=false,changed=false,teleported=false,fish_changed=false,durable_dirty=false;
  bool can_host=false,can_discover=false,browser=false,secure=false;
  uint32_t now=0,last_service=0,last_snapshot=0,last_pose=0,last_heartbeat=0,clock=0;
  uint32_t next_sequence=1,ack=0,roster_at=0,pose_at=0;
  uint8_t last_roster[MaxPlayers*PoseBytes]{},last_local_pose[PoseBytes]{};
  uint16_t day=0,port=DefaultPort;
  char address[24]="192.168.1.10",status[96]{};
  struct Found {char name[56]{},ip[24]{};uint16_t port=0;};
  Found found[3]{};
  unsigned found_count=0;
  void bind(farm::State&s){state=&s;}
  bool active()const{return ready&&(mode==Mode::Hosting||mode==Mode::Guest);}
  bool guest()const{return mode==Mode::Guest||mode==Mode::Joining;}
  unsigned players()const{unsigned n=0;for(auto&f:farmers)n+=f.pose.active;return n;}
  void message(const char*s){std::snprintf(status,sizeof status,"%s",s);changed=true;}
  void leave(const char*reason=""){
    if(close)close();
    if(restore&&state){*state=original;restore=false;teleported=true;}
    mode=Mode::Offline;ready=false;count=first=0;next_sequence=1;ack=0;
    for(auto&f:farmers)f=Farmer{};
    for(auto&f:floors)f=Floor{};
    message(reason);
  }
  void host(uint16_t value){
    leave();port=value;mode=Mode::Hosting;my_id=0;day=state?state->day:0;
    if(state){farmers[0].personal.capture(*state);farmers[0].pose.active=true;}
    message("Starting host...");
    if(start)start(true,nullptr,value);else leave("Network unavailable");
  }
  void join(const char*ip,uint16_t value){
    leave();if(!state)return;original=*state;restore=true;port=value;
    std::snprintf(address,sizeof address,"%s",ip);mode=Mode::Joining;my_id=0;
    message("Connecting...");
    if(start)start(false,address,value);else leave("Network unavailable");
  }
  void host_ready(const char*ip){
    if(mode!=Mode::Hosting)return;ready=true;
    std::snprintf(address,sizeof address,"%s",ip);
    std::snprintf(status,sizeof status,"%s:%u",address,port);changed=true;
  }
  void put_floor(const farm::State&s){
    if(!farm::in_mine(s)||!s.mine_depth)return;
    auto &f=floors[s.mine_depth-1];f.loaded=true;
    std::memcpy(f.damage,s.ore_damage,sizeof f.damage);std::memcpy(f.mobs,s.mobs,sizeof f.mobs);
  }
  void get_floor(farm::State&s){
    if(!farm::in_mine(s)||!s.mine_depth){
      std::fill(s.ore_damage,s.ore_damage+farm::MineRocks,uint8_t(0));
      for(auto&m:s.mobs)m={};return;
    }
    auto &f=floors[s.mine_depth-1];
    if(!f.loaded){auto x=s.px,y=s.py;farm::load_mine_floor(s,s.mine_depth);s.px=x;s.py=y;put_floor(s);}
    std::memcpy(s.ore_damage,f.damage,sizeof f.damage);std::memcpy(s.mobs,f.mobs,sizeof f.mobs);
    for(unsigned i=0;i<farm::MineMobs;++i)
      if(s.mine_defeated[s.mine_depth-1]&(1u<<i))s.mobs[i].hp=0;
    for(unsigned i=0;i<farm::MineRocks;++i)
      if(s.mine_cleared[s.mine_depth-1]&(1u<<i))s.ore_damage[i]=0;
  }
  void update_visual(Pose p){
    if(!state)return;p.x=state->px;p.y=state->py;p.region=state->region;p.depth=state->mine_depth;
    p.appearance=state->appearance;p.tool=state->tool;p.item=state->active_slot<farm::InventorySize?state->slots[state->active_slot]:0;
    p.seed=state->selected;p.grade=uint8_t(farm::tool_grade(*state,p.tool));p.active=active();
    farmers[my_id].pose=p;
    if(mode==Mode::Hosting){farmers[0].personal.capture(*state);put_floor(*state);}
  }
  Pose interpolated(unsigned id)const{
    auto p=farmers[id].pose;auto&old=farmers[id].previous;
    if(id!=my_id&&p.region==old.region&&p.depth==old.depth){
      unsigned t=std::min(100u,now-farmers[id].received_at);
      if(std::abs(int(p.x)-old.x)+std::abs(int(p.y)-old.y)<64){
        p.x=uint16_t((unsigned(old.x)*(100-t)+unsigned(p.x)*t)/100);
        p.y=uint16_t((unsigned(old.y)*(100-t)+unsigned(p.y)*t)/100);
        unsigned distance=uint16_t(p.distance-old.distance);
        if(p.walking&&distance<64)p.frame=uint8_t(farm::walk_frame(unsigned(old.distance)+distance*t/100));
      }
    }
    if(p.swing){unsigned elapsed=now-farmers[id].received_at;p.swing=uint16_t(elapsed<p.swing?p.swing-elapsed:0);}
    return p;
  }
  void roster_changed(unsigned id,const Pose&p){
    auto &f=farmers[id];uint8_t before[PoseBytes],after[PoseBytes];
    encode_pose(f.pose,before);encode_pose(p,after);if(!std::memcmp(before,after,PoseBytes))return;
    f.previous=interpolated(id);f.pose=p;f.received_at=now;changed=true;
  }
  bool accept_pose(unsigned id,const Pose&p,bool command=false){
    if(id>=MaxPlayers||!state||!p.active)return false;
    auto &f=farmers[id];auto &a=f.personal;
    if(p.region!=a.region||p.depth!=a.depth)return false;
    unsigned distance=unsigned(std::abs(int(p.x)-a.x)+std::abs(int(p.y)-a.y));
    if(distance>std::min(160u,20u+(now-f.last_seen)*130/1000))return false;
    Personal host;host.capture(*state);a.install(*state);get_floor(*state);
    bool walkable=farm::walkable(*state,p.x/16,p.y/16);
    host.install(*state);get_floor(*state);
    if(!walkable)return false;
    a.appearance=p.appearance;a.x=p.x;a.y=p.y;a.tool=p.tool;a.seed=p.seed;
    // Selected item is a reference to the shared inventory, never a quantity.
    if(p.item&&farm::item_count(*state,p.item)==0)return false;
    a.slot=255;
    if(p.item){for(unsigned i=0;i<farm::InventorySize;++i)if(a.slots[i]==p.item){a.slot=uint8_t(i);break;}}
    if(command&&p.item&&a.slot==255){a.slots[farm::InventorySize-1]=p.item;a.slot=farm::InventorySize-1;}
    f.last_seen=now;roster_changed(id,p);return true;
  }
  int local(Op op,int a=0,int b=0,unsigned elapsed=0){
    if(!state)return 0;
    Command c{next_sequence,op,int16_t(a),int16_t(b),uint16_t(elapsed)};
    if(!valid_command(c))return 0;
    if(mode==Mode::Joining)return 0;
    if(mode==Mode::Guest){
      if(!ready||count==8){message("Waiting for host...");return 0;}
      auto &p=pending[(first+count)%8];p={c,farmers[my_id].pose,false};
      p.pose.x=state->px;p.pose.y=state->py;p.pose.region=state->region;p.pose.depth=state->mine_depth;
      p.pose.tool=state->tool;p.pose.seed=state->selected;
      p.pose.item=state->active_slot<farm::InventorySize?state->slots[state->active_slot]:0;
      ++count;++next_sequence;
    }
    int result=execute(*state,farmers[my_id],c,now);
    if(active()){farmers[my_id].personal.capture(*state);if(mode==Mode::Hosting)put_floor(*state);changed=true;if(mode==Mode::Hosting)durable_dirty=true;}
    return result;
  }
  void disconnected(unsigned id){
    if(mode==Mode::Guest||mode==Mode::Joining){leave("Host disconnected. Single-player restored.");return;}
    if(id&&id<MaxPlayers){farmers[id]=Farmer{};changed=true;}
  }
  bool send_snapshot(unsigned id){
    if(!state||!send||id==0||id>=MaxPlayers)return false;
    uint8_t bytes[MaxPacket]{};put32(bytes,farmers[id].last_command);bytes[4]=uint8_t(id);
    put32(bytes+5,uint32_t(farmers[id].last_result));
    for(unsigned i=0;i<MaxPlayers;++i)encode_pose(farmers[i].pose,bytes+12+i*PoseBytes);
    Personal host;host.capture(*state);farmers[id].personal.install(*state);get_floor(*state);
    farm::sync_inventory(*state);
    auto n=farm::encode(*state,bytes+12+MaxPlayers*PoseBytes,farm::SaveBytes);
    host.install(*state);get_floor(*state);
    if(!n)return false;
    // Personal fishing state is needed to reconcile the minigame's random roll.
    auto &f=farmers[id].fishing;auto *p=bytes+12+MaxPlayers*PoseBytes+n;
    p[0]=uint8_t(f.phase);p[1]=uint8_t(f.species+1);p[2]=uint8_t(f.length);p[3]=uint8_t(f.attempts);
    put16(p+4,f.bite_at);put16(p+6,f.center);put16(p+8,f.radius);put16(p+10,f.cycle_ms);put16(p+12,f.elapsed);
    p[14]=uint8_t(f.count);p[15]=uint8_t(f.perfect)|(uint8_t(f.discovered)<<1)|(uint8_t(f.record)<<2)|(uint8_t(f.junk)<<3)|(uint8_t(f.no_bait)<<4);
    unsigned size=unsigned(12+MaxPlayers*PoseBytes+n+16);uint32_t hash=2166136261u;
    for(unsigned i=0;i<size;++i)hash=(hash^bytes[i])*16777619u;
    auto &peer=farmers[id];
    if(hash==peer.snapshot_hash&&now-peer.snapshot_at<1000)return true;
    if(!send(id,Message::Snapshot,bytes,size))return false;
    peer.snapshot_hash=hash;peer.snapshot_at=now;return true;
  }
  bool receive(unsigned id,Message type,const uint8_t*bytes,unsigned size){
    if(!state)return false;
    if(mode==Mode::Hosting){
      if(id==0||id>=MaxPlayers)return false;
      auto &f=farmers[id];
      if(type==Message::Hello){
        if(size!=4||get32(bytes)!=ProtocolVersion||f.pose.active)return false;
        f=Farmer{};f.personal.capture(*state);
        static constexpr farm::Point spawn[]={{13,14},{15,14},{14,15}};
        auto point=spawn[id-1];f.personal.region=0;f.personal.depth=0;
        f.personal.x=uint16_t(point.x*16+8);f.personal.y=uint16_t(point.y*16+8);
        f.personal.energy=100;f.personal.water=state->capacity;f.personal.health=100;
        f.pose.active=true;f.pose.x=f.personal.x;f.pose.y=f.personal.y;f.last_seen=now;
        message("Farmer joined");send_snapshot(id);return true;
      }
      if(!f.pose.active)return false;
      if(type==Message::Pose){Pose p;if(!decode_pose(p,bytes,size))return false;accept_pose(id,p);return true;}
      if(type!=Message::Command||size!=PoseBytes+CommandBytes)return false;
      Pose p;Command c;
      if(!decode_pose(p,bytes,PoseBytes)||!decode_command(c,bytes+PoseBytes,CommandBytes))return false;
      if(c.sequence<=f.last_command){send_snapshot(id);return true;}
      if(c.sequence!=f.last_command+1||!valid_command(c))return false;
      if(now-f.command_window>=1000){f.command_window=now;f.command_count=0;}
      if(++f.command_count>32)return false;
      if(!accept_pose(id,p,true)){f.last_command=c.sequence;f.last_result=0;send_snapshot(id);return true;}
      Personal host;host.capture(*state);f.personal.install(*state);get_floor(*state);
      uint8_t old_depth=state->mine_depth;
      f.last_result=execute(*state,f,c,now);f.last_command=c.sequence;f.personal.capture(*state);
      put_floor(*state);
      f.pose.x=state->px;f.pose.y=state->py;f.pose.region=state->region;f.pose.depth=state->mine_depth;
      if(state->mine_depth!=old_depth)f.pose.swing=0;
      host.install(*state);get_floor(*state);changed=true;durable_dirty=true;
      send_snapshot(id);return true;
    }
    if(mode!=Mode::Guest&&mode!=Mode::Joining)return false;
    if(type==Message::Pose){
      if(size!=MaxPlayers*PoseBytes)return false;
      for(unsigned i=0;i<MaxPlayers;++i){Pose p;if(!decode_pose(p,bytes+i*PoseBytes,PoseBytes))return false;if(i!=my_id)roster_changed(i,p);}
      last_heartbeat=now;return true;
    }
    constexpr unsigned base=12+MaxPlayers*PoseBytes;
    if(type!=Message::Snapshot||size!=base+farm::SaveBytes+16||bytes[4]==0||bytes[4]>=MaxPlayers)return false;
    uint32_t received_ack=get32(bytes);if(received_ack<ack)return true;
    unsigned assigned=bytes[4];
    for(unsigned i=0;i<MaxPlayers;++i){Pose p;if(!decode_pose(p,bytes+12+i*PoseBytes,PoseBytes))return false;if(i!=assigned)roster_changed(i,p);}
    farm::State candidate;
    if(!farm::decode(candidate,bytes+base,farm::SaveBytes))return false;
    auto *p=bytes+base+farm::SaveBytes;
    if(p[0]>unsigned(farm::FishingSession::Phase::Escaped)||p[1]>farm::FishSpeciesCount||p[3]>2||get16(p+12)>12000||
       get16(p+6)>1000||get16(p+8)>500||get16(p+10)<1000||get16(p+10)>5000||p[14]>2||p[15]>31||
       ((p[15]&8)&&(p[1]||p[0]==unsigned(farm::FishingSession::Phase::Reeling))))return false;
    while(count&&pending[first].command.sequence<=received_ack){first=(first+1)%8;--count;}
    bool new_ack=received_ack>ack;last_heartbeat=now;ack=received_ack;
    if(count)return true;
    Personal local;local.capture(*state);
    bool first_join=mode==Mode::Joining;
    bool warp=first_join||state->region!=candidate.region||state->mine_depth!=candidate.mine_depth||state->day!=candidate.day;
    *state=candidate;my_id=assigned;
    state->appearance=local.appearance;
    // Preserve ongoing local movement and the private quickbar. Authoritative
    // energy/health/water and shared quantities still come from the host.
    if(!warp){state->px=local.x;state->py=local.y;}
    state->show_joystick=local.joystick;state->show_action=local.action;state->music=local.music;
    state->auto_run=local.run;state->sound_effects=local.effects;
    if(!first_join){std::memcpy(state->slots,local.slots,sizeof local.slots);state->active_slot=local.slot;state->tool=local.tool;state->selected=local.seed;farm::sync_inventory(*state);}
    auto &f=farmers[my_id].fishing;f.phase=farm::FishingSession::Phase(p[0]);f.species=int(p[1])-1;f.length=p[2];f.attempts=p[3];
    f.bite_at=get16(p+4);f.center=get16(p+6);f.radius=get16(p+8);f.cycle_ms=get16(p+10);f.elapsed=get16(p+12);f.count=p[14];
    f.perfect=p[15]&1;f.discovered=p[15]&2;f.record=p[15]&4;f.junk=p[15]&8;f.no_bait=p[15]&16;
    if(new_ack)fish_changed=true;
    farmers[my_id].personal.capture(*state);farmers[my_id].pose.active=true;
    mode=Mode::Guest;ready=true;changed=true;teleported|=warp;
    if(first_join)message("Connected to farm");
    return true;
  }
  void service(uint32_t timestamp){
    unsigned elapsed=last_service?std::min(100u,timestamp-last_service):0;
    now=timestamp;last_service=timestamp;
    if(!active())return;
    if(mode==Mode::Hosting){
      farmers[0].personal.capture(*state);put_floor(*state);
      clock+=elapsed;
      while(clock>=500){clock-=500;++state->minute;changed=true;if(state->minute%20==0)durable_dirty=true;}
      if(state->minute>=1560){farm::sleep(*state,true);state->region=unsigned(farm::Region::Home);state->px=264;state->py=280;state->visited|=1u<<state->region;teleported=true;}
      if(day!=state->day){
        day=state->day;durable_dirty=true;for(auto&f:floors)f=Floor{};
        for(unsigned i=1;i<MaxPlayers;++i)if(farmers[i].pose.active){auto &p=farmers[i].personal;p.region=unsigned(farm::Region::Home);p.depth=0;p.x=uint16_t(248+(i-1)*16);p.y=296;p.energy=100;p.health=100;p.water=state->capacity;}
      }
      bool ticked[farm::MineFloors]{};
      Personal host;host.capture(*state);
      for(unsigned i=0;i<MaxPlayers;++i){auto&f=farmers[i];
        if(!f.pose.active||!f.personal.depth)continue;
        f.personal.install(*state);get_floor(*state);
        unsigned floor=f.personal.depth-1;
        if(!ticked[floor]){ticked[floor]=true;farm::mine_tick(*state,elapsed);put_floor(*state);f.personal.capture(*state);if(i==0)host=f.personal;}
      }
      host.install(*state);get_floor(*state);
      if(now-last_snapshot>=250){for(unsigned i=1;i<MaxPlayers;++i)if(farmers[i].pose.active)send_snapshot(i);last_snapshot=now;}
      if(now-last_pose>=100&&send){
        uint8_t data[MaxPlayers*PoseBytes];for(unsigned i=0;i<MaxPlayers;++i)encode_pose(farmers[i].pose,data+i*PoseBytes);
        if(std::memcmp(data,last_roster,sizeof data)||now-roster_at>=1000){
          for(unsigned i=1;i<MaxPlayers;++i)if(farmers[i].pose.active)send(i,Message::Pose,data,sizeof data);
          std::memcpy(last_roster,data,sizeof data);roster_at=now;
        }
        last_pose=now;
      }
    }else{
      if(last_heartbeat&&now-last_heartbeat>8000){leave("Connection lost. Single-player restored.");return;}
      for(unsigned i=0;i<count;++i){auto&p=pending[(first+i)%8];if(p.sent)continue;
        uint8_t data[PoseBytes+CommandBytes];encode_pose(p.pose,data);encode_command(p.command,data+PoseBytes);
        if(!send||!send(0,Message::Command,data,sizeof data))break;p.sent=true;
      }
      if(now-last_pose>=100&&send){
        uint8_t data[PoseBytes];encode_pose(farmers[my_id].pose,data);
        if(std::memcmp(data,last_local_pose,sizeof data)||now-pose_at>=1000){
          if(send(0,Message::Pose,data,sizeof data)){std::memcpy(last_local_pose,data,sizeof data);pose_at=now;}
        }
        last_pose=now;
      }
    }
  }
};
inline Room room;
} // namespace dewfield_net
