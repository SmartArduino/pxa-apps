// SPDX-License-Identifier: MIT
#include "net_wire.hpp"
#include <cassert>
#include <iostream>
using namespace ash;
int main(){
    using namespace ash::lan;
    Arena host,client;host.reset(false,3,2);host.ai=false;
    host.phase=Phase::fight;host.f[0].meter=83;host.f[0].x=143.125f;host.f[1].x=181.25f;
    host.f[0].set(Action::super);host.f[0].age=19;host.shots[0]={true,1,12,55,151.5f,38,-2.5f,17};
    host.grabOwner=1;host.grabAge=7;host.escapeOwner=0;host.escapeAge=30;host.escapes=5;
    Writer w(Kind::state,42,91,5);writeArena(w,host);w.finish();
    assert(w.size<capacity);Packet p{};assert(parse(w.view(),p));assert(readArena(p.body,client));
    assert(client.f[0].character==3&&client.f[1].character==2&&client.f[0].age==19);
    assert(client.f[0].x==host.f[0].x&&client.f[0].meter==83&&client.shots[0].speed==-2.5f&&!client.ai);
    assert(client.grabOwner==1&&client.grabAge==7&&client.escapeOwner==0&&client.escapeAge==30&&client.escapes==5);
    for(unsigned n=0;n<w.size;++n){Packet truncated{};assert(!parse(w.view().first(n),truncated));}
    auto bad=w;bad.bytes[4]=std::byte{99};assert(!parse(bad.view(),p));
    bad=w;bad.bytes[4]=std::byte{1};assert(!parse(bad.view(),p)); // Old peers lack sequenced link reports.
    bad=w;bad.bytes[4]=std::byte{2};assert(!parse(bad.view(),p)); // Old peers settle throws immediately.
    bad=w;bad.bytes[6]=std::byte{1};assert(!parse(bad.view(),p));
    // Every payload truncation fails, leaving the previous visual state intact.
    for(unsigned n=0;n<w.size-header;++n){Reader r{w.view().subspan(header,n)};client.f[0].hp=77;
        assert(!readArena(r,client));assert(client.f[0].hp==77);}
    host.f[0].x=std::numeric_limits<float>::quiet_NaN();Writer nan(Kind::state,42,91);writeArena(nan,host);nan.finish();
    assert(parse(nan.view(),p));assert(!readArena(p.body,client));
    Inputs sender,receiver;
    const uint16_t motion[]={Down,Down|Right,Right,Right|Punch,Right,0};
    for(auto key:motion)assert(sender.push(key));assert(sender.count==6);
    // A future packet arriving first cannot eat a missing diagonal/press edge.
    assert(receiver.receive(sender.events[2]));assert(receiver.ack==0);
    for(unsigned i=0;i<sender.count;++i){assert(receiver.receive(sender.events[i]));assert(receiver.receive(sender.events[i]));}
    assert(receiver.count==6&&receiver.ack==6);sender.acknowledge(receiver.ack);assert(sender.count==0);
    Arena fight;fight.reset();fight.ai=false;fight.phase=Phase::fight;
    for(unsigned i=0;i<6;++i){fight.feed(0,receiver.step());fight.step();}
    assert(fight.lastMove==int(Action::fire));assert(receiver.held==0&&receiver.count==0);
    sender={};for(unsigned i=0;i<32;++i)assert(sender.push(i%2?0:Punch));assert(!sender.push(Punch));
    assert(newer(1,0xffffffff)&&!newer(0xffffffff,1));
    std::array<uint8_t,4> ip{};
    for(auto text:{"192.168.1.2","127.0.0.1","10.0.0.2"})assert(ipv4(text,ip));
    for(auto text:{"","1.2.3","1.2.3.4.5","256.0.0.1","1..2.3","1.2.3.-1","0.0.0.0","224.1.1.2","1.2.3.255"})assert(!ipv4(text,ip));
    std::cout<<"LAN wire: state roundtrip, truncation, NaN, reordered/duplicated motion, queue limits, IPv4 passed; snapshot="<<w.size<<" bytes\n";
}
