#include "game.hpp"

#include <algorithm>
#include <cstdlib>

namespace tide {
namespace {

int granary_capacity(const EnclaveHolding& holding) {
    return holding.buildings&building_flag(EnclaveWarehouse)?70:32;
}

int housing_capacity(const EnclaveHolding& holding) {
    return holding.buildings&building_flag(EnclaveHouses)?18:6;
}

bool has_building(const EnclaveHolding& holding,int building) {
    return holding.buildings&building_flag(building);
}

} // namespace

int Game::enclave_here() const {
    if (sea() || land_away() || encounter()) return -1;
    for (int site=0;site<EnclaveCount;++site)
        if (s.origin==EnclaveSites[site].home) return site;
    return -1;
}

int Game::enclave_nearby(WaterTest water) const {
    if (!s.helm || encounter()) return -1;
    const auto here=position();
    for (int site=0;site<EnclaveCount;++site) {
        const int home=EnclaveSites[site].home;
        if (!charted(home)) continue;
        const auto port=Anchor[home];
        if (std::abs(here.x-port.x)<=5 && std::abs(here.y-port.y)<=5 &&
            nearby_anchorage(water)==home) return site;
    }
    return -1;
}

Text Game::enclave_lease_requirement(int site) const {
    if (site!=enclave_here() || site<0) return "请先抵达村镇";
    if (s.enclaves[site].status) return "已有地契";
    if (market_held() || std::max(s.court_hold_until[Portugal],s.sea_warrant[Portugal])>s.day) return "港簿上的旧案须先了结";
    if (!understands(Portuguese)) return "签地契需要葡萄牙语";
    if (!has_royal_pass(Portugal) && (s.renown<2 || s.trust<1)) return "需声望2、信任1或王家委状";
    if (s.gold<EnclaveSites[site].fee) return "金币不足";
    return {};
}

bool Game::lease_enclave(int site) {
    const auto reason=enclave_lease_requirement(site);
    if (!reason.empty()) { notice=reason; return false; }
    earn(-EnclaveSites[site].fee);
    s.enclaves[site]={.status=EnclaveLease,.residents=4,.food=18,.timber=4,.term=2};
    notice="签下近岸地契，四名村民愿意留下";
    return true;
}

Text Game::enclave_build_requirement(int site,int building) const {
    if (site!=enclave_here() || site<0 || building<0 || building>=EnclaveBuildingCount) return "这里不能施工";
    const auto& holding=s.enclaves[site];
    const auto& order=BuildingOrders[building];
    if (!holding.status || holding.residents<2) return "要有地契和两名居民";
    if (has_building(holding,building)) return "已建成";
    if (holding.under_construction) return "等眼下工程完成";
    if (holding.food<2) return "工地需要口粮";
    if (holding.timber<order.timber || s.gold<order.coins) return "要更多木料或金币";
    return {};
}

bool Game::build_enclave(int site,int building) {
    const auto reason=enclave_build_requirement(site,building);
    if (!reason.empty()) { notice=reason; return false; }
    const auto& order=BuildingOrders[building];
    auto& holding=s.enclaves[site];
    earn(-order.coins);
    holding.timber-=order.timber;
    holding.under_construction=1+building;
    holding.building_day=std::min(99999,s.day+order.days);
    notice="木匠已经开工";
    return true;
}

int Game::enclave_export_quote(int site) const {
    if (site!=enclave_here() || site<0 || market_held() || std::max(s.court_hold_until[Portugal],s.sea_warrant[Portugal])>s.day) return 0;
    const auto& holding=s.enclaves[site];
    if (!holding.status || !has_building(holding,EnclaveWharf)) return 0;
    return holding.exports*(site==0?14:19);
}

bool Game::sell_enclave_exports(int site) {
    const int quote=enclave_export_quote(site);
    if (!quote) return false;
    earn(quote);
    s.enclaves[site].exports=0;
    notice="本地船主买走了工坊货物";
    return true;
}

bool Game::supply_enclave(int site,int action) {
    if (site!=enclave_here() || site<0) return false;
    auto& holding=s.enclaves[site];
    if (!holding.status) return false;
    int count=0;
    const int cap=granary_capacity(holding);
    if (action==DeliverEnclaveFood) {
        count=std::min({10,s.stores,cap-holding.food});
        s.stores-=count;
        holding.food+=count;
    } else if (action==DeliverEnclaveTimber) {
        count=std::min({5,s.cargo[Timber],cap-holding.timber});
        s.cargo[Timber]-=count;
        holding.timber+=count;
    } else if (action==CollectEnclaveFood && has_building(holding,EnclaveWharf)) {
        count=std::clamp(int(holding.food)-holding.residents,0,std::max(0,std::min(10,food_capacity()-s.stores)));
        s.stores+=count;
        holding.food-=count;
    } else if (action==CollectEnclaveTimber && has_building(holding,EnclaveWharf)) {
        count=std::min({5,int(holding.timber),capacity()-load()});
        s.cargo[Timber]+=count;
        holding.timber-=count;
    }
    notice=count>0?"船和仓房之间已交接":"没有可交接的粮料";
    return count>0;
}

bool Game::hire_enclave_people(int site,bool carpenter) {
    if (site!=enclave_here() || site<0) return false;
    auto& holding=s.enclaves[site];
    const int pay=carpenter?76:41;
    if (!holding.status || s.gold<pay || holding.food<5 ||
        holding.residents+2>housing_capacity(holding) || (carpenter && holding.carpenter) ||
        (holding.status==EnclaveOccupied && s.day<holding.peace_day) ||
        !understands(Portuguese)) {
        notice="要有空屋、余粮和工钱，居民才愿意来";
        return false;
    }
    earn(-pay);
    if (carpenter) {
        holding.carpenter=true;
        ++holding.residents;
        notice="木匠玛丽亚愿意接掌工坊";
    } else {
        holding.residents+=2;
        notice="两名村民自愿来做工";
    }
    return true;
}

bool Game::take_enclave_migrants(int site) {
    if (!port() || s.origin!=lisbon || site<0 || site>=EnclaveCount || s.migrant_site ||
        !s.enclaves[site].status || s.gold<48 || capacity()-load()<3) {
        notice="需在里斯本、有三处客位和四十八金安置费";
        return false;
    }
    const auto& holding=s.enclaves[site];
    if (holding.residents+3>housing_capacity(holding) || !has_building(holding,EnclaveWharf)) {
        notice="到港需有码头和三处住处";
        return false;
    }
    s.migrant_site=1+site;
    earn(-48);
    notice="三名拓居者已登船，靠港后安排住处";
    return true;
}

bool Game::land_enclave_migrants(int site) {
    if (site!=enclave_here() || site<0 || s.migrant_site!=site+1) return false;
    auto& holding=s.enclaves[site];
    if (!holding.status || !has_building(holding,EnclaveWharf) ||
        holding.residents+3>housing_capacity(holding)) {
        notice="要建成码头并留三处住处";
        return false;
    }
    holding.residents+=3;
    s.migrant_site=0;
    notice="拓居者下船，分到房屋和粮食";
    return true;
}

Text Game::enclave_siege_requirement(int site) const {
    if (site!=enclave_here() || site<0) return "请先抵达村镇";
    const auto& holding=s.enclaves[site];
    if (holding.status || holding.peace_day>s.day) return "暂不能再发起争夺";
    if (s.crew<10 || !s.guns || s.hull<35) return "需船员10、火炮1、船体35";
    if (s.stores<2*daily_rations() || s.water<2*daily_water(Fair)) return "需要两日水粮";
    return {};
}

bool Game::siege_enclave(int site) {
    return begin_land_battle(WharfAssault,site);
}

void Game::resolve_enclave_siege(int site,bool victory) {
    auto& holding=s.enclaves[site];
    s.stores-=2*daily_rations();
    s.water-=2*daily_water(Fair);
    s.hull-=victory?10:25;
    s.spirits=std::max(0,s.spirits-(victory?8:15));
    s.crew=std::max(4,s.crew-(victory?1:3));
    s.trust=std::max(0,s.trust-3);
    spend_day(2);
    auto& warrant=s.court_hold_until[Portugal];
    warrant=std::max(warrant,std::min(99999,s.day+(victory?21:14)));
    holding.peace_day=std::min(99999,s.day+(victory?14:7));
    if (victory) {
        holding.status=EnclaveOccupied;
        holding.buildings=building_flag(EnclaveWharf);
        holding.residents=2;
        holding.food=12;
        holding.timber=4;
        holding.prisoners=3;
        notice="夺下码头，三名守卫投降。港务已发出通报";
    } else notice="守卫挡住攻势，船队撤回锚地";
}

bool Game::decide_enclave_prisoners(int site,int choice) {
    if (site!=enclave_here() || site<0) return false;
    auto& holding=s.enclaves[site];
    if (!holding.prisoners) return false;
    if (choice==ReleaseEnclavePrisoners) {
        auto& warrant=s.court_hold_until[Portugal];
        if (warrant>s.day) warrant=std::max(s.day,warrant-5);
        if (holding.peace_day>s.day) holding.peace_day=std::max(s.day,holding.peace_day-6);
        ++s.trust;
        notice="守卫获释，家人来码头接人";
    } else if (choice==RansomEnclavePrisoners) {
        earn(holding.prisoners*24);
        notice="港务付了赎金，守卫获准归家";
    } else return false;
    holding.prisoners=0;
    return true;
}

void Game::wait_enclave_day() {
    if (enclave_here()<0) return;
    spend_day(1);
    notice="村镇过了一日";
}

void Game::tick_enclaves() {
    for (int i=0;i<EnclaveCount;++i) {
        auto& holding=s.enclaves[i];
        if (!holding.status) continue;
        if (holding.under_construction && s.day>=holding.building_day) {
            holding.buildings|=building_flag(holding.under_construction-1);
            holding.under_construction=0;
            holding.building_day=0;
        }
        const int cap=granary_capacity(holding), needed=(holding.residents+2)/3;
        if (has_building(holding,EnclaveGardens) && holding.residents>=2)
            holding.food=std::min(cap,holding.food+EnclaveSites[i].crop_yield);
        if (holding.food>=needed) {
            holding.food-=needed;
            holding.term=std::min(6,int(holding.term)+1);
        } else {
            holding.food=0;
            if (holding.term) --holding.term;
            else if (holding.residents>2) --holding.residents;
        }
        if (holding.status==EnclaveOccupied && !has_building(holding,EnclaveWatch) &&
            (s.day+3*i)%17==0) {
            holding.food=std::max(0,int(holding.food)-6);
            holding.exports=std::max(0,int(holding.exports)-4);
        }
        if (has_building(holding,EnclaveWorkshop) && holding.residents>=3 && holding.term) {
            const auto timber=EnclaveSites[i].timber_yield+int(holding.carpenter);
            holding.timber=std::min(cap,holding.timber+timber);
            if (s.day%2==0 && holding.exports<EnclaveWorkshopLimit)
                holding.exports=std::min(EnclaveWorkshopLimit,int(holding.exports)+timber);
        }
    }
}

} // namespace tide
