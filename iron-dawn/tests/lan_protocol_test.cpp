// SPDX-License-Identifier: MIT
#include "lan_protocol.hpp"
#include <cassert>

using namespace iron;

int main(){
 assert(lan::valid_address("192.168.1.20:31888"));
 assert(lan::valid_address("127.0.0.1:65535"));
 assert(!lan::valid_address("256.1.2.3:8899"));
 assert(!lan::valid_address("01.1.2.3:8877"));
 assert(!lan::valid_address("1.1.2.3:0"));
 assert(!lan::valid_address("1.1.2.3:65536"));
 assert(!lan::valid_address("192.168.1.20:8888/path"));
 std::array<std::byte,lan::max_payload> bytes{};
 lan::Header header{};
 lan::write_header(bytes,lan::Message::hello,0);
 assert(lan::read_header(bytes,header)&&header.type==lan::Message::hello&&header.size==0);
 bytes[2]=std::byte(0);
 assert(!lan::read_header(bytes,header));
 lan::write_header(bytes,lan::Message::state,lan::max_payload+1);
 assert(!lan::read_header(bytes,header));

 World host,guest,incoming;
 for(unsigned map=0;map<defense_map_count;++map){
  for(auto faction:{Faction::northern,Faction::gray_tower}){
   host.init_defense(5,faction,DefenseMap(map));
   host.select_all();
   host.extra.damage=1;
   int chosen=-1;
   for(int i=0;i<MAX;++i)if(host.e[i].selected){chosen=i;break;}
   assert(chosen>=0);
   host.select_one(chosen);
   size_t size=lan::encode_snapshot(bytes,host);
   assert(size>0&&size<6000);
   assert(lan::decode_snapshot(std::span{bytes}.first(size),guest));
   assert(guest.selected()==0&&guest.faction()==faction&&guest.defense_map()==DefenseMap(map));
   for(int y=0;y<host.side;++y)for(int x=0;x<host.side;++x){
    int c=y*N+x;
    assert(guest.terrain[c]==host.terrain[c]&&guest.fog[c]==host.fog[c]&&guest.ore[c]==host.ore[c]);
   }
   assert(guest.extra.damage==1);
   assert(!lan::decode_snapshot(std::span{bytes}.first(size-1),incoming));
   assert(!lan::decode_snapshot(std::span{bytes}.first(size+1),incoming));

   int teammate=-1;
   for(int i=chosen+1;i<MAX;++i)if(deployed(guest.e[i])&&guest.e[i].team==0&&land_combat(guest.e[i].kind)){teammate=i;break;}
   assert(teammate>=0);
   guest.select_one(teammate);
   assert(guest.assign_squad(1));
   auto move=lan::selected_order(guest,host.e[teammate].p,-1);
   size=lan::encode_order(bytes,move);
   lan::write_header(std::span{bytes}.last(lan::header_size),lan::Message::order,size);
   assert(lan::read_header(std::span{bytes}.last(lan::header_size),header));
   lan::Order decoded;
   assert(lan::decode_order(std::span{bytes}.first(size),decoded));
   assert(lan::apply_order(host,decoded));
   assert(host.e[teammate].moving&&host.e[chosen].selected&&!host.e[teammate].selected);
   auto stop=lan::selected_order(guest,{},-1,true);
   assert(lan::apply_order(host,stop)&&!host.e[teammate].moving);
   ++host.e[teammate].cargo;
   assert(!lan::apply_order(host,decoded));
   --host.e[teammate].cargo;

   int foe=host.add(rifle,1,{host.e[teammate].p.x+2,host.e[teammate].p.y});
   assert(foe>=0);
   host.reveal();
   size=lan::encode_snapshot(bytes,host);
   assert(lan::decode_snapshot(std::span{bytes}.first(size),incoming));
   lan::merge_snapshot(guest,incoming);
   assert(guest.e[teammate].selected&&guest.squads[teammate]==1);
   auto attack=lan::selected_order(guest,guest.e[foe].p,foe);
   assert(lan::apply_order(host,attack)&&host.e[teammate].target==foe);
   host.fog[cell(host.e[foe].p)]=0;
   assert(!lan::apply_order(host,attack));

   for(int t=0;t<2400&&!host.defeated;++t){
    host.tick(.05f);
    if(t%5)continue;
    size=lan::encode_snapshot(bytes,host);
    assert(size&&lan::decode_snapshot(std::span{bytes}.first(size),incoming));
    assert(incoming.wave==host.wave&&incoming.credits==host.credits&&incoming.time==host.time);
   }
   assert(host.wave>0);
  }
 }
}
