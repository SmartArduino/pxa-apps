#include "game.hpp"

namespace tide {
namespace {

bool inside_land(Point p) {
    return p.x>=0 && p.x<LandColumns && p.y>=0 && p.y<LandRows;
}

bool land_friends(int a,int b) { return (a<LandFriends)==(b<LandFriends); }

bool can_choose(const LandFight& fight,int unit,int order) {
    return fight.kind && unit>=0 && unit<LandUnits && fight.units[unit].hp &&
           !(fight.units[unit].orders&order);
}

int land_damage(const LandFight& fight,int attacker,int defender) {
    const auto& unit=fight.units[defender];
    const int base=attacker<LandFriends || attacker>=LandUnits-2?2:1;
    const bool shield=(unit.orders&LandBraced) || land_ground(fight,unit.cell)==LandCover;
    return std::max(1,base-int(shield));
}

void land_hit(LandFight& fight,int attacker,int defender) {
    auto& target=fight.units[defender];
    target.hp=std::max(0,int(target.hp)-land_damage(fight,attacker,defender));
    fight.units[attacker].orders|=LandStruck;
}

int opponent_at_range(const LandFight& fight,int attacker) {
    int best=-1, best_score=0;
    for (int i=0;i<LandFriends;++i) {
        if (!land_can_strike(fight,attacker,i)) continue;
        const int damage=land_damage(fight,attacker,i);
        const int hp=fight.units[i].hp;
        const int score=damage*4+int(damage>=hp)*12+int(i==0)*3-hp;
        if (score>best_score || best<0) { best=i; best_score=score; }
    }
    return best;
}

using LandDistances=std::array<std::uint8_t,LandCells>;
constexpr std::uint8_t Unreachable=255;
constexpr Point Neighbours[]{{-1,0},{1,0},{0,-1},{0,1}};

LandDistances land_paths(std::uint64_t passable,std::uint64_t starts,int mobility=LandCells) {
    std::array<std::uint8_t,LandCells> steps;
    steps.fill(Unreachable);
    std::array<std::uint8_t,LandCells> queue{};
    int count=0;
    for (int cell=0;cell<LandCells;++cell) {
        if (!(starts&(std::uint64_t{1}<<cell))) continue;
        steps[cell]=0;
        queue[count++]=cell;
    }
    for (int i=0;i<count;++i) {
        const int cell=queue[i];
        if (steps[cell]>=mobility) continue;
        const auto p=land_point(cell);
        for (const auto delta : Neighbours) {
            const Point next{p.x+delta.x,p.y+delta.y};
            if (!inside_land(next)) continue;
            const int index=land_cell(next);
            if (steps[index]!=Unreachable || !(passable&(std::uint64_t{1}<<index))) continue;
            steps[index]=steps[cell]+1;
            queue[count++]=index;
        }
    }
    return steps;
}

constexpr std::uint64_t land_bit(int cell) { return std::uint64_t{1}<<cell; }

std::uint64_t land_open(const LandFight& fight,int unit) {
    std::uint64_t open=0;
    for (int cell=0;cell<LandCells;++cell)
        if (land_ground(fight,cell)!=LandBarrier) open|=land_bit(cell);
    for (int i=0;i<LandUnits;++i)
        if (i!=unit && fight.units[i].hp) open&=~land_bit(fight.units[i].cell);
    return open;
}

int enemy_destination(const LandFight& fight,int unit,int objective) {
    const auto& enemy=fight.units[unit];
    const auto open=land_open(fight,unit);
    const auto moves=land_paths(open,land_bit(enemy.cell),2);
    std::uint64_t engagement=0;
    for (int rival=0;rival<LandFriends;++rival) {
        if (!fight.units[rival].hp) continue;
        const auto p=land_point(fight.units[rival].cell);
        for (const auto delta : Neighbours) {
            const Point cell{p.x+delta.x,p.y+delta.y};
            if (inside_land(cell)) engagement|=land_bit(land_cell(cell));
        }
    }
    const auto approach=land_paths(open,engagement&open);
    const bool attack_flag=fight.kind==WharfDefense;
    const bool guard_flag=!attack_flag && unit==LandUnits-1;
    const auto flag_path=land_paths(open,(attack_flag || guard_flag)?open&land_bit(objective):0);
    auto candidate=fight;
    int best=enemy.cell, best_score=-10000;
    for (int cell=0;cell<LandCells;++cell) {
        if (moves[cell]==Unreachable) continue;
        candidate.units[unit].cell=cell;
        const int target=opponent_at_range(candidate,unit);
        const bool cover=land_ground(fight,cell)==LandCover;
        int distance=approach[cell];
        if (flag_path[cell]!=Unreachable)
            distance=attack_flag?std::min(distance,int(flag_path[cell])):std::min(distance+2,int(flag_path[cell]));
        int score=-distance*7-moves[cell]+int(cover)*3;
        if (target>=0) {
            const int damage=land_damage(candidate,unit,target);
            score+=23+damage*3+int(damage>=fight.units[target].hp)*12+int(target==0)*2;
        }
        if (cell==objective && (guard_flag || attack_flag)) score+=attack_flag?70:12;
        if (score>best_score) { best=cell; best_score=score; }
    }
    return best;
}

} // namespace

int land_occupant(const LandFight& fight,int cell) {
    for (int i=0;i<LandUnits;++i)
        if (fight.units[i].hp && fight.units[i].cell==cell) return i;
    return -1;
}

std::array<std::uint8_t,LandCells> land_steps(const LandFight& fight,int unit) {
    if (!can_choose(fight,unit,LandMoved)) return land_paths(0,0);
    return land_paths(land_open(fight,unit),land_bit(fight.units[unit].cell),unit==0?3:2);
}

bool land_can_strike(const LandFight& fight,int unit,int target) {
    if (!can_choose(fight,unit,LandStruck) || target<0 || target>=LandUnits ||
        !fight.units[target].hp || land_friends(unit,target)) return false;
    const int from=fight.units[unit].cell, to=fight.units[target].cell;
    const int distance=land_distance(from,to);
    if (distance==1) return true;
    if (distance!=2 || unit!=1) return false;
    const auto a=land_point(from),b=land_point(to);
    if (a.x!=b.x && a.y!=b.y) return false;
    const int between=(from+to)/2;
    return land_ground(fight,between)!=LandBarrier && land_occupant(fight,between)<0;
}

bool valid_land_fight(const LandFight& fight,int origin,bool underway) {
    if (!fight.kind) {
        if (fight.site || fight.round || fight.holding) return false;
        return std::none_of(fight.units.begin(),fight.units.end(),[](LandUnit u){return u.hp || u.orders || u.cell;});
    }
    if (underway || fight.kind>WharfDefense || !fight.round || fight.round>30 || fight.holding>1 ||
        fight.site>=EnclaveCount || (fight.kind==RoadEscort?fight.site || origin!=lisbon:origin!=EnclaveSites[fight.site].home))
        return false;
    std::uint64_t occupied=0;
    for (int i=0;i<LandUnits;++i) {
        const auto unit=fight.units[i];
        if (unit.cell>=LandCells || unit.hp>land_max_hp(i) || (unit.orders&~7) ||
            ((unit.orders&LandBraced) && !(unit.orders&LandStruck))) return false;
        if (!unit.hp) continue;
        const auto bit=std::uint64_t{1}<<unit.cell;
        if ((occupied&bit) || land_ground(fight,unit.cell)==LandBarrier) return false;
        occupied|=bit;
    }
    return fight.units[0].hp>0 &&
           std::any_of(fight.units.begin()+LandFriends,fight.units.end(),[](LandUnit u){return u.hp>0;});
}

Text Game::land_battle_requirement(int kind,int site) const {
    if (s.land_fight.kind) return "先结束当前战斗";
    if (sea() || encounter() || land_away()) return "请在泊地集结人手";
    if (kind==RoadEscort) {
        if (s.origin!=lisbon) return "林道护送在里斯本办理";
        if (s.shore_campaign&1) return "护送已经完成";
        if (s.crew<6 || s.stores<4) return "需要六名船员和四份口粮";
        return {};
    }
    if (site<0 || site>=EnclaveCount || enclave_here()!=site) return "先到据点锚地";
    if (kind==WharfAssault) return enclave_siege_requirement(site);
    if (kind!=WharfDefense) return "这里没有陆战委托";
    if (s.shore_campaign&(2U<<site)) return "仓库的骚扰已经平息";
    const auto& holding=s.enclaves[site];
    if (!holding.status || holding.residents<3 || holding.food<12)
        return "据点需有三位居民和十二份存粮";
    return {};
}

bool Game::begin_land_battle(int kind,int site) {
    const auto reason=land_battle_requirement(kind,site);
    if (!reason.empty()) { notice=reason; return false; }
    s.land_fight={};
    auto& fight=s.land_fight;
    fight.kind=kind;
    fight.site=site;
    fight.round=1;
    const auto& field=land_field(kind,site);
    constexpr Point defenders[]{{7,3},{8,1},{8,4}};
    constexpr Point raiders[]{{0,1},{1,2},{0,4},{2,5}};
    for (int i=0;i<LandUnits;++i) {
        const auto start=kind==WharfDefense?(i<LandFriends?defenders[i]:raiders[i-LandFriends]):field.starts[i];
        fight.units[i]={static_cast<std::uint8_t>(land_cell(start)),static_cast<std::uint8_t>(land_max_hp(i)),0};
    }
    notice="点选小人，再点地格移动或攻击";
    return true;
}

bool Game::move_land_unit(int unit,int cell) {
    if (unit<0 || unit>=LandFriends || cell<0 || cell>=LandCells) return false;
    auto& fight=s.land_fight;
    const auto steps=land_steps(fight,unit);
    if (!steps[cell] || steps[cell]==255) return false;
    fight.units[unit].cell=cell;
    fight.units[unit].orders|=LandMoved;
    notice=land_ground(fight,cell)==LandCover?"已进入掩体":"小队成员已就位";
    settle_land_battle();
    return true;
}

bool Game::strike_land_unit(int unit,int target) {
    auto& fight=s.land_fight;
    if (unit<0 || unit>=LandFriends || !land_can_strike(fight,unit,target)) return false;
    land_hit(fight,unit,target);
    notice=fight.units[target].hp?"命中对手":"对手退出战斗";
    settle_land_battle();
    return true;
}

bool Game::brace_land_unit(int unit) {
    if (unit<0 || unit>=LandFriends || !can_choose(s.land_fight,unit,LandStruck)) return false;
    s.land_fight.units[unit].orders|=LandStruck|LandBraced;
    notice="已架盾，防护持续到下回合";
    return true;
}

bool Game::end_land_round() {
    auto& fight=s.land_fight;
    if (!fight.kind) return false;
    const int objective=land_cell(land_field(fight.kind,fight.site).objective);
    const int flag=land_occupant(fight,objective);
    fight.holding=fight.kind==WharfDefense || (fight.kind==WharfAssault && flag>=0 && flag<LandFriends);
    for (int i=LandFriends;i<LandUnits;++i) {
        auto& enemy=fight.units[i];
        if (!enemy.hp) continue;
        enemy.orders=0;
        const int next=enemy_destination(fight,i,objective);
        if (next!=enemy.cell) { enemy.cell=next; enemy.orders|=LandMoved; }
        const int target=opponent_at_range(fight,i);
        if (target>=0) land_hit(fight,i,target);
        else enemy.orders|=LandBraced|LandStruck;
        if (!fight.units[0].hp || (fight.kind==WharfDefense && enemy.cell==objective)) break;
    }
    if (settle_land_battle(false,true)) return true;
    ++fight.round;
    fight.holding=0;
    for (int i=0;i<LandFriends;++i) fight.units[i].orders=0;
    notice="轮到你，队员可以再次行动";
    return true;
}

bool Game::settle_land_battle(bool retreat,bool after_enemy_turn) {
    const auto& fight=s.land_fight;
    if (!fight.kind) return false;
    const int kind=fight.kind, site=fight.site;
    const int objective=land_cell(land_field(kind,site).objective);
    const int flag=land_occupant(fight,objective);
    const bool losses=retreat || !fight.units[0].hp ||
                      (kind==WharfDefense && flag>=LandFriends);
    const bool clear=std::none_of(fight.units.begin()+LandFriends,fight.units.end(),[](LandUnit u){return u.hp;});
    const bool victory=!losses && (clear || (kind==RoadEscort && fight.units[0].cell==objective) ||
        (kind==WharfAssault && fight.holding && flag>=0 && flag<LandFriends) ||
        (kind==WharfDefense && fight.round>=5 && fight.holding));
    if (!losses && !victory && !(after_enemy_turn && fight.round>=30)) return false;
    s.land_fight={};
    if (kind==WharfAssault) {
        resolve_enclave_siege(site,victory);
    } else if (kind==RoadEscort) {
        s.stores=std::max(0,s.stores-4);
        if (victory) {
            s.shore_campaign|=1;
            s.renown=std::min(999999,s.renown+1);
            earn(90);
        }
        notice=victory?"护送成功，货主给了90金":"护送中断，队伍退回城门";
    } else {
        auto& holding=s.enclaves[site];
        if (victory) {
            s.shore_campaign|=2U<<site;
            holding.exports=std::min(EnclaveExportCap,int(holding.exports)+4);
            ++s.trust;
        } else holding.food=std::max(0,int(holding.food)-8);
        notice=victory?"仓房守住了，居民送来四份货物":"队伍撤回仓房，损失八份存粮";
    }
    return true;
}

void Game::withdraw_land_battle() {
    settle_land_battle(true);
}

} // namespace tide
