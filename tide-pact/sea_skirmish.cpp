#include "game.hpp"

#include <algorithm>
#include <cmath>

namespace tide {
namespace {

constexpr int EnemyReach=66, ChaseLimit=151;
constexpr int ShotSpeed=9, HullRadius=9;

int distance_squared(Point a,Point b) {
    const int dx=a.x-b.x, dy=a.y-b.y;
    return dx*dx+dy*dy;
}

Point step_toward(Point position,int heading,int speed) {
    const auto direction=Directions[heading];
    return {position.x+int(std::lround(direction.x*speed/16.f)),
            position.y+int(std::lround(direction.y*speed/16.f))};
}

bool in_water(Point fixed,WaterTest water) {
    return fixed.x>=0 && fixed.y>=0 && fixed.x<ChartWidth*16 && fixed.y<ChartHeight*16 &&
           water({fixed.x/16,fixed.y/16});
}

int near_broadside(Point from,Point target,int heading) {
    const int bearing=direction_to({target.x-from.x,target.y-from.y});
    const int relative=(bearing-heading+8)%8;
    if (relative>=1 && relative<=3) return 1;
    return relative>=5 && relative<=7?-1:0;
}

bool fire(SeaShot& shot,Point at,int heading,int side,int distance) {
    if (shot.steps) return false;
    shot={at,static_cast<std::uint8_t>((heading+side*2+8)%8),
          static_cast<std::uint8_t>((distance+ShotSpeed-1)/ShotSpeed)};
    return true;
}

enum ShotResult { InFlight, InWater, Struck };

ShotResult fly(SeaShot& shot,Point target,WaterTest water) {
    if (!shot.steps) return InFlight;
    const auto from=shot.position;
    const auto end=step_toward(from,shot.heading,ShotSpeed);
    for (int i=1;i<=ShotSpeed;++i) {
        shot.position={from.x+(end.x-from.x)*i/ShotSpeed,from.y+(end.y-from.y)*i/ShotSpeed};
        if (!in_water(shot.position,water)) {
            shot.steps=0;
            return InWater;
        }
        if (distance_squared(shot.position,target)<=HullRadius*HullRadius) {
            shot.steps=0;
            return Struck;
        }
    }
    return --shot.steps?InFlight:InWater;
}

int turn_distance(int a,int b) {
    const int shift=(a-b+8)%8;
    return std::min(shift,8-shift);
}

int rival_course(const SeaSkirmish& battle,Point player,int speed,int preferred_range,
                 bool fleeing,WaterTest water) {
    int best=battle.heading, best_score=-1000000;
    for (int heading=0;heading<8;++heading) {
        const auto first=step_toward(battle.rival,heading,speed);
        const auto ahead=step_toward(battle.rival,heading,speed*4);
        const auto middle=step_toward(battle.rival,heading,speed*2);
        if (!in_water(first,water)) continue;
        const bool clear=in_water(middle,water) && in_water(ahead,water);
        const auto position=clear?ahead:first;
        const int gap=distance_squared(player,position);
        int score=fleeing?gap:-std::abs(gap-preferred_range*preferred_range);
        if (!fleeing && gap<EnemyReach*EnemyReach && near_broadside(position,player,heading)) score+=380;
        if (fleeing && battle.opponent>=FirstMerchantOpponent && !clear) score-=350;
        if (!clear) score-=450;
        if (battle.rope_ticks && distance_squared(position,battle.rope)<=15*15) score-=280;
        score-=turn_distance(heading,battle.heading)*17;
        if (score>best_score) { best=heading; best_score=score; }
    }
    return best;
}

int predicted_broadside(Point origin,int heading,Point player,int player_heading,int speed,WaterTest water) {
    const int side=near_broadside(origin,player,heading);
    if (!side) return 0;
    SeaShot trial;
    fire(trial,origin,heading,side,EnemyReach);
    while (trial.steps) {
        const auto result=fly(trial,player,water);
        if (result==Struck) return side;
        if (result==InWater) break;
        const auto next=step_toward(player,player_heading,speed);
        if (in_water(next,water)) player=next;
    }
    return 0;
}

} // namespace

bool Game::begin_skirmish(WaterTest water) {
    if (!s.helm || s.weather!=Privateer || s.skirmish.active) return false;
    const Point player{s.helm_x,s.helm_y};
    for (int offset : {-2,2,3,5,4,-1,1,0}) {
        const int heading=(s.heading+offset+8)%8;
        const auto rival=step_toward(player,heading,58);
        if (!in_water(rival,water) || !in_water(step_toward(player,heading,32),water) ||
            !in_water(step_toward(player,heading,16),water) || !in_water(step_toward(player,heading,48),water)) continue;
        const auto opponent=s.skirmish.opponent, court=s.skirmish.court;
        s.skirmish={.active=true,.opponent=opponent,.court=court};
        s.skirmish.rival=rival;
        s.skirmish.heading=(heading+4)%8;
        s.skirmish.enemy_reload=22;
        if (s.needs_reload) s.skirmish.reload=22;
        paused=true;
        notice=opponent==PatrolOpponent?"巡逻船正在逼近":
            convoy_battle()?"断铃号正在逼近":"私掠船正在逼近";
        return true;
    }
    notice="这片浅滩无法展开海战";
    return false;
}

bool Game::fire_skirmish(int side) {
    auto& battle=s.skirmish;
    if (!battle.active || paused || battle.reload || (side!=-1 && side!=1)) return false;
    if (!fire(battle.shots[0],{s.helm_x,s.helm_y},s.heading,side,SkirmishPlayerReach)) return false;
    battle.reload=skirmish_reload_ticks(s.guns);
    return true;
}

bool Game::drop_drift_rope() {
    auto& battle=s.skirmish;
    if (!battle.active || paused || battle.rope_ticks || !s.cargo[Timber]) return false;
    --s.cargo[Timber];
    battle.rope={s.helm_x,s.helm_y};
    battle.rope_ticks=92;
    notice="投下带浮木的缆索";
    return true;
}

bool Game::can_board_skirmish() const {
    return s.skirmish.active && !paused && s.crew>=6 &&
           distance_squared({s.helm_x,s.helm_y},s.skirmish.rival)<=15*15;
}

bool Game::board_skirmish() {
    if (!can_board_skirmish()) return false;
    const auto opponent=s.skirmish.opponent, court=s.skirmish.court;
    s.skirmish={.opponent=opponent,.court=court};
    s.range=1;
    start_duel(Boarding);
    notice="两船接舷";
    return true;
}

unsigned Game::tick_skirmish(WaterTest water) {
    auto& battle=s.skirmish;
    if (!battle.active || paused) return 0;
    unsigned effects=0;
    const auto weather=conditions();
    const int heading_shift=std::abs(((s.heading-weather.direction+12)%8)-4);
    const int speed=std::clamp(3+int(weather.strength>=3 && heading_shift<=1)-
        int(s.reefed)-int(s.hull<35)-int(weather.weather==Squall && !s.reefed),1,4);
    Point player{s.helm_x,s.helm_y};
    const auto next=step_toward(player,s.heading,speed);
    if (in_water(next,water)) {
        player=next;
        s.helm_x=player.x;
        s.helm_y=player.y;
    }
    const int separation=distance_squared(player,battle.rival);
    if (separation>=ChaseLimit*ChaseLimit) {
        if (s.enemy<=15 && battle.opponent==PrivateerOpponent) win_at_sea(false);
        else {
            const auto opponent=battle.opponent;
            log_encounter(1);
            finish_encounter();
            notice=opponent>=FirstMerchantOpponent?"商船已脱离战斗":
                opponent==PatrolOpponent?"已甩开巡逻船":"已甩开私掠船";
        }
        return SkirmishEnded;
    }

    if (battle.rope_ticks) {
        --battle.rope_ticks;
        if (distance_squared(battle.rope,battle.rival)<=12*12) {
            battle.rope_ticks=0;
            battle.snared=25;
            notice="浮索拖住了对方";
            effects|=SkirmishRope;
        }
    }
    if (battle.reload) --battle.reload;
    if (battle.enemy_reload) --battle.enemy_reload;
    const bool merchant=battle.opponent>=FirstMerchantOpponent;
    const bool patrol=battle.opponent==PatrolOpponent;
    const bool fleeing=merchant || s.enemy<=15;
    const int rival_speed=merchant?2:fleeing || patrol?3:2;
    if (battle.turn_delay) --battle.turn_delay;
    if (!battle.turn_delay) {
        const int desired=rival_course(battle,player,rival_speed,patrol?52:37,fleeing,water);
        if (desired!=battle.heading) {
            const int shift=(desired-battle.heading+8)%8;
            battle.heading=(battle.heading+(shift<=4?1:7))%8;
            battle.turn_delay=3;
        }
    }
    if (battle.snared) --battle.snared;
    else {
        const auto away=step_toward(battle.rival,battle.heading,rival_speed);
        if (in_water(away,water)) battle.rival=away;
        else battle.turn_delay=0;
    }

    if (distance_squared(player,battle.rival)<=EnemyReach*EnemyReach && !battle.enemy_reload &&
        !battle.shots[1].steps && !fleeing) {
        const int side=predicted_broadside(battle.rival,battle.heading,player,s.heading,speed,water);
        if (side && fire(battle.shots[1],battle.rival,battle.heading,side,EnemyReach)) {
            battle.enemy_reload=patrol?34:40;
            effects|=SkirmishEnemyFire;
        }
    }
    const auto player_hit=fly(battle.shots[0],battle.rival,water);
    if (player_hit==Struck) {
        s.enemy-=13+s.guns*8;
        if (s.enemy<=0) {
            win_at_sea(false);
            return effects|SkirmishEnded|SkirmishHit;
        }
        effects|=SkirmishHit;
        notice="炮弹击中敌船";
    }
    const auto enemy_hit=fly(battle.shots[1],player,water);
    if (enemy_hit==Struck) {
        const int damage=s.first_mate==Boatswain?6:9;
        s.hull-=damage;
        wear_company_hull(damage/3);
        s.spirits=std::max(0,s.spirits-2);
        if (s.hull<=0) {
            log_encounter(2);
            rescue();
            return effects|SkirmishEnded|SkirmishDamaged;
        }
        effects|=SkirmishDamaged;
        notice="船身被打中";
    }
    if (player_hit==InWater || enemy_hit==InWater) effects|=SkirmishSplash;
    return effects;
}

} // namespace tide
