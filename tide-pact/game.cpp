#include "game.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace tide {

namespace {
constexpr int base[] = {48,44,86,32,35,22,108,125};
constexpr int holds[] = {28,48,76};
constexpr int provisions[] = {90,135,185};
constexpr int berths[] = {18,27,38};
constexpr int MerchantMilesPerDay=104;
constexpr int SailingTimeRevision=1;

int length(Point a, Point b) {
    const int x = a.x-b.x, y = a.y-b.y;
    return std::max(1, static_cast<int>(std::sqrt(float(x*x+y*y)) * 10));
}

int sailing_wind(const Leg& leg, bool reverse, int month) {
    auto a = Anchor[reverse ? leg.to : leg.from], b = Anchor[reverse ? leg.from : leg.to];
    int latitude = ChartNorth - (a.y+b.y)/16;
    const int dx = b.x-a.x, dy = b.y-a.y;
    if (latitude >= 37 || latitude < -31) return dx > 10 ? 15 : (dx < -10 ? -13 : 0);
    if (latitude < 6 && b.x < 450 && latitude > -6) return -28;
    if (a.x > 563 && b.x > 563) return (month >= 4 && month <= 8) == (dy < 0) ? 18 : -14;
    if (latitude > 6 && a.x < 282 && b.x < 282) return dy > 8 ? 18 : (dy < -8 ? -13 : 0);
    if (latitude < -14 && a.x < 447) return dy > 8 ? -14 : 12;
    return 0;
}

int sailing_miles_per_day(const State& ship, int load, int spirits, int wind, SeaWeather weather) {
    constexpr int hull_pace[]{92,110,100};
    const int drag=load*26/holds[ship.ship];
    const int seas=weather==Squall?(ship.reefed?9:24):weather==Rain?5:0;
    return std::clamp(hull_pace[ship.ship]+std::min(18,ship.crew)+(spirits-60)/5-drag+wind
                     -seas-(ship.reefed?24:0)+(ship.allies&1?6:0)+(ship.allies&2?4:0)
                     +(ship.first_mate==Pilot?12:0),35,170);
}

int wind_along(int heading, SeaConditions weather) {
    const auto ship=Directions[heading], breeze=Directions[weather.direction];
    return (ship.x*breeze.x+ship.y*breeze.y)*weather.strength/64;
}

int route_wind(const Passage& path, int leg, int day, SeaConditions weather) {
    const auto& edge=Legs[path.edge[leg]];
    const bool reverse=path.reverse[leg];
    const auto from=Anchor[reverse?edge.to:edge.from], to=Anchor[reverse?edge.from:edge.to];
    return sailing_wind(edge,reverse,(day/30)%12)+wind_along(direction_to({to.x-from.x,to.y-from.y}),weather)/2;
}

SeaConditions route_weather(const Passage& path, int leg, int progress, int day) {
    return conditions_at(vessel_at(path,leg,float(progress)/Legs[path.edge[leg]].nautical_miles),day);
}

int sailing_spirits(int spirits, int day, SailingRation ration) {
    return std::max(0,spirits-ration.morale_cost-(day%6==0?3:0));
}

int tenths_of_a_knot(int miles_per_day) {
    return (miles_per_day*10+HoursPerDay/2)/HoursPerDay;
}

Point helm_displacement(Point direction, int y_subpixels, int miles_per_day) {
    constexpr float radians_per_degree=0.01745329252f;
    const float latitude=ChartNorth-y_subpixels/(16.f*ChartLatitudeScale);
    const float east=direction.x*(60.f/ChartLongitudeScale)*std::cos(latitude*radians_per_degree);
    const float north=direction.y*(60.f/ChartLatitudeScale);
    const float step=miles_per_day*(16.f/SailingWatches)/std::sqrt(east*east+north*north);
    return {int(std::lround(direction.x*step)), int(std::lround(direction.y*step))};
}

int checksum(std::span<const std::byte> bytes) {
    std::uint32_t hash = 2166136261U;
    for (std::size_t i = 0; i < bytes.size()-4; ++i)
        hash = (hash ^ std::to_integer<unsigned>(bytes[i])) * 16777619U;
    return static_cast<int>(hash);
}

bool open_water(Point from, Point to, WaterTest water_at) {
    if (!water_at) return true;
    const int dx=to.x-from.x, dy=to.y-from.y;
    const int samples=std::max(std::abs(dx),std::abs(dy));
    for (int step=1;step<=samples;++step)
        if (!water_at({from.x+dx*step/samples,from.y+dy*step/samples})) return false;
    return true;
}
}

SeaConditions conditions_at(Point p, int day) {
    const int latitude=ChartNorth-p.y/8;
    const unsigned cell=unsigned(p.x/54+1)*73856093U ^ unsigned(p.y/60+1)*19349663U ^
                        unsigned(day/2+1)*83492791U;
    const unsigned sample=(cell^(cell>>13))%100U;
    SeaConditions out{
        latitude>=34 || latitude<-34 ? (latitude>=34?3:1) : latitude>=0?5:7,
        2+int((cell>>7)%4U),Fair
    };
    if (p.x>=575 && latitude<22 && latitude>-23)
        out.direction=(day/30)%12>=4 && (day/30)%12<=8 ? 1 : 5;
    if (sample<12 && (latitude<-27 || latitude>28 || sample<5)) {
        out.weather=Squall;
        out.strength=6;
    } else if (sample<28) out.weather=Rain;
    else if (std::abs(latitude)<24 && sample<78) out.weather=Heat;
    return out;
}

int direction_to(Point offset) {
    const int x=std::abs(offset.x), y=std::abs(offset.y);
    if (x*2<y) return offset.y<0?0:4;
    if (y*2<x || (!x && !y)) return offset.x>=0?2:6;
    if (offset.y<0) return offset.x>0?1:7;
    return offset.x>0?3:5;
}

Passage passage(int from, int to) {
    Passage route;
    if (from < 0 || to < 0 || from >= PlaceCount || to >= PlaceCount || from == to) return route;
    std::array<int, PlaceCount> distance, previous;
    std::array<bool, PlaceCount> done{};
    distance.fill(std::numeric_limits<int>::max());
    previous.fill(-1);
    distance[from] = 0;
    for (int i = 0; i < PlaceCount; ++i) {
        int a = -1;
        for (int p = 0; p < PlaceCount; ++p)
            if (!done[p] && distance[p] < (a < 0 ? std::numeric_limits<int>::max() : distance[a])) a = p;
        if (a < 0 || a == to) break;
        done[a] = true;
        for (int j = 0; j < int(std::size(Legs)); ++j) {
            const auto& leg = Legs[j];
            const int b = leg.from == a ? leg.to : (leg.to == a ? leg.from : -1);
            if (b < 0 || done[b] || distance[a] + leg.nautical_miles >= distance[b]) continue;
            distance[b] = distance[a] + leg.nautical_miles;
            previous[b] = j;
        }
    }
    for (int a = to; a != from;) {
        const int j = previous[a];
        if (j < 0) return {};
        const bool reverse = Legs[j].from == a;
        route.edge[route.count] = j;
        route.reverse[route.count++] = reverse;
        route.miles += Legs[j].nautical_miles;
        a = reverse ? Legs[j].to : Legs[j].from;
    }
    std::reverse(route.edge.begin(), route.edge.begin()+route.count);
    std::reverse(route.reverse.begin(), route.reverse.begin()+route.count);
    return route;
}

Point vessel_at(const Passage& route, int leg, float fraction) {
    const auto& l = Legs[route.edge[leg]];
    const auto* p = Waypoints+l.offset;
    int total = 0;
    for (int i = 1; i < l.count; ++i) total += length(p[i-1], p[i]);
    auto remaining = std::clamp(route.reverse[leg] ? 1-fraction : fraction, 0.0f, 1.0f) * total;
    for (int i = 1; i < l.count; ++i) {
        int dist = length(p[i-1], p[i]);
        if (remaining <= dist) {
            const float t = remaining/dist;
            return {int(std::lround(p[i-1].x + (p[i].x-p[i-1].x)*t)),
                    int(std::lround(p[i-1].y + (p[i].y-p[i-1].y)*t))};
        }
        remaining -= dist;
    }
    return p[l.count-1];
}

namespace {
const Passage& merchant_lane(int from, int to) {
    static const auto lanes=[] {
        std::array<std::array<Passage,MerchantPorts.size()>,MerchantPorts.size()> routes{};
        for (std::size_t i=0;i<MerchantPorts.size();++i)
            for (std::size_t j=0;j<MerchantPorts.size();++j)
                routes[i][j]=passage(MerchantPorts[i],MerchantPorts[j]);
        return routes;
    }();
    return lanes[from][to];
}

int merchant_leg(const Passage& lane, int& miles) {
    int leg=0;
    while (leg+1<lane.count && miles>=Legs[lane.edge[leg]].nautical_miles)
        miles-=Legs[lane.edge[leg++]].nautical_miles;
    return leg;
}

int merchant_days(const Passage& lane) { return (lane.miles+MerchantMilesPerDay-1)/MerchantMilesPerDay; }
}

Point Game::merchant_position(int company) const {
    const auto& ship=s.merchants[company];
    if (ship.destination==NoMerchantPort) return Anchor[MerchantPorts[ship.port]];
    const auto& lane=merchant_lane(ship.port,ship.destination);
    int miles=ship.progress;
    const int leg=merchant_leg(lane,miles);
    return vessel_at(lane,leg,float(miles)/Legs[lane.edge[leg]].nautical_miles);
}

int Game::capacity() const { return holds[s.ship]; }
int Game::max_stores() const { return provisions[s.ship]; }
int Game::food_capacity() const {
    return std::min(max_stores()+20,2*max_stores()-s.water);
}
int Game::water_capacity() const {
    return std::min(max_stores()+20,2*max_stores()-s.stores);
}
int Game::crew_limit() const { return berths[s.ship]; }

int Game::load() const {
    int all = dispatch_space();
    for (const int count : s.cargo) all += count;
    return all;
}

const MarketNotice* market_notice(int port, int good, int day) {
    for (const auto& notice : MarketNotices)
        if (notice.port==port && notice.good==good && notice.remaining(day)) return &notice;
    return nullptr;
}

namespace {
int stock_base(int port, int good, int day, int road_timber) {
    constexpr int seasonal[]{-2,0,3,1};
    return 17+Bias[port][good]*6+seasonal[(day/29+port+good)%4]+(port==lisbon && good==Timber?road_timber:0);
}

int stock_limit(int port, int good, int day, int road_timber) {
    return std::min(65,stock_base(port,good,day,road_timber)+48);
}

int daily_stock_change(int port, int good, int delta, int day, int road_timber) {
    const int base=stock_base(port,good,day,road_timber);
    const int supply=std::clamp(base+delta,0,stock_limit(port,good,day,road_timber));
    const auto* news=market_notice(port,good,day);
    const int target=std::max(0,base+(news?news->reserve_shift:0));
    const bool exports=Bias[port][good]>0;
    const int interval=news?1:supply<target && !exports?5:3;
    if ((day+port+good*2)%interval!=0) return delta;
    const int step=news || (exports && supply<target)?2:1;
    return supply+std::clamp(target-supply,-step,step)-base;
}
}

int Game::market_stock(int port, int good) const {
    if (port<0 || port>=PortCount || good<0 || good>=GoodsCount) return 0;
    return std::clamp(stock_base(port,good,s.day,road_timber_bonus())+s.stock_delta[port*GoodsCount+good],0,65);
}

int Game::stock(int good) const {
    return market_stock(s.origin,good);
}

int Game::market_price(int port, int good, bool sell, int supply, int day, bool player) const {
    const auto* news=market_notice(port,good,day);
    const int market=base[good]*(100-Bias[port][good]*15+
                     ((day/29+good+port*3)%7-3)*3+(news?news->price_percent:0))/100;
    const int pressure=17-supply+2*std::max(0,8-supply);
    const int price=std::max(7,market+pressure*std::max(1,base[good]/52)+
                             (sell ? -std::max(3,market/11) : std::max(4,market/12)));
    return player && !sell && guild_stake(port) ? std::max(7,(price*94+99)/100) : price;
}

int Game::price_at(int port, int good, bool sell) const {
    if (port<0 || port>=PortCount || good<0 || good>=GoodsCount) return 0;
    return market_price(port,good,sell,market_stock(port,good),s.day);
}

int Game::quote(int good, bool sell) const { return price_at(s.origin,good,sell); }

TradeQuote Game::quote_order(int good, bool sell, int quantity, int rate, bool limit_gold) const {
    if (!port() || good<0 || good>=GoodsCount || quantity<=0) return {};
    const int limit=std::min(quantity,sell ? s.cargo[good] : capacity()-load());
    const int budget=sell ? 999999-s.gold : s.gold;
    const int port=s.origin, base_stock=stock_base(port,good,s.day,road_timber_bonus());
    const int stock_cap=stock_limit(port,good,s.day,road_timber_bonus());
    TradeQuote offer{.stock_change=s.stock_delta[port*GoodsCount+good]};
    int supply=market_stock(port,good);
    int normal=0, reverse=0;
    const int shift=sell?1:-1;
    for (;offer.units<limit;++offer.units) {
        if (sell?supply>=stock_cap:supply==0) break;
        const int total=normal+market_price(port,good,sell,supply,s.day);
        if (rate) reverse+=market_price(port,good,!sell,supply+shift,s.day);
        const int gain=std::min(total*rate/100,std::max(0,(reverse-total)*shift));
        if (limit_gold && total+shift*gain>budget) break;
        normal=total;
        offer.gold=total+shift*gain;
        offer.saved=gain;
        supply+=shift;
        offer.stock_change=supply-base_stock;
    }
    return offer;
}

TradeQuote Game::trade_quote(int good, bool sell, int quantity) const {
    if (!sell && quantity==TradeLot && s.bargain_good==good+1) {
        const auto discounted=quote_order(good,false,quantity,s.bargain_rate,true);
        if (discounted.units==TradeLot) return discounted;
    }
    return quote_order(good,sell,quantity,0,true);
}

int Game::negotiation_rate() const {
    if (!port()) return 0;
    const auto language=PortSpeech[s.origin];
    return understands(language)?8:fluency(language)==Phrases?4:0;
}

Text Game::bargain_block(int good, bool sell) const {
    if (!port()) return "靠港后才能议价";
    if (s.bargain_good) return "本次停泊的议价已结束";
    if (!negotiation_rate()) return "需本地短语、随船口译或码头口译";
    if (bargain_quote(good,sell,0).units!=TradeLot)
        return sell?"需有五件货，港口也得收得下":"需港口库存和舱位各有五件";
    return {};
}

TradeQuote Game::bargain_quote(int good, bool sell, int rate) const {
    return quote_order(good,sell,TradeLot,std::clamp(rate,0,MaxHaggleRate),false);
}

HaggleResult Game::bargain_offer(int good, bool sell, int rate) {
    if (rate<0 || rate>MaxHaggleRate || !bargain_block(good,sell).empty()) return {};
    const auto offer=bargain_quote(good,sell,rate);
    if (rate && !offer.saved) { notice="这批货没有让价空间"; return {}; }
    if (offer.gold>(sell?999999-s.gold:s.gold)) {
        notice=sell?"金币已接近上限":"金币不够付这批货";
        return {};
    }
    const int supply=stock(good);
    const int stock_room=supply<=10?-4:supply>=24?4:0;
    const auto* news=market_notice(s.origin,good,s.day);
    const int news_room=news?(news->price_percent>0?-2:2):0;
    const int timber_room=!sell && s.origin==lisbon && good==Timber && s.timber_trail==TimberReturned?4:0;
    const int room=std::clamp(negotiation_rate()+std::min(s.trust/3,3)+
                              (sell?-1:1)*(stock_room+news_room)+timber_room,2,MaxHaggleRate);
    const auto limit=bargain_quote(good,sell,room);
    if (sell?offer.gold<=limit.gold:offer.gold>=limit.gold) {
        apply_trade(good,sell,offer);
        if (rate || s.bargain_warnings) s.bargain_good=BargainUsed;
        notice="整批成交";
        return {DealAgreed,0};
    }
    const int lost=1+(rate-room-1)/4;
    s.bargain_warnings=std::min(HagglePatience,int(s.bargain_warnings)+lost);
    if (s.bargain_warnings==HagglePatience) {
        s.bargain_good=BargainUsed;
        s.trust=std::max(0,s.trust-1);
        notice="商人收起账簿，本次停泊不再议价";
        return {TalksEnded,0};
    }
    notice="商人给了还价";
    return {CounterOffer,std::max(1,room-(HagglePatience-s.bargain_warnings))};
}

bool Game::inspect_timber_trail() {
    if (!port() || s.origin!=lisbon || s.timber_trail!=TrailUnknown) return false;
    s.timber_trail=TrailFound;
    notice="拾到盖着货主印记的货签";
    return true;
}

bool Game::resolve_timber_trail(bool return_cargo) {
    if (!port() || s.origin!=lisbon || s.timber_trail!=TrailFound) return false;
    s.timber_trail=return_cargo?TimberReturned:TimberPaid;
    if (return_cargo) ++s.trust;
    else earn(55);
    notice=return_cargo?"货主记下了人情，木料议价更宽松":"拿到55金谢礼";
    return true;
}

bool Game::knows_land_pass() const {
    return s.timber_trail!=TrailUnknown || s.first_mate==Pilot;
}

Text Game::land_requirement(LandAction action) const {
    if (action<FollowStream || action>ReturnToCity || s.origin!=lisbon || sea() || encounter())
        return "要从里斯本出发";
    const bool outward=action==FollowStream || action==CrossPass;
    if (outward ? s.land_stage!=LandUnvisited : !land_away())
        return "这段路走不了了";
    if (action==CrossPass && !knows_land_pass()) return "需要货签背面的线索，或请安娜领路";
    if ((action==CopyMilestone || action==SurveyLookout) && s.land_stage!=LandRidge)
        return "已经记下这段驿道";
    const auto leg=LandLegs[action];
    const auto reserve=action==ReturnToCity?LandLeg{}:LandLegs[ReturnToCity];
    if (s.stores<leg.food+reserve.food || s.water<leg.water+reserve.water) return "留足返程水粮才能走这一段";
    return {};
}

bool Game::explore_land(LandAction action) {
    if (const Text blocked=land_requirement(action); !blocked.empty()) { notice=blocked; return false; }
    const auto leg=LandLegs[action];
    s.stores-=leg.food;
    s.water-=leg.water;
    spend_day(leg.days);
    switch (action) {
    case FollowStream: case CrossPass:
        s.land_stage=LandRidge;
        notice="走到了山脊的老路碑";
        break;
    case CopyMilestone:
        s.land_stage=LandInscription;
        notice="拓下路碑，记下了山道";
        break;
    case SurveyLookout:
        s.land_stage=LandLookout;
        notice="登上望台，补画了背坡的岔路";
        break;
    case ReturnToCity:
        s.land_stage=s.land_stage==LandRidge?LandUnvisited:s.land_stage==LandLookout?LandFullNotes:LandNotes;
        notice=s.land_stage==LandUnvisited?"已返回河堤":"已返回河堤，可去档案室交图";
        break;
    }
    return true;
}

int Game::road_timber_bonus() const {
    return s.land_stage==LandFullFiled?6:s.land_stage==LandFiled?3:0;
}

bool Game::file_land_report() {
    if (!port() || s.origin!=lisbon || (s.land_stage!=LandNotes && s.land_stage!=LandFullNotes)) return false;
    const bool complete=s.land_stage==LandFullNotes;
    earn(complete?215:150);
    s.renown+=complete?2:1;
    s.land_stage=complete?LandFullFiled:LandFiled;
    notice="驿道图已入档，商队可以通行";
    return true;
}

int Game::wind(int leg) const {
    if (s.helm) return wind_along(s.heading,conditions());
    return route_wind(route,leg,s.day,route_weather(route,leg,s.progress,s.day));
}

int Game::next_route_pace() const {
    const int day=std::min(99999,s.day+1);
    const auto weather=route_weather(route,s.leg,s.progress,day);
    return sailing_miles_per_day(s,load(),sailing_spirits(s.spirits,day,sailing_ration(day,weather.weather)),
                               route_wind(route,s.leg,day,weather),weather.weather);
}

int Game::speed_in_tenths_of_a_knot() const {
    if (!sea() || paused || encounter()) return 0;
    if (!s.helm) return tenths_of_a_knot(next_route_pace());
    const auto weather=conditions();
    return tenths_of_a_knot(sailing_miles_per_day(s,load(),s.spirits,
                                               wind_along(s.heading,weather),weather.weather));
}

PassageForecast Game::forecast(const Passage& path, int extra_load) const {
    return forecast_from(path,0,0,path.count,load()+extra_load);
}

int Game::sailing_target() const {
    if (!sea() || s.helm) return -1;
    if (!s.stop_next) return s.destination;
    const auto& leg=Legs[route.edge[s.leg]];
    return route.reverse[s.leg]?leg.from:leg.to;
}

PassageForecast Game::remaining_forecast() const {
    if (!sea() || s.helm) return {};
    return forecast_from(route,s.leg,s.progress,s.stop_next?s.leg+1:route.count,load());
}

std::array<ForecastDay, ForecastDays> Game::local_forecast() const {
    std::array<ForecastDay, ForecastDays> result{};
    const auto here=position();
    for (int i=0;i<ForecastDays;++i) {
        const int day=std::min(99999,s.day+i+1);
        const auto weather=conditions_at(here,day);
        const auto ration=sailing_ration(day,weather.weather);
        result[i]={day,weather,0,ration.water,ration.food};
    }
    return result;
}

PassageForecast Game::forecast_from(const Passage& path, int first_leg, int progress, int end, int cargo) const {
    PassageForecast result;
    int spirits=s.spirits;
    for (int i=first_leg;i<end;++i) {
        const auto& leg=Legs[path.edge[i]];
        int sailed=i==first_leg?progress:0;
        while (sailed<leg.nautical_miles) {
            const int day=std::min(99999,s.day+result.days+1);
            const auto weather=route_weather(path,i,sailed,day);
            const auto ration=sailing_ration(day,weather.weather);
            spirits=sailing_spirits(spirits,day,ration);
            const int miles=sailing_miles_per_day(s,cargo,spirits,route_wind(path,i,day,weather),weather.weather);
            if (result.days<ForecastDays)
                result.outlook[result.days]={day,weather,tenths_of_a_knot(miles),ration.water,ration.food};
            ++result.days;
            sailed+=miles;
            result.water+=ration.water;
            result.provisions+=ration.food;
        }
        result.arrival_days[i]=result.days;
    }
    return result;
}

int Game::proposal() const {
    constexpr int outbound[] = {madeira, bordeaux, cadiz, canaries, ribeira, elmina,
                                tangier, valencia, tunis, genoa, malindi, lisbon,
                                ribeira, elmina, mpinda, elmina, kilwa, malindi, aden, kilwa};
    return outbound[s.origin];
}
int Game::proposal_good() const { return (s.origin + s.day / 60) % GoodsCount; }
int Game::reward(int port) const { return 230 + base[s.contract_good]*4 + Zone[port]*34; }

Point Game::provisions_order(int food, int water) const {
    const int more_food=std::max(0,std::min(food,food_capacity())-s.stores);
    const int more_water=std::max(0,std::min(water,water_capacity())-s.water);
    const int room=2*max_stores()-s.stores-s.water;
    const int total=more_food+more_water;
    if (total<=room) return {more_food,more_water};
    const int food_share=room*more_food/total;
    return {food_share,room-food_share};
}

int Game::resupply_cost(int food, int water) const {
    const auto order=provisions_order(food,water);
    const int count=order.x+order.y;
    if (!count) return 0;
    const int price=(count+7)/8;
    return port() && Zone[s.origin]==3 && s.tales[Monsoon]==Shared ? (price*3+3)/4 : price;
}

int Game::ration_cost() const { return resupply_cost(food_capacity(),water_capacity()); }

bool Game::resupply(int food, int water) {
    if (!port()) return false;
    const int price=resupply_cost(food,water);
    if (!price) { notice="已有足够水粮"; return false; }
    if (s.gold<price) { notice="金币不足"; return false; }
    const auto order=provisions_order(food,water);
    earn(-price);
    s.stores+=order.x;
    s.water=static_cast<std::uint8_t>(s.water+order.y);
    notice="补给已装船";
    return true;
}

bool Game::unload_provisions(bool water) {
    if (!port()) return false;
    const int count=std::min(10,water?int(s.water):s.stores);
    if (!count) return false;
    if (water) s.water-=count;
    else s.stores-=count;
    earn(count/10);
    notice=water?"腾出淡水舱位":"腾出食物舱位";
    return true;
}

int Game::repair_cost() const {
    const int price = (100-s.hull)*2;
    const bool helpers=port() && ((Zone[s.origin]==1 && s.tales[Chartmakers]==Shared) ||
                                 (s.origin==cadiz && s.drift_stage==DriftReturned));
    return helpers?(price*3+3)/4:price;
}

int Game::daily_rations() const { return s.crew>=27 ? 3 : (s.crew>14 ? 2 : 1); }
int Game::daily_water(SeaWeather weather) const {
    return std::max(1,daily_rations()+(weather==Heat)-(s.first_mate==Quartermaster));
}

SailingRation Game::sailing_ration(int day, SeaWeather weather) const {
    // The displayed voyage day is one-based.
    const bool lean_day=day%2==1;
    const bool less_food=lean_day && rationing(SaveFood);
    const bool less_water=lean_day && rationing(SaveWater) && weather!=Heat;
    return {daily_rations()-less_food,daily_water(weather)-less_water,3*less_food+4*less_water};
}

void Game::toggle_ration(RationFlag flag) {
    s.ration_flags^=flag;
}

Game::Game() {
    for (int p=0;p<PlaceCount;++p)
        if (StartingChart&(1U<<p)) reveal_chart(Anchor[p],18);
    reveal_chart(Anchor[lisbon],34);
}

bool Game::charted(int place) const {
    if (place<0 || place>=PlaceCount) return false;
    return ((s.chart_marks|s.visited|s.surveyed)&(1U<<place)) ||
           place==s.origin || place==s.destination ||
           (s.chapter<Chapters && Story[s.chapter].place==place);
}

bool Game::chart_cell(int column, int row) const {
    if (column<0 || column>=ChartColumns || row<0 || row>=ChartRows) return false;
    const int cell=row*ChartColumns+column;
    return s.coast[cell/8]&(1U<<(cell%8));
}

void Game::reveal_chart(Point center, int radius) {
    const int top=std::max(0,(center.y-radius)/ChartCell);
    const int left=std::max(0,(center.x-radius)/ChartCell);
    const int right=std::min(ChartColumns-1,(center.x+radius)/ChartCell);
    const int bottom=std::min(ChartRows-1,(center.y+radius)/ChartCell);
    for (int y=top;y<=bottom;++y) {
        const int dy=y*ChartCell+ChartCell/2-center.y;
        for (int x=left;x<=right;++x) {
            const int dx=x*ChartCell+ChartCell/2-center.x;
            if (dx*dx+dy*dy>radius*radius) continue;
            const int cell=y*ChartColumns+x;
            s.coast[cell/8]|=1U<<(cell%8);
        }
    }
}

int Game::sighting(Point center) {
    const int radius=lookout_radius();
    reveal_chart(center,radius+12);
    int seen=-1;
    for (int p=0;p<PlaceCount;++p) {
        if (charted(p)) continue;
        const int dx=center.x-Anchor[p].x, dy=center.y-Anchor[p].y;
        if (dx*dx+dy*dy>radius*radius) continue;
        s.chart_marks|=1U<<p;
        if (seen<0) seen=p;
    }
    return seen;
}

bool Game::heard_rumor(int id) const {
    return id>=0 && id<RumorCount && (s.heard_rumors&(1U<<id));
}

bool Game::hear_rumor(int id) {
    if (!port() || id<0 || id>=RumorCount || heard_rumor(id) || s.origin!=Rumors[id].port)
        return false;
    if (!understands(PortSpeech[s.origin])) return false;
    s.heard_rumors|=1U<<id;
    notice="传闻已记在航海簿上";
    return true;
}

PortStoryStage Game::port_story_stage(int id) const {
    if (id<0 || id>=PortStoryCount) return PortStoryUnknown;
    return PortStoryStage((s.port_stories>>(2*id))&3U);
}

bool Game::meet_port_story(int id) {
    if (id<0 || id>=PortStoryCount || !port() ||
        s.origin!=PortStories[id].port || port_story_stage(id)!=PortStoryUnknown) return false;
    s.port_stories|=PortStoryMet<<(2*id);
    notice="新见闻已记在日志";
    return true;
}

Text Game::port_story_requirement(int id, int choice) const {
    if (id<0 || id>=PortStoryCount || choice<0 || choice>1 || !port() ||
        s.origin!=PortStories[id].port || port_story_stage(id)!=PortStoryMet) return "在当地交谈";
    if (id==TideSkiff) {
        if (choice==0 && !s.cargo[Timber]) return "缺一件木料";
        if (choice==1 && s.spirits<8) return "船员太累";
    }
    if (id==LastSugar && choice==0) {
        if (s.gold<20) return "不够20金";
        if (capacity()-load()<2) return "船舱空位不足";
    }
    if (id==TornChart && choice==0) {
        if (charted(ribeira)) return "已知泊地";
        if (s.gold<28) return "不够28金";
    }
    if (id==TwoLetters && !understands(Castilian)) return "需要口译";
    return {};
}

bool Game::resolve_port_story(int id, int choice) {
    if (!port_story_requirement(id,choice).empty()) return false;
    int renown=0;
    switch (id) {
    case TideSkiff:
        if (choice==0) {
            --s.cargo[Timber];
            earn(54);
            renown=2;
        } else {
            s.spirits-=8;
            earn(28);
        }
        break;
    case LastSugar:
        if (choice==0) {
            earn(-20);
            s.cargo[Sugar]+=2;
        } else {
            earn(18);
            renown=1;
        }
        break;
    case TornChart:
        if (choice==0) {
            earn(-28);
            s.chart_marks|=1U<<ribeira;
        } else {
            s.heard_rumors|=1U<<RibeiraRumor;
        }
        break;
    case TwoLetters:
        if (choice==0) earn(84);
        else {
            renown=4;
            s.trust=std::min(999999,s.trust+2);
        }
        break;
    }
    s.renown=std::min(999999,s.renown+renown);
    const unsigned shift=2*id;
    s.port_stories=(s.port_stories&~(3U<<shift))|((PortStoryFirst+choice)<<shift);
    notice="见闻结果已记在日志";
    return true;
}

int Game::water_price() const { return s.tales[Monsoon]==Shared ? 18 : 30; }

int Game::tale_stop(int tale) const {
    if (tale<0 || tale>=TaleCount) return -1;
    switch (s.tales[tale]) {
    case Unmet: return Tales[tale].start;
    case InProgress: return Tales[tale].finish;
    default: return -1;
    }
}

int Game::tale_here() const {
    if (!port()) return -1;
    for (int i=0;i<TaleCount;++i)
        if (tale_stop(i)==s.origin) return i;
    return -1;
}

Text Game::tale_requirement(int tale) const {
    if (tale==Chartmakers && s.cargo[Timber]<2) return "需要两件木料，可在市场购买";
    if (tale==Monsoon && (s.stores<8 || s.water<8)) return "需要八份水粮，可在船坞补给";
    return {};
}

bool Game::choose_tale(int tale, bool share) {
    if (tale<0 || tale>=TaleCount || !port() || tale_stop(tale)!=s.origin) return false;
    auto& state=s.tales[tale];
    if (state==Unmet) {
        if (!share) return false;
        state=InProgress;
        s.chart_marks|=1U<<Tales[tale].finish;
        notice="故事已记入日志";
        return true;
    }
    if (share) {
        if (!(notice=tale_requirement(tale)).empty()) return false;
        if (tale==Chartmakers) s.cargo[Timber]-=2;
        if (tale==Monsoon) { s.stores-=8; s.water-=8; }
        s.trust+=2;
        s.renown+=3;
        state=Shared;
    } else {
        constexpr int payment[]{220,250,270};
        earn(payment[tale]);
        state=Sold;
    }
    if (tale==Lamps && s.beacon!=BeaconCharted) {
        spend_day(1);
        s.beacon=BeaconCharted;
        notice="在港查清灯讯，用去一日";
    } else notice="故事结局已记入日志";
    return true;
}

Point Game::position(float frame) const {
    if (!sea()) return Anchor[s.origin];
    if (s.helm) return {s.helm_x/16,s.helm_y/16};
    const auto& edge = Legs[route.edge[s.leg]];
    const float distance=s.progress+(frame>0?frame*next_route_pace():0);
    return vessel_at(route,s.leg,distance/edge.nautical_miles);
}

void Game::earn(int gold) { s.gold = std::clamp(s.gold + gold, 0, 999999); }

void Game::plan_merchant(int company) {
    auto& ship=s.merchants[company];
    const int origin=MerchantPorts[ship.port];
    int best_good=Wine, best_units=0, best_score=0;
    int best_port=(ship.port+1+company)%MerchantPorts.size();
    int best_cost=4+2*merchant_days(merchant_lane(ship.port,best_port));
    for (std::size_t hub=0;hub<MerchantPorts.size();++hub) {
        if (hub==ship.port) continue;
        const auto& lane=merchant_lane(ship.port,hub);
        const int days=merchant_days(lane);
        const int arrival=std::min(99999,s.day+days);
        const int provisions=4+2*days;
        const int port=MerchantPorts[hub];
        for (int good=0;good<GoodsCount;++good) {
            const int local=market_stock(origin,good);
            int remote=s.stock_delta[port*GoodsCount+good];
            for (int day=s.day+1;day<=arrival;++day) remote=daily_stock_change(port,good,remote,day,road_timber_bonus());
            const int cap=stock_limit(port,good,arrival,road_timber_bonus());
            const int supply=std::clamp(stock_base(port,good,arrival,road_timber_bonus())+remote,0,cap);
            int cost=provisions, revenue=0;
            for (int unit=0;unit<std::min({MerchantLot,local,cap-supply});++unit) {
                cost+=market_price(origin,good,false,local-unit,s.day,false);
                if (cost>ship.gold) break;
                revenue+=market_price(port,good,true,supply+unit,arrival,false);
                const int score=(revenue-cost)*12/days;
                if (score<=best_score) continue;
                best_score=score;
                best_port=hub;
                best_good=good;
                best_units=unit+1;
                best_cost=cost;
            }
        }
    }
    if (best_cost>ship.gold) return;
    ship.gold-=best_cost;
    ship.destination=best_port;
    ship.good=best_good;
    ship.units=best_units;
    if (best_units)
        s.stock_delta[origin*GoodsCount+best_good]=
            market_stock(origin,best_good)-best_units-stock_base(origin,best_good,s.day,road_timber_bonus());
}

void Game::merchant_day(int company) {
    auto& ship=s.merchants[company];
    ship.held=conditions_at(merchant_position(company),s.day).weather==Squall;
    if (ship.held) return;
    if (ship.destination==NoMerchantPort) {
        plan_merchant(company);
        return;
    }
    const auto& lane=merchant_lane(ship.port,ship.destination);
    int miles=ship.progress;
    const int leg=merchant_leg(lane,miles);
    const int wind=sailing_wind(Legs[lane.edge[leg]],lane.reverse[leg],s.day/30%12);
    ship.progress+=MerchantMilesPerDay+wind;
    if (ship.progress<lane.miles) return;

    ship.port=ship.destination;
    ship.destination=NoMerchantPort;
    ship.progress=0;
    const int port=MerchantPorts[ship.port];
    const int supply=market_stock(port,ship.good);
    const int sold=std::min(int(ship.units),stock_limit(port,ship.good,s.day,road_timber_bonus())-supply);
    for (int unit=0;unit<sold;++unit)
        ship.gold=std::min(MerchantReserve,ship.gold+
            market_price(port,ship.good,true,supply+unit,s.day,false));
    if (sold) s.stock_delta[port*GoodsCount+ship.good]=supply+sold-stock_base(port,ship.good,s.day,road_timber_bonus());
    ship.units=sold;
}

unsigned Game::roll(unsigned range) {
    s.rng = s.rng * 1664525U + 1013904223U;
    return (s.rng >> 8) % range;
}

void Game::spend_day(int days) {
    for (int i = 0; i < days && s.day < 99999; ++i) {
        ++s.day;
        for (int port=0;port<PortCount;++port)
            for (int good=0;good<GoodsCount;++good) {
                auto& delta=s.stock_delta[port*GoodsCount+good];
                delta=daily_stock_change(port,good,delta,s.day,road_timber_bonus());
            }
        if (s.day>=2)
            for (std::size_t company=0;company<s.merchants.size();++company) merchant_day(company);
    }
}

bool Game::depart(int to) {
    if (sea() || encounter() || land_away() || to==s.origin) return false;
    if (!charted(to)) { notice="此处尚未定位，需掌舵探索"; return false; }
    auto path = passage(s.origin, to);
    if (!path.count) return false;
    if (s.hull<=12) { notice="船体破损，需要维修"; return false; }
    if (s.stores<daily_rations() || s.water<daily_water(Fair)) { notice="水粮不足一日"; return false; }
    route = path;
    s.interpreter_port=-1;
    s.bargain_good=s.bargain_rate=s.bargain_warnings=0;
    s.destination = to;
    s.leg = s.progress = s.weather = s.enemy = s.range = s.aim = 0;
    s.event_mask = 0;
    s.stop_next = 0;
    paused = false;
    notice = "已离岸";
    return true;
}

void Game::turn_back() {
    if (!sea() || s.helm || encounter() || !paused) return;
    const auto& leg = Legs[route.edge[s.leg]];
    const int from = route.reverse[s.leg] ? leg.to : leg.from;
    const int to = route.reverse[s.leg] ? leg.from : leg.to;
    if (!s.progress) {
        s.destination = from;
        arrive();
        return;
    }
    const auto reverse = passage(to,from);
    if (reverse.count != 1 || reverse.edge[0] != route.edge[s.leg]) {
        notice = "航道过窄，继续到下一处再回航";
        return;
    }
    route = reverse;
    s.origin = to;
    s.destination = from;
    s.progress = leg.nautical_miles - s.progress;
    s.leg = 0;
    s.event_mask = 1;
    s.stop_next = 0;
    paused = false;
    notice = "已转向上一处停泊点";
}

void Game::wait_at_port() {
    if (!port()) return;
    spend_day(1);
    notice = "在港内等候了一日";
}

void Game::restore() {
    route=s.helm?Passage{}:passage(s.origin,s.destination);
    paused = sea() && !encounter();
    s.chart_marks|=StartingChart;
    for (int i=0;i<=s.chapter && i<Chapters;++i) s.chart_marks|=1U<<Story[i].place;
    for (int i=0;i<TaleCount;++i)
        if (s.tales[i]!=Unmet) s.chart_marks|=1U<<Tales[i].finish;
    for (int i=0;i<ExpeditionCount;++i)
        if (s.expeditions[i].flags&HeardWitness) s.chart_marks|=1U<<Expeditions[i].site;
    if (s.contract>=0) s.chart_marks|=1U<<s.contract;
    if (s.destination>=0) s.chart_marks|=1U<<s.destination;
    for (int p=0;p<PlaceCount;++p) {
        if ((StartingChart|s.visited|s.surveyed)&(1U<<p)) reveal_chart(Anchor[p],18);
    }
    reveal_chart(Anchor[lisbon],34);
    reveal_chart(position(),24);
    if (s.helm && !s.wake_count) record_wake();
}

bool Game::consume_day(SeaConditions weather) {
    const auto [food,drink,morale_cost]=sailing_ration(s.day,weather.weather);
    const bool hunger=s.stores<food, thirst=s.water<drink;
    s.stores=std::max(0,s.stores-food);
    s.water=static_cast<std::uint8_t>(std::max(0,int(s.water)-drink));
    if (hunger || thirst) {
        s.hull-=(hunger?5:0)+(thirst?8:0);
        s.spirits=std::max(0,s.spirits-(thirst?12:8));
        if (s.crew>4 && s.day%2==0) --s.crew;
        notice=thirst?"缺淡水，士气与船况下降":"缺食物，士气与船况下降";
    } else s.spirits=sailing_spirits(s.spirits,s.day,{food,drink,morale_cost});
    if (s.hull<=0) { rescue(); return false; }
    return true;
}

bool Game::start_helm(int destination) {
    if (sea() || encounter() || land_away() || s.hull<=12 ||
        s.stores<daily_rations() || s.water<daily_water(Fair)) {
        notice="掌舵前请备足一日水粮并修船";
        return false;
    }
    const auto here=Anchor[s.origin];
    s.interpreter_port=-1;
    s.bargain_good=s.bargain_rate=s.bargain_warnings=0;
    s.helm=1;
    s.helm_x=here.x*16;
    s.helm_y=here.y*16;
    s.wake_head=s.wake_count=0;
    record_wake();
    s.destination=charted(destination) && destination!=s.origin ? destination : -1;
    const auto target=s.destination>=0?Anchor[s.destination]:Point{here.x-12,here.y};
    s.heading=static_cast<std::uint8_t>(direction_to({target.x-here.x,target.y-here.y}));
    s.event_mask=0;
    route={};
    paused=false;
    notice="已接手船舵";
    return true;
}

void Game::clear_helm() {
    s.helm=s.heading=0;
    s.helm_x=s.helm_y=0;
}

int Game::visible_landmark() const {
    const auto here=position();
    const int radius=lookout_radius();
    int nearest=-1, best=radius*radius+1;
    for (int p=0;p<PlaceCount;++p) {
        if (!charted(p)) continue;
        const int dx=Anchor[p].x-here.x, dy=Anchor[p].y-here.y;
        const int distance=dx*dx+dy*dy;
        if (distance>=best) continue;
        nearest=p;
        best=distance;
    }
    return nearest;
}

HarborApproach Game::harbor_approach(int port, WaterTest water_at) const {
    HarborApproach course;
    if (!s.helm || !charted(port) || !water_at) return course;
    const Point ship=position(), anchor=Anchor[port];
    const int dx=ship.x-anchor.x, dy=ship.y-anchor.y;
    constexpr int radius=23, width=2*radius+1, cells=width*width, center=cells/2;
    if (std::abs(dx)>radius || std::abs(dy)>radius || !water_at(anchor)) return course;
    if (open_water(ship,anchor,water_at)) {
        course.points[0]=ship;
        course.points[1]=anchor;
        course.count=2;
        course.direct=true;
        return course;
    }

    // Search outward from the anchorage so every direction points back toward it.
    constexpr Point offsets[]{{0,-1},{1,-1},{1,0},{1,1},{0,1},{-1,1},{-1,0},{-1,-1}};
    constexpr std::uint8_t unvisited=255;
    std::array<std::uint8_t,cells> back;
    back.fill(unvisited);
    std::array<std::uint16_t,cells> queue;
    const int destination=center+dy*width+dx;
    queue[0]=center;
    back[center]=8;
    int tail=1;
    for (int head=0;head<tail && back[destination]==unvisited;++head) {
        const int node=queue[head], x=node%width, y=node/width;
        for (int bearing=0;bearing<8;++bearing) {
            const auto step=offsets[bearing];
            const int nx=x+step.x, ny=y+step.y;
            if (nx<0 || nx>=width || ny<0 || ny>=width) continue;
            const int next=ny*width+nx;
            if (back[next]!=unvisited) continue;
            const Point p{anchor.x+nx-radius,anchor.y+ny-radius};
            if (!water_at(p)) continue;
            if (step.x && step.y &&
                (!water_at({p.x-step.x,p.y}) || !water_at({p.x,p.y-step.y}))) continue;
            back[next]=(bearing+4)%8;
            queue[tail++]=next;
        }
    }
    if (back[destination]==unvisited) return course;
    int cell=destination;
    Point p=ship;
    while (course.count<int(course.points.size())-1 && cell!=center) {
        course.points[course.count++]=p;
        const auto step=offsets[back[cell]];
        cell+=step.y*width+step.x;
        p={p.x+step.x,p.y+step.y};
    }
    course.points[course.count++]=p;
    return course;
}

int Game::nearby_anchorage(WaterTest water_at) const {
    return s.helm?anchorage_at(position(),water_at):-1;
}

int Game::anchorage_at(Point here, WaterTest water_at) const {
    int best=26, index=-1;
    for (int i=0;i<PlaceCount;++i) {
        const int x=Anchor[i].x-here.x, y=Anchor[i].y-here.y;
        const int distance=x*x+y*y;
        if (distance>=best) continue;
        if (!open_water(here,Anchor[i],water_at)) continue;
        best=distance;
        index=i;
    }
    return index;
}

Point Game::wake_point(int index) const {
    return s.wake[(s.wake_head+WakeCapacity-s.wake_count+index)%WakeCapacity];
}

void Game::record_wake() {
    const auto point=position();
    if (s.wake_count) {
        const auto previous=wake_point(s.wake_count-1);
        if (point.x==previous.x && point.y==previous.y) return;
    }
    s.wake[s.wake_head]=point;
    s.wake_head=(s.wake_head+1)%WakeCapacity;
    s.wake_count=std::min(WakeCapacity,int(s.wake_count)+1);
}

bool Game::dock_helm(WaterTest water_at) {
    if (encounter()) return false;
    const int port=nearby_anchorage(water_at);
    if (port<0) { notice="请靠近锚地再停靠"; return false; }
    s.helm_x=Anchor[port].x*16;
    s.helm_y=Anchor[port].y*16;
    record_wake();
    s.destination=port;
    arrive();
    paused=false;
    return true;
}

void Game::helm_step(WaterTest water_at) {
    if (!s.helm || paused || encounter()) return;
    const bool started_near_drift=near_drift();
    const auto weather=conditions();
    const bool has_pilot=s.first_mate==Pilot;
    const auto [dx,dy]=helm_displacement(Directions[s.heading],s.helm_y,
                                       sailing_miles_per_day(s,load(),s.spirits,
                                                            wind_along(s.heading,weather),weather.weather));
    const int steps=std::max(1,(std::max(std::abs(dx),std::abs(dy))+3)/4);
    const int start_x=s.helm_x, start_y=s.helm_y;
    auto last_cell=position();
    const int starting_berth=anchorage_at(last_cell,water_at);
    bool reached_berth=false;
    Text interruption;
    for (int i=1;i<=steps;++i) {
        const int x=start_x+dx*i/steps, y=start_y+dy*i/steps;
        if (x<0 || y<0 || x>=ChartWidth*16 || y>=ChartHeight*16) {
            interruption="到达海图边界";
            break;
        }
        if (!water_at({x/16,y/16})) {
            s.hull-=std::max(1,(s.reefed?2:6)-(has_pilot?3:0));
            interruption="触及浅滩，转舵离岸";
            break;
        }
        s.helm_x=x;
        s.helm_y=y;
        const Point current{x/16,y/16};
        if (current.x==last_cell.x && current.y==last_cell.y) continue;
        last_cell=current;
        const int berth=anchorage_at(current,water_at);
        if (berth>=0 && berth!=starting_berth) {
            reached_berth=true;
            break;
        }
    }
    record_wake();
    const int found=sighting(position());
    if (found>=0) {
        notice=Places[found].name;
        paused=true;
    }
    if (reached_berth) {
        notice="已到锚地，可以靠泊";
        paused=true;
    }
    const bool new_day=++s.sail_watch==SailingWatches;
    if (new_day) {
        s.sail_watch=0;
        spend_day(1);
    }
    if (s.hull<=0) { rescue(); return; }
    if (new_day) {
        if (!consume_day(conditions())) return;
        if (weather.weather==Rain)
            s.water=static_cast<std::uint8_t>(std::min(water_capacity(),int(s.water)+1));
        if (weather.weather==Squall && !s.reefed) {
            s.hull-=3;
            notice="骤风伤船，缩帆可减损";
            if (s.hull<=0) { rescue(); return; }
        }
    }
    if (!interruption.empty()) {
        notice=interruption;
        paused=true;
        return;
    }
    if (!paused && !started_near_drift && near_drift()) {
        notice="木匣已靠近船舷，可以打捞";
        paused=true;
    }
    if (!new_day || paused) return;
    if (!(s.event_mask&1U) && convoy_battle()) {
        start_encounter(Privateer);
        notice="断铃号拦住了护航商船";
    } else if (s.tales[Lamps]==InProgress && s.beacon==BeaconUnseen && on_leg(madeira,canaries)) {
        start_encounter(FalseBeacon);
    } else {
        const int local=regional_encounter(position());
        if (local!=Calm && s.day%2==0 && roll(100)<40) start_encounter(local);
        else if (s.day%4==0 && roll(100)<30)
            start_encounter(weather.weather==Squall?Storm:roll(10)<5?ReliefShip:ReversedFlag);
    }
}

void Game::chart_passage(const Leg& leg, int progress) {
    for (int mark=s.progress+30;mark<progress;mark+=30)
        sighting(vessel_at(route,s.leg,float(mark)/leg.nautical_miles));
    sighting(vessel_at(route,s.leg,float(progress)/leg.nautical_miles));
}

int Game::regional_encounter(Point here) const {
    int kind=Calm;
    const auto shoal=Anchor[arguin];
    if (here.x>=shoal.x-82 && here.x<=shoal.x+5 && std::abs(here.y-shoal.y)<=34) {
        if (conditions_at(here,s.day).weather!=Squall) kind=ShoalLantern;
    } else if (here.x<Anchor[mpinda].x && std::abs(here.y-Anchor[equator].y)<=40) {
        const auto sea=conditions_at(here,s.day);
        if (sea.weather==Heat && sea.strength<=3) kind=Doldrums;
    }
    if (kind==Calm) return Calm;
    for (const auto event : s.sea_log) {
        const auto entry=read_sea(event);
        if (entry.kind==kind && s.day-entry.day<12) return Calm;
    }
    return kind;
}

void Game::advance() {
    if (!sea() || s.helm || paused || encounter()) return;
    spend_day(1);
    const auto weather=route_weather(route,s.leg,s.progress,s.day);
    if (!consume_day(weather)) return;
    const int w=route_wind(route,s.leg,s.day,weather);
    const int speed=sailing_miles_per_day(s,load(),s.spirits,w,weather.weather);
    const auto& leg = Legs[route.edge[s.leg]];
    if (s.escort==EscortSailing && on_leg(lisbon,madeira) && !(s.event_mask & (1U<<s.leg))) {
        chart_passage(leg,std::min(leg.nautical_miles-1,s.progress+speed));
        s.progress=std::min(leg.nautical_miles-1,s.progress+speed);
        start_encounter(Privateer);
        notice="断铃号拦住了护航商船";
        return;
    }
    if (s.tales[Lamps]==InProgress && s.beacon==BeaconUnseen && on_leg(madeira,canaries)) {
        chart_passage(leg,std::min(leg.nautical_miles-1,s.progress+speed));
        s.progress=std::min(leg.nautical_miles-1,s.progress+speed);
        start_encounter(FalseBeacon);
        return;
    }
    chart_passage(leg,std::min(leg.nautical_miles,s.progress+speed));
    s.progress += speed;
    if (s.progress >= leg.nautical_miles) {
        if (s.stop_next) {
            s.destination = route.reverse[s.leg] ? leg.from : leg.to;
            return arrive();
        }
        if (++s.leg == route.count) return arrive();
        s.progress = 0;
        return;
    }
    const auto here = position();
    if (s.progress > speed && !(s.event_mask & (1U << s.leg))) {
        int chance = here.y > 790 ? 32 : (w < -15 ? 23 : 18);
        if (s.patron && s.chapter>=2 && !s.ending) chance += 5;
        if (s.reefed) chance /= 2;
        if (roll(100) >= unsigned(chance)) return;
        const auto draw=roll(100);
        const int local=draw<50?regional_encounter(here):Calm;
        if (local!=Calm) start_encounter(local);
        else if (here.y>790 || draw<26) start_encounter(Storm);
        else if (draw<51) start_encounter(Privateer);
        else if (draw<63) start_encounter(Castaways);
        else if (draw<74) start_encounter(Rigging);
        else if (draw<85 || s.stores<20) start_encounter(ReliefShip);
        else if (draw<88) start_encounter(here.x<300 && here.y<530 ? FalseBeacon : Fog);
        else if (draw<94) start_encounter(ReversedFlag);
        else start_encounter(StrandedPost);
    }
}

void Game::arrive() {
    s.origin = s.destination;
    s.destination = -1;
    clear_helm();
    s.leg = s.progress = s.stop_next = 0;
    if (!(s.visited & (1U << s.origin))) {
        s.visited |= 1U << s.origin;
        if (port()) ++s.renown;
    }
    reveal_chart(Anchor[s.origin],26);
    notice = port() ? "安全入港" : "到达测绘水域";
    if (s.dispatch.number) {
        const auto& order=DispatchOrders[dispatch_order()];
        if (s.origin==order.stop && !s.dispatch.stopped) {
            s.dispatch.stopped=1;
            notice="已在中途港停靠";
        } else if (s.origin==dispatch_target()) notice="可以在港务交付运单";
    }
}

void Game::rescue() {
    earn(-160);
    for (auto& item : s.cargo) item /= 2;
    const auto here = position();
    int nearest = 0;
    for (int p = 1; p < PortCount; ++p)
        if (length(here, Anchor[p]) < length(here, Anchor[nearest])) nearest = p;
    s.origin = nearest;
    s.destination = -1;
    s.leg=s.progress=0;
    clear_helm();
    finish_encounter();
    s.stop_next=0;
    s.stores=24;
    s.water=24;
    s.hull = 45;
    s.spirits = 48;
    s.crew = std::max(8, s.crew - 3);
    s.visited |= 1U << nearest;
    reveal_chart(Anchor[nearest],26);
    if (s.dispatch.number) {
        finish_dispatch(DispatchLost);
        notice="获救入港，运单中断，押金已扣";
    } else notice="获商船救援，部分货物遗失";
}

bool Game::tow_needed() const {
    return !sea() && !encounter() && !port() &&
           (s.stores<daily_rations() || s.water<daily_water(Fair) || s.hull<=12);
}

bool Game::request_tow() {
    if (!tow_needed()) return false;
    int nearby=-1, distance=std::numeric_limits<int>::max();
    for (int p=0;p<PortCount;++p) {
        const auto route=passage(s.origin,p);
        if (route.count==0 || route.miles>=distance) continue;
        nearby=p;
        distance=route.miles;
    }
    if (nearby<0) return false;
    earn(-65);
    spend_day(1);
    s.hull=std::max(s.hull,30);
    s.stores=std::max(s.stores,12);
    s.water=static_cast<std::uint8_t>(std::max(int(s.water),12));
    s.destination=nearby;
    arrive();
    notice="救助船已拖带回港";
    return true;
}

TradeQuote Game::trade(int good, bool sell, int quantity) {
    const auto offer=trade_quote(good,sell,quantity);
    if (!offer.units) {
        notice=sell ? "没有可出售的货物" : "金币、舱位或库存不足";
        return offer;
    }
    apply_trade(good,sell,offer);
    if (offer.saved) {
        s.bargain_good=BargainUsed;
        s.bargain_rate=0;
    }
    notice=sell ? "已出售货物" : "已装货入舱";
    return offer;
}

void Game::apply_trade(int good, bool sell, TradeQuote offer) {
    s.cargo[good]+=sell?-offer.units:offer.units;
    s.stock_delta[s.origin*GoodsCount+good]=static_cast<std::int8_t>(offer.stock_change);
    earn(sell?offer.gold:-offer.gold);
}

void Game::service(int action) {
    if (!port()) return;
    const int next_ship = s.ship == 0 ? 800 : 1850;
    auto pay = [&](int price) {
        if (s.gold < price) { notice = "金币不足"; return false; }
        earn(-price);
        return true;
    };
    switch (action) {
    case 0:
        if (s.stores+s.water==2*max_stores()) { notice="补给已满"; break; }
        resupply(food_capacity(),water_capacity());
        break;
    case 1:
        if (s.hull == 100) { notice = "船体完好"; break; }
        if (pay(repair_cost())) { s.hull = 100; notice = "船体已修复"; }
        break;
    case 2:
        if (s.spirits == 100) { notice = "士气已满"; break; }
        if (pay(24)) { spend_day(2); s.spirits = 100; notice = "船员休整完毕"; }
        break;
    case 3:
        if (s.crew == crew_limit()) { notice = "床位已满"; break; }
        if (pay(12*(std::min(3, crew_limit()-s.crew)))) {
            s.crew = std::min(crew_limit(), s.crew+3);
            notice = "水手已登船";
        }
        break;
    case 4:
        if (s.ship == 2) { notice = "已有远洋商船"; break; }
        if (pay(next_ship)) {
            ++s.ship; s.hull = 100; s.stores=max_stores(); s.water=static_cast<std::uint8_t>(max_stores()); notice = "新船已接收";
        }
        break;
    case 5:
        if (s.guns == 3) { notice = "火炮已装满"; break; }
        if (pay(145+s.guns*120)) { ++s.guns; notice = "火炮已加装"; }
        break;
    case 6:
        if (s.gold<80 && (s.stores<24 || s.water<24 || s.hull<25)) {
            s.stores=std::max(s.stores,24);
            s.water=static_cast<std::uint8_t>(std::max(int(s.water),24));
            s.trust = std::max(0, s.trust-1);
            if (s.hull < 25) s.hull = 25;
            notice = "行会借给你应急补给";
        } else notice = "应急信贷供缺粮或船体破损时使用";
        break;
    }
}

void Game::take_contract() {
    if (!port()) return;
    if (s.contract >= 0 && s.day <= s.deadline) { notice = "已有未交付委托"; return; }
    s.contract = proposal();
    s.chart_marks|=1U<<s.contract;
    s.contract_good = proposal_good();
    s.deadline = s.day + forecast(passage(s.origin,s.contract)).days * 2 + 9;
    notice = "委托已记入航海日志";
}

void Game::deliver() {
    if (!port() || s.origin != s.contract) { notice = "请到指定港口交付"; return; }
    if (s.day > s.deadline) { s.contract = -1; notice = "已过交付期限"; return; }
    if (s.cargo[s.contract_good] < 3) { notice = "需要三件委托货物"; return; }
    s.cargo[s.contract_good] -= 3;
    earn(reward(s.origin));
    s.contract = -1;
    s.renown += 3;
    notice = "委托交付，赏金入账";
}

int Game::dispatch_target() const {
    if (!s.dispatch.number) return -1;
    const auto& order=DispatchOrders[dispatch_order()];
    return order.stop>=0 && !s.dispatch.stopped?order.stop:order.to;
}

int Game::dispatch_space() const {
    return s.dispatch.number?DispatchRates[DispatchOrders[dispatch_order()].kind].space:0;
}

bool Game::dispatch_board_here() const {
    if (!port()) return false;
    if (s.dispatch.number) return true;
    return std::any_of(std::begin(DispatchOrders),std::end(DispatchOrders),
                       [this](const auto& order) { return order.from==s.origin; });
}

DispatchQuote Game::dispatch_quote(int id) const {
    if (id<0 || id>=DispatchCount) return {};
    const auto& order=DispatchOrders[id];
    const auto& rate=DispatchRates[order.kind];
    auto path=passage(order.from,order.stop>=0?order.stop:order.to);
    if (!path.count) return {};
    if (order.stop>=0) {
        const auto second=passage(order.stop,order.to);
        if (!second.count || path.count+second.count>PlaceCount) return {};
        std::copy_n(second.edge.begin(),second.count,path.edge.begin()+path.count);
        std::copy_n(second.reverse.begin(),second.count,path.reverse.begin()+path.count);
        path.count+=second.count;
        path.miles+=second.miles;
    }
    return {forecast(path,dispatch_order()==id?0:rate.space).days,rate.fee+path.miles/7};
}

bool Game::dispatch_offer_here(int id) const {
    return id>=0 && id<DispatchCount && port() && DispatchOrders[id].from==s.origin &&
           (id!=RoadFreight || (road_timber_bonus()>0 && !s.road_delivery));
}

Text Game::dispatch_requirement(int id) const {
    if (id<0 || id>=DispatchCount || !port() || DispatchOrders[id].from!=s.origin) return "在发单港接单";
    if (!dispatch_offer_here(id)) return "暂无驿道货运";
    const auto& order=DispatchOrders[id];
    const auto& rate=DispatchRates[order.kind];
    if (s.dispatch.number) return "已有一份运单";
    if (!charted(order.to) || (order.stop>=0 && !charted(order.stop))) return "先定位沿途港口";
    if (capacity()-load()<rate.space) return "舱位不足";
    if (s.gold<rate.bond) return "押金不足";
    return {};
}

bool Game::accept_dispatch(int id) {
    if (!dispatch_requirement(id).empty()) return false;
    const auto quote=dispatch_quote(id);
    if (!quote.days) return false;
    const auto& order=DispatchOrders[id];
    const auto& rate=DispatchRates[order.kind];
    earn(-rate.bond);
    s.chart_marks|=1U<<order.to;
    if (order.stop>=0) s.chart_marks|=1U<<order.stop;
    s.dispatch={.due=std::min(99999,s.day+quote.days+rate.slack), .number=std::uint8_t(id+1),
                .last=s.dispatch.last, .outcome=s.dispatch.outcome};
    notice="已接运单，货件装船";
    return true;
}

bool Game::deliver_dispatch() {
    if (!s.dispatch.number || !port()) return false;
    const int id=dispatch_order();
    const auto& order=DispatchOrders[id];
    if (s.origin!=order.to || dispatch_target()!=order.to) return false;
    const auto& rate=DispatchRates[order.kind];
    const auto quote=dispatch_quote(id);
    const bool on_time=s.day<=s.dispatch.due;
    earn(rate.bond+(on_time?quote.fee:quote.fee/2));
    if (on_time) s.renown=std::min(999999,s.renown+rate.renown);
    if (id==RoadFreight) {
        const int base=stock_base(s.origin,Timber,s.day,road_timber_bonus());
        const int cap=stock_limit(s.origin,Timber,s.day,road_timber_bonus());
        s.stock_delta[s.origin*GoodsCount+Timber]=std::min(cap,stock(Timber)+rate.space)-base;
        s.road_delivery=true;
    }
    finish_dispatch(on_time?DispatchOnTime:DispatchLate);
    notice=on_time?"按期交付，押金已退":"逾期交付，运费减半";
    return true;
}

bool Game::cancel_dispatch() {
    if (!s.dispatch.number || !port()) return false;
    finish_dispatch(DispatchCancelled);
    notice="已退单，押金不退";
    return true;
}

void Game::finish_dispatch(DispatchOutcome outcome) {
    s.dispatch={.last=s.dispatch.number,.outcome=std::uint8_t(outcome)};
}

void Game::survey() {
    if (sea() || encounter() || port()) return;
    const auto bit = 1U << s.origin;
    if (s.surveyed & bit) { notice = "此处已测绘"; return; }
    s.surveyed |= bit;
    ++s.charts;
    s.renown += 2;
    earn(85 + (s.origin >= cape ? 45 : 0));
    notice = "新发现已载入图鉴";
}

void Game::navigator() {
    if (!port() || s.origin != tunis) return;
    if (s.allies & 1) { notice = "优素福已经在船上"; return; }
    if (s.cargo[Salt] >= 2) {
        s.cargo[Salt] -= 2;
        s.allies |= 1;
        s.trust += 2;
        notice = "优素福登船，航速提升";
    } else notice = "带来两件海盐，优素福愿意领航";
}

bool Game::has_officer(int officer) const {
    return officer>NoOfficer && officer<OfficerLimit && (s.officer_roster & (1U<<(officer-1)));
}

int Game::hiring_fee(int officer) const {
    return officer==Boatswain && s.cadiz_victory ? 85 : Officers[officer].fee;
}

bool Game::hire_officer(int officer) {
    if (!port() || officer<=NoOfficer || officer>=OfficerLimit ||
        s.origin!=Officers[officer].port || has_officer(officer)) return false;
    const int fee=hiring_fee(officer);
    if (s.gold<fee) { notice="船金不足，暂不能招募"; return false; }
    earn(-fee);
    s.officer_roster|=1U<<(officer-1);
    if (s.first_mate==NoOfficer) {
        s.first_mate=officer;
        notice="新同伴上任副官";
    } else notice="新同伴登船待命";
    return true;
}

bool Game::assign_officer(int officer) {
    if (!port() || officer==s.first_mate || (officer!=NoOfficer && !has_officer(officer))) return false;
    s.first_mate=officer;
    notice=officer==NoOfficer?"已卸任，留在船上":"已轮换副官";
    return true;
}

Fluency Game::fluency(Language language) const {
    if (language==Portuguese) return Fluent;
    if (language>=LanguageCount) return Unfamiliar;
    return static_cast<Fluency>((s.language_skills>>(2*language))&3U);
}

int Game::crew_interpreter(Language language) const {
    for (int id=Pilot;id<OfficerLimit;++id)
        if (has_officer(id) && std::ranges::find(Officers[id].languages,language)!=Officers[id].languages.end())
            return id;
    return NoOfficer;
}

bool Game::can_study(Language language) const {
    return language<LanguageCount && port() &&
           (language==PortSpeech[s.origin] || language==port_second_language(s.origin));
}

bool Game::local_interpreter(Language language) const {
    return can_study(language) && s.interpreter_port==s.origin;
}

bool Game::understands(Language language) const {
    return fluency(language)==Fluent || crew_interpreter(language)!=NoOfficer || local_interpreter(language);
}

int Game::lesson_fee(Language language) const { return fluency(language)==Unfamiliar?18:38; }
int Game::lesson_days(Language language) const { return fluency(language)==Unfamiliar?1:2; }

bool Game::hire_interpreter() {
    if (!port() || s.interpreter_port==s.origin || s.gold<InterpreterFee) return false;
    earn(-InterpreterFee);
    s.interpreter_port=s.origin;
    notice="口译已在码头等候";
    return true;
}

bool Game::take_lesson(Language language) {
    if (!can_study(language) || fluency(language)==Fluent || s.gold<lesson_fee(language)) return false;
    earn(-lesson_fee(language));
    spend_day(lesson_days(language));
    s.language_skills+=1U<<(2*language);
    notice=fluency(language)==Phrases?"学会了问路常用语":"已能通读手记和询问细节";
    return true;
}

bool Game::tide_ledger_here() const {
    return port() && (s.origin==tangier || (s.origin==cadiz && s.tide_ledger>=LedgerCopied));
}

bool Game::copy_tide_ledger() {
    if (!port() || s.origin!=tangier || s.tide_ledger!=LedgerUnseen || !understands(Arabic))
        return false;
    s.tide_ledger=LedgerCopied;
    notice="带抄本到加的斯渡口核对";
    return true;
}

bool Game::decide_tide_ledger(bool publish) {
    if (!port() || s.origin!=cadiz || s.tide_ledger!=LedgerCopied || !understands(Castilian))
        return false;
    s.tide_ledger=publish?LedgerPublished:LedgerReturned;
    if (publish) { s.trust+=2; ++s.renown; }
    else earn(100);
    s.chart_marks|=1U<<gibraltar;
    notice=publish?"渡口挂出了双语潮簿":"收下谢金，潮图留在船上";
    return true;
}

bool Game::take_drift_lead() {
    if (!port() || s.origin!=madeira || s.drift_stage!=DriftUnheard) return false;
    s.drift_stage=DriftSearching;
    notice="已圈出丰沙尔西南的搜索海域";
    return true;
}

Point Game::drift_position() const {
    const auto wind=conditions_at(DriftGround,s.day);
    const auto downwind=Directions[wind.direction];
    const int drift=std::clamp(wind.strength,1,4)+(s.day%3);
    return {DriftGround.x+downwind.x*drift/16,DriftGround.y+downwind.y*drift/16};
}

bool Game::drift_in_range(int radius) const {
    if (!s.helm || encounter() || s.drift_stage!=DriftSearching) return false;
    const auto boat=position(), drift=drift_position();
    const int dx=boat.x-drift.x, dy=boat.y-drift.y;
    return dx*dx+dy*dy<=radius*radius;
}

bool Game::sees_drift() const { return drift_in_range(lookout_radius()); }
bool Game::near_drift() const { return drift_in_range(DriftReach); }

Text Game::drift_block(DriftMethod method) const {
    if (!near_drift()) return "先掌舵靠近木匣";
    const auto weather=conditions();
    if (method==LaunchBoat && (weather.weather==Squall || weather.strength>=4))
        return "风浪太大，不能放艇";
    if (method==RigHook && !s.cargo[Timber]) return "架钩须用一件木料";
    return {};
}

bool Game::recover_drift(DriftMethod method) {
    if (const auto block=drift_block(method); !block.empty()) { notice=block; return false; }
    if (method==RigHook) --s.cargo[Timber];
    s.drift_stage=DriftRecovered;
    paused=true;
    notice="木匣已捞起，带去加的斯";
    return true;
}

bool Game::wait_for_drift() {
    if (!near_drift() || !observation_day()) return false;
    paused=true;
    notice=near_drift()?"原地等候一日，已耗水粮":"等候一日，木匣漂出了打捞范围";
    return true;
}

bool Game::finish_drift(bool return_to_crew) {
    if (!port() || s.origin!=cadiz || s.drift_stage!=DriftRecovered || !understands(Castilian))
        return false;
    s.drift_stage=return_to_crew?DriftReturned:DriftFiled;
    if (return_to_crew) {
        s.trust+=3;
        ++s.renown;
        notice="船工认出了名单，本港维修降价";
    } else {
        earn(180);
        s.renown+=2;
        notice="港务馆收下名册，赏金一百八十";
    }
    return true;
}

int Game::archive_here() const {
    if (!port()) return -1;
    for (int i=0;i<ExpeditionCount;++i)
        if (Expeditions[i].archive==s.origin) return i;
    return -1;
}

int Game::expedition_here() const {
    if (sea() || encounter()) return -1;
    for (int i=0;i<ExpeditionCount;++i)
        if (Expeditions[i].site==s.origin && s.expeditions[i].stage==AtSeaSearch) return i;
    return -1;
}

bool Game::read_expedition(int id) {
    if (id<0 || archive_here()!=id || s.expeditions[id].stage!=UnknownClue) return false;
    if (!understands(BookLanguage[id])) return false;
    auto& record=s.expeditions[id];
    record.stage=ReadClue;
    record.rival_day=std::min(99999,s.day+Expeditions[id].race_days);
    notice="手记已抄入考察簿，另一艘船也在找线索";
    return true;
}

bool Game::consult_witness(int id) {
    if (id<0 || archive_here()!=id) return false;
    auto& record=s.expeditions[id];
    if (record.stage<ReadClue || record.stage>=HasEvidence || (record.flags&HeardWitness)) return false;
    if (!understands(WitnessLanguage[id])) return false;
    record.flags|=HeardWitness;
    s.chart_marks|=1U<<Expeditions[id].site;
    notice="口述线索已抄入考察簿";
    return true;
}

bool Game::launch_expedition(int id, bool sponsor) {
    if (id<0 || archive_here()!=id || s.expeditions[id].stage!=ReadClue) return false;
    auto& record=s.expeditions[id];
    record.stage=AtSeaSearch;
    if (sponsor) {
        record.flags|=Sponsored;
        earn(Expeditions[id].advance);
    }
    notice=sponsor?"已领取预支，回馆交付观测记录":"已登记自费考察";
    return true;
}

Text Game::observation_block(int id) const {
    if (id<0 || expedition_here()!=id) return "请先抵达考察水域";
    const auto weather=conditions();
    if (weather.weather==Squall) return "骤风中无法观测";
    if (id==BirdsAtLowWater && (s.day%2)) return "潮水淹没沙洲，等退潮";
    return {};
}

bool Game::observation_day() {
    const int water_used=daily_water(conditions().weather);
    if (s.stores<daily_rations() || s.water<water_used) {
        notice="水粮不足，须先补给";
        return false;
    }
    s.stores-=daily_rations();
    s.water-=water_used;
    spend_day(1);
    return true;
}

bool Game::wait_to_observe(int id) {
    if (id<0 || expedition_here()!=id || !observation_day()) return false;
    notice="在水域等候一日，消耗了水粮";
    return true;
}

bool Game::observe(int id, int method) {
    if (method<0 || method>1) return false;
    if (auto block=observation_block(id); !block.empty()) { notice=block; return false; }
    if (!observation_day()) return false;
    const auto& study=Expeditions[id];
    if (method!=study.method) {
        notice=study.retry;
        return false;
    }
    auto& record=s.expeditions[id];
    record.stage=HasEvidence;
    if (s.day<=record.rival_day) record.flags|=FirstObserver;
    notice=record.flags&FirstObserver?"抢先取得记录，返回档案室":"对手已先行，仍可回馆提交复测";
    return true;
}

int Game::expedition_purse(int id) const {
    const auto& record=s.expeditions[id];
    const auto& study=Expeditions[id];
    int purse=record.stage>=HasEvidence && !(record.flags&FirstObserver)?study.purse/2:study.purse;
    if (record.flags&Sponsored) purse-=study.advance*3/2;
    return purse;
}

bool Game::file_expedition(int id) {
    if (id<0 || archive_here()!=id || s.expeditions[id].stage!=HasEvidence) return false;
    auto& record=s.expeditions[id];
    earn(expedition_purse(id));
    s.renown=std::min(999999,s.renown+(record.flags&FirstObserver?4:2));
    record.stage=FiledReport;
    notice="记录已入藏，赏金入账";
    return true;
}

void Game::chapter_choice(int choice) {
    if (sea() || encounter() || s.chapter >= Chapters || Story[s.chapter].place != s.origin) return;
    switch (s.chapter) {
    case 0:
        if (choice) { earn(220); s.patron = 1; }
        else ++s.trust;
        break;
    case 1:
        if (choice) {
            if (s.gold < 70) { notice = "需要70金币"; return; }
            earn(-70);
        } else { spend_day(s.tales[Lamps]==Shared ? 1 : 2); s.trust += 2; earn(85); }
        break;
    case 2:
        if (choice) { earn(140); s.patron = 1; }
        else {
            s.trust+=2;
            s.stores=std::max(s.stores,std::min(max_stores(),food_capacity()));
            s.water=static_cast<std::uint8_t>(std::max(int(s.water),std::min(max_stores(),water_capacity())));
            s.hull=std::max(s.hull,82);
        }
        break;
    case 3:
        if (!choice) { survey(); ++s.trust; }
        break;
    case 4:
        if (choice) { earn(260); s.patron = 1; }
        else { s.trust += 2; s.allies |= 2; }
        break;
    case 5:
        s.ending = choice ? 2 : 1;
        earn(choice ? 1700 : 800 + s.trust*100);
        s.renown += choice ? 4 : 10;
        break;
    }
    ++s.chapter;
    if (s.chapter<Chapters) s.chart_marks|=1U<<Story[s.chapter].place;
    ++s.renown;
    notice = s.ending ? "结局已记进日志" : "日志里记下了下一站";
}

bool Game::on_leg(int from, int to) const {
    if (s.helm) {
        if (s.origin!=from || s.destination!=to) return false;
        const auto start=Anchor[from], end=Anchor[to], here=position();
        return (here.x-start.x)*(end.x-start.x)+(here.y-start.y)*(end.y-start.y)>0;
    }
    if (!sea() || s.leg>=route.count) return false;
    const auto& leg=Legs[route.edge[s.leg]];
    const bool reverse=route.reverse[s.leg];
    return (reverse ? leg.to : leg.from)==from && (reverse ? leg.from : leg.to)==to;
}

bool Game::convoy_battle() const {
    return s.escort==EscortSailing && on_leg(lisbon,madeira);
}

bool Game::accept_escort() {
    if (!port() || s.origin!=lisbon || s.escort!=EscortOpen) return false;
    s.escort=EscortSailing;
    notice="已接下银铃护航";
    return true;
}

bool Game::deliver_escort(bool publish) {
    if (!port() || s.origin!=madeira || (s.escort!=EscortWonGuns && s.escort!=EscortWonDuel))
        return false;
    s.escort=publish?EscortPublished:EscortSold;
    earn(publish?210:340);
    s.renown+=publish?4:2;
    if (publish) s.trust+=3;
    notice=publish?"船契已贴在码头":"船契已私下交付";
    return true;
}

bool Game::guild_stake(int port) const {
    return port>=0 && port<PortCount && (s.guild_stakes & (1U<<port));
}

int Game::guild_cost() const { return port()?240+Zone[s.origin]*40:0; }

bool Game::invest() {
    if (!port() || guild_stake(s.origin)) return false;
    if (s.gold<guild_cost()) { notice="金币不足"; return false; }
    earn(-guild_cost());
    s.guild_stakes |= 1U<<s.origin;
    ++s.renown;
    notice="商栈已入股，本港买价下调";
    return true;
}

bool Game::start_dock_duel() {
    if (!port() || s.origin!=cadiz || s.cadiz_victory) return false;
    start_duel(DockDuel);
    notice="费罗在裁判面前举剑";
    return true;
}

void Game::start_encounter(int kind) {
    s.event_mask |= 1U << s.leg;
    s.weather=kind;
    if (kind==Privateer) {
        s.enemy=convoy_battle()?42:35+s.ship*11+int(roll(18));
        s.range=3;
        s.aim=s.naval_turn=s.needs_reload=0;
    }
    if (kind==FalseBeacon && s.tales[Lamps]==InProgress && on_leg(madeira,canaries)) s.beacon=BeaconSighted;
}

Text Game::encounter_requirement(int choice) const {
    if (s.weather==Privateer && choice==FireGuns && s.needs_reload)
        return "火炮需装填，可先机动或转向";
    if (s.weather==Privateer && choice==BoardShip && (s.range!=1 || s.crew<6))
        return "接舷需要最近船距及至少六名船员";
    if (s.weather==ReversedFlag && choice==1 && s.water<4) return "需要四份淡水";
    if (s.weather==StrandedPost && choice==0 && s.charts<1) return "需要测绘过一处水域";
    if (s.weather==StrandedPost && choice==1 && !s.cargo[Timber]) return "需要一件木料";
    if (s.weather==FalseBeacon && choice==0 && s.tales[Lamps]!=Shared && s.stores<3)
        return "核对潮表需要三份口粮";
    if (s.weather==Rigging && choice==0 && !s.cargo[Timber]) return "需要一件木料";
    if (s.weather==ReliefShip && choice!=2 && s.water==water_capacity()) return "淡水已满";
    if (s.weather==ReliefShip && choice==0 && !s.cargo[Salt]) return "需要一件海盐";
    if (s.weather==ReliefShip && choice==1 && s.gold<water_price()) return "金币不足";
    if (s.weather==Fog && choice==1 && s.charts<2) return "需测绘两处水域";
    if (s.weather==ShoalLantern && choice==0 && !s.cargo[Timber]) return "需要一件木料";
    if (s.weather==ShoalLantern && choice==1 && s.first_mate!=Pilot && !(s.surveyed&(1U<<arguin)))
        return "需安娜领航或测过阿尔金";
    if (s.weather==Doldrums && choice==0 && !s.cargo[Cloth]) return "需要一件织锦";
    if (s.weather==Doldrums && choice==1 && s.first_mate!=Pilot && s.charts<2)
        return "需安娜领航或测过两处水域";
    return {};
}

void Game::log_encounter(int choice) {
    for (std::size_t i=s.sea_log.size()-1;i>0;--i) s.sea_log[i]=s.sea_log[i-1];
    s.sea_log[0]=record_sea(s.weather,choice,s.day);
}

void Game::finish_encounter() {
    s.weather=s.enemy=s.range=s.aim=0;
    s.naval_turn=s.needs_reload=s.duel_turn=s.vigor=s.rival=0;
}

int Game::naval_intent() const {
    if (s.enemy<=13) return EnemyEvade;
    if (s.aim && s.range>1 && s.enemy<=25) return EnemyEvade;
    if (s.range>=4 || (s.range>1 && s.needs_reload)) return EnemyClose;
    return EnemyFire;
}

int Game::duel_intent() const {
    return (s.duel_turn+(s.weather==Boarding?s.naval_turn%3:0))%3;
}

int Game::vigor_limit() const {
    if (s.weather==DockDuel) return 44;
    return 36+std::min(12,(s.crew-4)/2)+(s.cadiz_victory?8:0)+(s.first_mate==Boatswain?6:0);
}

int Game::rival_limit() const { return s.weather==DockDuel?44:30+s.enemy/4; }

void Game::start_duel(int kind) {
    s.weather=kind;
    s.duel_turn=0;
    s.vigor=static_cast<std::uint8_t>(vigor_limit());
    s.rival=static_cast<std::uint8_t>(rival_limit());
}

void Game::win_at_sea(bool boarded) {
    const bool convoy=convoy_battle();
    if (convoy) s.escort=boarded?EscortWonDuel:EscortWonGuns;
    earn(boarded?155+s.ship*45:135+s.ship*65);
    s.renown+=boarded?3:2;
    log_encounter(0);
    finish_encounter();
    notice=convoy ? "商船获救，船契带往丰沙尔" :
           boarded ? "赢得决斗，收下对方船长的赎金" : "击退私掠船，捞起散落货银";
}

void Game::naval_action(int action) {
    const int intent=naval_intent();
    const int breeze=wind(s.leg);
    if (action==BoardShip) {
        start_duel(Boarding);
        notice="两船接舷";
        return;
    }
    if (action==FireGuns) {
        const int hit=11+s.guns*8+s.aim*8-(s.range-1)*3+int(roll(6));
        s.enemy-=std::max(3,hit);
        s.aim=0;
        s.needs_reload=1;
        if (s.enemy<=0) { win_at_sea(false); return; }
        notice="命中，待装填";
    } else {
        s.needs_reload=0;
        if (action==CloseRange) {
            s.range=std::max(1,s.range-1);
            s.aim=1;
            notice="装填并瞄准";
        } else {
            const int chance=std::clamp(29+s.range*14+s.ship*4+breeze/2-
                    load()*16/capacity()+(s.allies&1?8:0)-(s.reefed?10:0),10,90);
            if (int(roll(100))<chance) {
                const bool convoy=convoy_battle();
                log_encounter(1);
                finish_encounter();
                notice=convoy?"护航失散，可从里斯本重试":"借风摆脱私掠船";
                return;
            }
            s.aim=0;
            s.range=std::min(4,s.range+1);
            notice="未能脱身，已装填火炮";
        }
    }
    if (intent==EnemyFire) {
        const int hit=std::max(2,13-s.range*2+s.ship+int(roll(5))-(action==BreakAway?3:0));
        s.hull-=std::max(0,hit-(s.first_mate==Boatswain?3:0));
        s.spirits-=3;
    } else if (intent==EnemyClose) {
        s.range=std::max(1,s.range-1);
        s.hull-=2;
    } else {
        s.range=std::min(4,s.range+1);
    }
    s.naval_turn=(s.naval_turn+1)%3;
    if (s.hull<=0) log_encounter(2);
    check_ship();
}

void Game::lose_duel(bool yielded) {
    log_encounter(yielded?1:2);
    if (s.weather==DockDuel) {
        spend_day(1);
        if (yielded) earn(-25);
        notice=yielded?"向裁判交了押金，明日可再来":"费罗收剑，约你明日再来";
    } else {
        s.hull-=yielded?6:13;
        s.crew=std::max(4,s.crew-(yielded?1:2));
        s.spirits-=8;
        notice=yielded?"割断缆绳，脱离敌船":"决斗落败，船员掩护你撤回";
    }
    finish_encounter();
    check_ship();
}

void Game::duel_action(int action) {
    if (action==Yield) return lose_duel(true);
    const int advantage=(action+3-duel_intent())%3;
    const int hurt=advantage==1 ? 0 : advantage==0 ? 5 : 14;
    constexpr int counter_damage[]{18,12,16};
    const int hit=advantage==1 ? counter_damage[action] : advantage==0 ? 6 : 0;
    if (hurt>=s.vigor) return lose_duel(false);
    s.vigor-=hurt;
    if (hit>=s.rival) {
        if (s.weather==Boarding) return win_at_sea(true);
        s.cadiz_victory=1;
        earn(110);
        s.renown+=3;
        log_encounter(0);
        finish_encounter();
        notice="赢得旧誓决斗，接舷体力永久增强";
        return;
    }
    s.rival-=hit;
    s.duel_turn=(s.duel_turn+1)%3;
    notice=advantage==1?"识破动作，反击得手":advantage==0?"双方交锋，互有损耗":"失了先机，体力下降";
}

void Game::check_ship() {
    s.hull = std::min(s.hull, 100);
    s.spirits = std::clamp(s.spirits, 0, 100);
    if (s.hull <= 0) rescue();
}

void Game::resolve_regional_encounter(int choice) {
    const int kind=s.weather;
    const bool delayed=kind==ShoalLantern?choice==1:choice!=1;
    if (kind==ShoalLantern && choice==0) --s.cargo[Timber];
    if (kind==Doldrums && choice==0) {
        --s.cargo[Cloth];
        s.water=static_cast<std::uint8_t>(std::min(water_capacity(),int(s.water)+10));
    }
    if (delayed) spend_day(1);
    log_encounter(choice);
    if (delayed && !consume_day(conditions())) return;
    if (kind==ShoalLantern && choice!=2 && !heard_rumor(ArguinRumor)) {
        s.heard_rumors|=1U<<ArguinRumor;
        if (choice==0) ++s.trust;
        else ++s.renown;
    }
    notice=Encounters[kind].results[choice];
    finish_encounter();
}

void Game::encounter_action(int action) {
    if (!encounter() || action<0 || action>((duel() || s.weather==Privateer)?3:2)) return;
    if (const auto why=encounter_requirement(action); !why.empty()) { notice=why; return; }
    if (duel()) return duel_action(action);
    if (s.weather==Privateer) return naval_action(action);
    if (s.weather==ShoalLantern || s.weather==Doldrums) return resolve_regional_encounter(action);
    if (s.weather == Storm) {
        const int loss = action == 0 ? 4 : (action == 1 ? 12 : 20);
        s.hull -= std::max(1, loss + int(roll(8)) - (s.reefed ? 5 : 0));
        if (action == 0) { spend_day(1); s.stores = std::max(0,s.stores-4); s.reefed = 1; }
        if (action == 2) { s.spirits -= 12; s.progress = std::max(0,s.progress-90); }
        notice = action == 0 ? "缩帆熬过风暴" : (action == 1 ? "船已冲出风暴" : "顶浪转舵，航程倒退");
        log_encounter(action);
        finish_encounter();
    } else if (s.weather == Castaways) {
        if (action == 0) {
            s.stores = std::max(0, s.stores-4); earn(105); ++s.trust;
            notice = "渔船获救，家属送来谢礼";
        } else if (action == 1) {
            s.water=static_cast<std::uint8_t>(std::min(water_capacity(),int(s.water)+11));
            notice = "漂桶内的淡水仍可用";
        } else notice = "继续航行";
        log_encounter(action);
        finish_encounter();
    } else {
        switch (s.weather) {
        case FalseBeacon:
            if (action==0) {
                if (s.tales[Lamps]!=Shared) {
                    spend_day(1);
                    s.stores-=3;
                }
                if (s.tales[Lamps]==InProgress && on_leg(madeira,canaries)) s.beacon=BeaconCharted;
            } else if (action==1) {
                s.hull-=13+int(roll(9));
            } else {
                spend_day(1);
                s.stores=std::max(0,s.stores-daily_rations());
                s.water=static_cast<std::uint8_t>(std::max(0,int(s.water)-daily_water(Fair)));
            }
            break;
        case Rigging:
            if (action==0) {
                --s.cargo[Timber];
                s.hull+=5;
            } else if (action==1) {
                s.reefed=1;
            } else {
                s.hull-=9+int(roll(7));
            }
            break;
        case ReliefShip:
            if (action==0) {
                --s.cargo[Salt];
                s.water=static_cast<std::uint8_t>(std::min(water_capacity(),int(s.water)+18));
            } else if (action==1) {
                earn(-water_price());
                s.water=static_cast<std::uint8_t>(std::min(water_capacity(),int(s.water)+14));
            }
            break;
        case ReversedFlag:
            if (action==0) {
                log_encounter(action);
                start_encounter(Privateer);
                s.range=2;
                s.aim=1;
                notice="借倒挂旗号接近追船";
                return;
            }
            if (action==1) { s.water-=4; ++s.trust; ++s.renown; }
            else spend_day(1);
            break;
        case StrandedPost:
            if (action==0) { earn(125); s.renown+=2; }
            else if (action==1) { --s.cargo[Timber]; earn(90); spend_day(1); ++s.trust; }
            else ++s.renown;
            break;
        case Fog:
            if (action==0) {
                spend_day(1);
                s.stores=std::max(0,s.stores-daily_rations());
                s.water=static_cast<std::uint8_t>(std::max(0,int(s.water)-daily_water(Fair)));
            } else if (action==2) {
                s.hull-=7+int(roll(12));
            }
            break;
        }
        notice=Encounters[s.weather].results[action];
        log_encounter(action);
        finish_encounter();
    }
    check_ship();
}

static_assert(4 + 28*4 + 4*4 + GoodsCount + PortCount*GoodsCount +
              TaleCount + 1 + 3*4 + 4 == LegacySaveBytes);
static_assert(CombatSaveBytes==LegacySaveBytes+7+4+5);
static_assert(HelmSaveBytes>=LegacySaveBytes+7+4+8+4);
static_assert(CoastSaveBytes>=LegacySaveBytes-4+7+4+12+6*ExpeditionCount+2+8+ChartBytes+4);
constexpr auto WakeRecordBytes=LegacySaveBytes-4+7+4+12+6*ExpeditionCount+2+8+ChartBytes+2+4*WakeCapacity;
static_assert(WakeSaveBytes>=WakeRecordBytes+4);
static_assert(LanguageSaveBytes>=WakeRecordBytes+9+4);
static_assert(SaveBytes>=WakeRecordBytes+9+13*MerchantNames.size()+1+8+3+4);

std::array<std::byte, SaveBytes> encode(const State& s) {
    std::array<std::byte, SaveBytes> bytes{};
    std::size_t offset = 0;
    auto put = [&](std::uint32_t value) {
        for (int i = 0; i < 4; ++i) bytes[offset++] = std::byte(value >> (i*8));
    };
    put(0x41445054);
    for (const int field : {
        s.gold,s.day,s.hull,s.stores,s.spirits,s.ship,s.guns,s.crew,s.charts,s.renown,s.trust,
        s.origin,s.destination,s.leg,s.progress,s.reefed,s.chapter,s.ending,s.patron,s.allies,
        s.contract,s.contract_good,s.deadline,s.weather,s.enemy,s.range,s.aim,s.stop_next}) put(field);
    for (const auto field : {s.visited,s.surveyed,s.rng,s.event_mask}) put(field);
    for (int count : s.cargo) bytes[offset++] = std::byte(count);
    for (auto delta : s.stock_delta) bytes[offset++] = std::byte(delta + 48);
    for (auto state : s.tales) bytes[offset++]=std::byte(state);
    bytes[offset++]=std::byte(s.beacon);
    for (auto event : s.sea_log) put(event);
    for (auto field : {s.naval_turn,s.needs_reload,s.duel_turn,s.vigor,s.rival,s.escort,s.cadiz_victory})
        bytes[offset++]=std::byte(field);
    put(s.guild_stakes);
    put(s.helm_x); put(s.helm_y);
    for (auto value : {s.water,s.helm,s.heading,s.sail_watch}) bytes[offset++]=std::byte(value);
    for (const auto& record : s.expeditions) {
        bytes[offset++]=std::byte(record.stage);
        bytes[offset++]=std::byte(record.flags);
        put(record.rival_day);
    }
    bytes[offset++]=std::byte(s.officer_roster);
    bytes[offset++]=std::byte(s.first_mate);
    put(s.chart_marks);
    put(s.heard_rumors);
    for (auto cell : s.coast) bytes[offset++]=std::byte(cell);
    bytes[offset++]=std::byte(s.wake_head);
    bytes[offset++]=std::byte(s.wake_count);
    for (const auto point : s.wake) put(std::uint32_t(point.x)|(std::uint32_t(point.y)<<16));
    put(s.language_skills);
    bytes[offset++]=std::byte(s.interpreter_port+1);
    bytes[offset++]=std::byte(s.tide_ledger);
    // Earlier language saves left these reserved bytes at zero.
    bytes[offset++]=std::byte(s.bargain_good);
    bytes[offset++]=std::byte(s.bargain_rate);
    bytes[offset++]=std::byte(s.drift_stage);
    for (const auto& ship : s.merchants) {
        put(ship.gold);
        put(ship.progress);
        for (auto value : {ship.port,ship.destination,ship.good,ship.units,ship.held})
            bytes[offset++]=std::byte(value);
    }
    // Reuse one padding byte. Existing merchant saves initialized it to zero.
    bytes[offset++]=std::byte(s.port_stories);
    for (auto field : {s.dispatch.number,s.dispatch.stopped,s.dispatch.last,s.dispatch.outcome})
        bytes[offset++]=std::byte(field);
    put(s.dispatch.due);
    bytes[offset++]=std::byte(SailingTimeRevision);
    bytes[offset++]=std::byte(s.ration_flags);
    bytes[offset++]=std::byte(s.bargain_warnings|(s.timber_trail<<2)|(s.land_stage<<4)|
                              (s.road_delivery?0x80:0));
    offset=bytes.size()-4;
    put(checksum(bytes));
    return bytes;
}

bool decode(State& output, std::span<const std::byte> bytes) {
    if (bytes.size()!=LegacySaveBytes && bytes.size()!=CombatSaveBytes &&
        bytes.size()!=HelmSaveBytes && bytes.size()!=OfficerSaveBytes &&
        bytes.size()!=CoastSaveBytes && bytes.size()!=WakeSaveBytes &&
        bytes.size()!=LanguageSaveBytes && bytes.size()!=SaveBytes) return false;
    std::size_t offset=bytes.size()-4;
    auto get = [&]() {
        std::uint32_t value = 0;
        for (int i = 0; i < 4; ++i) value |= std::to_integer<std::uint32_t>(bytes[offset++]) << (8*i);
        return value;
    };
    if (get() != std::uint32_t(checksum(bytes))) return false;
    offset = 0;
    const auto version=get();
    const bool with_merchants=version==0x41445054 && bytes.size()==SaveBytes;
    const bool with_languages=with_merchants || (version==0x39445054 && bytes.size()==LanguageSaveBytes);
    const bool with_wake=with_languages || (version==0x38445054 && bytes.size()==WakeSaveBytes);
    const bool with_coast=with_wake || (version==0x37445054 && bytes.size()==CoastSaveBytes);
    const bool with_officers=with_coast || (version==0x36445054 && bytes.size()==OfficerSaveBytes);
    const bool with_expeditions=with_officers || (version==0x35445054 && bytes.size()==OfficerSaveBytes);
    const bool with_helm=with_expeditions || (version==0x34445054 && bytes.size()==HelmSaveBytes);
    const bool current=with_helm || (version==0x33445054 && bytes.size()==CombatSaveBytes);
    if (!current && (bytes.size()!=LegacySaveBytes ||
        (version!=0x31445054 && version!=0x32445054))) return false;
    State s;
    bool legacy_sailing_time=true;
    for (auto* field : {
        &s.gold,&s.day,&s.hull,&s.stores,&s.spirits,&s.ship,&s.guns,&s.crew,&s.charts,&s.renown,&s.trust,
        &s.origin,&s.destination,&s.leg,&s.progress,&s.reefed,&s.chapter,&s.ending,&s.patron,&s.allies,
        &s.contract,&s.contract_good,&s.deadline,&s.weather,&s.enemy,&s.range,&s.aim,&s.stop_next}) *field = int(get());
    for (auto* field : {&s.visited,&s.surveyed,&s.rng,&s.event_mask}) *field = get();
    for (auto& count : s.cargo) count = std::to_integer<int>(bytes[offset++]);
    for (auto& delta : s.stock_delta) {
        const int stock = std::to_integer<int>(bytes[offset++])-48;
        if (stock > 48 || stock < -48) return false;
        delta = static_cast<std::int8_t>(stock);
    }
    if (version!=0x31445054) {
        for (auto& state : s.tales) {
            state=std::to_integer<std::uint8_t>(bytes[offset++]);
            if (state>Sold) return false;
        }
        s.beacon=std::to_integer<std::uint8_t>(bytes[offset++]);
        if (s.beacon>BeaconCharted) return false;
        int ceiling=s.day;
        for (auto& event : s.sea_log) {
            event=get();
            if (!event) { ceiling=-1; continue; }
            const auto entry=read_sea(event);
            if (entry.kind<=Calm || entry.kind>=(current?EncounterCount:Boarding) ||
                entry.choice>2 || entry.day>ceiling)
                return false;
            ceiling=entry.day;
        }
    }
    if (current) {
        for (auto* field : {&s.naval_turn,&s.needs_reload,&s.duel_turn,&s.vigor,&s.rival,&s.escort,&s.cadiz_victory})
            *field=std::to_integer<std::uint8_t>(bytes[offset++]);
        s.guild_stakes=get();
        if (s.naval_turn>2 || s.needs_reload>1 || s.duel_turn>2 || s.vigor>(with_officers?62:56) || s.rival>55 ||
            s.escort>EscortSold || s.cadiz_victory>1 || s.guild_stakes>>PortCount) return false;
    }
    if (with_helm) {
        s.helm_x=int(get()); s.helm_y=int(get());
        for (auto* value : {&s.water,&s.helm,&s.heading,&s.sail_watch})
            *value=std::to_integer<std::uint8_t>(bytes[offset++]);
        if (s.helm>1 || s.heading>7 || s.sail_watch>=SailingWatches) return false;
    } else {
        if (s.stores<0 || s.stores>255) return false;
        s.water=static_cast<std::uint8_t>(s.stores);
    }
    if (with_expeditions) {
        for (auto& record : s.expeditions) {
            record.stage=std::to_integer<std::uint8_t>(bytes[offset++]);
            record.flags=std::to_integer<std::uint8_t>(bytes[offset++]);
            record.rival_day=int(get());
            if (record.stage>FiledReport || record.flags>(HeardWitness|Sponsored|FirstObserver) ||
                record.rival_day<0 || record.rival_day>99999 ||
                (record.stage==UnknownClue && (record.flags || record.rival_day)) ||
                (record.stage!=UnknownClue && !record.rival_day) ||
                (record.stage<AtSeaSearch && (record.flags&Sponsored)) ||
                (record.stage<HasEvidence && (record.flags&FirstObserver))) return false;
        }
    }
    if (with_officers) {
        s.officer_roster=std::to_integer<std::uint8_t>(bytes[offset++]);
        s.first_mate=std::to_integer<std::uint8_t>(bytes[offset++]);
        if (s.officer_roster>>(OfficerLimit-1) || s.first_mate>=OfficerLimit ||
            (s.first_mate!=NoOfficer && !(s.officer_roster & (1U<<(s.first_mate-1))))) return false;
    }
    if (with_coast) {
        s.chart_marks=get();
        s.heard_rumors=get();
        if (s.chart_marks>>PlaceCount || s.heard_rumors>>RumorCount) return false;
        for (auto& cell : s.coast) cell=std::to_integer<std::uint8_t>(bytes[offset++]);
    }
    if (with_wake) {
        s.wake_head=std::to_integer<std::uint8_t>(bytes[offset++]);
        s.wake_count=std::to_integer<std::uint8_t>(bytes[offset++]);
        if (s.wake_head>=WakeCapacity || s.wake_count>WakeCapacity ||
            (s.wake_count<WakeCapacity && s.wake_head!=s.wake_count)) return false;
        for (auto& point : s.wake) {
            const auto value=get();
            point={int(value&0xffffU),int(value>>16)};
            if (point.x>=ChartWidth || point.y>=ChartHeight) return false;
        }
    }
    if (with_languages) {
        s.language_skills=get();
        s.interpreter_port=std::to_integer<int>(bytes[offset++])-1;
        s.tide_ledger=std::to_integer<std::uint8_t>(bytes[offset++]);
        s.bargain_good=std::to_integer<std::uint8_t>(bytes[offset++]);
        s.bargain_rate=std::to_integer<std::uint8_t>(bytes[offset++]);
        s.drift_stage=std::to_integer<std::uint8_t>(bytes[offset++]);
        const bool pending_deal=s.bargain_good>0 && s.bargain_good<=GoodsCount;
        if (s.language_skills>>(2*LanguageCount) || (s.language_skills&3U) ||
            s.tide_ledger>LedgerReturned || s.drift_stage>DriftFiled || s.interpreter_port>=PortCount ||
            (s.interpreter_port>=0 && (s.interpreter_port!=s.origin || s.helm || s.destination>=0)))
            return false;
        if (s.bargain_good>BargainUsed ||
            (pending_deal ? s.bargain_rate!=4 && s.bargain_rate!=8 : s.bargain_rate!=0) ||
            (s.bargain_good && (s.origin>=PortCount || s.helm || s.destination>=0)))
            return false;
        for (int language=1;language<LanguageCount;++language)
            if (((s.language_skills>>(2*language))&3U)>Fluent) return false;
    }
    if (with_merchants) {
        for (auto& ship : s.merchants) {
            const auto gold=get(), progress=get();
            for (auto* value : {&ship.port,&ship.destination,&ship.good,&ship.units,&ship.held})
                *value=std::to_integer<std::uint8_t>(bytes[offset++]);
            if (ship.port>=MerchantPorts.size() || ship.destination>NoMerchantPort ||
                ship.destination==ship.port || ship.good>=GoodsCount ||
                ship.units>MerchantLot || ship.held>1 || gold>MerchantReserve) return false;
            if (ship.destination==NoMerchantPort ? progress!=0 :
                    progress>=unsigned(merchant_lane(ship.port,ship.destination).miles)) return false;
            ship.gold=int(gold);
            ship.progress=int(progress);
        }
        s.port_stories=std::to_integer<std::uint8_t>(bytes[offset++]);
        auto& order=s.dispatch;
        for (auto* field : {&order.number,&order.stopped,&order.last,&order.outcome})
            *field=std::to_integer<std::uint8_t>(bytes[offset++]);
        const auto due=get();
        if (order.number>DispatchCount || order.last>DispatchCount || order.stopped>1 ||
            order.outcome>DispatchLost || bool(order.last)!=bool(order.outcome) || due>99999 ||
            (!order.number && (due || order.stopped)) ||
            (order.number && (!due || (order.stopped && DispatchOrders[order.number-1].stop<0)))) return false;
        if (order.number) {
            const auto& cargo=DispatchOrders[order.number-1];
            if (!(s.chart_marks & (1U<<cargo.to)) ||
                (cargo.stop>=0 && !(s.chart_marks & (1U<<cargo.stop)))) return false;
        }
        order.due=int(due);
        const int sailing_revision=std::to_integer<int>(bytes[offset++]);
        if (sailing_revision>SailingTimeRevision) return false;
        legacy_sailing_time=sailing_revision==0;
        // The old writer left this byte zero, representing standard rations.
        s.ration_flags=std::to_integer<std::uint8_t>(bytes[offset++]);
        if (s.ration_flags & ~(SaveFood|SaveWater)) return false;
        const auto shore=std::to_integer<std::uint8_t>(bytes[offset++]);
        s.bargain_warnings=shore&3;
        s.timber_trail=(shore>>2)&3;
        s.land_stage=(shore>>4)&7;
        s.road_delivery=(shore&0x80)!=0;
        const bool road_loaded=order.number==RoadFreight+1;
        const bool road_delivered=order.last==RoadFreight+1 &&
                                  (order.outcome==DispatchOnTime || order.outcome==DispatchLate);
        if ((s.road_delivery || road_loaded) && s.land_stage<LandFiled) return false;
        if ((s.road_delivery && road_loaded) || (road_delivered && !s.road_delivery)) return false;
        if (s.land_stage>=LandRidge && s.land_stage<=LandLookout &&
            (s.origin!=lisbon || s.helm || s.destination>=0 || s.weather!=Calm ||
             s.stores<LandLegs[ReturnToCity].food || s.water<LandLegs[ReturnToCity].water)) return false;
        if (s.bargain_warnings && (s.helm || s.destination>=0 || s.origin>=PortCount ||
            (s.bargain_warnings==HagglePatience && s.bargain_good!=BargainUsed))) return false;
    }
    const auto bound = [](int n, int min, int max) { return n >= min && n <= max; };
    if (!bound(s.ship,0,2) || !bound(s.gold,0,999999) || !bound(s.day,0,99999) ||
        !bound(s.hull,1,100) || !bound(s.stores,0,provisions[s.ship]+(with_helm?20:0)) ||
        s.water>provisions[s.ship]+(with_helm?20:0) || s.stores+s.water>2*provisions[s.ship] ||
        !bound(s.spirits,0,100) || !bound(s.crew,4,berths[s.ship]) ||
        !bound(s.guns,0,3) || !bound(s.charts,0,PlaceCount-PortCount) ||
        !bound(s.chapter,0,Chapters) || !bound(s.ending,0,2) ||
        !bound(s.origin,0,PlaceCount-1) || !bound(s.destination,-1,PlaceCount-1) ||
        !bound(s.contract,-1,PortCount-1) || !bound(s.contract_good,0,GoodsCount-1) ||
        !bound(s.weather,Calm,current?EncounterCount-1:version==0x31445054?Castaways:Fog) || !bound(s.range,0,4) || !bound(s.enemy,0,100) ||
        !bound(s.renown,0,999999) || !bound(s.trust,0,999999) ||
        !bound(s.deadline,0,100300) || !bound(s.reefed,0,1) ||
        !bound(s.stop_next,0,1) || !bound(s.aim,0,1) || !bound(s.allies,0,3) || !bound(s.patron,0,1)) return false;
    int load = s.dispatch.number?DispatchRates[DispatchOrders[s.dispatch.number-1].kind].space:0;
    for (const int count : s.cargo) load += count;
    if (load > holds[s.ship]) return false;
    if (s.helm) {
        if (!bound(s.helm_x,0,ChartWidth*16-1) || !bound(s.helm_y,0,ChartHeight*16-1) ||
            s.weather==DockDuel || s.leg || s.progress || s.destination==s.origin) return false;
    } else if (s.destination >= 0) {
        const auto route = passage(s.origin,s.destination);
        if (!bound(s.leg,0,route.count-1) ||
            !bound(s.progress,0,Legs[route.edge[s.leg]].nautical_miles-1)) return false;
        if (s.weather==DockDuel) return false;
    } else if ((s.weather!=Calm && (s.weather!=DockDuel || s.origin!=cadiz)) || s.leg || s.progress)
        return false;
    if ((s.weather==Privateer || s.weather==Boarding) && (s.range==0 || s.enemy==0)) return false;
    if (s.weather==Boarding || s.weather==DockDuel) {
        if (!s.vigor || !s.rival) return false;
    } else if (s.vigor || s.rival || s.duel_turn) return false;
    // Old contracts were issued at roughly twice the distance per day.
    // Grant the extra time once, leaving already overdue work overdue.
    if (legacy_sailing_time) {
        if (s.contract>=0 && s.deadline>=s.day)
            s.deadline=std::min(100300,s.deadline+(s.deadline-s.day+1));
        if (s.dispatch.number && s.dispatch.due>=s.day)
            s.dispatch.due=std::min(99999,s.dispatch.due+(s.dispatch.due-s.day+1));
    }
    output = s;
    return true;
}

} // namespace tide
