#pragma once
#include <array>
#include <utility>
#include <iterator>

namespace han {
constexpr int oldCityCount = 32;
constexpr int expandedCityCount = 38;
constexpr int seventhCityCount = 48;
constexpr int previousCityCount = 56;
enum AddedSite { fancheng=previousCityCount, shangyong, fucheng, zigui, wancheng,
                 ruxu, linzi, qishan };
constexpr int cityCount = qishan+1;
constexpr int ownerCount = 8;
constexpr int officerCount = 8;
constexpr int guestSlot = officerCount;
constexpr int talentFirst = guestSlot+1;
constexpr int columnCount = 3;
constexpr int garrison = 100;
constexpr int armyLimit = 2400;
constexpr int lastTurn = 84;
constexpr int worldWidth = 512, worldHeight = 512;

enum Troop { spear, bow, horse, shields, crossbow, heavyHorse, troopCount };
enum Formation { square, wedge, wings };
enum Weapon { longspear, saber, longbow };
enum Engine { noEngine, ram, stoneThrower };
constexpr int laneCount=3;
using Lineup=std::array<Troop,laneCount>;
struct BattlePlan {
    constexpr BattlePlan(Troop troop=spear,Formation form=square,Weapon arm=longspear)
        : lineup{troop,troop,troop},formation(form),weapon(arm) {}
    Lineup lineup;
    Formation formation;
    Weapon weapon;
    Engine engine=noEngine;
    int engineLane=1;
};
enum Terrain { plain, hills, river };
enum SiteKind { county, stronghold, crossing, battleSite, pass };
constexpr const char* siteKinds[]={"县邑","军镇","渡口","战场","关隘"};
enum Work { recruit, farm, walls, commerce, pacify };
enum Gear { sword, armor, saddle, manual, twinJian, dragonBlade, snakeSpear, featherBow,
            brightArmor, scaleArmor, redHare, whiteHorse, featherFan, noGear };
enum GearSlot { armsSlot, armorSlot, mountSlot, strategySlot, gearSlotCount };
constexpr int legacyGearCount=4;
constexpr const char* gearSlotNames[]={"兵器","盔甲","坐骑","军策"};
struct GearSpec {
    const char* name;
    const char* hint;
    GearSlot slot;
    int price;
    Weapon weapon=longspear;
};
constexpr GearSpec gearSpecs[]={
    {"青锋剑","挥剑出招，兵器伤害提高",armsSlot,120,saber},
    {"山文甲","会战减损，护住本路武将",armorSlot,150},
    {"赤绒鞍","出征粮耗减四分之一",mountSlot,130},
    {"行军策","开战多18军令",strategySlot,160},
    {"双股剑","近身双击，强攻时更猛",armsSlot,225,saber},
    {"青龙偃月刀","挥斩波及相邻敌阵",armsSlot,340,saber},
    {"丈八蛇矛","出招距离更远，拒骑更强",armsSlot,290,longspear},
    {"铁胎弓","可从后阵远射，射程更长",armsSlot,200,longbow},
    {"明光铠","本路武将受创更轻，全军小幅减损",armorSlot,240},
    {"鱼鳞甲","全军减免部分弓弩伤害",armorSlot,210},
    {"赤兔","阵前带队推进更快，出征省粮",mountSlot,330},
    {"的卢","出征省粮，能护阵前武将",mountSlot,270},
    {"白羽扇","开战多12军令，将技少耗6令",strategySlot,235},
};
static_assert(std::size(gearSpecs)==noGear);
enum Maneuver { measured, press, hold, flank, fallBack };
enum DuelMove { thrust, parry, feint };
enum DispatchSize { vanguard, division, legion };
enum Power { charge, guard, guile, inspire, wildfire, volley, breakSupport, rallyGuard, fireArrow,
             rallyOath, crescentStrike, bridgeRoar, eightArray, whiteSpear, tigerBanner, ironCommand,
             markVolley, eyeValor, suddenRaid, brushFire };
enum class Role { vanguard, marshal, strategist };
inline constexpr const char* roleNames[]={"猛将","统将","军师"};
enum class Specialty { envoy, treasury, convoy, command, scout, granary };

struct Officer {
    const char* name;
    const char* skill;
    const char* hint;
    Troop favored;
    Power power;
    int strength;
    int portrait;
    int valor, counsel;
};
constexpr Role battleRole(const Officer& actor) {
    if ((actor.counsel>=89 && actor.valor<75) || actor.power==brushFire) return Role::strategist;
    if (actor.power==guard || actor.power==inspire || actor.power==rallyGuard ||
        actor.power==rallyOath || actor.power==tigerBanner || actor.power==ironCommand ||
        actor.power==eyeValor) return Role::marshal;
    return Role::vanguard;
}
constexpr Officer officers[3][officerCount] = {
    {{"赵云","龙胆冲阵","集中突击本路，稳住军心",horse,whiteSpear,132,3,96,74},
     {"关羽","青龙破阵","重斩本路并横扫相邻敌阵",spear,crescentStrike,147,6,98,77},
     {"张飞","长坂断喝","震慑军心并打断协战",spear,bridgeRoar,32,7,99,52},
     {"诸葛亮","八阵伏兵","两合护军并扰乱敌方调度",bow,eightArray,20,8,39,100},
     {"黄忠","百步穿杨","超远射击本路，弓阵更强",bow,markVolley,179,9,95,73},
     {"法正","奇谋破壁","瓦解阵势与军心",bow,guile,49,18,45,96},
     {"魏延","子午奇兵","骑阵突击更强",horse,charge,156,19,93,76},
     {"马超","西凉铁骑","骑阵冲击兼稳军",horse,charge,183,20,97,65}},
    {{"夏侯惇","拔矢奋战","回复体力，稳军心并护军",spear,eyeValor,32,4,92,72},
     {"张辽","威震逍遥","吓退敌心，本路强攻两合",horse,suddenRaid,104,10,95,82},
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
     {"陆逊","焚营断势","火攻本路，拖慢敌军并耗军令",bow,brushFire,125,17,75,97},
     {"程普","江表宿卫","护军并补充军令",spear,guard,17,24,91,85},
     {"凌统","横江急袭","疾攻兼提振士气",horse,charge,165,25,92,68},
     {"徐盛","江东疑城","瓦解阵势与军心",bow,guile,43,26,88,86}},
};
inline constexpr Officer rulers[]={
    {"刘备","同袍誓约","恢复军心，集结溃兵并护军一合",shields,rallyOath,27,0,80,88},
    {"曹操","临阵筹谋","蓄满军令，护军一合",spear,ironCommand,0,1,86,96},
    {"孙权","江东旌旗","恢复军心，三路两合拒敌",spear,tigerBanner,24,2,79,90},
};
struct WildTalent {
    Officer officer;
    int city[3];
    int fee;
    Specialty specialty;
    const char* service;
};
inline constexpr WildTalent talents[] = {
    {{"程砚","守望军阵","护军并补充军令",spear,guard,28,27,61,94},
     {10,30,2},135,Specialty::envoy,"任太守时，每季额外恢复6民心"},
    {{"许棠","算无遗筹","削弱敌军军心",bow,guile,36,28,48,96},
     {4,4,4},160,Specialty::treasury,"任太守时，每季增加22金"},
    {{"沈汐","横波破阵","水战可重创敌军",bow,wildfire,146,29,83,87},
     {9,8,8},175,Specialty::convoy,"担任领军时，出征最多省22粮"},
    {{"霍毅","铁槊突阵","突击兼稳军心",horse,charge,177,30,97,61},
     {7,7,7},175,Specialty::command,"担任领军时，开战多12军令"},
    {{"黎岚","穿林连射","弓阵连射伤害更高",bow,volley,90,31,93,80},
     {39,39,39},145,Specialty::scout,"负责搜寻时，更容易找到军械"},
    {{"梁稷","同袍共济","恢复军心并补充军令",spear,inspire,39,32,67,91},
     {36,36,36},140,Specialty::granary,"任太守时，每季增加28粮"},
    {{"庞统","连环奇策","瓦解阵势与军心",crossbow,guile,57,33,42,98},
     {6,58,-1},230,Specialty::command,"担任领军时，开战多12军令"},
    {{"张任","伏弩截道","弩阵连射伤害更高",crossbow,volley,104,34,92,85},
     {47,47,-1},185,Specialty::scout,"负责搜寻时，更容易找到军械"},
    {{"吕玲绮","飞骑卷旗","重骑突击兼稳军心",heavyHorse,charge,171,35,94,66},
     {24,49,24},190,Specialty::command,"担任领军时，开战多12军令"},
    {{"陈到","白毦固阵","护军并补充军令",shields,guard,23,36,88,80},
     {18,52,17},175,Specialty::convoy,"担任领军时，出征最多省22粮"},
    {{"贺齐","山越奇袭","迂回并瓦解敌军",spear,guile,51,37,89,83},
     {20,20,20},175,Specialty::scout,"负责搜寻时，更容易找到军械"},
    {{"马良","白眉安军","恢复军心并补充军令",bow,inspire,48,38,47,94},
     {17,15,59},160,Specialty::envoy,"任太守时，每季额外恢复6民心"},
    {{"徐庶","断援之计","断敌协战并压制弓弩两合",bow,breakSupport,0,39,66,97},
     {11,1,1},200,Specialty::treasury,"任太守时，每季增加22金"},
    {{"王平","无当定军","收住溃退，两合护军",shields,rallyGuard,0,40,87,80},
     {55,55,52},185,Specialty::command,"担任领军时，开战多12军令"},
    {{"朱然","江防火矢","火箭突袭本路，水战更强",bow,fireArrow,115,41,87,83},
     {43,43,53},185,Specialty::convoy,"担任领军时，出征最多省22粮"},
};
constexpr int talentCount=int(std::size(talents));
constexpr int legacyTalentCount=6;
constexpr int previousTalentCount=12;
constexpr int extraGuestFirst=talentFirst+previousTalentCount;
constexpr int guestCount=3;
constexpr int newTalentFirst=extraGuestFirst+guestCount-1;
constexpr int rulerSlot=newTalentFirst+talentCount-previousTalentCount;
constexpr int commanderSlots=rulerSlot+1;
constexpr int guestIndex(int slot) {
    return slot==guestSlot?0:slot>=extraGuestFirst && slot<newTalentFirst?slot-extraGuestFirst+1:-1;
}
constexpr int guestCommand(int index) {return index==0?guestSlot:extraGuestFirst+index-1;}
constexpr int talentSlot(int id) {return id<previousTalentCount?talentFirst+id:newTalentFirst+id-previousTalentCount;}
constexpr int talentIndex(int slot) {
    if (slot>=talentFirst && slot<extraGuestFirst) return slot-talentFirst;
    if (slot>=newTalentFirst && slot<rulerSlot) return slot-newTalentFirst+previousTalentCount;
    return -1;
}
inline const WildTalent* talentAtSlot(int slot) {
    const int id=talentIndex(slot);
    return id>=0?&talents[id]:nullptr;
}
constexpr const char* maneuvers[]={"整阵","强攻","固守","迂回","回撤"};
constexpr const char* duelMoves[]={"突刺","格挡","佯攻"};
constexpr const char* dispatchNames[]={"先锋","偏师","主力"};
constexpr int levyPercent[]={45,65,80};
struct Faction {const char* title;};
constexpr Faction factions[] = {
    {"刘备军"},{"曹操军"},{"孙权军"},{"荆南诸郡"},
    {"刘璋军"},{"张鲁军"},{"关西诸部"},{"交州守军"},
};
constexpr const char* troopNames[]={"枪兵","弓兵","骑兵","盾兵","弩兵","重骑"};
constexpr const char* troopHints[]={"枪阵拒骑","远射克枪","快袭弓手","举盾挡箭","远射破甲","冲阵破盾"};
constexpr const char* engineNames[]={"无器械","冲车","投石车"};
constexpr bool mounted(Troop troop) {return troop==horse || troop==heavyHorse;}
constexpr bool ranged(Troop troop) {return troop==bow || troop==crossbow;}
constexpr const char* formations[]={"方阵","锋矢阵","雁行阵"};
constexpr const char* formationHints[]={"收拢减伤","中军强攻","两翼包抄"};
constexpr const char* weaponNames[]={"长枪","环首刀","长弓"};
constexpr const char* weaponAttacks[]={"挺枪","挥刀","放箭"};
constexpr const char* weaponHints[]={"前排破骑","前排破阵","后排可射"};
constexpr Weapon preferredWeapon(Troop troop) {
    return ranged(troop)?longbow:mounted(troop)?saber:longspear;
}
constexpr Troop counterTroop(Troop enemy) {
    constexpr Troop counters[]={bow,horse,spear,heavyHorse,shields,spear};
    return counters[enemy];
}
constexpr const char* terrains[] = {"平原", "山地", "水乡"};

// Gazetteer anchors are approximate present-day coordinates for late-Han sites.
constexpr float worldX(float lon) {return (lon-101.5f)*worldWidth/22.f;}
constexpr float worldY(float lat) {return (41.f-lat)*worldHeight/23.8f;}
struct Site {
    const char* name;
    const char* region;
    float lon, lat;
    Terrain terrain;
    int yield, owner, troops, fort;
    SiteKind kind=county;
    int x() const {return int(worldX(lon)+.5f);}
    int y() const {return int(worldY(lat)+.5f);}
};
constexpr Site sites[] = {
    {"长安","京兆尹",108.86f,34.32f,hills,4,1,620,2},
    {"洛阳","河南尹",112.60f,34.73f,plain,4,1,520,2},
    {"成都","蜀郡",104.07f,30.66f,plain,5,4,940,3},
    {"南郑","汉宁郡",107.02f,33.06f,hills,4,5,730,3},
    {"许都","颍川郡",114.03f,33.99f,plain,5,1,1100,3},
    {"邺城","魏郡",114.42f,36.27f,plain,4,1,600,2},
    {"襄阳","襄阳郡",112.16f,32.03f,river,4,1,820,2},
    {"江陵","南郡",112.19f,30.35f,river,5,1,720,2},
    {"秣陵","丹阳郡",118.83f,31.85f,river,4,2,860,2},
    {"柴桑","豫章郡",115.94f,29.64f,river,4,2,1120,2},
    {"夏口","江夏郡",114.28f,30.55f,river,4,0,1700,2,stronghold},
    {"宛城","南阳郡",112.53f,33.01f,plain,3,1,530,2},
    {"夷陵","临江郡",111.29f,30.70f,hills,2,1,520,1},
    {"江州","巴郡",106.58f,29.56f,river,3,4,630,2},
    {"梓潼","广汉郡",105.17f,31.64f,hills,2,4,510,1},
    {"临湘","长沙郡",112.97f,28.18f,river,4,3,470,1},
    {"泉陵","零陵郡",111.63f,26.24f,hills,2,3,385,1},
    {"临沅","武陵郡",111.69f,29.03f,river,3,3,420,1},
    {"郴县","桂阳郡",113.03f,25.80f,hills,3,3,415,1},
    {"吴县","吴郡",120.62f,31.31f,river,5,2,630,2},
    {"山阴","会稽郡",120.59f,30.00f,hills,3,2,550,1},
    {"南昌","豫章郡",115.88f,28.69f,river,3,2,620,2},
    {"寿春","九江郡",116.78f,32.58f,river,4,1,490,2},
    {"合肥","九江郡",117.25f,31.89f,plain,3,1,600,2},
    {"下邳","下邳郡",117.89f,34.12f,plain,4,1,540,2},
    {"剧县","北海国",118.77f,36.66f,plain,3,1,480,1},
    {"蓟县","广阳郡",116.36f,39.90f,plain,3,1,480,2},
    {"襄武","陇西郡",104.63f,35.01f,hills,2,6,560,1},
    {"姑臧","武威郡",102.64f,37.93f,hills,2,6,600,2},
    {"番禺","南海郡",113.27f,23.13f,river,3,7,600,2},
    {"葭萌","广汉郡",105.72f,32.32f,hills,2,4,510,1},
    {"广陵","广陵郡",119.42f,32.39f,river,3,1,520,2},
    {"始安","零陵郡",110.29f,25.27f,hills,3,3,420,2},
    {"临贺","苍梧郡",111.62f,24.23f,river,3,7,430,2},
    {"广信","苍梧郡",111.32f,23.48f,river,4,7,590,2},
    {"高要","苍梧郡",112.46f,23.05f,river,3,7,330,1},
    {"合浦","合浦郡",109.16f,21.64f,river,3,7,465,2},
    {"徐闻","合浦郡",110.13f,20.26f,river,3,7,390,1},
    {"侯官","会稽郡",119.30f,26.08f,river,3,2,410,1},
    {"建安","会稽郡",118.32f,27.02f,hills,3,2,440,2},
    {"羸娄","交趾郡",106.08f,21.02f,river,4,7,680,2},
    {"胥浦","九真郡",105.78f,19.87f,river,3,7,475,2},
    {"西卷","日南郡",106.40f,17.75f,river,3,7,340,1},
    {"鄂县","江夏郡",114.88f,30.41f,river,4,2,450,2},
    {"陈仓","右扶风",107.25f,34.36f,hills,4,1,575,2},
    {"谯县","沛国",115.78f,33.87f,plain,3,1,510,2},
    {"平原","平原郡",116.44f,37.02f,plain,3,1,420,1},
    {"雒县","广汉郡",104.28f,30.98f,plain,4,4,565,2},
    {"濮阳","东郡",115.03f,35.71f,plain,3,1,470,2},
    {"小沛","沛国",116.93f,34.73f,plain,3,1,420,1},
    {"官渡","河南尹",114.08f,34.73f,plain,3,1,540,2,crossing},
    {"赤壁","江夏郡沿江",113.63f,29.88f,river,3,2,585,2,battleSite},
    {"白帝城","巴东郡",109.57f,31.04f,hills,3,4,475,2,stronghold},
    {"麦城","南郡",111.89f,30.73f,hills,2,1,430,2,stronghold},
    {"虎牢关","河南尹",113.17f,34.85f,hills,3,1,575,3,pass},
    {"剑阁","广汉郡",105.57f,32.21f,hills,2,4,610,3,pass},
    {"樊城","襄阳郡",112.15f,32.06f,river,3,1,580,2,stronghold},
    {"上庸","汉宁郡",110.24f,32.10f,hills,2,5,440,2},
    {"涪城","广汉郡",104.74f,31.45f,plain,3,4,460,1},
    {"秭归","临江郡",110.97f,30.85f,river,2,1,440,2},
    {"皖城","庐江郡",116.57f,30.63f,river,3,1,550,2},
    {"濡须口","庐江郡沿江",118.10f,31.39f,river,2,2,490,2,crossing},
    {"临淄","齐国",118.34f,36.87f,plain,4,1,475,2},
    {"祁山","陇右",105.11f,34.30f,hills,2,6,435,2,stronghold},
};
static_assert(std::size(sites) == cityCount);
constexpr bool reachedDate(int year,int season,int fromYear,int fromSeason=0) {
    return year*4+season>=fromYear*4+fromSeason;
}
constexpr const char* cityLabel(int city,int year,int season=0) {
    if (city==8 && year>=212) return "建业";
    if (city==4 && year>=221) return "许昌";
    if (city==43 && reachedDate(year,season,221,1)) return "武昌";
    if (city==12 && year>=222) return "西陵";
    if (city==30 && year>=217) return "汉寿";
    if (city==52 && reachedDate(year,season,222,1)) return "永安";
    return sites[city].name;
}
constexpr const char* cityDistrict(int city,int year,int season=0) {
    if (city==3 && reachedDate(year,season,215,2)) return "汉中郡";
    if (city==12 && year>=210) return "宜都郡";
    if ((city==14 || city==30 || city==55 || city==fucheng) && year>=217) return "梓潼郡";
    if ((city==43 || city==9) && reachedDate(year,season,221,1)) return "武昌郡";
    if (city==36 && year>=228) return "珠官郡";
    if (city==shangyong && reachedDate(year,season,237,1)) return "上庸郡";
    if (city==shangyong && reachedDate(year,season,220,3)) return "新城郡";
    if (city==shangyong && reachedDate(year,season,215,2)) return "上庸郡";
    if (city==zigui && year>=210) return "宜都郡";
    return sites[city].region;
}
inline std::pair<float,float> cityMapPoint(int city,int year) {
    if (city==8 && year>=212) return {worldX(118.76f),worldY(32.05f)};
    return {worldX(sites[city].lon),worldY(sites[city].lat)};
}
struct GeoPoint {float lon=0,lat=0;};
constexpr int roadBends=8;
struct Road {
    int from,to;
    GeoPoint bends[roadBends]{};
    const char* name=nullptr;
    bool coastal=false;
    constexpr bool joins(int a,int b) const {
        return (from==a&&to==b)||(from==b&&to==a);
    }
};
constexpr Road roads[] = {
    {0,1,{{109.77f,34.51f},{110.29f,34.56f},{111.20f,34.77f},{112.04f,34.73f}},"崤函道"},
    {0,3,{{108.25f,34.15f},{107.77f,34.19f},{107.70f,34.06f},{107.18f,33.86f},{106.94f,33.43f},{106.96f,33.20f}},"褒斜道"},
    {0,27,{{106.11f,34.72f}}},
    {1,4,{{112.54f,34.52f},{112.79f,34.45f},{113.02f,34.38f},{113.43f,34.27f},{113.79f,34.13f}},"嵩山南麓"},
    {1,5,{{114.10f,35.10f}}},
    {1,11,{{112.47f,34.56f},{112.40f,33.94f},{112.43f,33.50f}},"伊阙道"},
    {2,13,{{104.39f,30.86f},{105.15f,30.59f},{105.57f,30.50f},{105.84f,30.18f},{106.27f,29.98f}},"涪江走廊"},
    {2,14,{{104.39f,31.13f},{104.70f,31.47f},{104.95f,31.63f}},"金牛道"},
    {3,12,{{107.34f,33.15f},{108.51f,32.89f},{108.94f,32.69f},{109.36f,32.83f},{110.11f,32.81f},{110.23f,32.23f},{110.73f,32.06f},{110.76f,31.35f}},"汉水至荆山"},
    {3,30,{{106.63f,33.16f},{106.25f,32.83f},{106.02f,32.68f},{105.89f,32.64f},{105.83f,32.45f}},"金牛道"},
    {3,27,{{106.14f,33.33f},{106.16f,33.92f},{105.70f,34.57f},{104.70f,34.76f}},"陇右山道"},
    {4,5},
    {4,11},
    {4,22,{{115.34f,33.10f}}},
    {4,24,{{116.18f,34.30f}}},
    {5,25},
    {5,26,{{115.77f,38.13f}}},
    {6,7,{{112.24f,31.69f},{112.17f,31.26f},{112.20f,31.03f},{112.31f,30.76f}},"荆襄走廊"},
    {6,10,{{112.26f,31.71f},{112.57f,31.17f},{112.59f,30.70f},{113.14f,30.47f},{113.45f,30.38f},{114.03f,30.60f}},"汉水"},
    {6,11},
    {7,10,{{112.25f,30.23f},{112.41f,29.97f},{112.88f,29.70f},{113.13f,29.46f},{113.63f,29.88f},{114.04f,30.28f}},"长江"},
    {7,12,{{111.90f,30.42f},{111.67f,30.39f}},"长江"},
    {7,17},
    {7,15,{{112.42f,30.09f},{112.86f,29.68f},{113.09f,29.39f},{112.82f,28.96f},{112.91f,28.55f}},"江湘水道"},
    {8,19,{{119.18f,31.54f},{119.60f,31.33f},{120.03f,31.38f}},"太湖通道"},
    {8,23,{{118.49f,31.83f},{118.35f,31.40f},{118.10f,31.39f},{117.95f,31.50f},{117.83f,31.58f},{117.54f,31.59f},{117.34f,31.75f}},"巢湖濡须"},
    {8,31,{{118.85f,32.14f},{119.06f,32.22f},{119.32f,32.22f}},"渡江走廊"},
    {9,10,{{115.67f,29.86f},{115.41f,29.93f},{115.25f,30.15f},{115.07f,30.24f},{114.81f,30.49f},{114.55f,30.61f}},"长江"},
    {9,21,{{116.13f,29.75f},{116.16f,29.37f},{116.02f,28.94f}},"鄱阳赣水"},
    {9,23,{{116.15f,29.76f},{116.90f,30.43f},{117.72f,30.95f},{118.10f,31.39f},{117.95f,31.50f},{117.83f,31.58f},{117.54f,31.59f},{117.34f,31.75f}},"长江濡须"},
    {10,15,{{113.89f,30.24f},{113.52f,29.63f},{113.09f,29.35f},{112.81f,28.94f},{112.91f,28.55f}},"江湘水道"},
    {11,22,{{114.82f,32.70f}}},
    {12,13,{{110.98f,30.85f},{110.74f,30.96f},{109.88f,31.08f},{109.57f,31.04f},{108.70f,30.89f},{108.39f,30.79f},{108.03f,30.30f},{107.39f,29.70f}},"三峡水道"},
    {12,17,{{110.79f,29.80f}}},
    {13,14,{{105.71f,30.75f}}},
    {14,30,{{105.34f,31.78f},{105.48f,31.92f},{105.51f,32.05f},{105.57f,32.21f},{105.65f,32.29f}},"金牛道"},
    {15,16,{{112.97f,27.86f},{112.85f,27.22f},{112.60f,26.93f},{112.40f,26.60f},{112.08f,26.60f},{111.86f,26.59f},{111.61f,26.45f}},"湘水"},
    {15,17,{{112.86f,28.63f},{112.43f,28.86f},{112.00f,28.92f}},"洞庭水路"},
    {15,18},
    {15,21,{{114.29f,28.06f}}},
    {16,17,{{111.22f,27.64f}}},
    {16,18,{{112.12f,26.12f},{112.73f,25.94f}},"南岭北麓"},
    {16,29,{{112.15f,25.89f},{112.39f,25.59f},{112.39f,24.78f},{113.39f,24.18f},{113.03f,23.70f}},"连江水道"},
    {18,29,{{113.19f,25.42f},{113.53f,25.10f},{113.60f,24.81f},{113.40f,24.20f},{113.03f,23.70f}},"北江水道"},
    {19,20,{{120.30f,31.03f},{120.10f,30.87f},{120.12f,30.38f},{120.16f,30.22f},
            {120.17f,30.14f},{120.27f,30.11f}},"钱唐通道"},
    {20,21,{{120.23f,29.71f},{119.65f,29.10f},{119.17f,29.03f},{118.85f,28.95f},
            {118.24f,28.68f},{117.96f,28.44f},{116.69f,28.70f},{116.23f,28.57f}},"会稽豫章道"},
    {21,29,{{115.52f,28.16f},{115.02f,27.11f},{114.93f,25.83f},{114.34f,25.32f},{113.60f,24.81f},{113.08f,23.69f}},"赣粤通道"},
    {22,23,{{116.93f,32.45f},{116.97f,32.30f},{117.12f,32.13f},{117.16f,32.04f}},"淝水走廊"},
    {22,24},
    {23,31,{{117.34f,31.75f},{117.54f,31.59f},{117.83f,31.58f},{117.95f,31.50f},{118.10f,31.39f},{118.63f,31.86f},{119.22f,32.17f}},"巢湖濡须"},
    {24,25},
    {24,31},
    {25,26,{{117.29f,38.40f}}},
    {27,28,{{104.02f,35.58f},{103.87f,36.06f},{103.31f,36.69f},{102.88f,37.16f}},"陇右河西"},
    {16,32,{{111.59f,25.93f},{111.00f,25.48f},{110.67f,25.61f},{110.57f,25.60f}},"灵渠水道"},
    {16,33,{{111.58f,25.50f},{111.57f,25.09f},{111.57f,24.69f}},"潇贺古道"},
    {32,34,{{110.48f,24.84f},{110.67f,24.51f},{110.55f,24.20f},{110.89f,23.89f},{111.24f,23.59f}},"桂江水道"},
    {33,34,{{111.70f,24.01f},{111.73f,23.81f},{111.51f,23.43f}},"贺江水道"},
    {34,35,{{111.49f,23.40f},{111.55f,23.31f},{111.77f,23.21f},{112.09f,23.08f},{112.30f,23.15f}},"西江水道"},
    {29,35,{{113.00f,23.11f},{112.86f,23.04f},{112.64f,23.10f}},"西江水道"},
    {34,36,{{110.97f,23.37f},{110.48f,23.41f},{110.09f,22.72f},{109.67f,22.22f},{109.35f,21.82f}},"苍梧合浦道"},
    {36,37,{{109.17f,21.48f},{109.36f,21.23f},{109.55f,20.94f},
            {109.68f,20.61f},{109.79f,20.31f},{109.99f,20.27f}},"北部湾海道",true},
    // Approximate inland corridor via Shan, the Zhang'an district and Yongning.
    {20,38,{{120.82f,29.58f},{121.02f,29.14f},{121.13f,28.79f},{120.97f,28.21f},
            {120.65f,28.02f},{120.29f,27.32f},{119.64f,27.08f},{119.47f,26.53f}},"会稽南道"},
    {21,39,{{116.23f,28.57f},{116.69f,28.70f},{117.55f,28.38f},{117.96f,28.44f},
            {118.24f,28.68f},{118.50f,28.28f},{118.53f,27.92f},{118.11f,27.33f}},"闽赣山道"},
    {38,39,{{119.02f,26.19f},{118.79f,26.24f},{118.55f,26.50f},
            {118.18f,26.64f},{118.22f,26.89f}},"闽江建溪"},
    {36,40,{{109.13f,21.47f},{108.67f,21.26f},{108.08f,21.22f},
            {107.61f,20.87f},{106.88f,20.73f},{106.57f,20.91f},{106.34f,20.98f}},"北部湾海道",true},
    {40,41,{{106.09f,20.82f},{106.03f,20.54f},{105.97f,20.26f},{105.89f,19.99f}},"交九陆路"},
    {41,42,{{105.70f,19.62f},{105.55f,19.23f},{105.64f,18.73f},{105.87f,18.35f},
            {106.07f,18.18f},{106.30f,17.97f}},"九真日南道"},
    {10,43,{{114.55f,30.61f},{114.76f,30.55f}},"长江"},
    {43,9,{{115.07f,30.33f},{115.25f,30.15f},{115.41f,29.93f},{115.67f,29.86f},{115.73f,29.83f}},"长江"},
    {0,44,{{108.22f,34.28f},{107.78f,34.30f},{107.58f,34.33f}},"渭水通道"},
    {44,3,{{107.08f,34.29f},{106.65f,33.99f},{106.30f,33.90f},{106.15f,33.33f},{106.63f,33.16f}},"陈仓道"},
    {4,45,{{114.53f,33.88f},{115.04f,33.78f},{115.42f,33.88f}},"颍涡通道"},
    {45,22,{{116.21f,33.50f},{116.74f,33.15f},{116.80f,32.97f},{116.78f,32.63f}},"涡淮水路"},
    {45,24,{{116.40f,34.22f},{117.20f,34.24f}}},
    {5,46,{{115.23f,36.63f},{115.71f,36.92f}},"河北平原"},
    {46,25,{{116.97f,37.14f},{117.84f,37.00f}}},
    {46,26,{{116.60f,37.90f},{116.62f,38.83f},{116.42f,39.47f}}},
    {2,47,{{104.12f,30.80f}},"金牛道"},
    {47,14,{{104.45f,31.19f},{104.67f,31.40f},{104.89f,31.58f}},"金牛道"},
    {48,5},{48,46},{48,50},
    {49,24},{49,45},{49,22},
    {50,4},{50,54},
    {51,7,{{113.13f,29.46f},{112.88f,29.70f},{112.41f,29.97f},{112.25f,30.23f}},"长江"},
    {51,10,{{113.92f,30.03f},{114.07f,30.27f},{114.15f,30.40f}},"长江"},
    {52,12,{{109.88f,31.08f},{110.24f,31.06f},{110.74f,30.96f},{110.98f,30.85f}},"三峡水道"},
    {52,13,{{108.70f,30.89f},{108.39f,30.79f},{108.03f,30.30f},{107.39f,29.70f},{106.92f,29.63f}},"三峡水道"},
    {53,6,{{112.19f,31.27f}},"荆襄通道"},
    {53,7},{53,12},
    {54,1},
    {55,30,{{105.65f,32.29f}},"金牛道"},
    {55,14,{{105.51f,32.05f},{105.48f,31.92f},{105.34f,31.78f}},"金牛道"},
    {56,6,{{112.15f,32.04f}},"汉水渡口"},
    {56,11,{{112.26f,32.36f},{112.36f,32.68f}},"南阳盆地"},
    {57,3,{{109.72f,32.80f},{109.03f,32.69f},{108.50f,32.89f},{107.55f,33.07f}},"汉水上游"},
    {57,59,{{110.74f,32.06f},{110.73f,31.43f},{110.75f,31.06f}},"房陵山道"},
    {58,47,{{104.62f,31.28f},{104.43f,31.14f}},"金牛道"},
    {58,14,{{104.87f,31.52f},{105.00f,31.58f}},"金牛道"},
    {59,12,{{111.01f,30.85f},{111.18f,30.75f}},"西陵峡"},
    {59,52,{{110.74f,30.96f},{110.24f,31.06f},{109.88f,31.08f}},"三峡水道"},
    {60,9,{{116.43f,30.41f},{116.26f,30.13f},{116.15f,29.76f}},"皖水长江"},
    {60,61,{{117.00f,30.51f},{117.39f,30.70f},{117.72f,30.95f}},"沿江水路"},
    {61,23,{{117.95f,31.50f},{117.83f,31.58f},{117.54f,31.59f},{117.34f,31.75f}},"濡须水"},
    {61,8,{{118.35f,31.40f},{118.49f,31.83f}},"长江"},
    {62,25,{{118.57f,36.84f}},"淄水走廊"},
    {62,46,{{117.82f,36.91f},{117.40f,36.87f},{116.99f,37.16f}},"齐地平原"},
    {63,27,{{105.14f,34.50f},{104.86f,34.70f},{104.69f,34.84f}},"陇右山道"},
    {63,3,{{105.46f,34.06f},{106.06f,33.72f},{106.16f,33.33f},{106.63f,33.16f}},"西汉水谷地"},
};
constexpr bool adjacent(int a, int b) {
    for (const auto& r : roads) if (r.joins(a,b)) return true;
    return false;
}
}
