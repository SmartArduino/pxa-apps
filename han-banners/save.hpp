#pragma once
#include <cstddef>
#include "campaign.hpp"

namespace han {

constexpr size_t saveWords=18+3*cityCount+2*(ownerCount+1)+noGear+
    3*commanderSlots+4*columnCount+6+1;
using Save=std::array<std::byte,saveWords*2>;
using SecondSave=std::array<std::byte,194>;
using LegacySave=std::array<std::byte,78>;
constexpr const char* saveKey="campaign-v3";

template<size_t N>
uint16_t checksum(const std::array<std::byte,N>& bytes,size_t count) {
    uint16_t check=0x852f;
    for (size_t i=0;i<count;++i) check=uint16_t(check*31+unsigned(bytes[i]));
    return check;
}
template<size_t N>
struct Reader {
    const std::array<std::byte,N>& bytes;
    size_t i=0;
    int get() {
        i+=2;
        return int(bytes[i-2])|(int(bytes[i-1])<<8);
    }
    Order order() {return {get()-1,get()-1};}
    Captive captive() {return {get()-1,get()-1,get()-1};}
    bool begins(int version) {return get()==0x4842 && get()==version;}
    bool finish() {
        const auto expected=checksum(bytes,i);
        return expected==get();
    }
};

inline Save encode(const Campaign& c) {
    Save bytes{};
    size_t i=0;
    auto put=[&](int n) {bytes[i++]=std::byte(n&255);bytes[i++]=std::byte((n>>8)&255);};
    auto order=[&](Order o) {put(o.from+1);put(o.to+1);};
    auto captive=[&](Captive p) {put(p.side+1);put(p.officer+1);put(p.city+1);};
    put(0x4842);put(3);put(c.player);put(c.turn);put(c.gold);put(c.grain);
    put(c.actions);put(c.won+1);put(c.commander);put(c.recruited);put(c.tours);
    for (int n:c.truce) put(n);
    put(c.chapter);put(c.objectivePaid);put(c.surveyed&65535u);put(c.surveyed>>16);
    for (const auto& city:c.cities) {
        put(city.owner|(city.fields<<4)|(city.fort<<8));
        put(city.troops);
        put(city.order|(city.market<<8));
    }
    for (auto o:c.orders) order(o);
    order(c.defense);
    for (int n:c.stock) put(n);
    for (const auto& g:c.generals) {
        put(g.xp);
        put(g.fatigue|((g.station+1)<<8));
        put(g.gear);
    }
    for (const auto& unit:c.columns) {
        order(unit.road);
        put(unit.troops);
        put((unit.leader+1)|(unit.eta<<8)|(int(unit.blockade)<<12));
    }
    captive(c.prisoner);
    captive(c.guest);
    put(checksum(bytes,i));
    return bytes;
}

template<size_t N>
bool readCommon(Reader<N>& r,Campaign& c,bool old=false) {
    c.player=r.get();c.turn=r.get();c.gold=r.get();c.grain=r.get();
    c.actions=r.get();c.won=r.get()-1;c.commander=r.get();c.recruited=r.get();
    c.tours=r.get();
    for (int& t:c.truce) {t=r.get();if (t>3) return false;}
    return c.player<3 && c.turn>=1 && c.turn<=lastTurn+1 && c.gold<=9999 && c.grain<=9999 &&
        c.actions<=4 && c.won>=-1 && c.won<=1 && c.tours<=4*lastTurn &&
        c.commander<(old?5:commanderSlots) && c.recruited<=(old?31:255) && (c.recruited&7)==7;
}
inline bool validRoad(Order o) {
    if (!o) return o.from==-1 && o.to==-1;
    return o.from<cityCount && o.to>=0 && o.to<cityCount && adjacent(o.from,o.to);
}
template<size_t N>
bool readMap(Reader<N>& r,Campaign& c,bool old=false) {
    for (auto& city:c.cities) {
        const int flags=r.get();
        city={flags&15,r.get(),(flags>>4)&15,flags>>8};
        if (!old) {
            const int civic=r.get();
            city.order=civic&255;
            city.market=civic>>8;
        }
        if (city.owner>=ownerCount || city.troops>armyLimit || city.fields<1 ||
            city.fields>4 || city.fort<1 || city.fort>4 || city.order>100 || city.market>3)
            return false;
    }
    for (auto& o:c.orders) {o=r.order();if (!validRoad(o)) return false;}
    c.defense=r.order();
    return validRoad(c.defense) && (!c.defense ||
        (c.own(c.defense.to) && !c.own(c.defense.from) && c.dispatch(c.defense.from)>=180));
}

inline bool decode(const Save& bytes,Campaign& campaign) {
    Reader<Save{}.size()> r{bytes};
    Campaign loaded;
    if (!r.begins(3) || !readCommon(r,loaded)) return false;
    loaded.chapter=r.get();
    const int reward=r.get();
    if (loaded.chapter>=chapterCount || reward>1) return false;
    loaded.objectivePaid=reward;
    loaded.surveyed=uint32_t(r.get());
    loaded.surveyed|=uint32_t(r.get())<<16;
    if (!readMap(r,loaded)) return false;
    for (int& count:loaded.stock) {count=r.get();if (count>33) return false;}
    for (auto& veteran:loaded.generals) {
        veteran.xp=r.get();
        const auto status=r.get();
        veteran.fatigue=status&255;
        veteran.station=(status>>8)-1;
        veteran.gear=Gear(r.get());
        if (veteran.xp>549 || veteran.fatigue>100 || veteran.gear>noGear ||
            (veteran.station>=0 && !loaded.own(veteran.station))) return false;
    }
    for (auto& unit:loaded.columns) {
        unit.road=r.order();
        unit.troops=r.get();
        const int flags=r.get();
        unit.leader=(flags&255)-1;
        unit.eta=(flags>>8)&15;
        unit.blockade=flags&4096;
        if (!validRoad(unit.road) || (flags>>13) ||
            (unit && (unit.troops<180 || unit.troops>armyLimit || unit.leader<0 ||
                      unit.leader>=commanderSlots || unit.eta>1 || (unit.eta && !unit.blockade))) ||
            (!unit && (unit.leader!=-1 || unit.troops || unit.eta || unit.blockade))) return false;
    }
    loaded.prisoner=r.captive();
    loaded.guest=r.captive();
    auto validCaptive=[&](Captive p,bool visiting) {
        if (!p) return p.side==-1 && p.officer==-1 && p.city==-1;
        return p.side<3 && p.side!=loaded.player && p.officer>=0 && p.officer<officerCount &&
            (visiting?p.city==-1:loaded.own(p.city));
    };
    if (!validCaptive(loaded.prisoner,false) || !validCaptive(loaded.guest,true) ||
        !loaded.unlocked(loaded.commander) || !r.finish()) return false;
    std::array<bool,cityCount> seatUsed{},targeted{};
    std::array<bool,commanderSlots> leading{};
    for (int slot=0;slot<commanderSlots;++slot) {
        const auto station=loaded.generals[slot].station;
        if (station<0) continue;
        if (seatUsed[station] || !loaded.unlocked(slot)) return false;
        seatUsed[station]=true;
    }
    for (const auto& unit:loaded.columns) if (unit) {
        if (leading[unit.leader] || targeted[unit.road.to] || !loaded.unlocked(unit.leader) ||
            loaded.generals[unit.leader].station>=0) return false;
        leading[unit.leader]=true;
        targeted[unit.road.to]=true;
    }
    campaign=loaded;
    return true;
}

inline bool migrate(const SecondSave& bytes,Campaign& campaign) {
    Reader<SecondSave{}.size()> r{bytes};
    Campaign loaded;
    if (!r.begins(2) || !readCommon(r,loaded,true) || !readMap(r,loaded,true) ||
        !loaded.unlocked(loaded.commander) || !r.finish()) return false;
    campaign=loaded;
    return true;
}

inline bool migrate(const LegacySave& bytes,Campaign& campaign) {
    constexpr size_t checksumOffset=16+10*4+4*4;
    const int stored=int(bytes[checksumOffset])|(int(bytes[checksumOffset+1])<<8);
    Reader<LegacySave{}.size()> r{bytes};
    if (stored!=checksum(bytes,checksumOffset) || !r.begins(1)) return false;
    const int side=r.get(),turn=r.get(),gold=r.get(),food=r.get(),actions=r.get(),result=r.get()-1;
    if (side>2 || turn<1 || turn>37 || gold>9999 || food>9999 || actions>2 || result<-1 || result>1)
        return false;
    Campaign loaded;
    loaded.start(side);
    loaded.turn=turn;
    loaded.gold=gold;
    loaded.grain=food;
    loaded.actions=actions;
    for (int j=0;j<10;++j) {
        const int flags=r.get(),troops=r.get(),owner=flags&15,fields=(flags>>4)&15,fort=flags>>8;
        if (owner>3 || troops>armyLimit || fields<1 || fields>4 || fort<1 || fort>4) return false;
        if (owner==side) loaded.cities[j]={side,troops,fields,fort};
    }
    loaded.makeOrders();
    campaign=loaded;
    return true;
}

}
