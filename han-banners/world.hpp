#pragma once
#include <utility>
#include <iterator>

namespace han {
constexpr int cityCount = 32;
constexpr int ownerCount = 8;
constexpr int officerCount = 8;
constexpr int guestSlot = officerCount;
constexpr int commanderSlots = officerCount+1;
constexpr int columnCount = 3;
constexpr int garrison = 100;
constexpr int armyLimit = 2400;
constexpr int lastTurn = 64;
constexpr int worldWidth = 512, worldHeight = 416;

enum Formation { spear, bow, horse };
enum Terrain { plain, hills, river };
enum Work { recruit, farm, walls, commerce, pacify };
enum Gear { sword, armor, saddle, manual, noGear };
enum Maneuver { measured, press, hold, flank };
enum DuelMove { thrust, parry, feint };
enum DispatchSize { vanguard, division, legion };
enum Power { charge, guard, guile, inspire, wildfire };

struct Officer {
    const char* name;
    const char* skill;
    const char* hint;
    Formation favored;
    Power power;
    int strength;
    int portrait;
    int valor, counsel;
};
constexpr Officer officers[3][officerCount] = {
    {{"赵云","龙胆冲阵","突击兼稳军心",horse,charge,132,3,96,74},
     {"关羽","青龙破阵","枪阵破城更强",spear,charge,163,6,98,77},
     {"张飞","长坂断喝","震慑敌军军心",spear,guile,42,7,99,52},
     {"诸葛亮","八阵伏兵","护军并补充军令",bow,guard,20,8,39,100},
     {"黄忠","百步穿杨","弓阵强攻压阵",bow,charge,179,9,95,73},
     {"法正","奇谋破壁","瓦解阵势与军心",bow,guile,49,18,45,96},
     {"魏延","子午奇兵","骑阵突击更强",horse,charge,156,19,93,76},
     {"马超","西凉铁骑","骑阵冲击兼稳军",horse,charge,183,20,97,65}},
    {{"夏侯惇","拔矢奋战","鼓舞我军军心",spear,inspire,80,4,92,72},
     {"张辽","威震逍遥","骑袭令敌胆寒",horse,guile,28,10,95,82},
     {"曹仁","铁垒坚守","抵消两轮损失",spear,guard,5,11,88,81},
     {"徐晃","长驱破壁","强攻压制守军",horse,charge,173,12,92,78},
     {"贾诩","离间军心","削弱敌军阵势",bow,guile,46,13,42,98},
     {"张郃","应变突围","护军并补充军令",horse,guard,15,21,94,83},
     {"许褚","虎痴震阵","近阵猛攻兼稳军",spear,charge,186,22,98,49},
     {"满宠","合围坚城","护军愈战愈勇",bow,guard,22,23,79,91}},
    {{"周瑜","江火连营","水战可重创敌军",bow,wildfire,125,5,76,98},
     {"甘宁","锦帆夜袭","快攻兼挫敌心",horse,charge,168,14,94,77},
     {"吕蒙","白衣渡江","护军并补充军令",spear,guard,12,15,86,93},
     {"鲁肃","义济三军","回稳军心与军令",bow,inspire,42,16,59,96},
     {"陆逊","焚营断势","水战可重创敌军",bow,wildfire,145,17,75,97},
     {"程普","江表宿卫","护军并补充军令",spear,guard,17,24,91,85},
     {"凌统","横江急袭","疾攻兼提振士气",horse,charge,165,25,92,68},
     {"徐盛","江东疑城","瓦解阵势与军心",bow,guile,43,26,88,86}},
};
constexpr const char* gearNames[]={"青锋剑","山文甲","赤绒鞍","行军策"};
constexpr const char* gearHints[]={"会战伤害增加","减少会战损失","出征节省粮草","开战军令增加"};
constexpr const char* maneuvers[]={"整阵","强攻","固守","迂回"};
constexpr const char* duelMoves[]={"突刺","格挡","佯攻"};
constexpr const char* dispatchNames[]={"先锋","偏师","主力"};
constexpr int levyPercent[]={45,65,80};
struct Faction {const char* title;};
constexpr Faction factions[] = {
    {"刘备军"},{"曹操军"},{"孙权军"},{"荆南诸郡"},
    {"刘璋军"},{"张鲁军"},{"关西诸部"},{"交州守军"},
};
constexpr const char* formations[] = {"枪阵", "弓阵", "骑阵"};
constexpr const char* formationHints[] = {"结阵克骑", "远射克枪", "突袭克弓"};
constexpr const char* terrains[] = {"平原", "山地", "水乡"};

// Gazetteer anchors are approximate present-day coordinates for late-Han sites.
constexpr float worldX(float lon) {return (lon-101.5f)*worldWidth/22.f;}
constexpr float worldY(float lat) {return (41.f-lat)*worldHeight/18.5f;}
struct Site {
    const char* name;
    const char* region;
    float lon, lat;
    Terrain terrain;
    int yield, owner, troops, fort;
    int x() const {return int(worldX(lon)+.5f);}
    int y() const {return int(worldY(lat)+.5f);}
};
constexpr Site sites[] = {
    {"长安","司隶",108.87f,34.30f,hills,4,1,620,2},
    {"洛阳","司隶",112.63f,34.73f,plain,4,1,520,2},
    {"成都","益州",104.07f,30.66f,plain,5,4,940,3},
    {"南郑","益州",107.02f,33.06f,hills,4,5,730,3},
    {"许都","豫州",113.99f,34.01f,plain,5,1,1100,3},
    {"邺城","冀州",114.61f,36.33f,plain,4,1,600,2},
    {"襄阳","荆州",112.15f,32.05f,river,4,1,820,2},
    {"江陵","荆州",112.19f,30.34f,river,5,1,720,2},
    {"秣陵","扬州",118.84f,31.94f,river,4,2,860,2},
    {"柴桑","扬州",115.82f,29.62f,river,4,2,1120,2},
    {"夏口","荆州",114.28f,30.55f,river,4,0,1700,2},
    {"宛城","荆州",112.53f,33.01f,plain,3,1,530,2},
    {"夷陵","荆州",111.29f,30.70f,hills,2,1,520,1},
    {"江州","益州",106.58f,29.56f,river,3,4,630,2},
    {"梓潼","益州",105.17f,31.64f,hills,2,4,510,1},
    {"临湘","长沙郡",112.97f,28.18f,river,4,3,470,1},
    {"泉陵","零陵郡",111.63f,26.24f,hills,2,3,385,1},
    {"临沅","武陵郡",111.69f,29.03f,river,3,3,420,1},
    {"郴县","桂阳郡",113.03f,25.80f,hills,3,3,415,1},
    {"吴县","扬州",120.62f,31.31f,river,5,2,630,2},
    {"山阴","会稽郡",120.59f,30.00f,hills,3,2,550,1},
    {"南昌","豫章郡",115.88f,28.69f,river,3,2,620,2},
    {"寿春","扬州",116.78f,32.58f,river,4,1,490,2},
    {"合肥","扬州",117.25f,31.89f,plain,3,1,600,2},
    {"下邳","徐州",117.89f,34.02f,plain,4,1,540,2},
    {"剧县","青州",118.73f,36.86f,plain,3,1,480,1},
    {"蓟县","幽州",116.36f,39.90f,plain,3,1,480,2},
    {"襄武","凉州",104.63f,35.01f,hills,2,6,560,1},
    {"姑臧","凉州",102.64f,37.93f,hills,2,6,600,2},
    {"番禺","南海郡",113.26f,23.13f,river,3,7,600,2},
    {"葭萌","益州",105.86f,32.35f,hills,2,4,510,1},
    {"广陵","徐州",119.42f,32.39f,river,3,1,520,2},
};
static_assert(std::size(sites) == cityCount);
constexpr const char* cityLabel(int city,int year) {
    if (city==8 && year>=211) return "建业";
    if (city==4 && year>=221) return "许昌";
    return sites[city].name;
}
struct Road {
    int from, to;
    // Optional route bend, following the main valley or regional pass.
    float lon = 0, lat = 0;
    constexpr bool joins(int a,int b) const {
        return (from==a&&to==b)||(from==b&&to==a);
    }
};
constexpr Road roads[] = {
    {0,1,110.06f,34.58f},{0,3,106.83f,34.05f},{0,27,106.11f,34.72f},
    {1,4},{1,5,114.1f,35.1f},{1,11},{2,13,105.05f,29.98f},{2,14},
    {3,12,109.46f,32.35f},{3,30,106.78f,32.85f},{3,27,105.81f,34.12f},
    {4,5},{4,11},{4,22,115.34f,33.1f},{4,24,116.18f,34.3f},
    {5,25},{5,26,115.77f,38.13f},{6,7,112.26f,31.04f},{6,10,113.66f,31.33f},{6,11},
    {7,10,113.16f,29.78f},{7,12},{7,17},{7,15,112.61f,29.42f},
    {8,19,119.48f,31.2f},{8,23,117.96f,31.6f},{8,31},{9,10,115.10f,30.17f},
    {9,21},{9,23,116.78f,30.61f},{10,15,113.55f,29.30f},{11,22,114.82f,32.70f},
    {12,13,109.47f,31.01f},{12,17,110.79f,29.8f},{13,14,105.71f,30.75f},
    {14,30},{15,16,112.45f,27.26f},{15,17},{15,18},{15,21,114.29f,28.06f},
    {16,17,111.22f,27.64f},{16,18},{16,29,112.15f,24.07f},{18,29,113.62f,24.77f},
    {19,20},{20,21,118.33f,28.95f},{21,29,114.67f,25.87f},
    {22,23},{22,24},{23,31},{24,25},{24,31},{25,26,117.29f,38.40f},
    {27,28,103.71f,36.06f},
};
constexpr bool adjacent(int a, int b) {
    for (const auto& r : roads) if (r.joins(a,b)) return true;
    return false;
}
}
