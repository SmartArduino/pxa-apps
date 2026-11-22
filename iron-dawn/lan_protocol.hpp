// SPDX-License-Identifier: MIT
#pragma once
#include "model.hpp"
#include <bit>
#include <cstddef>
#include <span>
#include <string_view>
#include <type_traits>

namespace iron::lan {
constexpr size_t max_payload=24000,header_size=6;
enum class Message : uint8_t {hello=1,state,order,ping};
struct Header {Message type;size_t size;};
constexpr unsigned wire_version=1;

inline void write_header(std::span<std::byte> out,Message type,size_t size){
 out[0]=std::byte{'I'};out[1]=std::byte{'D'};out[2]=std::byte(wire_version);
 out[3]=std::byte(type);out[4]=std::byte(size&255);out[5]=std::byte(size>>8);
}
inline bool read_header(std::span<const std::byte> in,Header& header){
 if(in.size()<header_size||in[0]!=std::byte{'I'}||in[1]!=std::byte{'D'}||
    in[2]!=std::byte(wire_version))return false;
 auto kind=std::to_integer<unsigned>(in[3]);
 if(kind<unsigned(Message::hello)||kind>unsigned(Message::ping))return false;
 size_t size=std::to_integer<unsigned>(in[4])+(std::to_integer<unsigned>(in[5])<<8);
 const auto type=Message(kind);
 if(size>max_payload||((type==Message::hello||type==Message::ping)&&size)||
    (type==Message::order&&(size<17||size>14+3*MAX)))return false;
 header={type,size};
 return true;
}

inline bool valid_address(std::string_view address){
 for(int piece=0;piece<5;++piece){
  size_t end=piece==4?address.size():address.find(piece==3?':':'.');
  if(end==0||end>(piece==4?5u:3u)||(end>1&&address.front()=='0'))return false;
  unsigned value=0;
  for(char ch:address.substr(0,end)){
   if(ch<'0'||ch>'9')return false;
   value=value*10+unsigned(ch-'0');
  }
  if(value>(piece==4?65535u:255u)||((piece==0||piece==4)&&!value))return false;
  if(piece<4)address.remove_prefix(end+1);
 }
 return true;
}

template<bool Reading> class Archive {
 using Byte=std::conditional_t<Reading,const std::byte,std::byte>;
 std::span<Byte> bytes;
 size_t offset=0;
public:
 static constexpr bool reading=Reading;
 bool ok=true;
 explicit Archive(std::span<Byte> buffer):bytes(buffer){}
 size_t size()const{return ok?offset:0;}
 bool done()const{return ok&&offset==bytes.size();}
 template<class T> void number(T& value){
  using Value=std::remove_cv_t<T>;
  using Bits=std::conditional_t<sizeof(Value)==1,uint8_t,std::conditional_t<sizeof(Value)==2,uint16_t,uint32_t>>;
  static_assert(sizeof(Value)==sizeof(Bits)&&!std::is_same_v<Value,bool>);
  if(!ok||sizeof(Value)>bytes.size()-offset){ok=false;return;}
  if constexpr(Reading){
   uint32_t bits=0;
   for(size_t i=0;i<sizeof(Value);++i)bits|=std::to_integer<uint32_t>(bytes[offset+i])<<(8*i);
   value=std::bit_cast<Value>(Bits(bits));
  }else{
   uint32_t bits=std::bit_cast<Bits>(value);
   for(size_t i=0;i<sizeof(Value);++i)bytes[offset+i]=std::byte((bits>>(8*i))&255);
  }
  offset+=sizeof(Value);
 }
 template<class T> void bounded(T& value,std::remove_cv_t<T> lo,std::remove_cv_t<T> hi){
  number(value);
  if constexpr(Reading)ok=ok&&value>=lo&&value<=hi;
 }
 template<class T> void flag(T& value){
  uint8_t encoded=value?1:0;
  bounded(encoded,0,1);
  if constexpr(Reading)value=encoded==1;
 }
 template<class V> void position(V& point,float lo,float hi){
  bounded(point.x,lo,hi);bounded(point.y,lo,hi);
 }
};

template<class A,class Grid> void grid(A& a,int side,Grid value){
 // Runs span the active square only. Inactive cells never enter the frame.
 const int count=side*side;
 for(int i=0;i<count&&a.ok;){
  uint8_t sample=0,run=1;
  if constexpr(!A::reading){
   sample=value(i,0);
   while(i+run<count&&run<255&&value(i+run,0)==sample)++run;
  }
  a.number(sample);a.number(run);
  if constexpr(A::reading){
   if(!run||run>count-i){a.ok=false;return;}
   for(int j=0;j<run&&a.ok;++j)value(i+j,sample);
  }
  i+=run;
 }
}

template<class A,class W> void snapshot(A& a,W& w,uint8_t cue=0){
 a.bounded(cue,0,13);
 if constexpr(A::reading)w.sound=cue;
 a.bounded(w.extra.mode,1,gray_tower_defense);
 a.bounded(w.extra.map_rules,1,defense_map_count);
 if(!a.ok)return;
 const int side=w.arena().side;
 if constexpr(A::reading)w.side=side;
 a.number(w.extra.seed);
 a.bounded(w.earned,0,100000000);a.bounded(w.credits,0,10000000);a.bounded(w.power_net,-10000,10000);
 a.bounded(w.wave,0,1000000);a.bounded(w.extra.cleared,0,unsigned(w.wave));
 a.bounded(w.time,0,10000000);a.bounded(w.next_wave,0,10000000);
 a.bounded(w.strike_cd,0,45);
 a.bounded(w.extra.damage,0,2);a.bounded(w.extra.armor,0,2);
 a.bounded(w.extra.research,0,2);a.bounded(w.extra.research_left,0,24);
 a.flag(w.defeated);a.flag(w.won);
 grid(a,side,[&](int i,uint8_t sample){
  int c=(i/side)*N+i%side;
  if constexpr(A::reading){
   a.ok=sample<64&&(sample&15)<=railbed;
   w.terrain[c]=sample&15;w.fog[c]=(sample>>4)&1;w.seen[c]=(sample>>5)&1;
  }
  return uint8_t(w.terrain[c]|(w.fog[c]?16:0)|(w.seen[c]?32:0));
 });
 grid(a,side,[&](int i,uint8_t sample){
  int c=(i/side)*N+i%side;
  if constexpr(A::reading)w.ore[c]=sample;
  return w.ore[c];
 });
 for(auto& unit:w.e){
  uint8_t code=unit.hp>0?uint8_t(unit.kind+1):0;
  a.bounded(code,0,kind_count);
  if(!a.ok)return;
  if constexpr(A::reading){unit={};if(code)unit.kind=code-1;}
  if(!code)continue;
  a.bounded(unit.team,0,2);a.number(unit.cargo);a.number(unit.kills);
  a.bounded(unit.hp,.0000001f,100000);
  a.position(unit.p,0,float(side));a.position(unit.goal,-4,float(side+4));
  a.bounded(unit.dir,-16,16);a.bounded(unit.flash,0,100);
  a.bounded(unit.progress,0,1000);a.bounded(unit.cool,0,1000);
  a.bounded(unit.target,last_raid_order,MAX-1);a.flag(unit.moving);
 }
 for(auto& shot:w.shots){
  a.flag(shot.alive);
  if(!shot.alive)continue;
  a.bounded(shot.kind,0,kind_count-1);a.number(shot.team);a.number(shot.target);
  a.position(shot.p,-4,float(side+4));a.position(shot.from,-4,float(side+4));a.position(shot.dest,-4,float(side+4));
  a.bounded(shot.t,0,100);a.bounded(shot.duration,.0000001f,100);
 }
 for(auto& blast:w.blasts){
  a.flag(blast.alive);
  if(!blast.alive)continue;
  a.bounded(blast.type,0,6);a.position(blast.p,-4,float(side+4));a.bounded(blast.age,0,100);
 }
 a.bounded(w.job_count,0,int(w.jobs.size()));
 if(!a.ok)return;
 for(int i=0;i<w.job_count;++i){
  auto& job=w.jobs[i];
  a.bounded(job.kind,0,kind_count-1);a.bounded(job.producer,0,MAX);
  a.bounded(job.total,.0000001f,1000);a.bounded(job.left,0,job.total);
 }
 a.bounded(w.construction.kind,0,kind_count-1);
 if(w.construction.kind){
  a.bounded(w.construction.total,.0000001f,1000);
  a.bounded(w.construction.left,0,w.construction.total);
 }
}

inline size_t encode_snapshot(std::span<std::byte> bytes,const World& w,uint8_t cue=0){
 Archive<false> a(bytes);snapshot(a,w,cue);return a.size();
}
inline bool decode_snapshot(std::span<const std::byte> bytes,World& w){
 Archive<true> a(bytes);snapshot(a,w);return a.done();
}

inline bool same_unit(const Entity& a,const Entity& b){
 return deployed(a)&&deployed(b)&&a.team==b.team&&a.kind==b.kind&&
        (!combat_kind(a.kind)||a.cargo==b.cargo);
}
inline void merge_snapshot(World& local,World& incoming){
 for(int i=0;i<MAX;++i){
  bool keep=same_unit(local.e[i],incoming.e[i]);
  incoming.e[i].selected=keep&&local.e[i].selected;
  incoming.squads[i]=keep?local.squads[i]:0;
 }
 // Guest feedback belongs to its own selection and commands.
 std::copy_n(local.notice,sizeof local.notice,incoming.notice);
 incoming.notice_time=local.notice_time;
 local=incoming;
}

enum class Intent:uint8_t {move,attack,stop};
struct Order {
 Intent intent=Intent::move;
 Vec point{};
 int16_t target=-1;
 uint8_t target_kind=0,target_token=0;
 uint8_t count=0;
 struct Unit {uint8_t slot,kind,token;};
 std::array<Unit,MAX> units{};
};

inline Order selected_order(const World& w,Vec point,int target,bool stop=false){
 Order order;
 order.intent=stop?Intent::stop:target>=0?Intent::attack:Intent::move;
 order.point=point;order.target=int16_t(target);
 if(target>=0){order.target_kind=w.e[target].kind;order.target_token=w.e[target].cargo;}
 for(int i=0;i<MAX;++i){
  const auto& unit=w.e[i];
  if(deployed(unit)&&unit.selected&&unit.team==0&&unit_kind(unit.kind))
   order.units[order.count++]={uint8_t(i),unit.kind,unit.cargo};
 }
 return order;
}
template<class A,class T> void order_fields(A& a,T& order){
 uint8_t intent=uint8_t(order.intent);
 a.bounded(intent,0,uint8_t(Intent::stop));
 if constexpr(A::reading)order.intent=Intent(intent);
 a.position(order.point,0,float(N));
 a.bounded(order.target,-1,MAX-1);
 a.bounded(order.target_kind,0,kind_count-1);a.number(order.target_token);
 a.bounded(order.count,1,MAX);
 if(!a.ok)return;
 for(unsigned i=0;i<order.count;++i){
  auto& unit=order.units[i];
  a.bounded(unit.slot,0,MAX-1);a.bounded(unit.kind,0,kind_count-1);a.number(unit.token);
 }
}
inline size_t encode_order(std::span<std::byte> bytes,const Order& order){
 if(!order.count)return 0;
 Archive<false> a(bytes);order_fields(a,order);return a.size();
}
inline bool decode_order(std::span<const std::byte> bytes,Order& order){
 Archive<true> a(bytes);order_fields(a,order);return a.done();
}
inline bool apply_order(World& w,const Order& order){
 if(w.won||w.defeated||order.point.x>=w.side||order.point.y>=w.side)return false;
 if(order.intent!=Intent::stop&&(order.point.x<1||order.point.y<1||
    order.point.x>=w.side-1||order.point.y>=w.side-1))return false;
 if(order.intent==Intent::attack){
  if(order.target<0)return false;
  const auto& victim=w.e[order.target];
  if(!deployed(victim)||victim.team!=1||!w.fog[cell(victim.p)]||
     victim.kind!=order.target_kind||(combat_kind(victim.kind)&&victim.cargo!=order.target_token))return false;
 }else if(order.target!=-1)return false;
 std::array<bool,MAX> selected;
 for(int i=0;i<MAX;++i){selected[i]=w.e[i].selected;w.e[i].selected=false;}
 bool any=false;
 for(unsigned i=0;i<order.count;++i){
  const auto& ref=order.units[i];auto& unit=w.e[ref.slot];
  if(deployed(unit)&&unit.team==0&&unit_kind(unit.kind)&&unit.kind==ref.kind&&
     (!combat_kind(unit.kind)||unit.cargo==ref.token)){
   unit.selected=true;
   any=true;
  }
 }
 if(any){
  if(order.intent==Intent::stop)w.stop_selected();
  else w.order(order.intent==Intent::attack?w.e[order.target].p:order.point,order.target);
 }
 for(int i=0;i<MAX;++i)w.e[i].selected=selected[i];
 return any;
}
}
