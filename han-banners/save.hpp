#pragma once
#include <cstddef>
#include "campaign.hpp"

namespace han {

constexpr size_t thirdSaveWords=18+3*oldCityCount+2*(ownerCount+1)+legacyGearCount+
    3*talentFirst+4*columnCount+6+1;
static_assert(seventhCityCount==48 && previousCityCount==56 && cityCount<=64);
constexpr size_t fourthSaveWords=thirdSaveWords+3*(expandedCityCount-oldCityCount)+1;
constexpr size_t fifthSaveWords=thirdSaveWords+3*(seventhCityCount-oldCityCount)+1;
constexpr size_t sixthSaveWords=fifthSaveWords+1;
constexpr int legacyCommanders=talentFirst+legacyTalentCount;
constexpr size_t seventhSaveWords=sixthSaveWords+2+3*legacyTalentCount;
constexpr size_t eighthSaveWords=seventhSaveWords+1+3*(previousCityCount-seventhCityCount);
constexpr size_t ninthSaveWords=eighthSaveWords+1;
constexpr size_t tenthSaveWords=ninthSaveWords+3*(newTalentFirst-legacyCommanders)+2+previousCityCount+
    newTalentFirst+1+4*(Campaign{}.prisoners.size()-1)+3*(guestCount-1);
constexpr size_t eleventhSaveWords=tenthSaveWords+4*(rulerSlot-newTalentFirst);
constexpr size_t twelfthSaveWords=eleventhSaveWords+4+noGear-legacyGearCount+commanderSlots*gearSlotCount;
constexpr size_t thirteenthSaveWords=twelfthSaveWords+3;
constexpr size_t fourteenthSaveWords=thirteenthSaveWords+5;
constexpr size_t cityPolicyWords=(previousCityCount+7)/8;
constexpr size_t fifteenthSaveWords=fourteenthSaveWords+1+cityPolicyWords;
constexpr int gearBits=4;
static_assert(noGear<(1<<gearBits) && gearSlotCount*gearBits==16);
constexpr size_t sixteenthSaveWords=fifteenthSaveWords-commanderSlots*(gearSlotCount-1);
constexpr size_t saveWords=sixteenthSaveWords+3*(cityCount-previousCityCount)-
    previousCityCount-cityPolicyWords+1;
using Save=std::array<std::byte,saveWords*2>;
using SixteenthSave=std::array<std::byte,sixteenthSaveWords*2>;
using FifteenthSave=std::array<std::byte,fifteenthSaveWords*2>;
static_assert(sizeof(Save)<=960, "Campaign records must fit the ESP private KV limit");
using FourteenthSave=std::array<std::byte,fourteenthSaveWords*2>;
using ThirteenthSave=std::array<std::byte,thirteenthSaveWords*2>;
using TwelfthSave=std::array<std::byte,twelfthSaveWords*2>;
using EleventhSave=std::array<std::byte,eleventhSaveWords*2>;
using TenthSave=std::array<std::byte,tenthSaveWords*2>;
using NinthSave=std::array<std::byte,ninthSaveWords*2>;
using EighthSave=std::array<std::byte,eighthSaveWords*2>;
using SeventhSave=std::array<std::byte,seventhSaveWords*2>;
using SixthSave=std::array<std::byte,sixthSaveWords*2>;
using FifthSave=std::array<std::byte,fifthSaveWords*2>;
using FourthSave=std::array<std::byte,fourthSaveWords*2>;
using ThirdSave=std::array<std::byte,thirdSaveWords*2>;
using SecondSave=std::array<std::byte,194>;
using LegacySave=std::array<std::byte,78>;
constexpr const char* saveKey="campaign-v17";
struct SaveSlot {
    const char* key;
    size_t size;
};
inline constexpr SaveSlot saveSlots[] = {
    {saveKey,Save{}.size()},
    {"campaign-v16",SixteenthSave{}.size()},
    {"campaign-v15",FifteenthSave{}.size()},
    {"campaign-v14",FourteenthSave{}.size()},
    {"campaign-v13",ThirteenthSave{}.size()},
    {"campaign-v12",TwelfthSave{}.size()},
    {"campaign-v11",EleventhSave{}.size()},
    {"campaign-v10",TenthSave{}.size()},
    {"campaign-v9",NinthSave{}.size()},
    {"campaign-v8",EighthSave{}.size()},
    {"campaign-v7",SeventhSave{}.size()},
    {"campaign-v6",SixthSave{}.size()},
    {"campaign-v5",FifthSave{}.size()},
    {"campaign-v4",FourthSave{}.size()},
    {"campaign-v3",ThirdSave{}.size()},
    {"campaign-v2",SecondSave{}.size()},
    {"campaign-v1",LegacySave{}.size()},
};

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
    auto veteran=[&](const Veteran& g) {
        put(g.xp);
        put(g.fatigue|((g.station+1)<<8));
        const auto legacy=std::find_if(g.kit.begin(),g.kit.end(),[](Gear item){return item<legacyGearCount;});
        put(legacy==g.kit.end()?legacyGearCount:int(*legacy));
    };
    put(0x4842);put(17);put(c.player);put(c.turn);put(c.gold);put(c.grain);
    put(c.actions);put(c.won+1);put(c.commander);put(c.recruited);put(c.tours);
    for (int n:c.truce) put(n);
    put(c.chapter);put(c.objectivePaid);
    for (int word=0;word<4;++word) put((c.surveyed>>(16*word))&65535u);
    for (const auto& city:c.cities) {
        put(city.owner|((city.fields-1)<<3)|((city.fort-1)<<5)|(city.order<<7)|(city.market<<14));
        put(city.troops|(int(city.policy)<<12));
        put(city.prisoners);
    }
    for (auto o:c.orders) order(o);
    order(c.defense);
    for (int item=0;item<legacyGearCount;++item) put(c.stock[item]);
    for (int slot=0;slot<talentFirst;++slot) veteran(c.generals[slot]);
    for (const auto& unit:c.columns) {
        order(unit.road);
        put(unit.troops);
        put((unit.leader+1)|(unit.eta<<8)|(int(unit.blockade)<<12));
    }
    captive(c.prisoners[0]);
    captive(c.guests[0]);
    put(c.regionalRewards);
    put(c.talentSeen);
    put(c.talentHired);
    for (int slot=talentFirst;slot<legacyCommanders;++slot) veteran(c.generals[slot]);
    put(int(c.difficulty));
    for (int slot=legacyCommanders;slot<newTalentFirst;++slot) veteran(c.generals[slot]);
    put(c.hireOffered);
    put(c.talentOffered);
    auto detention=[&](Detention person) {put((person.city+1)|(person.since<<8));};
    for (int slot=0;slot<newTalentFirst;++slot) detention(c.detained[slot]);
    put(c.prisoners[0].since|(c.prisoners[0].offered<<8));
    for (size_t i=1;i<c.prisoners.size();++i) {
        const auto& person=c.prisoners[i];
        captive(person);
        put(person.since|(person.offered<<8));
    }
    for (size_t i=1;i<c.guests.size();++i) captive(c.guests[i]);
    for (int slot=newTalentFirst;slot<commanderSlots;++slot) {
        veteran(c.generals[slot]);
        detention(c.detained[slot]);
    }
    for (int item=legacyGearCount;item<noGear;++item) put(c.stock[item]);
    for (const auto& person:c.generals) {
        int kit=0;
        for (int category=0;category<gearSlotCount;++category)
            kit|=int(person.kit[category])<<(gearBits*category);
        put(kit);
    }
    for (int n:c.alliance) put(n);
    for (const auto request:{c.alliedRequest,c.alliedReport.request}) {
        put(request.side+1);
        put(request.target+1);
    }
    put(int(c.alliedReport.outcome));
    put(c.marchBase+1);
    put(c.chronicleDeeds);
    put(checksum(bytes,i));
    return bytes;
}

template<size_t N>
bool readCommon(Reader<N>& r,Campaign& c,bool old=false) {
    c.player=r.get();c.turn=r.get();c.gold=r.get();c.grain=r.get();
    c.actions=r.get();c.won=r.get()-1;c.commander=r.get();c.recruited=r.get();
    c.tours=r.get();
    for (int& t:c.truce) {t=r.get();if (t>truceSeasons) return false;}
    return c.player<3 && c.turn>=1 && c.turn<=lastTurn+1 && c.gold<=9999 && c.grain<=9999 &&
        c.actions<=4 && c.won>=-1 && c.won<=1 && c.tours<=4*lastTurn &&
        c.commander<(old?5:commanderSlots) && c.recruited<=(old?31:255) && (c.recruited&7)==7;
}
inline bool validRouteEndpoints(Order o) {
    if (!o) return o.from==-1 && o.to==-1;
    return o.from<cityCount && o.to>=0 && o.to<cityCount && o.from!=o.to;
}
inline bool validRoad(Order o) {
    return validRouteEndpoints(o) && (!o || adjacent(o.from,o.to));
}
template<bool Compact=false,size_t N>
bool readMap(Reader<N>& r,Campaign& c,int count,bool old=false) {
    for (int i=0;i<count;++i) {
        auto& city=c.cities[i];
        const int flags=r.get();
        const int troops=r.get();
        if constexpr (Compact) {
            const int policy=troops>>12;
            city={flags&7,troops&4095,((flags>>3)&3)+1,((flags>>5)&3)+1,
                  flags>>14,(flags>>7)&127,r.get(),CityPolicy(policy)};
            if (city.prisoners>armyLimit || policy>int(CityPolicy::garrison) ||
                (policy && city.owner!=c.player)) return false;
        } else {
            city={flags&15,troops,(flags>>4)&15,flags>>8};
            if (!old) {
                const int civic=r.get();
                city.order=civic&255;
                city.market=civic>>8;
            }
        }
        if (city.owner>=ownerCount || city.troops>armyLimit || city.fields<1 ||
            city.fields>4 || city.fort<1 || city.fort>4 || city.order>100 || city.market>3)
            return false;
    }
    for (int i=count;i<cityCount;++i) c.cities[i]=Campaign::initialCity(i,c.chapter);
    for (auto& o:c.orders) {o=r.order();if (!validRoad(o)) return false;}
    c.defense=r.order();
    return validRoad(c.defense) && (!c.defense ||
        (c.own(c.defense.to) && !c.own(c.defense.from) && c.dispatch(c.defense.from)>=180));
}

template<int Version,int Count,size_t N>
bool decodeCampaign(const std::array<std::byte,N>& bytes,Campaign& campaign) {
    Reader<N> r{bytes};
    Campaign loaded;
    if (!r.begins(Version) || !readCommon(r,loaded)) return false;
    loaded.chapter=r.get();
    const int reward=r.get();
    if (loaded.chapter>=chapterCount || reward>1) return false;
    loaded.objectivePaid=reward;
    loaded.surveyed=uint64_t(r.get());
    loaded.surveyed|=uint64_t(r.get())<<16;
    if constexpr (Version>=4) loaded.surveyed|=uint64_t(r.get())<<32;
    if constexpr (Version>=8) loaded.surveyed|=uint64_t(r.get())<<48;
    if constexpr (Count<64) if (loaded.surveyed>>Count) return false;
    if (!readMap<(Version>=17)>(r,loaded,Count)) return false;
    if constexpr (Version<12) if (loaded.commander==rulerSlot) return false;
    constexpr int stockLimit=Version>=12?24+commanderSlots:33;
    for (int i=0;i<legacyGearCount;++i) {
        loaded.stock[i]=r.get();
        if (loaded.stock[i]>stockLimit) return false;
    }
    auto readVeteran=[&](Veteran& veteran) {
        veteran.xp=r.get();
        const auto status=r.get();
        veteran.fatigue=status&255;
        veteran.station=(status>>8)-1;
        const int item=r.get();
        if (item<legacyGearCount) veteran.kit[gearSpecs[item].slot]=Gear(item);
        return veteran.xp<=549 && veteran.fatigue<=100 && item<=legacyGearCount &&
            (veteran.station<0 || loaded.own(veteran.station));
    };
    for (int slot=0;slot<talentFirst;++slot)
        if (!readVeteran(loaded.generals[slot])) return false;
    for (auto& unit:loaded.columns) {
        unit.road=r.order();
        unit.troops=r.get();
        const int flags=r.get();
        unit.leader=(flags&255)-1;
        unit.eta=(flags>>8)&15;
        unit.blockade=flags&4096;
        if (!(Version>=13?validRouteEndpoints(unit.road):validRoad(unit.road)) || (flags>>13) ||
            (unit && (unit.troops<180 || unit.troops>armyLimit || unit.leader<0 ||
                      unit.leader>=(Version>=12?commanderSlots:rulerSlot) || unit.eta>1 || (unit.eta && !unit.blockade))) ||
            (!unit && (unit.leader!=-1 || unit.troops || unit.eta || unit.blockade))) return false;
    }
    loaded.prisoners[0]=r.captive();
    loaded.guests[0]=r.captive();
    if constexpr (Version>=6) {
        loaded.regionalRewards=r.get();
        if (loaded.regionalRewards>>regionalGoalCount) return false;
    }
    if constexpr (Version>=7) {
        loaded.talentSeen=r.get();
        loaded.talentHired=r.get();
        constexpr int limit=Version>=11?talentCount:Version>=10?previousTalentCount:legacyTalentCount;
        if ((loaded.talentSeen>>limit) ||
            (loaded.talentHired&~loaded.talentSeen)) return false;
        for (int slot=talentFirst;slot<legacyCommanders;++slot)
            if (!readVeteran(loaded.generals[slot])) return false;
    }
    if constexpr (Version>=9) {
        const int difficulty=r.get();
        if (difficulty>int(Difficulty::veteran)) return false;
        loaded.difficulty=Difficulty(difficulty);
    }
    if constexpr (Version>=10) {
        for (int slot=legacyCommanders;slot<newTalentFirst;++slot)
            if (!readVeteran(loaded.generals[slot])) return false;
        loaded.hireOffered=r.get();
        loaded.talentOffered=r.get();
        if ((loaded.hireOffered&~0xf8u) || (loaded.talentOffered&~loaded.talentSeen)) return false;
        if constexpr (Version<17)
            for (int i=0;i<Count;++i) {
                auto& city=loaded.cities[i];
                city.prisoners=r.get();
                if (city.prisoners>armyLimit) return false;
            }
        auto readDetention=[&](Detention& person) {
            const int status=r.get();
            person={(status&255)-1,status>>8};
            return person.city<cityCount && (person? !loaded.own(person.city) &&
                person.since>0 && person.since<=loaded.turn:!person.since);
        };
        for (int slot=0;slot<newTalentFirst;++slot)
            if (!readDetention(loaded.detained[slot])) return false;
        for (size_t i=0;i<loaded.prisoners.size();++i) {
            auto& person=loaded.prisoners[i];
            if (i) person=r.captive();
            const int status=r.get();
            person.since=status&255;
            person.offered=status>>8;
        }
        for (size_t i=1;i<loaded.guests.size();++i) loaded.guests[i]=r.captive();
        if constexpr (Version>=11) {
            for (int slot=newTalentFirst;slot<(Version>=12?commanderSlots:rulerSlot);++slot)
                if (!readVeteran(loaded.generals[slot]) || !readDetention(loaded.detained[slot])) return false;
        }
    } else if (loaded.prisoners[0]) {
        loaded.prisoners[0].since=loaded.turn;
    }
    if constexpr (Version>=12) {
        for (int item=legacyGearCount;item<noGear;++item) {
            loaded.stock[item]=r.get();
            if (loaded.stock[item]>stockLimit) return false;
        }
        for (auto& person:loaded.generals) {
            const int kit=Version>=16?r.get():0;
            for (int category=0;category<gearSlotCount;++category) {
                const int item=Version>=16?(kit>>(gearBits*category))&((1<<gearBits)-1):r.get();
                if (item>noGear || (item!=noGear && gearSpecs[item].slot!=category)) return false;
                person.kit[category]=Gear(item);
            }
        }
    } else if (loaded.player==0) {
        loaded.generals[rulerSlot].kit[armsSlot]=twinJian;
    }
    if constexpr (Version>=13) {
        for (int side=0;side<3;++side) {
            const int duration=r.get();
            if (duration>allianceSeasons || (duration && (side==loaded.player || loaded.truce[side]))) return false;
            loaded.alliance[side]=duration;
        }
    }
    if constexpr (Version>=14) {
        loaded.alliedRequest={r.get()-1,r.get()-1};
        auto& report=loaded.alliedReport;
        report.request={r.get()-1,r.get()-1};
        const int outcome=r.get();
        auto validRequest=[&](AlliedRequest request) {
            return request?request.side<3 && request.side!=loaded.player &&
                request.target>=0 && request.target<cityCount:request.target==-1;
        };
        if (!validRequest(loaded.alliedRequest) || !validRequest(report.request) ||
            outcome>int(AlliedOutcome::captured) || bool(outcome)!=bool(report.request)) return false;
        report.outcome=AlliedOutcome(outcome);
        if (loaded.alliedRequest) {
            const auto request=loaded.alliedRequest;
            const auto order=loaded.orders[request.side];
            if (!loaded.allied(request.side) || report.request || (order && order.to!=request.target))
                return false;
        }
    }
    if constexpr (Version>=15) {
        loaded.marchBase=r.get()-1;
        if (loaded.marchBase>=cityCount) return false;
        if constexpr (Version<17) for (int first=0;first<Count;first+=8) {
            const int policies=r.get();
            for (int i=0;i<8;++i) {
                const int policy=(policies>>(i*2))&3;
                const int city=first+i;
                if (policy>int(CityPolicy::garrison) || (policy && !loaded.own(city))) return false;
                if (city<Count) loaded.cities[city].policy=CityPolicy(policy);
            }
        }
    }
    if constexpr (Version>=17) {
        loaded.chronicleDeeds=r.get();
        if (loaded.chronicleDeeds&~deedMask) return false;
    }
    bool found[3][officerCount]{};
    auto validCaptive=[&](const Captive& p,bool visiting) {
        if (!p) return p.side==-1 && p.officer==-1 && p.city==-1 && !p.since && !p.offered;
        if (p.side>=3 || p.side==loaded.player || p.officer<0 || p.officer>=officerCount ||
            found[p.side][p.officer]) return false;
        found[p.side][p.officer]=true;
        if (visiting) return p.city==-1 && !p.since && !p.offered;
        return loaded.own(p.city) && p.since>0 && p.since<=loaded.turn &&
            (!p.offered || (p.offered>=p.since && p.offered<=loaded.turn));
    };
    for (const auto& p:loaded.prisoners) if (!validCaptive(p,false)) return false;
    for (const auto& p:loaded.guests) if (!validCaptive(p,true)) return false;
    if (!loaded.unlocked(loaded.commander) || r.i+2!=N || !r.finish()) return false;
    std::array<bool,cityCount> seatUsed{},targeted{};
    std::array<bool,commanderSlots> leading{};
    for (int slot=0;slot<commanderSlots;++slot) {
        const auto station=loaded.generals[slot].station;
        if (loaded.held(slot) && (!loaded.unlocked(slot) || station>=0)) return false;
        if (station<0) continue;
        if (seatUsed[station] || !loaded.unlocked(slot)) return false;
        seatUsed[station]=true;
    }
    for (const auto& unit:loaded.columns) if (unit) {
        if (leading[unit.leader] || targeted[unit.road.to] || !loaded.unlocked(unit.leader) ||
            loaded.generals[unit.leader].station>=0 || loaded.held(unit.leader)) return false;
        leading[unit.leader]=true;
        targeted[unit.road.to]=true;
    }
    campaign=loaded;
    return true;
}
inline bool decode(const Save& bytes,Campaign& campaign) {
    return decodeCampaign<17,cityCount>(bytes,campaign);
}
inline bool migrate(const SixteenthSave& bytes,Campaign& campaign) {
    return decodeCampaign<16,previousCityCount>(bytes,campaign);
}
inline bool migrate(const FifteenthSave& bytes,Campaign& campaign) {
    return decodeCampaign<15,previousCityCount>(bytes,campaign);
}
inline bool migrate(const FourteenthSave& bytes,Campaign& campaign) {
    return decodeCampaign<14,previousCityCount>(bytes,campaign);
}
inline bool migrate(const ThirteenthSave& bytes,Campaign& campaign) {
    return decodeCampaign<13,previousCityCount>(bytes,campaign);
}
inline bool migrate(const TwelfthSave& bytes,Campaign& campaign) {
    return decodeCampaign<12,previousCityCount>(bytes,campaign);
}
inline bool migrate(const EleventhSave& bytes,Campaign& campaign) {
    return decodeCampaign<11,previousCityCount>(bytes,campaign);
}
inline bool migrate(const TenthSave& bytes,Campaign& campaign) {
    return decodeCampaign<10,previousCityCount>(bytes,campaign);
}
inline bool migrate(const NinthSave& bytes,Campaign& campaign) {
    return decodeCampaign<9,previousCityCount>(bytes,campaign);
}
inline bool migrate(const EighthSave& bytes,Campaign& campaign) {
    return decodeCampaign<8,previousCityCount>(bytes,campaign);
}
inline bool migrate(const SeventhSave& bytes,Campaign& campaign) {
    return decodeCampaign<7,seventhCityCount>(bytes,campaign);
}
inline bool migrate(const SixthSave& bytes,Campaign& campaign) {
    return decodeCampaign<6,seventhCityCount>(bytes,campaign);
}
inline bool migrate(const FifthSave& bytes,Campaign& campaign) {
    return decodeCampaign<5,seventhCityCount>(bytes,campaign);
}
inline bool migrate(const FourthSave& bytes,Campaign& campaign) {
    return decodeCampaign<4,expandedCityCount>(bytes,campaign);
}
inline bool migrate(const ThirdSave& bytes,Campaign& campaign) {
    return decodeCampaign<3,oldCityCount>(bytes,campaign);
}

inline bool migrate(const SecondSave& bytes,Campaign& campaign) {
    Reader<SecondSave{}.size()> r{bytes};
    Campaign loaded;
    if (!r.begins(2) || !readCommon(r,loaded,true) || !readMap(r,loaded,oldCityCount,true) ||
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

template<size_t N>
bool restoreSave(const std::array<std::byte,N>& data,size_t size,Campaign& campaign) {
    if (size>N) return false;
    auto older=[&](auto previous) {
        std::copy_n(data.begin(),previous.size(),previous.begin());
        return migrate(previous,campaign);
    };
    switch (size) {
        case Save{}.size(): {
            Save current;
            std::copy_n(data.begin(),current.size(),current.begin());
            return decode(current,campaign);
        }
        case SixteenthSave{}.size():return older(SixteenthSave{});
        case FifteenthSave{}.size():return older(FifteenthSave{});
        case FourteenthSave{}.size():return older(FourteenthSave{});
        case ThirteenthSave{}.size():return older(ThirteenthSave{});
        case TwelfthSave{}.size():return older(TwelfthSave{});
        case EleventhSave{}.size():return older(EleventhSave{});
        case TenthSave{}.size():return older(TenthSave{});
        case NinthSave{}.size():return older(NinthSave{});
        case EighthSave{}.size():return older(EighthSave{});
        case SeventhSave{}.size():return older(SeventhSave{});
        case SixthSave{}.size():return older(SixthSave{});
        case FifthSave{}.size():return older(FifthSave{});
        case FourthSave{}.size():return older(FourthSave{});
        case ThirdSave{}.size():return older(ThirdSave{});
        case SecondSave{}.size():return older(SecondSave{});
        case LegacySave{}.size():return older(LegacySave{});
        default:return false;
    }
}

}
