#include "game.hpp"

#include <algorithm>
#include <cstdlib>

namespace tide {
namespace {

constexpr RoyalCourt MerchantCourts[]{Portugal,Castile};
constexpr Text CourtNames[]{"葡萄牙","卡斯蒂利亚"};

bool water_between(Point from,Point to,WaterTest water) {
    const int steps=std::max({1,std::abs(to.x-from.x),std::abs(to.y-from.y)});
    for (int i=1;i<=steps;++i)
        if (!water({from.x+(to.x-from.x)*i/steps,from.y+(to.y-from.y)*i/steps})) return false;
    return true;
}

} // namespace

int Game::raid_target(WaterTest water) const {
    if (!s.helm || encounter() || s.hull<20 || s.crew<6) return -1;
    const auto here=position();
    int chosen=-1, best=65;
    for (std::size_t i=0;i<s.merchants.size();++i) {
        const auto& ship=s.merchants[i];
        if (ship.destination==NoMerchantPort) continue;
        const auto point=merchant_position(i);
        if (!chart_cell(point.x/ChartCell,point.y/ChartCell)) continue;
        const int x=point.x-here.x, y=point.y-here.y, distance=x*x+y*y;
        if (distance<=1 || distance>=best || !water_between(here,point,water)) continue;
        chosen=static_cast<int>(i);
        best=distance;
    }
    return chosen;
}

Text Game::merchant_jurisdiction(int company) const {
    return company>=0 && company<int(std::size(MerchantCourts))?CourtNames[MerchantCourts[company]]:Text{};
}

bool Game::begin_merchant_raid(int company,WaterTest water) {
    if (company<0 || company!=raid_target(water)) {
        notice="商船已离开附近海域";
        return false;
    }
    const auto point=merchant_position(company);
    const auto court=MerchantCourts[company];
    start_encounter(Privateer);
    s.fishery={};
    s.enemy=31+s.merchants[company].units*2;
    s.skirmish={};
    s.skirmish.opponent=FirstMerchantOpponent+company;
    s.skirmish.court=court+1;
    s.skirmish.active=true;
    s.skirmish.rival={point.x*16+8,point.y*16+8};
    s.skirmish.heading=direction_to({point.x-s.helm_x/16,point.y-s.helm_y/16});
    s.skirmish.enemy_reload=30;
    const auto duration=s.colors==FurledColors?15:23;
    s.sea_warrant[court]=std::max(s.sea_warrant[court],std::min(99999,s.day+duration));
    s.patrol_after[court]=std::min(99999,s.day+2);
    s.trust=std::max(0,s.trust-2);
    paused=true;
    notice="商船升旗求援，港务将追查";
    return true;
}

int Game::piracy_docket_until() const {
    const auto court=port_court(s.origin);
    return court==NoCourt?0:s.sea_warrant[court];
}

bool Game::start_patrol_interception(Point position) {
    for (int port : {lisbon,madeira,ribeira,cadiz,canaries,santo_domingo}) {
        const auto court=port_court(port);
        if (s.sea_warrant[court]<=s.day || s.patrol_after[court]>s.day) continue;
        const auto anchor=Anchor[port];
        if (std::abs(anchor.x-position.x)>32 || std::abs(anchor.y-position.y)>28 ||
            conditions().weather==Squall) continue;
        s.patrol_after[court]=std::min(99999,s.day+6);
        start_encounter(Privateer);
        s.skirmish.opponent=PatrolOpponent;
        s.skirmish.court=court+1;
        s.enemy=49+s.guns*4;
        notice="巡逻船认出了你的船，正在靠近";
        return true;
    }
    return false;
}

void Game::resolve_special_skirmish() {
    const auto opponent=s.skirmish.opponent;
    if (opponent==PatrolOpponent) {
        auto& warning=s.sea_warrant[s.skirmish.court-1];
        warning=std::max(warning,std::min(99999,s.day+13));
        log_encounter(0);
        finish_encounter();
        notice="巡逻船退出交战，港务留案仍在";
        return;
    }
    auto& trader=s.merchants[opponent-FirstMerchantOpponent];
    const int amount=std::min({int(trader.units),std::max(0,capacity()-load()),5});
    const int silver=std::min(trader.gold,52);
    trader.units-=amount;
    trader.gold-=silver;
    s.cargo[trader.good]+=amount;
    earn(silver);
    log_encounter(0);
    finish_encounter();
    notice=amount?"商船交出货物。王室港口暂停交易":"商船交出银钱。王室港口暂停交易";
}

} // namespace tide
