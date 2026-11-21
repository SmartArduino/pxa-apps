// SPDX-License-Identifier: MIT
#include "../battle_feedback.hpp"
#include "../net_wire.hpp"
#include <cassert>
#include <iostream>
using namespace ash;
int main(){
    for(int character=0;character<characterCount;++character){
        Arena a;a.reset(false,(character+1)%characterCount,character);a.phase=Phase::fight;a.ai=false;
        BattleFeedback feedback;feedback.observe(a);
        a.beginMove(1,Action::fire);feedback.observe(a);
        assert(std::string_view(feedback.moveName(a,1))==roster[character].arts[0]);
        assert(!*feedback.moveName(a,0)); // P2 must not overwrite P1's notice.
        a.beginMove(0,Action::rise);feedback.observe(a);
        assert(std::string_view(feedback.moveName(a,0))==roster[(character+1)%characterCount].arts[1]);
        assert(std::string_view(feedback.moveName(a,1))==roster[character].arts[0]);
        a.tick=70;assert(!*feedback.moveName(a,1));
        a.winner=1;a.wins[1]=2;
        assert(localRoundWon(a,1)&&localMatchWon(a,1));assert(!localRoundWon(a,0)&&!localMatchWon(a,0));
    }
    Arena host,client;host.reset(false,3,1);host.ai=false;host.phase=Phase::fight;host.tick=10;
    BattleFeedback feedback;feedback.observe(client);
    host.f[1].meter=100;host.beginMove(1,Action::super);host.f[1].age=6;host.tick=16;
    lan::Writer w(lan::Kind::state,1,2,3);lan::writeArena(w,host);w.finish();lan::Packet packet{};
    assert(lan::parse(w.view(),packet)&&lan::readArena(packet.body,client));feedback.observe(client);
    assert(std::string_view(feedback.moveName(client,1))=="月满潮");
    client.reset();feedback.observe(client);assert(!*feedback.moveName(client,1));
    const uint32_t masks[]={Down|Left,Down,Down|Right,Left,0,Right,Up|Left,Up,Up|Right};
    const char* arrows[]={"↙","↓","↘","←","","→","↖","↑","↗"};
    for(unsigned i=0;i<9;++i)assert(std::string_view(directionGlyph(masks[i]))==arrows[i]);
    assert(!*directionGlyph(Up|Down|Left|Right));
    std::cout<<"Both player perspectives, all four move names, wire snapshots, expiry, reset and nine direction states passed\n";
}
