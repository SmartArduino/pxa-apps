#include "game.hpp"

#include <algorithm>

namespace tide {
namespace {

constexpr Point IberiaNW=chart_point(-16,44.5f), IberiaSE=chart_point(-6.5f,35);
constexpr Point BiscayNW=chart_point(-11,47), BiscaySE=chart_point(-1.6f,43.1f);
constexpr int NetSetHours=8, BoatWorkHours=10, NetReach=18, BoatReach=24;

bool inside(Point p,Point top_left,Point bottom_right) {
    return p.x>=top_left.x && p.x<=bottom_right.x && p.y>=top_left.y && p.y<=bottom_right.y;
}

bool sea_position(Point p,WaterTest water) {
    return p.x>=0 && p.x<ChartWidth*16 && p.y>=0 && p.y<ChartHeight*16 && water({p.x/16,p.y/16});
}

Point move(Point p,int heading,int speed) {
    const auto vector=Directions[heading];
    return {p.x+(vector.x*speed+(vector.x>=0?8:-8))/16,
            p.y+(vector.y*speed+(vector.y>=0?8:-8))/16};
}

bool near(Point a,Point b,int reach) {
    const int x=a.x-b.x, y=a.y-b.y;
    return x*x+y*y<=reach*reach;
}

} // namespace

int Game::fishery_available() const {
    if (!s.helm || encounter()) return NoFishery;
    if (s.fishery.target) return s.fishery.target;
    const auto p=position();
    const int season=s.day%365;
    if ((season<74 || season>=290) && inside(p,BiscayNW,BiscaySE) &&
        s.day>=s.fishery_next[WhaleFishery-1])
        return WhaleFishery;
    if (inside(p,IberiaNW,IberiaSE) && s.day>=s.fishery_next[SardineFishery-1])
        return SardineFishery;
    return NoFishery;
}

bool Game::begin_fishery(WaterTest water) {
    const int kind=fishery_available();
    if (!kind || s.fishery.target) return false;
    if (conditions().weather==Squall) {
        notice="浪高，先等骤风过去";
        return false;
    }
    constexpr int headings[]{0,2,6,4,1,3,5,7};
    const Point player{s.helm_x,s.helm_y};
    for (const int offset : headings) {
        const int bearing=(s.heading+offset)%8;
        const auto target=move(player,bearing,kind==WhaleFishery?42:32);
        if (!sea_position(target,water) || !sea_position(move(player,bearing,16),water)) continue;
        s.fishery={.animal=target,.target=static_cast<std::uint8_t>(kind),
                   .bearing=static_cast<std::uint8_t>((bearing+1)%8)};
        notice=kind==WhaleFishery?"远处水面升起双柱鲸息":"船员看见了银色鱼群";
        return true;
    }
    notice="离岸远些再找鱼群";
    return false;
}

bool Game::whale_at_surface() const {
    return s.fishery.target==WhaleFishery && s.fishery.age%28<19;
}

Text Game::fishery_action_requirement() const {
    const auto& job=s.fishery;
    if (!job.target) return "附近暂未找到渔场";
    const Point player{s.helm_x,s.helm_y};
    if (job.stage==GearSet) return job.target==SardineFishery?"等鱼群进网":"等小艇返回";
    if (job.stage==GearReady)
        return near(job.gear,player,NetReach+6)?Text{}:"把船开回渔具旁";
    if (conditions().weather==Squall) return "骤风时不能放渔具";
    if (!s.reefed) return "先缩帆减速";
    if (job.target==WhaleFishery && s.crew<8) return "至少需要八名船员";
    if (job.target==WhaleFishery && !whale_at_surface()) return "鲸已潜下，等它浮出";
    const int reach=job.target==WhaleFishery?BoatReach:NetReach;
    if (!near(player,job.animal,reach)) return "再靠近一些";
    return {};
}

bool Game::work_fishery() {
    if (!s.helm || encounter()) return false;
    if (const auto reason=fishery_action_requirement(); !reason.empty()) { notice=reason; return false; }
    auto& job=s.fishery;
    if (job.stage==TrackingFish) {
        job.stage=GearSet;
        job.gear=job.target==SardineFishery?job.animal:Point{s.helm_x,s.helm_y};
        job.work=0;
        notice=job.target==SardineFishery?"网已撒开，留意浮标":"渔队已放下小艇";
        return true;
    }
    const bool whale=job.target==WhaleFishery;
    if (whale) {
        const int barrels=std::min({3,capacity()-load(),12-int(s.whale_oil)});
        s.whale_oil+=std::max(0,barrels);
        notice=barrels>0?"渔队带回鲸油，可交海港商人":"收回小艇，货舱没有鲸油空位";
    } else {
        const int season=s.day%365;
        const int haul=season>=95 && season<=238?12:8;
        const int food=std::min(haul,food_capacity()-s.stores);
        s.stores+=std::max(0,food);
        notice=food>0?"鱼已起网，水手将鱼整理成口粮":"渔网已收回，口粮舱已满";
    }
    s.fishery_next[job.target-1]=std::min(99999,s.day+(whale?48:7));
    s.fishery={};
    return true;
}

bool Game::cancel_fishery() {
    if (!s.fishery.target) return false;
    s.fishery={};
    notice="已召回渔队，继续航行";
    return true;
}

void Game::tick_fishery(WaterTest water) {
    auto& job=s.fishery;
    if (!job.target) return;
    const Point player{s.helm_x,s.helm_y};
    if (!near(player,job.stage==TrackingFish?job.animal:job.gear,120) ||
        (job.stage==GearSet && conditions().weather==Squall)) {
        cancel_fishery();
        notice="渔场已离开视野，渔队返回";
        return;
    }
    job.age=(job.age+1)%252;
    if (job.stage!=GearReady && job.age%3==0) {
        if (job.age%18==0) job.bearing=(job.bearing+(job.age%36?7:1))%8;
        const auto next=move(job.animal,job.bearing,job.target==WhaleFishery?2:1);
        if (sea_position(next,water)) job.animal=next;
        else job.bearing=(job.bearing+3)%8;
    }
    if (job.stage!=GearSet) return;
    ++job.work;
    if (job.target==WhaleFishery && !near(job.gear,job.animal,6)) {
        const int bearing=direction_to({job.animal.x-job.gear.x,job.animal.y-job.gear.y});
        const auto next=move(job.gear,bearing,3);
        if (sea_position(next,water)) job.gear=next;
    }
    if (job.work>=(job.target==WhaleFishery?BoatWorkHours:NetSetHours)) {
        job.stage=GearReady;
        notice=job.target==WhaleFishery?"渔队正等待接应":"浮标沉了一截，靠过去收网";
    }
}

int Game::whale_oil_quote() const {
    if (!port() || market_held() || !s.whale_oil) return 0;
    return s.whale_oil*(s.origin==bordeaux?41:33);
}

bool Game::sell_whale_oil() {
    const int price=whale_oil_quote();
    if (!price) return false;
    earn(price);
    s.whale_oil=0;
    notice="港口商人已收走鲸油";
    return true;
}

} // namespace tide
