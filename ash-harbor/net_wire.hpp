// SPDX-License-Identifier: MIT
#pragma once
#include "combat.hpp"
#include <bit>
#include <span>
#include <string_view>

namespace ash::lan {
constexpr uint16_t port=37940;
constexpr uint32_t keys=15|Punch|Kick|Evade|Skill;
constexpr unsigned capacity=768,header=20;
enum class Kind:uint8_t {find=1,offer,join,welcome,input,state,leave,reject};
// Version includes the combat rules and visual-state schema. No native structs
// or command-buffer memory cross the wire; the host alone resolves combat.
constexpr uint8_t version=3;
struct Writer {
    std::array<std::byte,capacity> bytes{};unsigned size=0;
    void u8(unsigned n){if(size<capacity)bytes[size++]=std::byte(n);}
    void u16(unsigned n){u8(n);u8(n>>8);}
    void u32(uint32_t n){u16(n);u16(n>>16);}
    void real(float n){u32(std::bit_cast<uint32_t>(n));}
    std::span<const std::byte> view()const{return {bytes.data(),size};}
    Writer(Kind k,uint32_t room,uint32_t session,uint32_t seq=0){
        u32(0x4e4c4841);u8(version);u8(unsigned(k));u16(0);u32(room);u32(session);u32(seq);
    }
    void finish(){bytes[6]=std::byte(size);bytes[7]=std::byte(size>>8);}
};
struct Reader {
    std::span<const std::byte> b;unsigned pos=0;bool valid=true;
    unsigned u8(){if(pos>=b.size()){valid=false;return 0;}return unsigned(b[pos++]);}
    unsigned u16(){unsigned a=u8();return a|(u8()<<8);}
    uint32_t u32(){uint32_t a=u16();return a|(uint32_t(u16())<<16);}
    float real(){float f=std::bit_cast<float>(u32());if(!std::isfinite(f)||std::abs(f)>1e7f)valid=false;return f;}
    bool done()const{return valid&&pos==b.size();}
};
struct Packet {Kind kind;uint32_t room,session,seq;Reader body;};
inline bool parse(std::span<const std::byte> b,Packet& out){
    if(b.size()<header||b.size()>capacity)return false;
    Reader r{b};if(r.u32()!=0x4e4c4841||r.u8()!=version)return false;
    unsigned k=r.u8(),size=r.u16();if(k<1||k>8||size!=b.size())return false;
    out={Kind(k),r.u32(),r.u32(),r.u32(),Reader{b.subspan(header)}};return r.valid;
}
inline bool newer(uint32_t a,uint32_t b){return int32_t(a-b)>0;}
struct Input {uint32_t seq=0;uint16_t buttons=0;};
// Acknowledge only contiguous events. Retransmission never creates a second
// attack edge. A full queue stops the match instead of silently dropping keys.
struct Inputs {
    std::array<Input,32> events{};unsigned count=0;uint32_t next=1,ack=0;uint16_t held=0;
    bool push(uint32_t mask){if(mask==held)return true;if(count==events.size())return false;
        held=mask;events[count++]={next++,uint16_t(mask)};return true;}
    void acknowledge(uint32_t n){unsigned take=0;while(take<count&&!newer(events[take].seq,n))++take;
        for(unsigned i=take;i<count;++i)events[i-take]=events[i];count-=take;}
    bool receive(Input e){if(!newer(e.seq,ack))return true;
        if(e.seq!=ack+1)return true; // A later retransmission fills the gap.
        if((e.buttons&~keys)||count==events.size())return false;
        events[count++]=e;ack=e.seq;return true;}
    uint32_t step(){if(count){held=events[0].buttons;for(unsigned i=1;i<count;++i)events[i-1]=events[i];--count;}return held;}
    void release(){count=0;held=0;}
};
inline void writeArena(Writer& w,const Arena& a){
    w.u32(a.tick);w.u8(unsigned(a.phase));w.u16(a.phaseAge);w.u8(a.round);
    w.u8(a.wins[0]);w.u8(a.wins[1]);w.u16(a.time);w.u8(a.freeze);w.u8(a.flash);w.u8(a.winner+1);
    w.u8(a.lastMove);w.u16(a.lastMoveAge);w.u32(a.damageEvents);w.u32(a.blocks);
    w.u8(a.hitOwner);w.u8(unsigned(a.hitAction));w.u16(a.hit.age);w.real(a.hit.x);w.real(a.hit.y);w.u8(a.hit.block);
    w.u8(a.grabOwner+1);w.u8(a.grabAge);w.u8(a.escapeOwner+1);w.u8(a.escapeAge);w.u32(a.escapes);
    for(auto& f:a.f){w.u8(f.character);w.u8(f.face+1);w.u8(f.hp);w.u8(f.meter);w.u16(f.age);
        w.u8(unsigned(f.action));w.u16(f.held);w.real(f.x);w.real(f.y);w.real(f.vx);w.real(f.vy);
        w.real(std::fmod(f.walkDistance,80.f));w.u16(f.combo);w.u16(f.comboDamage);w.u16(f.comboTimer);}
    for(auto& s:a.shots){w.u8(s.active);w.u8(s.owner);w.u16(s.life);w.u16(s.age);w.real(s.x);w.real(s.y);w.real(s.speed);}
}
inline bool readArena(Reader& r,Arena& target){
    Arena a; a.ai=false;a.tick=r.u32();a.phase=Phase(r.u8());a.phaseAge=r.u16();a.round=r.u8();
    a.wins[0]=r.u8();a.wins[1]=r.u8();a.time=r.u16();a.freeze=r.u8();a.flash=r.u8();a.winner=int(r.u8())-1;
    a.lastMove=r.u8();a.lastMoveAge=r.u16();a.damageEvents=r.u32();a.blocks=r.u32();
    a.hitOwner=r.u8();a.hitAction=Action(r.u8());a.hit.age=r.u16();a.hit.x=r.real();a.hit.y=r.real();a.hit.block=r.u8();
    a.grabOwner=int(r.u8())-1;a.grabAge=r.u8();a.escapeOwner=int(r.u8())-1;a.escapeAge=r.u8();a.escapes=r.u32();
    if(unsigned(a.phase)>3||a.round<1||a.round>9||a.wins[0]>2||a.wins[1]>2||a.time>3600||a.winner>1||
        a.lastMove>17||a.hitOwner>1||unsigned(a.hitAction)>17||a.grabOwner>1||a.grabAge>unsigned(Arena::throwWindow)||
        a.escapeOwner>1||a.escapeAge>100)return false;
    for(auto& f:a.f){f.character=r.u8();f.face=int(r.u8())-1;f.hp=r.u8();f.meter=r.u8();f.age=r.u16();
        f.action=Action(r.u8());f.held=r.u16();f.x=r.real();f.y=r.real();f.vx=r.real();f.vy=r.real();
        f.walkDistance=r.real();f.combo=r.u16();f.comboDamage=r.u16();f.comboTimer=r.u16();
        if(f.character>=characterCount||(f.face!=1&&f.face!=-1)||f.hp>100||f.meter>100||unsigned(f.action)>17||
            (f.held&~keys)||f.x<0||f.x>320||f.y<0||f.y>512)return false;}
    for(auto& s:a.shots){s.active=r.u8();s.owner=r.u8();s.life=r.u16();s.age=r.u16();s.x=r.real();s.y=r.real();s.speed=r.real();if(s.owner>1)return false;}
    if(!r.done())return false;target=a;return true;
}
inline bool ipv4(std::string_view text,std::array<uint8_t,4>& ip){
    unsigned part=0,value=0,digits=0;
    for(char c:text){if(c=='.'){if(!digits||part>=3)return false;ip[part++]=value;value=digits=0;}
        else if(c>='0'&&c<='9'){value=value*10+c-'0';if(++digits>3||value>255)return false;}else return false;}
    if(part!=3||!digits)return false;ip[3]=value;return ip[0]>0&&ip[0]<224&&ip[3]!=255;
}
} // namespace ash::lan
