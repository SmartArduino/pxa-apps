// SPDX-License-Identifier: MIT
#include "../multiplayer.hpp"
#include "../multiplayer_wire.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
using namespace dewfield_net;
namespace {
Room host,guests[3];farm::State world,visitors[3];
struct Packet {unsigned id;Message type;std::vector<uint8_t> data;};
std::vector<Packet> outgoing;
bool host_send(unsigned id,Message type,const uint8_t*p,unsigned n){outgoing.push_back({id,type,{p,p+n}});return true;}
void drain(){
  auto packets=std::move(outgoing);outgoing.clear();
  for(auto&p:packets)assert(guests[p.id-1].receive(0,p.type,p.data.data(),unsigned(p.data.size())));
}
void connect(unsigned id){
  farm::fresh(visitors[id-1]);visitors[id-1].day=18;visitors[id-1].gold=449;
  auto&r=guests[id-1];r.bind(visitors[id-1]);r.original=visitors[id-1];r.restore=true;r.mode=Mode::Joining;r.now=1000;
  uint8_t hello[4];put32(hello,1);assert(host.receive(id,Message::Hello,hello,4));drain();
  assert(r.ready&&r.mode==Mode::Guest&&r.my_id==id&&visitors[id-1].gold==world.gold);
}
void command(unsigned id,Command c,Pose pose){
  uint8_t bytes[PoseBytes+CommandBytes];encode_pose(pose,bytes);encode_command(c,bytes+PoseBytes);
  assert(host.receive(id,Message::Command,bytes,sizeof bytes));drain();
}
}
int main(){
  // Standard RFC6455 handshake vector and chunked/invalid handshakes.
  const char request[]="GET /dewfield HTTP/1.1\r\nHost: server.example.com\r\nUpgrade: websocket\r\nConnection: keep-alive, Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n\r\n";
  char reply[256];
  for(unsigned n=0;n<sizeof(request)-1;++n)assert(wire::upgrade(reinterpret_cast<const uint8_t*>(request),n,reply,sizeof reply)==0);
  assert(wire::upgrade(reinterpret_cast<const uint8_t*>(request),sizeof(request)-1,reply,sizeof reply)==sizeof(request)-1);
  assert(std::strstr(reply,"s3pPLMBiTxaQ9kYGzzhZRbK+xOo="));
  const uint8_t bad[]={0x82,0x02,1,2};wire::WebFrame frame;assert(wire::websocket_frame(bad,sizeof bad,frame)<0);
  uint16_t port;assert(wire::address("192.168.1.20","31889",port)&&port==31889);
  for(auto ip:{"1.2.3","1..2.3","256.1.2.3","1.2.3.4.5","0.0.0.0","1.2.3.4x"})assert(!wire::address(ip,"31889",port));
  for(auto p:{"0","65536","-1","1.2","999999"})assert(!wire::address("127.0.0.1",p,port));
  // Four participants join one authoritative farm. Guests keep their own save.
  farm::fresh(world);world.intro=farm::IntroScenes;host.bind(world);host.mode=Mode::Hosting;host.ready=true;
  host.send=host_send;host.now=1000;host.farmers[0].personal.capture(world);host.farmers[0].pose.active=true;
  for(unsigned id=1;id<MaxPlayers;++id)connect(id);
  assert(host.players()==4);
  // Two farmers harvesting the same crop award exactly one item.
  int plot=farm::plot_index(14,14);world.plots[plot]={1,1,2,0};
  Pose pose;pose.active=true;pose.x=14*16+8;pose.y=14*16+8;pose.tool=unsigned(farm::Tool::Hand);
  auto before=world.produce[0];
  command(1,{1,Op::Act,14,14,0},pose);
  command(2,{1,Op::Act,14,14,0},pose);
  assert(world.produce[0]==before+1&&world.plots[plot].crop==0);
  // An already accepted sequence cannot award the crop or spend twice.
  auto gold=world.gold;command(1,{1,Op::Act,14,14,0},pose);
  assert(world.produce[0]==before+1&&world.gold==gold);
  // Invalid target coordinates / future sequences are rejected atomically.
  uint8_t bytes[PoseBytes+CommandBytes];encode_pose(pose,bytes);
  encode_command({99,Op::Act,-1,400,0},bytes+PoseBytes);
  assert(!host.receive(1,Message::Command,bytes,sizeof bytes));
  assert(world.produce[0]==before+1&&world.gold==gold);
  // Personal water/energy are not replaced by another farmer's values.
  host.farmers[1].personal.energy=72;host.farmers[2].personal.energy=31;
  assert(host.send_snapshot(1)&&host.send_snapshot(2));drain();
  assert(visitors[0].energy==72&&visitors[1].energy==31);
  // Snapshot corruption leaves a guest's current State unchanged.
  host.now+=1001;assert(host.send_snapshot(1));auto corrupt=std::move(outgoing.back());outgoing.clear();
  uint8_t stable[farm::SaveBytes],after[farm::SaveBytes];farm::encode(visitors[0],stable,sizeof stable);
  corrupt.data[12+MaxPlayers*PoseBytes+100]^=1;
  assert(!guests[0].receive(0,corrupt.type,corrupt.data.data(),unsigned(corrupt.data.size())));
  farm::encode(visitors[0],after,sizeof after);assert(!std::memcmp(stable,after,sizeof stable));
  for(auto&r:guests){r.leave();assert(r.state->day==18&&r.state->gold==449&&!r.restore);}
  assert(world.produce[0]==before+1);
  std::puts("PASS: 4 farmers, authoritative harvest race, command replay rejection, bounded protocol, RFC6455 handshake, private stats and original save restoration");
}
