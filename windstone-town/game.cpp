#include "game.hpp"

#include <algorithm>
#include <bit>
#include <cstdlib>
#include <type_traits>

namespace wind {

unsigned texture(Point p) {
    uint32_t h=uint32_t(p.x)*0x9e3779b9u ^ uint32_t(p.y)*0x85ebca6bu;
    h=(h^(h>>16))*0x7feb352du;
    return h^(h>>15);
}

Tile landscape(Point p) {
    const auto [x,y]=p;
    if (y>=OriginalHeight) {
        const int height=x>=18?2:x>=15?1:0;
        const uint8_t ward=x>=18?3:x>=14?2:1;
        const bool cross=y==19 || y==22;
        if (x==14) return {cross?Bridge:River,0,ward};
        if ((cross && x>=6 && x<=18) || x==8 || (y>=19 && (x==6 || x==17))) {
            const bool stair=cross && (x==15 || x==18);
            return {stair?Stair:Street,uint8_t(height),ward};
        }
        if (y>=18 && x>=5 && x<=18) return {Plot,uint8_t(height),ward};
        const auto noise=texture(p)%7;
        return {noise<4?Forest:noise==4?Stone:Grass,uint8_t(height),ward};
    }
    const int creek[]{13,13,12,12,12,13,13,13,13,14,14,14,15,15,14,14,14};
    const int bank=creek[y];
    const bool highWest=(x<6 && y<7 && (x+y<9 || y<3)) || (x<=3 && y>=6 && y<=10);
    const bool north=y<=4 && x>=4 && x<=11 && x<bank;
    const int eastRise=14+(y>=8)+(y>=10);
    int h=highWest?1+(x<=2 && y<7):0;
    if (north) h=std::max(h,1+(y<=2)+(y==0 && x>=6 && x<=9));
    if (x>=eastRise) {
        const bool shelf=x>=17 && y<=11;
        const bool summit=(x>=18 && y>=5 && y<=9) || (x>=19 && y<=11);
        h=1+int(shelf)+int(summit);
    }
    const uint8_t ward=x>bank?uint8_t(h==3?4:3):uint8_t(y<7?2:1);
    if (x==bank || (y>0 && x==creek[y-1]) || (x==bank+1 && (y==10 || y==11)))
        return {y==8 ? Bridge : River,uint8_t(y<=5),ward};

    const bool road=x==8 || (y==9 && x>=4 && x<=11) ||
        (y==12 && x>=5 && x<=12) || (x==5 && y>=9 && y<=12) ||
        (x==11 && y>=8 && y<=12) || (y==8 && x>=11 && x<=16) ||
        (x==16 && y>=5 && y<=8) || (y==6 && x>=6 && x<=11) ||
        (y==5 && x>=14 && x<=18) || (x==18 && y>=5 && y<=7) ||
        (x==9 && y>=12 && y<=15) || (y==15 && x>=8 && x<=11) ||
        (y==4 && x>=6 && x<=10) || (y==2 && x>=7 && x<=9);
    if (road) {
        const bool stairs=p==Point{15,8} || p==Point{8,4} || p==Point{8,3} ||
                          p==Point{8,1} || p==Point{16,5} || p==Point{17,5};
        return {stairs?Stair:Street,uint8_t(h),ward};
    }
    if ((x==7 || x==9) && y>=7 && y<=14) return {Plot,0,1};
    if ((y==7 || y==10 || y==11 || y==13) && x>=5 && x<bank && !h)
        return {Plot,0,ward};
    if (north && x>=6 && x<=10) return {Plot,uint8_t(h),2};
    if (x>bank && x<=19 && y>=5 && y<=9) return {Plot,uint8_t(h),ward};
    const auto noise=texture(p)%100;
    if ((x<4 || y<=2 || x>=18 || y>=15 || highWest || (x>=10 && x<=11 && y<5)) && noise>39)
        return {Forest,uint8_t(h),ward};
    if ((x>=18 || highWest) && noise<13) return {Stone,uint8_t(h),ward};
    return {Grass,uint8_t(h),ward};
}

unsigned Game::random(unsigned range) {
    s.seed^=s.seed<<13;
    s.seed^=s.seed>>17;
    s.seed^=s.seed<<5;
    return s.seed%range;
}

void Game::reset() {
    s={};
    s.seed=0x527fa4bd;
    s.gold=320; s.day=1; s.rank=1;
    s.buildings[0]={7,8,Guild,1};
    s.buildings[1]={9,10,Inn,1};
    s.buildings[2]={6,10,Market,1};
    s.count=3;
    for (int i=0;i<2;++i) s.heroes[i]={1,0,100,96,19,0,55,NoHome};
    store(Item::HoneyBun);
    store(Item::LeafPotion);
    restore();
    note("公会开门了。为小镇建一间铺子吧！");
}

void Game::migrate_map() {
    const auto old=s.buildings;
    const int count=s.count;
    s.count=0;
    for (int i=0;i<Cells;++i) {
        const auto kind=landscape(point(i)).ground;
        if (kind!=Grass && kind!=Plot) s.roads[i/8]&=uint8_t(~(1u<<(i%8)));
    }
    roads_changed();
    for (int i=0;i<Cells;++i)
        if (!traffic[i]) s.roads[i/8]&=uint8_t(~(1u<<(i%8)));
    for (int i=0;i<count;++i) {
        const auto& b=old[i];
        auto target=b.at();
        int minimum=Cells;
        for (int index=0;index<OriginalCells;++index) {
            const auto p=point(index);
            const auto land=landscape(p);
            const int distance=std::abs(p.x-b.x)+std::abs(p.y-b.y);
            if (distance>=minimum || (land.ground!=Grass && land.ground!=Plot) ||
                road(p) || building(p)>=0 || !inside(entrance(p))) continue;
            if (land.ward>s.rank && p!=b.at()) continue;
            target=p; minimum=distance;
        }
        s.buildings[s.count++]={uint8_t(target.x),uint8_t(target.y),b.kind,b.level};
    }
}

void Game::restore(bool previous_map) {
    beat=0;
    walkers={};
    wild={};
    journal={};
    if (previous_map) migrate_map();
    else roads_changed();
    if (s.trip.phase!=Idle) {
        for (int i=0;i<HeroCount;++i) {
            if (!(s.trip.team&(1u<<i))) continue;
            const auto entrance=s.trip.route==Route::Frontier?frontier_party_tile(s.trip.team,i):
                                expedition_entrance(s.trip.route);
            if (s.trip.phase==Outbound) path(i,entrance);
            if (s.trip.phase==Fighting || s.trip.phase==Returning || s.trip.phase==Exploring)
                walkers[i].from=walkers[i].at=entrance;
            if (s.trip.phase==Returning) path(i,Square);
        }
    }
}

int Game::hero_count() const {
    return int(std::count_if(s.heroes.begin(),s.heroes.end(),[](const Hero& h){return h.level>0;}));
}
int Game::power(int id) const {
    return stats(id).attack;
}
HeroStats Game::stats(int id) const {
    const auto& h=s.heroes[id];
    if (!h.level) return {};
    HeroStats value{
        Heroes[id].power+h.level*3+h.gear*4+s.charms*2+(h.home!=NoHome?2:0)+h.talents[int(Talent::Arms)]*2,
        (h.level-1)/5+h.talents[int(Talent::Grit)],h.talents[int(Talent::Travel)]
    };
    for (const auto item:h.equipment) {
        if (item==Item::None) continue;
        const auto& equipment=item_type(item);
        value.attack+=equipment.attack;
        value.armor+=equipment.armor;
        value.stamina+=equipment.stamina;
    }
    const auto add=[&](HeroStats bonus) {
        value.attack+=bonus.attack;
        value.armor+=bonus.armor;
        value.stamina+=bonus.stamina;
    };
    if (valid_discipline(h.discipline)) add(Disciplines[unsigned(h.discipline)-1].bonus);
    for (unsigned i=0;i<h.practice.size();++i)
        if (h.practice[i]==MasteryLimit) add(Disciplines[i].mastery);
    return value;
}

int Game::building(Point p) const {
    for (int i=0;i<s.count;++i) if (s.buildings[i].at()==p) return i;
    return -1;
}
namespace {
bool flagged(const std::array<uint8_t,CellFlags>& flags, Point p) {
    return flags[cell(p)/8] & (1u<<(cell(p)%8));
}
void set_flag(std::array<uint8_t,CellFlags>& flags, Point p) {
    flags[cell(p)/8]|=uint8_t(1u<<(cell(p)%8));
}
}

Tile Game::terrain(Point p) const {
    auto tile=landscape(p);
    if (const auto height=s.elevations[cell(p)]) tile.height=height-1;
    if (flagged(s.steps,p)) tile.ground=Stair;
    return tile;
}
bool Game::road(Point p) const {
    if (!inside(p)) return false;
    const auto kind=terrain(p).ground;
    return kind==Street || kind==Bridge || kind==Stair || flagged(s.roads,p);
}
bool Game::usable(Point a,Point b) const {
    if (std::abs(a.x-b.x)+std::abs(a.y-b.y)!=1 || !road(a) || !road(b)) return false;
    const auto u=terrain(a),v=terrain(b);
    const int difference=std::abs(int(u.height)-int(v.height));
    return difference==0 || (difference==1 && (u.ground==Stair || v.ground==Stair));
}
void Game::roads_changed() {
    traffic={};
    std::array<Point,Cells> queue{};
    queue[0]=Gate; traffic[cell(Gate)]=1;
    int end=1;
    for (int i=0;i<end;++i) {
        for (const auto offset:Directions) {
            const auto next=queue[i]+offset;
            if (!usable(queue[i],next) || traffic[cell(next)]) continue;
            traffic[cell(next)]=1;
            queue[end++]=next;
        }
    }
    ++revision;
}
Point Game::entrance(Point at) const {
    for (auto offset:Directions) {
        const auto p=at+offset;
        if (inside(p) && traffic[cell(p)] && terrain(p).height==terrain(at).height) return p;
    }
    return {-1,-1};
}

Placement Game::placement(Point p, Kind type) const {
    if (!inside(p) || building(p)>=0 || road(p)) return Placement::Occupied;
    const auto tile=terrain(p);
    if (tile.ground!=Plot && tile.ground!=Grass) return Placement::Occupied;
    if (s.rank<Types[type].rank || s.rank<tile.ward) return Placement::Locked;
    if (earthwork(type)) {
        if ((type==RaiseGround && tile.height==MaxElevation) ||
            (type==LowerGround && tile.height==0)) return Placement::ElevationLimit;
    } else if (type==StoneSteps) {
        const bool joins_road=std::any_of(Directions.begin(),Directions.end(),[&](Point offset) {
            const auto next=p+offset;
            return inside(next) && traffic[cell(next)] &&
                   std::abs(int(terrain(next).height)-int(tile.height))==1;
        });
        if (!joins_road) return Placement::NoStairLink;
    } else if (!inside(entrance(p))) return Placement::NoRoad;
    if (s.gold<Types[type].cost) return Placement::Funds;
    if (type<Road && s.count==MaxBuildings) return Placement::Limit;
    return Placement::Good;
}
bool Game::build(Point p, Kind type) {
    if (type==Guild || type==CaveMouth || placement(p,type)!=Placement::Good) return false;
    s.gold-=Types[type].cost;
    if (earthwork(type)) {
        s.elevations[cell(p)]=uint8_t(terrain(p).height+(type==RaiseGround?2:0));
        ++revision;
        note(type==RaiseGround?"地块抬高了一层":"地块降低了一层");
    } else if (type==Road || type==StoneSteps) {
        set_flag(type==Road?s.roads:s.steps,p);
        roads_changed();
        note(type==Road?"碎石小路铺好了":"石阶已接入镇内道路");
    } else {
        s.buildings[s.count++]={uint8_t(p.x),uint8_t(p.y),uint8_t(type),1};
        ++revision;
        s.fame=uint16_t(std::min(9999,s.fame+4));
        note(type==House?"民居建好了，可以安排入住":"新的铺面开张了");
        record_commission(CommissionKind::Build);
    }
    return true;
}
int Game::upgrade_cost(int id) const {
    if (id<=0 || id>=s.count || s.buildings[id].level>=3) return 0;
    const auto& b=s.buildings[id];
    return Types[b.kind].cost*b.level/2+20;
}
bool Game::upgrade(int id) {
    const int cost=upgrade_cost(id);
    if (!cost || s.gold<cost) return false;
    s.gold-=uint16_t(cost);
    ++s.buildings[id].level;
    s.fame=uint16_t(std::min(9999,s.fame+3));
    ++revision;
    note(s.buildings[id].kind==House?"民居扩建完成":"铺面扩建完成");
    record_commission(CommissionKind::Build);
    return true;
}
bool Game::set_service_policy(int id,ServicePolicy policy) {
    if (id<=0 || id>=s.count || !valid_policy(policy)) return false;
    auto& b=s.buildings[id];
    if (!paid_service(b.kind) || b.account.policy==policy) return false;
    b.account.policy=policy;
    note("铺面已调整经营策略");
    return true;
}
bool Game::recruit(int id) {
    if (id<0 || id>=HeroCount || s.heroes[id].level || s.fame<Heroes[id].fame || s.gold<Heroes[id].fee)
        return false;
    s.gold-=Heroes[id].fee;
    s.heroes[id]={1,0,100,96,24,0,65,NoHome};
    walkers[id]={};
    note("新旅人走进了小镇");
    return true;
}
bool Game::gift(int id) {
    if (!eligible(id) || s.gold<34) return false;
    auto& h=s.heroes[id];
    if (h.mood==100 && h.gear==6) return false;
    s.gold-=34;
    h.mood=uint8_t(std::min(100,h.mood+17));
    h.gear=uint8_t(std::min(6,h.gear+1));
    note("旅人收下了小镇的馈赠");
    return true;
}
namespace {
void gain_experience(Hero& hero,int amount) {
    int xp=hero.xp+amount;
    while (hero.level<LevelLimit && xp>=experience_goal(hero.level)) {
        xp-=experience_goal(hero.level);
        ++hero.level;
    }
    hero.xp=hero.level==LevelLimit?0:uint8_t(xp);
}
void practice_discipline(Hero& hero,int amount) {
    if (!hero.practicing()) return;
    auto& progress=hero.practice[unsigned(hero.discipline)-1];
    progress=uint8_t(std::min(MasteryLimit,progress+amount));
}
int spent_points(const Hero& hero) {
    int points=0;
    for (const auto rank:hero.talents) points+=rank;
    return points;
}
}
int Game::growth_points(int id) const {
    const auto& hero=s.heroes[id];
    return std::max(0,hero.level-1-spent_points(hero));
}
bool Game::can_invest(int id,Talent talent) const {
    return eligible(id) && unsigned(talent)<TalentCount && growth_points(id)>0 &&
           s.heroes[id].talents[int(talent)]<TalentLimit;
}
bool Game::invest(int id,Talent talent) {
    if (!can_invest(id,talent)) return false;
    ++s.heroes[id].talents[int(talent)];
    return true;
}
bool Game::can_choose_discipline(int id,Discipline choice) const {
    if (!eligible(id) || !valid_discipline(choice)) return false;
    const auto& h=s.heroes[id];
    return h.level>=DisciplineLevel && choice!=h.discipline &&
           (h.practice[unsigned(choice)-1] || s.gold>=DisciplineCost);
}
bool Game::choose_discipline(int id,Discipline choice) {
    if (!can_choose_discipline(id,choice)) return false;
    auto& h=s.heroes[id];
    auto& progress=h.practice[unsigned(choice)-1];
    if (!progress) {
        s.gold-=DisciplineCost;
        progress=1;
    }
    h.discipline=choice;
    note("旅人已选定专精");
    return true;
}
bool Game::guards(int id) const {
    return shields_party(id) || s.heroes[id].discipline==Discipline::Guard;
}
int Game::owned(Item item) const {
    return valid_item(item)?s.stock[item_index(item)]:0;
}
bool Game::store(Item item) {
    if (!valid_item(item) || owned(item)>=StockLimit) return false;
    ++s.stock[item_index(item)];
    return true;
}
bool Game::sells(int shop,Item item) const {
    return valid_item(item) && shop>=0 && shop<s.count && s.buildings[shop].kind==item_type(item).vendor;
}
bool Game::can_buy(int shop,Item item) const {
    if (!sells(shop,item)) return false;
    const auto& type=item_type(item);
    return s.rank>=type.rank && s.buildings[shop].level>=stock_level(item) &&
           s.gold>=type.price && s.relics>=type.relics && owned(item)<StockLimit;
}
bool Game::buy(int shop,Item item) {
    if (!can_buy(shop,item)) return false;
    const auto& type=item_type(item);
    s.gold-=type.price;
    s.relics-=type.relics;
    store(item);
    note(type.relics?"饰物炼成并收入仓库":"物品已收入仓库");
    return true;
}
bool Game::sell(Item item) {
    if (!owned(item) || s.gold==60000) return false;
    --s.stock[item_index(item)];
    s.gold=uint16_t(std::min(60000,s.gold+item_type(item).price/3));
    return true;
}
bool Game::offers_recipe(int shop,int recipe) const {
    return shop>0 && shop<s.count && recipe>=0 && unsigned(recipe)<std::size(Recipes) &&
           s.buildings[shop].kind==Recipes[recipe].workshop;
}
bool Game::can_refine(int shop,int recipe) const {
    if (!offers_recipe(shop,recipe)) return false;
    const auto& r=Recipes[recipe];
    return s.rank>=r.rank && s.buildings[shop].level>=r.level && s.gold>=r.gold && s.relics>=r.relics &&
           owned(r.input)>0 && owned(r.output)<StockLimit;
}
bool Game::refine(int shop,int recipe) {
    if (!can_refine(shop,recipe)) return false;
    const auto& r=Recipes[recipe];
    s.gold-=uint16_t(r.gold);
    s.relics-=uint16_t(r.relics);
    --s.stock[item_index(r.input)];
    store(r.output);
    note("装备炼成并收入仓库");
    return true;
}
bool Game::can_equip(int id,Item item) const {
    if (!eligible(id) || !owned(item) || item_type(item).group==ItemGroup::Consumable) return false;
    const auto active=s.heroes[id].equipment[int(item_type(item).group)];
    return item!=active && owned(active)<StockLimit;
}
bool Game::equip(int id,Item item) {
    if (!can_equip(id,item)) return false;
    auto& slot=s.heroes[id].equipment[int(item_type(item).group)];
    if (slot!=Item::None) store(slot);
    slot=item;
    --s.stock[item_index(item)];
    return true;
}
bool Game::unequip(int id,ItemGroup group) {
    if (!eligible(id) || unsigned(group)>=GearSlots) return false;
    auto& slot=s.heroes[id].equipment[int(group)];
    if (slot==Item::None || !store(slot)) return false;
    slot=Item::None;
    return true;
}
bool Game::can_use(int id,Item item) const {
    if (!eligible(id) || !owned(item)) return false;
    const auto& h=s.heroes[id];
    switch (item) {
    case Item::LeafPotion: return h.hp<100;
    case Item::BlueTonic: return h.energy<100;
    case Item::HoneyBun: return h.mood<100;
    case Item::StudyBook: return h.level<LevelLimit;
    default: return false;
    }
}
bool Game::use(int id,Item item) {
    if (!can_use(id,item)) return false;
    auto& h=s.heroes[id];
    const int amount=item_type(item).amount;
    switch (item) {
    case Item::LeafPotion: h.hp=uint8_t(std::min(100,h.hp+amount)); break;
    case Item::BlueTonic: h.energy=uint8_t(std::min(100,h.energy+amount)); break;
    case Item::HoneyBun: h.mood=uint8_t(std::min(100,h.mood+amount)); break;
    case Item::StudyBook: gain_experience(h,amount); break;
    default: return false;
    }
    --s.stock[item_index(item)];
    return true;
}
bool Game::trained(int id) const {
    return id>=0 && id<HeroCount && (s.trained_today&(1u<<id));
}
bool Game::can_train(int id) const {
    if (!eligible(id) || trained(id) || s.gold<TrainingCost) return false;
    const auto& h=s.heroes[id];
    return (h.level<LevelLimit || h.practicing()) && h.hp>=TrainingHealth && h.energy>=TrainingEnergy;
}
int Game::training_experience() const {
    return 12+3*s.rank;
}
bool Game::train(int id) {
    if (!can_train(id)) return false;
    auto& h=s.heroes[id];
    const int amount=h.level<LevelLimit?training_experience():0;
    s.gold-=TrainingCost;
    h.energy-=TrainingEnergy;
    s.trained_today|=uint8_t(1u<<id);
    gain_experience(h,amount);
    practice_discipline(h,2);
    note("旅人完成了公会训练",amount);
    return true;
}
int Game::occupants(int house) const {
    return int(std::count_if(s.heroes.begin(),s.heroes.end(),
                             [house](const Hero& hero){return hero.level && hero.home==house;}));
}
int Game::daily_rent() const {
    int rent=0;
    for (const auto& h:s.heroes)
        if (h.level && h.home!=NoHome) rent+=room_rent(s.buildings[h.home].level);
    return rent;
}
bool Game::can_assign_home(int hero,int house) const {
    return eligible(hero) && house>0 && house<s.count && s.buildings[house].kind==House &&
           !occupants(house) && (s.heroes[hero].home!=NoHome || s.heroes[hero].mood>=SettleMood) &&
           inside(entrance(s.buildings[house].at()));
}
bool Game::assign_home(int hero,int house) {
    if (!can_assign_home(hero,house) || !send(hero,house)) return false;
    release_hunt(hunt_target(hero));
    auto& h=s.heroes[hero];
    if (h.home==NoHome) {
        s.fame=uint16_t(std::min(9999,s.fame+8));
        note("旅人有了自己的家");
    } else note("旅人搬进了新家");
    h.home=uint8_t(house);
    walkers[hero].at_home=false;
    walkers[hero].festival_guest=false;
    walkers[hero].dwell=0;
    return true;
}
bool Game::can_host_event(TownEvent event) const {
    return valid_event(event) && !s.festival && s.rank>=event_info(event).rank && s.gold>=event_info(event).gold;
}
bool Game::host_event(TownEvent event) {
    if (!can_host_event(event)) return false;
    s.gold-=event_info(event).gold;
    s.festival=2;
    s.event=event;
    s.event_attended=0;
    if (event==TownEvent::Lanterns) {
        s.fame=uint16_t(std::min(9999,s.fame+LanternFame));
        for (auto& h:s.heroes) if (h.level) h.mood=uint8_t(std::min(100,h.mood+LanternMood));
        note("灯会开场，旅人们围向广场");
    } else note(event==TownEvent::Drill?"集训开场，旅人们前往广场":"备战宴开席，旅人们前往广场");
    return true;
}
bool Game::prepared(int hero) const {
    return hero>=0 && hero<HeroCount && (s.prepared&(1u<<hero));
}
bool Game::craft() {
    if (s.relics<4 || s.charms>=8 || s.gold<28 ||
        std::none_of(s.buildings.begin(),s.buildings.begin()+s.count,
                     [](const Building& b){return b.kind==Shrine;})) return false;
    s.relics-=4; s.gold-=28; ++s.charms;
    note("风纹护符炼成，全员战力增加");
    return true;
}
bool Game::can_promote() const {
    if (!s.rank || s.rank>=5) return false;
    const auto& next=Ranks[s.rank];
    return s.fame>=next.fame && hero_count()>=next.people && std::popcount(s.cleared)>=next.floors;
}
bool Game::promote() {
    if (!can_promote()) return false;
    ++s.rank;
    s.gold=uint16_t(std::min(60000,s.gold+75*s.rank));
    if (s.rank==5) s.victory=1;
    note(s.rank==5?"风石谷迎来了自己的名城":"镇级提升，新的经营机会开放！");
    return true;
}

bool Game::path(int id, Point target) {
    auto& w=walkers[id];
    if (!inside(target) || !traffic[cell(target)]) return false;
    if (w.at==target) {
        w.length=w.next=0;
        return true;
    }
    std::array<int16_t,Cells> previous;
    previous.fill(-1);
    std::array<uint16_t,Cells> queue{};
    const int start=cell(w.at), goal=cell(target);
    queue[0]=uint16_t(start); previous[start]=int16_t(start);
    int end=1;
    for (int i=0;i<end && previous[goal]<0;++i) {
        const Point at=point(queue[i]);
        for (auto off:Directions) {
            auto next=at+off;
            if (!usable(at,next) || previous[cell(next)]>=0) continue;
            previous[cell(next)]=int16_t(queue[i]);
            queue[end++]=uint16_t(cell(next));
        }
    }
    if (previous[goal]<0) return false;
    w.next=w.length=0;
    for (int p=goal;p!=start;p=previous[p]) w.steps[w.length++]=uint16_t(p);
    std::reverse(w.steps.begin(),w.steps.begin()+w.length);
    return true;
}
bool Game::send(int id, int shop) {
    if (!path(id,entrance(s.buildings[shop].at()))) return false;
    walkers[id].shop=int8_t(shop);
    return true;
}
int Game::hunt_target(int hero) const {
    for (int i=0;i<int(wild.size());++i)
        if (wild[i].hp && wild[i].hunter==hero) return i;
    return -1;
}
Activity Game::activity(int hero) const {
    const auto& w=walkers[hero];
    if (s.trip.team&(1u<<hero)) {
        const bool frontier=s.trip.route==Route::Frontier;
        if (s.trip.phase==Outbound) return frontier?Activity::ToFrontier:Activity::ToCave;
        if (s.trip.phase==Fighting) return frontier?Activity::FrontierBattle:Activity::Expedition;
        if (s.trip.phase==Exploring) return Activity::CaveTrail;
        if (s.trip.phase==Returning) return Activity::Returning;
    }
    if (w.at_home) return Activity::Home;
    if (const int site=hunt_target(hero); site>=0)
        return wild[site].age?Activity::Hunt:Activity::ToHunt;
    if (w.festival_guest) {
        if (s.event==TownEvent::Lanterns) return w.dwell?Activity::Festival:Activity::ToFestival;
        if (!w.dwell) return Activity::ToTownEvent;
        return s.event==TownEvent::Drill?Activity::Drill:Activity::Feast;
    }
    if (w.shop>=0)
        return s.buildings[w.shop].kind==House?Activity::ToHome:w.dwell?Activity::Shop:Activity::ToShop;
    return w.dwell?Activity::Rest:Activity::Stroll;
}
bool Game::start_hunt(int hero) {
    const auto& h=s.heroes[hero];
    if (h.hp<75 || h.energy<76 || s.clock>=EveningBeat || (s.festival && s.event==TownEvent::Lanterns))
        return false;
    const auto at=walkers[hero].at;
    int target=-1, nearest=13+s.rank;
    for (int i=0;i<int(wild.size());++i) {
        if (!wild[i].hp || wild[i].hunter>=0) continue;
        const auto site=WildSites[i].at;
        const int distance=std::abs(at.x-site.x)+std::abs(at.y-site.y);
        if (distance<nearest) {target=i; nearest=distance;}
    }
    if (target<0 || !path(hero,WildSites[target].at)) return false;
    wild[target].hunter=int8_t(hero);
    return true;
}
void Game::release_hunt(int site) {
    if (site<0) return;
    wild[site].hunter=-1;
    wild[site].age=0;
}
void Game::fight_wild(int site) {
    auto& monster=wild[site];
    const int id=monster.hunter;
    auto& h=s.heroes[id];
    monster.age=uint8_t(monster.age%WildRound+1);
    if (monster.age!=WildImpact) return;
    const auto stats=this->stats(id);
    const int damage=std::max(2,stats.attack/2+2);
    monster.hp=uint8_t(std::max(0,int(monster.hp)-damage));
    h.energy=uint8_t(std::max(0,int(h.energy)-6));
    if (!monster.hp) {
        constexpr int bounty=5;
        s.gold=uint16_t(std::min(60000,s.gold+bounty));
        s.income=uint16_t(std::min(9999,s.income+bounty));
        gain_experience(h,3);
        practice_discipline(h,1);
        release_hunt(site);
        walkers[id].dwell=2;
        note("旅人击退了野怪",bounty);
        record_commission(CommissionKind::Hunt);
        return;
    }
    h.hp=uint8_t(std::max(1,int(h.hp)-std::max(1,WildSites[site].power-stats.armor)));
    if (h.hp<50 || h.energy<45) {
        release_hunt(site);
        choose(id);
    }
}
bool Game::visit(int id, int shop) {
    auto& h=s.heroes[id];
    auto& b=s.buildings[shop];
    if (b.kind==House) {
        if (h.home!=shop) return false;
        h.energy=uint8_t(std::min(100,h.energy+17+6*b.level));
        h.hp=uint8_t(std::min(100,h.hp+10+5*b.level));
        walkers[id].at_home=true;
        return true;
    }
    if (b.kind==Shrine) {
        h.mood=uint8_t(std::min(100,h.mood+4));
        return true;
    }
    if (!paid_service(b.kind)) return false;
    const auto policy=b.account.policy;
    const auto& service=ServicePlans[unsigned(policy)];
    const int cost=service_price(b,policy);
    if (h.wallet<cost) return false;
    h.wallet-=uint8_t(cost);
    s.gold=uint16_t(std::min(60000,s.gold+cost));
    s.income=uint16_t(std::min(9999,s.income+cost));
    s.visits=uint16_t(std::min(9999,s.visits+1));
    auto& today=b.account.today;
    today.visits=uint16_t(std::min(ShopAccount::DailyLimit,today.visits+1));
    today.revenue=uint16_t(std::min(ShopAccount::DailyLimit,today.revenue+cost));
    h.mood=uint8_t(std::min(100,h.mood+(b.kind==Market?7:3)+b.level+service.mood));
    if (b.kind==Inn || b.kind==Herb) h.hp=uint8_t(std::min(100,h.hp+16+4*b.level+service.recovery));
    if (b.kind==Inn || b.kind==Market) h.energy=uint8_t(std::min(100,h.energy+20+4*b.level+service.recovery));
    if (b.kind==Forge && h.gear<b.level+1+service.forge) ++h.gear;
    if (s.visits%4==0) s.fame=uint16_t(std::min(9999,s.fame+1));
    record_commission(CommissionKind::Trade);
    return true;
}
bool Game::can_attend_event(int id) const {
    if (!s.festival || s.clock>=EveningBeat) return false;
    if (s.event==TownEvent::Lanterns) return walkers[id].festival_day!=s.day;
    if (s.event_attended&(1u<<id)) return false;
    const auto& h=s.heroes[id];
    return s.event==TownEvent::Feast ||
           ((h.level<LevelLimit || h.practicing()) && h.hp>=TrainingHealth && h.energy>=TrainingEnergy);
}
bool Game::attend_event(int id) {
    if (!s.festival) return false;
    const auto bit=uint8_t(1u<<id);
    auto& h=s.heroes[id];
    switch (s.event) {
    case TownEvent::Lanterns: break;
    case TownEvent::Drill:
        if ((s.event_attended&bit) || h.hp<TrainingHealth || h.energy<DrillEnergy) return false;
        gain_experience(h,DrillExperience);
        practice_discipline(h,DrillPractice);
        h.energy-=DrillEnergy;
        break;
    case TownEvent::Feast:
        if (s.event_attended&bit) return false;
        s.prepared|=bit;
        h.energy=uint8_t(std::min(100,h.energy+FeastEnergy));
        h.mood=uint8_t(std::min(100,h.mood+FeastMood));
        break;
    default: return false;
    }
    s.event_attended|=bit;
    return true;
}
void Game::choose(int id) {
    auto& w=walkers[id];
    auto& h=s.heroes[id];
    if (h.home!=NoHome && (s.clock>=EveningBeat || h.hp<67 || h.energy<62) && send(id,h.home)) return;
    if (can_attend_event(id) && path(id,Square)) {
        w.festival_day=s.day;
        w.festival_guest=true;
        return;
    }
    if (start_hunt(id)) return;
    int best=-1, bestScore=-1;
    for (int i=1;i<s.count;++i) {
        const auto& b=s.buildings[i];
        if (!paid_service(b.kind) && b.kind!=Shrine) continue;
        const auto policy=b.account.policy;
        if (h.wallet<service_price(b,policy)) continue;
        const auto& service=ServicePlans[unsigned(policy)];
        int score=int(random(12))+b.level+service.appeal;
        if (b.kind==Inn && h.energy<82) score+=15;
        if (b.kind==Inn && h.hp<74) score+=12;
        if (b.kind==Herb && h.hp<74) score+=19;
        if (b.kind==Forge && h.gear<b.level+1+service.forge) score+=16;
        if (b.kind==Market && h.mood<55) score+=6;
        const auto door=entrance(b.at());
        if (!inside(door)) continue;
        const int distance=std::abs(w.at.x-door.x)+std::abs(w.at.y-door.y);
        score-=distance?std::min(5,distance/3):15;
        if (score>bestScore) { best=i; bestScore=score; }
    }
    if (best>=0 && (!s.festival || s.event!=TownEvent::Lanterns || random(3)) && send(id,best)) return;
    constexpr Point stroll[]{{8,5},{5,9},{8,12},{11,9},Square,Gate};
    path(id,stroll[random(std::size(stroll))]);
}
bool Game::eligible(int id) const {
    return id>=0 && id<HeroCount && s.heroes[id].level &&
        (s.trip.phase==Idle || !(s.trip.team&(1u<<id)));
}
bool Game::floor_open(int floor,Route route) const {
    if (route==Route::Frontier)
        return floor==0 && s.rank>=FrontierHunt.rank && s.day>=s.frontier_next_day;
    if (floor<0 || floor>=QuestCount || s.rank<Quests[floor].rank) return false;
    if (route==Route::Depths) return s.cleared&(1u<<floor);
    return route==Route::Standard && (!floor || (s.cleared&(1u<<(floor-1))));
}
bool Game::frontier_visible() const {
    const auto& t=s.trip;
    return floor_open(0,Route::Frontier) ||
           (t.route==Route::Frontier && (t.phase==Outbound || t.phase==Fighting));
}
int Game::travel_cost(int id,Tactic tactic,Route route) const {
    return std::max(10,tactic_energy(tactic,route)-stats(id).stamina-(prepared(id)?PreparedDiscount:0));
}
int Game::required_energy(int id,Tactic tactic,Route route) const {
    return std::max(14,std::max(25,tactic_energy(tactic,route))-stats(id).stamina-(prepared(id)?PreparedDiscount:0));
}
bool Game::ready_for_trip(int id,Tactic tactic,Route route) const {
    return eligible(id) && valid_tactic(tactic) && s.heroes[id].hp>=25 &&
           s.heroes[id].energy>=required_energy(id,tactic,route) &&
           inside(walkers[id].at) && traffic[cell(walkers[id].at)];
}
unsigned Game::suggest_team(int floor,Tactic tactic,Route route) const {
    if (s.trip.phase!=Idle || !floor_open(floor,route) || !valid_tactic(tactic)) return 0;
    const unsigned previous=s.trip.floor==floor && s.trip.route==route && std::popcount(s.trip.team)<=3?s.trip.team:0;
    const int size=previous?std::popcount(previous):route==Route::Standard?2:3;
    unsigned ready=0, guard_mask=0;
    for (int id=0;id<HeroCount;++id) {
        if (!ready_for_trip(id,tactic,route)) continue;
        ready|=1u<<id;
        if (guards(id)) guard_mask|=1u<<id;
    }
    unsigned team=previous&ready;
    while (std::popcount(team)<size && (ready&~team)) {
        unsigned candidates=ready&~team;
        if (route!=Route::Standard && !(team&guard_mask) && (candidates&guard_mask)) candidates&=guard_mask;
        int best=-1, value=-1;
        for (int id=0;id<HeroCount;++id) {
            if (!(candidates&(1u<<id))) continue;
            const int rating=stats(id).rating()+s.heroes[id].hp/10;
            if (rating>value) {best=id; value=rating;}
        }
        team|=1u<<best;
    }
    return team;
}
bool Game::can_launch(int floor,unsigned team,Tactic tactic,Route route) const {
    if (s.trip.phase!=Idle || !floor_open(floor,route) || !traffic[cell(expedition_entrance(route))] ||
        team>=1u<<HeroCount || !team || std::popcount(team)>3) return false;
    for (int i=0;i<HeroCount;++i)
        if ((team&(1u<<i)) && !ready_for_trip(i,tactic,route)) return false;
    return true;
}
bool Game::launch(int floor,unsigned team,Tactic tactic,Route route) {
    if (!can_launch(floor,team,tactic,route)) return false;
    s.trip={uint8_t(floor),uint8_t(team),Outbound,uint8_t(guardian_health(floor,route)),0,0,0,tactic};
    s.trip.route=route;
    for (int i=0;i<HeroCount;++i) if (team&(1u<<i)) {
        release_hunt(hunt_target(i));
        walkers[i].shop=-1; walkers[i].dwell=0; walkers[i].at_home=false; walkers[i].festival_guest=false;
        path(i,route==Route::Frontier?frontier_party_tile(team,i):expedition_entrance(route));
        s.heroes[i].energy-=uint8_t(travel_cost(i,tactic,route));
        s.prepared&=uint8_t(~(1u<<i));
    }
    note(route==Route::Frontier?"讨伐队沿南岭林道出发":
         route==Route::Depths?"远征小队正赶往山脊裂隙":"远征小队正赶往回声矿洞");
    return true;
}
QuestReward Game::preview_reward(int floor,Route route) const {
    const auto& quest=expedition_target(floor,route);
    if (route==Route::Frontier)
        return {quest.reward,quest.fame,quest.loot,20,s.frontier_next_day?Item::BlueTonic:Item::RiverPendant};
    const int xp=12+4*floor;
    if (route==Route::Depths) {
        const auto& rules=Depths[floor];
        const bool first=!(s.depth_cleared&(1u<<floor));
        return {first?quest.reward+quest.reward/2:quest.reward,
                first?quest.fame+5:quest.fame/3,quest.loot+(first?3:2),
                xp+(first?8:4),first?rules.first_loot:rules.repeat_loot};
    }
    constexpr Item first_loot[]{Item::IronSword,Item::PineVest,Item::EmberRing,
                                Item::RidgeHammer,Item::StarPlate,Item::WindAmulet};
    constexpr Item repeat_loot[]{Item::LeafPotion,Item::HoneyBun,Item::BlueTonic,
                                 Item::StudyBook,Item::StudyBook,Item::WindAmulet};
    const bool first=!(s.cleared&(1u<<floor));
    return {first?quest.reward:quest.reward/2,first?quest.fame:quest.fame/4,
            quest.loot,xp,(first?first_loot:repeat_loot)[floor]};
}
void Game::end_fight(bool win) {
    auto& t=s.trip;
    t.patrol_hp=t.patrol_timer=0;
    if (win) {
        const auto reward=preview_reward(t.floor,t.route);
        const int gold=s.gold, fame=s.fame, relics=s.relics;
        if (store(reward.loot)) t.loot=reward.loot;
        const int overflow=t.loot==Item::None?item_type(reward.loot).price/3:0;
        s.gold=uint16_t(std::min(60000,gold+reward.gold+overflow));
        s.fame=uint16_t(std::min(9999,fame+reward.fame));
        s.relics=uint16_t(std::min(999,relics+reward.relics));
        t.reward_gold=uint16_t(std::max(0,s.gold-gold));
        t.reward_fame=uint8_t(std::max(0,s.fame-fame));
        t.reward_relics=uint8_t(std::max(0,s.relics-relics));
        t.reward_xp=uint8_t(reward.xp);
        if (t.route==Route::Frontier) {
            s.frontier_next_day=uint16_t(s.day+FrontierRestDays);
            note("南岭讨伐告捷，赏金已入账",t.reward_gold);
        } else {
            auto& cleared=t.route==Route::Depths?s.depth_cleared:s.cleared;
            cleared|=uint8_t(1u<<t.floor);
            note(t.route==Route::Depths?"险境告捷，奖励已入账":"远征成功，获得金币",t.reward_gold);
            record_commission(CommissionKind::Survey,t.floor);
        }
    } else note("远征队撤退了，休整后再试");
    for (int i=0;i<HeroCount;++i) {
        if (!(t.team&(1u<<i))) continue;
        auto& hero=s.heroes[i];
        if (win) {
            hero.wallet=uint8_t(std::min(120,hero.wallet+25+5*t.floor));
            hero.mood=uint8_t(std::min(100,hero.mood+6));
            gain_experience(hero,t.reward_xp);
            practice_discipline(hero,t.route==Route::Depths?4:3);
        }
        path(i,Square);
    }
    t.phase=Returning; t.result=win?1:2;
}
bool Game::can_supply() const {
    if ((s.trip.phase!=Fighting && s.trip.phase!=Exploring) || s.trip.supplied || s.gold<SupplyCost) return false;
    for (int id=0;id<HeroCount;++id) {
        const auto hp=s.heroes[id].hp;
        if ((s.trip.team&(1u<<id)) && hp>0 && hp<100) return true;
    }
    return false;
}
bool Game::supply() {
    if (!can_supply()) return false;
    s.gold-=SupplyCost;
    s.trip.supplied=1;
    for (int id=0;id<HeroCount;++id) {
        auto& h=s.heroes[id];
        if ((s.trip.team&(1u<<id)) && h.hp) h.hp=uint8_t(std::min(100,h.hp+SupplyHealth));
    }
    note("远征队使用了补给");
    return true;
}
bool Game::retreat() {
    if (s.trip.phase!=Fighting && s.trip.phase!=Exploring) return false;
    end_fight(false);
    return true;
}

std::array<Commission,2> Game::commission_offers() const {
    int floor=0;
    while (floor_open(floor+1)) ++floor;
    if (s.day%2==0 && floor>0) --floor;
    const Commission town=s.day%3==1?Commission{CommissionKind::Trade,uint8_t(6+2*s.rank)}:
                          s.day%3==2?Commission{CommissionKind::Build,uint8_t(s.rank>=3?2:1)}:
                                     Commission{CommissionKind::Hunt,uint8_t(s.rank>=2?3:2)};
    return {Commission{CommissionKind::Survey,uint8_t(floor)},town};
}
bool Game::accept_commission(unsigned offer) {
    const auto offers=commission_offers();
    if (offer>=offers.size() || s.commission.kind!=CommissionKind::None || s.commission_reward_day==s.day)
        return false;
    s.commission=offers[offer];
    note("已接取公会委托");
    return true;
}
void Game::record_commission(CommissionKind kind,int floor) {
    auto& order=s.commission;
    if (order.kind!=kind || order.complete() || (kind==CommissionKind::Survey && order.target!=floor)) return;
    ++order.progress;
    if (order.complete()) note("委托完成，回公会领奖");
}
bool Game::abandon_commission() {
    if (s.commission.kind==CommissionKind::None || s.commission.complete()) return false;
    s.commission={};
    note("已放弃委托，可以重新选择");
    return true;
}
bool Game::claim_commission() {
    if (!s.commission.complete() || s.commission_reward_day==s.day) return false;
    const auto prize=commission_prize(s.commission);
    s.gold=uint16_t(std::min(60000,s.gold+prize.gold));
    s.fame=uint16_t(std::min(9999,s.fame+prize.fame));
    s.commission_reward_day=s.day;
    s.commission={};
    note("公会委托已结算",prize.gold);
    return true;
}
namespace {
int strike_damage(int floor,int strength,Tactic tactic,int roll,Route route) {
    const int base=std::max(3,strength/2+roll-floor*2);
    int damage=base;
    if (tactic==Tactic::Assault) damage+=std::max(2,base/5);
    if (tactic==Tactic::Guard) damage-=std::max(1,base/7);
    return route==Route::Depths?std::max(1,damage-Depths[floor].armor):damage;
}
}
int Game::opening_strike(int floor,unsigned team,Tactic tactic,int roll,Route route) const {
    if (!team) return 0;
    int strength=0;
    for (int id=0;id<HeroCount;++id)
        if ((team&(1u<<id)) && s.heroes[id].hp) strength+=power(id);
    return strike_damage(floor,strength,tactic,roll,route);
}
bool Game::can_choose_cave_path(CavePath path) const {
    const auto& t=s.trip;
    if (!waiting_at_fork(t)) return false;
    if (path==CavePath::Main) return true;
    if (path!=CavePath::Side) return false;
    bool alive=false;
    for (int id=0;id<HeroCount;++id) {
        const auto& member=s.heroes[id];
        if (!(t.team&(1u<<id)) || !member.hp) continue;
        if (member.energy<SideCaveCost) return false;
        alive=true;
    }
    return alive;
}
bool Game::choose_cave_path(CavePath path) {
    if (!can_choose_cave_path(path)) return false;
    auto& t=s.trip;
    t.cave_path=path;
    if (path==CavePath::Side) {
        for (int id=0;id<HeroCount;++id)
            if ((t.team&(1u<<id)) && s.heroes[id].hp) s.heroes[id].energy-=SideCaveCost;
        note("远征队沿侧洞搜寻宝箱");
    } else note("远征队沿主矿道前进");
    return true;
}
void Game::open_cave_cache(bool side) {
    auto& t=s.trip;
    auto& opened=(side?s.side_caches:s.cave_caches)[unsigned(t.route)];
    const auto bit=uint8_t(1u<<t.floor);
    if (opened&bit) return;
    opened|=bit;
    (side?t.side_cache_found:t.cache_found)=1;
    const bool depths=t.route==Route::Depths;
    const Item item=side?(depths?Item::StudyBook:Item::BlueTonic):(depths?Item::BlueTonic:Item::LeafPotion);
    s.relics=uint16_t(std::min(999,s.relics+cave_cache_relics(t.route,side)));
    if (!store(item)) s.gold=uint16_t(std::min(60000,s.gold+item_type(item).price/3));
    note(side?"侧洞宝箱已开启":"途中宝箱已开启");
}
void Game::explore_cave(bool wait_for_fork) {
    auto& t=s.trip;
    if (waiting_at_fork(t)) {
        if (wait_for_fork) return;
        choose_cave_path(CavePath::Main);
    }
    if (t.patrol_hp) {
        if (++t.patrol_timer<10) return;
        t.patrol_timer=0;
        int strength=0, defender=-1;
        for (int id=0;id<HeroCount;++id) {
            if (!(t.team&(1u<<id)) || !s.heroes[id].hp) continue;
            strength+=power(id);
            if (defender<0 || (guards(id) && !guards(defender))) defender=id;
        }
        if (defender<0) {end_fight(false); return;}
        const int strike=std::max(1,strength/2+3+(t.tactic==Tactic::Assault?3:t.tactic==Tactic::Guard?-1:0));
        t.patrol_hp=uint8_t(std::max(0,int(t.patrol_hp)-strike));
        if (!t.patrol_hp) {note("巡游怪已击退，继续深入"); return;}
        const int damage=std::max(1,2+t.floor+(t.route==Route::Depths?2:0)+(in_side_cave(t)?4:0)-
                                  stats(defender).armor-(t.tactic==Tactic::Guard?2:0));
        auto& hp=s.heroes[defender].hp;
        hp=uint8_t(std::max(0,int(hp)-damage));
        return;
    }
    const int length=cave_length(t.floor,t.route);
    ++t.distance;
    if (t.distance==length/4 || (second_patrol(t.floor,t.route) && t.distance==3*length/4) ||
        (in_side_cave(t) && t.distance==side_patrol(t))) {
        t.patrol_hp=uint8_t(current_patrol_health(t));
        note("矿道里出现了巡游怪");
    } else if (t.distance==length/2) {
        open_cave_cache(false);
    } else if (in_side_cave(t) && t.distance==side_cache(t)) {
        open_cave_cache(true);
    } else if (t.distance==cave_distance(t)) {
        t.phase=Fighting;
        note("远征队抵达守卫大厅");
    }
}
void Game::advance_trip(bool wait_for_fork) {
    auto& t=s.trip;
    if (t.phase==Idle) return;
    if (t.phase==Outbound || t.phase==Returning) {
        for (int i=0;i<HeroCount;++i) {
            const auto& w=walkers[i];
            if ((t.team&(1u<<i)) && (w.walk_beats || w.next<w.length)) return;
        }
        if (t.phase==Returning) { t.phase=Idle; note("远征队回到了广场"); }
        else if (t.route==Route::Frontier) { t.phase=Fighting; note("讨伐队遇上了岭角王"); }
        else { t.phase=Exploring; note("远征队进入矿道"); }
        return;
    }
    if (t.phase==Exploring) {explore_cave(wait_for_fork); return;}
    if (beat%(t.route==Route::Standard?10:16)) return;
    ++t.round;
    int strength=0;
    std::array<int,HeroCount> alive{}, shields{};
    unsigned count=0, shield_count=0;
    for (int i=0;i<HeroCount;++i) if ((t.team&(1u<<i)) && s.heroes[i].hp>0) {
        alive[count++]=i;
        if (guards(i)) shields[shield_count++]=i;
        strength+=power(i);
    }
    if (!count) {end_fight(false); return;}
    const int strike=strike_damage(t.floor,strength,t.tactic,int(random(7)),t.route);
    t.hp=uint8_t(std::max(0,int(t.hp)-strike));
    if (!t.hp) {end_fight(true); return;}
    const auto& quest=expedition_target(t.floor,t.route);
    const int defender=shield_count?shields[random(shield_count)]:alive[random(count)];
    int damage=quest.power+int(random(5));
    if (t.route==Route::Depths) damage+=3+t.floor;
    if (heavy_round(t.floor,t.route,t.round))
        damage+=t.route==Route::Frontier?FrontierHeavyBonus:Depths[t.floor].heavy_bonus;
    if (t.tactic==Tactic::Assault) damage+=std::max(1,damage/5);
    if (t.tactic==Tactic::Guard) damage-=std::max(2,damage/3);
    damage=std::max(1,damage-stats(defender).armor-(guards(defender)?3:0));
    auto& target=s.heroes[defender];
    target.hp=uint8_t(std::max(0,target.hp-damage));
}
void Game::step(bool wait_for_fork) {
    ++beat;
    if (s.clock==EveningBeat)
        for (auto& monster:wild)
            if (monster.hunter<0) monster={};
    int active=int(std::count_if(wild.begin(),wild.end(),[](const WildMonster& monster){return monster.hp>0;}));
    for (int i=0;i<int(wild.size());++i) {
        const auto& site=WildSites[i];
        if (active<std::min(3,1+s.rank) && s.clock==wild_spawn_clock(site,s.day) &&
            s.rank>=site.rank && !wild[i].hp && traffic[cell(site.at)]) {
            wild[i].hp=uint8_t(site.health);
            ++active;
        }
    }
    for (int id=0;id<HeroCount;++id) {
        auto& h=s.heroes[id];
        if (!h.level) continue;
        auto& w=walkers[id];
        const bool party=s.trip.phase!=Idle && (s.trip.team&(1u<<id));
        const int hunt=hunt_target(id);
        if (w.walk_beats) --w.walk_beats;
        if (!w.walk_beats) {
            if (w.next<w.length) {
                w.from=w.at;
                w.at=point(w.steps[w.next++]);
                w.walk_tile_beats=party && s.trip.route!=Route::Frontier?ExpeditionTileBeats:WalkTileBeats;
                w.walk_beats=w.walk_tile_beats;
            } else if (hunt>=0) fight_wild(hunt);
            else if (!party && (beat+unsigned(id))%3==0) {
                if (w.dwell) {
                    if (!--w.dwell) {
                        w.shop=-1;
                        w.festival_guest=false;
                    }
                } else if (w.festival_guest) {
                    if (attend_event(id)) w.dwell=8;
                    else w.festival_guest=false;
                }
                else if (w.shop>=0) {
                    if (visit(id,w.shop)) w.dwell=3;
                    else w.shop=-1;
                } else if (!w.at_home || s.clock<EveningBeat) {
                    w.at_home=false;
                    choose(id);
                }
            }
        }
        if (!party && (beat+unsigned(id))%12==0) {
            if (h.hp<100) h.hp=uint8_t(std::min(100,h.hp+5));
            if (h.energy<100) h.energy=uint8_t(std::min(100,h.energy+4));
        }
    }
    advance_trip(wait_for_fork);
    if (++s.clock==DayBeats) {
        s.clock=0;
        if (s.frontier_next_day==s.day+1 && s.day<9999)
            note("岭角王重返南岭，公会开放讨伐");
        s.day=uint16_t(std::min(9999,s.day+1));
        const int levy=9+s.rank*3+daily_rent();
        s.gold=uint16_t(std::min(60000,s.gold+levy));
        s.yesterday=uint16_t(std::min(9999,s.income+levy)); s.income=0;
        for (int i=1;i<s.count;++i) {
            auto& account=s.buildings[i].account;
            account.yesterday=account.today;
            account.today={};
        }
        for (auto& h:s.heroes) if (h.level) {
            const int rent=h.home==NoHome?0:room_rent(s.buildings[h.home].level);
            h.wallet=uint8_t(std::min(120,h.wallet+40)-rent);
        }
        s.trained_today=0;
        if (s.festival && !--s.festival)
            for (auto& walker:walkers) walker.festival_guest=false;
    }
}
void Game::note(Text text, int value) {
    if (journal[0].message==text && journal[0].value==value) return;
    std::move_backward(journal.begin(),journal.end()-1,journal.end());
    journal[0]={text,value};
}

namespace {
constexpr uint32_t Magic=0x44545357; // WSTD
constexpr uint32_t SaveVC=0x43545357;
constexpr uint32_t SaveVB=0x42545357;
constexpr uint32_t SaveVA=0x41545357;
constexpr uint32_t SaveV9=0x39545357;
constexpr uint32_t SaveV8=0x38545357;
constexpr uint32_t SaveV7=0x37545357;
constexpr uint32_t SaveV6=0x36545357;
constexpr uint32_t SaveV5=0x35545357;
constexpr uint32_t SaveV4=0x34545357;
constexpr uint32_t SaveV3=0x33545357;
constexpr uint32_t MapV2=0x32545357;
constexpr uint32_t MapV1=0x31545357;
constexpr uint32_t checksum(std::span<const std::byte> data) {
    uint32_t hash=2166136261u;
    for (auto byte:data) hash=(hash^std::to_integer<unsigned>(byte))*16777619u;
    return hash;
}
template<class S, class Transfer>
void fields(S& s, Transfer io) {
    io(s.seed); io(s.gold); io(s.fame); io(s.relics); io(s.day);
    io(s.income); io(s.yesterday); io(s.visits);
    io(s.rank); io(s.cleared); io(s.festival); io(s.charms); io(s.clock); io(s.count); io(s.victory);
    for (int i=0;i<OriginalCellFlags;++i) io(s.roads[i]);
    for (auto& b:s.buildings) {io(b.x); io(b.y); io(b.kind); io(b.level);}
    for (auto& h:s.heroes) {
        io(h.level); io(h.xp); io(h.hp); io(h.energy); io(h.mood); io(h.gear); io(h.wallet); io(h.home);
    }
    auto& t=s.trip;
    io(t.floor); io(t.team); io(t.phase); io(t.hp); io(t.result); io(t.round);
}
template<class S, class Transfer>
void items_and_growth(S& s,Transfer io) {
    for (auto& count:s.stock) io(count);
    for (auto& hero:s.heroes) {
        for (auto& item:hero.equipment) io(item);
        for (auto& talent:hero.talents) io(talent);
    }
    io(s.trip.loot);
}
template<class S,class Transfer>
void expedition_report_fields(S& s,Transfer io) {
    auto& trip=s.trip;
    io(s.depth_cleared); io(trip.route); io(trip.reward_gold);
    io(trip.reward_fame); io(trip.reward_relics); io(trip.reward_xp);
}
template<class S,class Transfer>
void discipline_fields(S& s,Transfer io) {
    for (auto& hero:s.heroes) {
        io(hero.discipline);
        for (auto& progress:hero.practice) io(progress);
    }
}
template<class S,class Transfer>
void shop_accounts(S& s,Transfer io) {
    for (auto& b:s.buildings) {
        io(b.account.policy);
        io(b.account.today.visits); io(b.account.today.revenue);
        io(b.account.yesterday.visits); io(b.account.yesterday.revenue);
    }
}
template<class S,class Transfer>
void southern_expansion(S& s,Transfer io) {
    for (int i=OriginalCellFlags;i<CellFlags;++i) {
        io(s.roads[i]); io(s.steps[i]);
    }
    for (int i=OriginalCells;i<Cells;++i) io(s.elevations[i]);
    auto& t=s.trip;
    io(t.distance); io(t.patrol_hp); io(t.patrol_timer); io(t.cache_found);
    for (auto& mask:s.cave_caches) io(mask);
}
template<class S,class Transfer>
void cave_branches(S& s,Transfer io) {
    io(s.trip.cave_path); io(s.trip.side_cache_found);
    for (auto& mask:s.side_caches) io(mask);
}
}
Save encode(const State& s) {
    Save data{};
    size_t at=0;
    const auto push=[&](auto n) {
        for (size_t i=0;i<sizeof(n);++i) data[at++]=std::byte((uint32_t(n)>>(8*i))&255u);
    };
    push(Magic);
    fields(s,push);
    for (int i=0;i<OriginalCellFlags;++i) push(s.steps[i]);
    for (int i=0;i<OriginalCells;++i) push(s.elevations[i]);
    push(s.commission.kind); push(s.commission.target); push(s.commission.progress);
    push(s.commission_reward_day); push(s.trip.supplied);
    push(s.trip.tactic); push(s.trained_today);
    items_and_growth(s,push);
    expedition_report_fields(s,push);
    discipline_fields(s,push);
    shop_accounts(s,push);
    southern_expansion(s,push);
    cave_branches(s,push);
    push(s.frontier_next_day);
    push(s.event); push(s.event_attended); push(s.prepared);
    push(checksum(std::span{data}.first(at)));
    return data;
}
bool decode(State& state,std::span<const std::byte> data,bool* previous_map) {
    if (data.size()!=SaveSize && data.size()!=FrontierSaveSize && data.size()!=ForkSaveSize && data.size()!=SouthernSaveSize &&
        data.size()!=ShopSaveSize && data.size()!=DisciplineSaveSize &&
        data.size()!=ExpeditionSaveSize && data.size()!=InventorySaveSize &&
        data.size()!=TacticsSaveSize && data.size()!=CommissionSaveSize && data.size()!=MapSaveSize &&
        data.size()!=LegacySaveSize) return false;
    size_t at=0;
    const auto read=[&](auto& n) {
        uint32_t value=0;
        for (size_t i=0;i<sizeof(n);++i) value|=std::to_integer<uint32_t>(data[at++])<<(8*i);
        n=static_cast<std::remove_reference_t<decltype(n)>>(value);
    };
    uint32_t magic=0,sig=0; read(magic);
    const bool legacy=magic==MapV2 || magic==MapV1;
    const size_t expected=magic==Magic?SaveSize:magic==SaveVC?FrontierSaveSize:
                          magic==SaveVB?ForkSaveSize:magic==SaveVA?SouthernSaveSize:
                          magic==SaveV9?ShopSaveSize:magic==SaveV8?DisciplineSaveSize:
                          magic==SaveV7?ExpeditionSaveSize:magic==SaveV6?InventorySaveSize:
                          magic==SaveV5?TacticsSaveSize:magic==SaveV4?CommissionSaveSize:
                          magic==SaveV3?MapSaveSize:legacy?LegacySaveSize:0;
    if (data.size()!=expected) return false;
    State s{};
    fields(s,read);
    if (!legacy) {
        for (int i=0;i<OriginalCellFlags;++i) read(s.steps[i]);
        for (int i=0;i<OriginalCells;++i) read(s.elevations[i]);
    }
    if (data.size()>=CommissionSaveSize) {
        read(s.commission.kind); read(s.commission.target); read(s.commission.progress);
        read(s.commission_reward_day); read(s.trip.supplied);
    }
    if (data.size()>=TacticsSaveSize) {read(s.trip.tactic); read(s.trained_today);}
    if (data.size()>=InventorySaveSize) items_and_growth(s,read);
    if (data.size()>=ExpeditionSaveSize) expedition_report_fields(s,read);
    if (data.size()>=DisciplineSaveSize) discipline_fields(s,read);
    if (data.size()>=ShopSaveSize) shop_accounts(s,read);
    if (data.size()>=SouthernSaveSize) southern_expansion(s,read);
    else {
        constexpr unsigned old_bits=OriginalCells%8?(1u<<(OriginalCells%8))-1:255;
        s.roads[OriginalCellFlags-1]&=old_bits;
        s.steps[OriginalCellFlags-1]&=old_bits;
    }
    if (data.size()>=ForkSaveSize) cave_branches(s,read);
    if (data.size()>=FrontierSaveSize) read(s.frontier_next_day);
    if (magic==Magic) {
        read(s.event); read(s.event_attended); read(s.prepared);
    } else if (s.festival) s.event=TownEvent::Lanterns;
    read(sig);
    for (const auto height:s.elevations)
        if (height>MaxElevation+1) return false;
    if (sig!=checksum(data.first(data.size()-4)) || !s.seed || !s.day ||
        !s.rank || s.rank>5 || s.count<2 || s.count>MaxBuildings || s.clock>=Game::DayBeats ||
        s.cleared>=1u<<QuestCount || (s.depth_cleared&~s.cleared) || s.festival>2 || s.charms>8 || s.victory>1 ||
        s.commission_reward_day>s.day || s.frontier_next_day>s.day+FrontierRestDays ||
        (s.frontier_next_day && s.rank<FrontierHunt.rank)) return false;
    if (s.event==TownEvent::None?(s.festival || s.event_attended || s.prepared):
        (!valid_event(s.event) || s.rank<event_info(s.event).rank)) return false;
    const auto& order=s.commission;
    switch (order.kind) {
    case CommissionKind::None: if (order.target) return false; break;
    case CommissionKind::Survey: if (order.target>=QuestCount) return false; break;
    case CommissionKind::Trade: if (order.target<8 || order.target>16) return false; break;
    case CommissionKind::Build: if (!order.target || order.target>2) return false; break;
    case CommissionKind::Hunt: if (data.size()<DisciplineSaveSize || order.target<2 || order.target>3) return false; break;
    default: return false;
    }
    if (order.progress>order.goal() || (order.kind!=CommissionKind::None && s.commission_reward_day==s.day))
        return false;
    for (int i=0;i<s.count;++i) {
        const auto& b=s.buildings[i];
        if (!inside(b.at()) || (data.size()<SouthernSaveSize && b.y>=OriginalHeight) || b.kind>=Road || !b.level || b.level>3)
            return false;
        const auto& account=b.account;
        if (!valid_policy(account.policy) || (!paid_service(b.kind) && account.policy!=ServicePolicy::Standard))
            return false;
        for (const auto day:{account.today,account.yesterday})
            if (day.visits>ShopAccount::DailyLimit || day.revenue>ShopAccount::DailyLimit ||
                ((day.visits || day.revenue) && !paid_service(b.kind))) return false;
    }
    for (const auto& h:s.heroes) {
        if (spent_points(h)>std::max(0,h.level-1)) return false;
        if (h.discipline!=Discipline::None &&
            (!valid_discipline(h.discipline) || !h.practice[unsigned(h.discipline)-1])) return false;
        for (const auto progress:h.practice)
            if (progress>MasteryLimit || (progress && h.level<DisciplineLevel)) return false;
        for (const auto allocation:h.talents) if (allocation>TalentLimit) return false;
        for (int slot=0;slot<GearSlots;++slot) {
            const auto item=h.equipment[slot];
            if (item==Item::None) continue;
            if (!h.level || !valid_item(item) || int(item_type(item).group)!=slot) return false;
        }
        if (!h.level) continue;
        if (h.level>LevelLimit || h.hp>100 || h.energy>100 || h.mood>100 || h.gear>6 ||
            (h.home!=Game::NoHome && (h.home>=s.count || s.buildings[h.home].kind!=House))) return false;
    }
    for (const auto count:s.stock) if (count>StockLimit) return false;
    auto& t=s.trip;
    if (t.phase>(data.size()>=SouthernSaveSize?Exploring:Returning) || t.floor>=QuestCount ||
        t.result>2 || t.route>(data.size()>=FrontierSaveSize?Route::Frontier:Route::Depths) ||
        t.hp>guardian_health(t.floor,t.route) || t.supplied>1 ||
        !valid_tactic(t.tactic) || (t.loot!=Item::None && (!valid_item(t.loot) || t.result!=1)) ||
        (t.phase!=Idle && (!t.team || std::popcount(t.team)>3))) return false;
    if ((t.route==Route::Depths && !(s.cleared&(1u<<t.floor))) ||
        (t.result!=1 && (t.reward_gold || t.reward_fame || t.reward_relics || t.reward_xp)) ||
        t.reward_gold>800 || t.reward_fame>60 || t.reward_relics>11 || t.reward_xp>40) return false;
    if (s.cave_caches[0]>=(1u<<QuestCount) || (s.cave_caches[1]&~s.cleared) ||
        s.side_caches[0]>=(1u<<QuestCount) || (s.side_caches[1]&~s.cleared)) return false;
    if (t.route==Route::Frontier) {
        if (t.floor || s.rank<FrontierHunt.rank || t.phase==Exploring || t.distance || t.patrol_hp ||
            t.patrol_timer || t.cache_found || t.side_cache_found || t.cave_path!=CavePath::Unchosen ||
            (t.result==1 && !s.frontier_next_day) ||
            ((t.phase==Fighting || t.phase==Outbound) && (t.result || s.day<s.frontier_next_day))) return false;
    } else {
        if (magic==SaveVA && t.distance>cave_fork(t)) t.cave_path=CavePath::Main;
        const int base=cave_length(t.floor,t.route), length=cave_distance(t), fork=cave_fork(t);
        const bool at_patrol=t.distance==base/4 || (second_patrol(t.floor,t.route) && t.distance==3*base/4) ||
                             (in_side_cave(t) && t.distance==side_patrol(t));
        const unsigned route=unsigned(t.route), floor_mask=1u<<t.floor;
        if (t.cave_path>CavePath::Side || t.distance>length || t.cache_found>1 || t.side_cache_found>1 ||
            (t.distance>fork && t.cave_path==CavePath::Unchosen) ||
            (t.distance<fork && t.cave_path!=CavePath::Unchosen) ||
            (t.phase==Exploring && (t.distance==length || t.result)) ||
            (t.patrol_hp && (t.phase!=Exploring || !at_patrol || t.patrol_hp>current_patrol_health(t))) ||
            (t.patrol_timer && (!t.patrol_hp || t.patrol_timer>=10)) ||
            (t.cache_found && (t.distance<base/2 || !(s.cave_caches[route]&floor_mask))) ||
            (t.side_cache_found && (t.cave_path!=CavePath::Side || t.distance<side_cache(t) ||
                                    !(s.side_caches[route]&floor_mask)))) return false;
    }
    if (t.phase!=Idle && (s.prepared&t.team)) return false;
    for (int i=0;i<HeroCount;++i)
        if (((t.team|s.event_attended|s.prepared)&(1u<<i)) && !s.heroes[i].level) return false;
    if (previous_map) *previous_map=legacy;
    state=s;
    return true;
}
}
