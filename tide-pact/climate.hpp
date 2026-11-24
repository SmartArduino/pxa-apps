#pragma once

namespace tide {

inline constexpr int CalendarYearDays=365;
inline constexpr std::array MonthStarts{0,31,59,90,120,151,181,212,243,273,304,334};

constexpr int annual_day(int day) { return day%CalendarYearDays; }

struct CalendarDate { int month, day; };
constexpr CalendarDate calendar_date(int voyage_day) {
    const int day=annual_day(voyage_day);
    int month=11;
    while (day<MonthStarts[month]) --month;
    return {month+1,day-MonthStarts[month]+1};
}

enum ClimateSeason { Spring, Summer, Autumn, Winter, DrySeason, WetSeason, SouthwestMonsoon, NortheastMonsoon, BetweenMonsoons };
inline constexpr Text ClimateSeasonNames[]{
    "春季","夏季","秋季","冬季","旱季","雨季","西南季风","东北季风","季风过渡期"
};

constexpr ClimateSeason monsoon_season(int year_day) {
    if (year_day>=MonthStarts[5] && year_day<MonthStarts[9]) return SouthwestMonsoon;
    if (year_day>=MonthStarts[10] || year_day<MonthStarts[2]) return NortheastMonsoon;
    return BetweenMonsoons;
}

constexpr bool guinea_rains(int year_day) {
    return (year_day>=MonthStarts[3] && year_day<MonthStarts[7]) ||
           (year_day>=MonthStarts[8] && year_day<MonthStarts[11]);
}

constexpr bool swahili_rains(int year_day) {
    return (year_day>=MonthStarts[2] && year_day<MonthStarts[5]) ||
           year_day>=MonthStarts[9];
}

constexpr bool arabian_sea(Point p) {
    return p.x>=chart_point(44,0).x && p.x<=chart_point(85,0).x &&
           p.y>=chart_point(0,24).y && p.y<=chart_point(0,-6).y;
}

constexpr bool guinea_coast(Point p) {
    return p.x>=chart_point(-15,0).x && p.x<=chart_point(12,0).x &&
           p.y>=chart_point(0,11).y && p.y<=chart_point(0,1).y;
}

constexpr ClimateSeason temperate_season(int day,bool south) {
    const int year_day=annual_day(day+(south?181:0));
    if (year_day<MonthStarts[2] || year_day>=MonthStarts[11]) return Winter;
    if (year_day<MonthStarts[5]) return Spring;
    if (year_day<MonthStarts[8]) return Summer;
    return Autumn;
}

constexpr ClimateSeason local_season(Point p,int day) {
    const int year_day=annual_day(day);
    if (arabian_sea(p)) return monsoon_season(year_day);
    if (guinea_coast(p)) return guinea_rains(year_day)?WetSeason:DrySeason;
    if (p.y>=chart_point(0,12).y && p.y<=chart_point(0,-12).y) {
        const bool wet=p.x>=chart_point(26,0).x?swahili_rains(year_day):guinea_rains(year_day);
        return wet?WetSeason:DrySeason;
    }
    return temperate_season(day,p.y>chart_point(0,0).y);
}

enum LandCondition { ClearTrail, DryHeat, WetTrail, FloodedTrail, SnowTrail, Whiteout };
inline constexpr Text LandConditionNames[]{
    "晴朗","干热","降雨","河水上涨","地面积雪","风雪"
};

constexpr bool shortcut_closed(LandCondition condition) {
    return condition==FloodedTrail || condition==Whiteout;
}

} // namespace tide
