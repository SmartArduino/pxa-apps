#pragma once

namespace tide {

enum Tale { Lamps, Chartmakers, Monsoon, TaleCount };
enum TaleState : std::uint8_t { Unmet, InProgress, Shared, Sold };
enum BeaconState : std::uint8_t { BeaconUnseen, BeaconSighted, BeaconCharted };

struct PortTale {
    int start, finish, start_portrait, finish_portrait;
    Text title, starter, listener, opening, delivery;
    Text accept, share, sell, shared_end, sold_end;
};
inline constexpr PortTale Tales[] {
    {madeira, canaries, 0, 1, "双灯之夜", "玛拉", "莱昂",
     "我爹只在退潮后点灯，昨晚海上却亮了两盏。带上这张潮表，往大加那利岛走一趟。查到什么，就去告诉莱昂。",
     "退潮后亮的那盏才是真的。把潮表给渔会，他们会贴出来。船行也愿出二百二十金，要把这消息留给自家船。",
     "带潮表查灯", "交给渔会", "卖220金",
     "莱昂把潮表钉在鱼市。以后再遇到假灯，你可以直接查潮表辨认暗礁。",
     "船行付了二百二十金。第二天有人把鱼市木板上的旧潮表撕了。"},
    {tangier, tunis, 1, 2, "欠名的海图", "纳迪娅", "优素福",
     "这张图是我娘画的，商号把她的名字刮了。你若去突尼斯，请让优素福看看纸背。他认得那些针孔。",
     "这些针孔是她的署名。给我两件木料，我刻一块印版，把名字印回去。商号也肯出二百五十金收图。",
     "带图去突尼斯", "给两件木料", "卖250金",
     "新印的图署上了纳迪娅母亲的名字。从此在地中海港口修船，船坞给你打七五折。",
     "商号付了二百五十金。优素福只留住一张拓印。拿起来对着光，还能看见纸背透出的针孔。"},
    {mozambique, malindi, 4, 4, "借来的季风", "阿明", "萨菲亚",
     "去年季风来得早，我没赶上船期。请把这封信带给马林迪的萨菲亚，告诉她我还会回来。",
     "他还记着那处泊位啊。留八份口粮、八份水，下条船就能在这里等风。也有商人肯出二百七十金买船期。",
     "带信去马林迪", "留下粮8水8", "卖270金",
     "萨菲亚把水留给下一条船。从此在印度洋港口补给更便宜，海上遇到补给船也能少花些钱。",
     "你收下二百七十金。萨菲亚把信折好，从那以后，她只把空泊位留到正午。"}
};
static_assert(std::size(Tales) == TaleCount);

enum Encounter : std::uint8_t {
    Calm, Storm, Privateer, Castaways, FalseBeacon, Rigging, ReliefShip, Fog,
    Boarding, DockDuel, ReversedFlag, StrandedPost, ShoalLantern, Doldrums, EncounterCount
};
enum EscortStage : std::uint8_t {
    EscortOpen, EscortSailing, EscortWonGuns, EscortWonDuel, EscortPublished, EscortSold
};
enum NavalOrder { CloseRange, FireGuns, BreakAway, BoardShip };
enum NavalIntent { EnemyClose, EnemyFire, EnemyEvade };
enum DuelOrder { Thrust, Parry, Feint, Yield };
inline constexpr Text SailOrders[]={"靠近并装填","开炮","尝试脱身","接舷决斗"};
inline constexpr Text EnemyOrders[]={"敌船正靠近","敌船将开炮","敌船转舵拉开"};
inline constexpr Text DuelOrders[]={"突刺","格挡","佯攻","认输"};
inline constexpr Text DuelIntent[]={"对手要突刺","对手举剑格挡","对手在虚晃"};
struct EncounterScene {
    Text title, message;
    std::array<Text,3> choices, results;
};
inline constexpr EncounterScene Encounters[] {
    {},
    {"风暴挡路", "浪已经打上甲板。缩帆等一天要耗四份口粮。硬闯可能伤船，掉头躲浪会丢掉一段航程。",
     {"缩帆等一天","顶浪前进","转舵避浪"}, {"等了一天，风暴过去了","船闯出风暴，船体受损","船避开大浪，也退回了一段"}},
    {"私掠船追上来了", "",
     {"贴近瞄准","一轮齐射","借风脱离"}, {"击退私掠船","借风摆脱私掠船","被商船救回港"}},
    {"折桅的小船", "有人在断桅下挥旗求救。救人上来要分四份口粮。小船旁边漂着一桶淡水。",
     {"救人上船","捞起水桶","继续航行"}, {"人救上来了，家属送来谢礼","水桶捞上来了，淡水还能喝","船继续向前"}},
    {"岬角上的双灯", "岬角有两盏灯，测深绳却探到了浅水。查潮表要花一天和三份口粮。也可等天亮再绕路。",
     {"查潮表","跟灯靠岸","等天亮"}, {"认出假灯，在海图上记下暗礁","跟错灯，船碰上了浅滩","天亮后从外海绕了过去"}},
    {"主帆断了索", "主帆的绳索断了。可用一件木料撑住桅杆，也可换小帆慢慢走。硬拉剩下的绳索可能伤船。",
     {"用木料修船","换小帆","硬拉绳索"}, {"木料撑住了桅杆，船补好了一些","换小帆后，船慢了下来","绳索脱手，船体受损"}},
    {"补给船", "小商船上的人举起水罐，问你要不要水。一件海盐最多换十八份淡水。花钱最多买十四份。",
     {"海盐换水","花钱买水","谢过离开"}, {"用海盐换到了水","买到了淡水","谢过对方，继续航行"}},
    {"雾里传来钟声", "听得见岸上的钟，却看不见礁石。等雾散得耗一天水粮。已测过两处水域，就能用自己的图绕过去。",
     {"等雾散","按海图绕行","冒雾前行"}, {"等了一天，雾散了","按海图绕过了浅滩","雾里擦到了船壳"}},
    {"接舷决斗","",{},{"在甲板上赢得决斗","割缆脱身，船员受损","落败，带伤退回本船"}},
    {"加的斯的旧誓","",{},{"裁判见证胜负，费罗交出信物","交出押金，改日再来","在演练中落败，船员未受伤"}},
    {"倒挂的旗","前面的商船倒挂旗求救，追它的船却打着救援旗。你可以上前迎战，也可给商船四份水，听听出了什么事。",
     {"上前迎战","给水问话","绕礁离开"},{"你靠近了挂假旗的私掠船","船员喝了水，说出了追船的来历","等追船走远，花了一天"}},
    {"搁浅的邮船","邮船卡在礁边，涨潮前出不来。邮袋里还有几张海图。认得水路就能登船，也可用木料帮它脱困。",
     {"按海图登船","用木料拖船","记下险滩"},{"邮袋拿上来了，海图没泡坏","邮船脱了浅，船主送来谢礼","你在图上标出了这处浅滩"}},
    {"浪里一盏灯","渔人萨法举灯求助。桨断了，潮水正把小船推向浅滩。给一件木料能做临时桨。有安娜或测过阿尔金，可花一天带船绕行。",
     {"送木料做桨","领船绕浅滩","避开沙洲"},{"萨法换上了临时桨，指给你看阿尔金的水鸟往哪儿飞","小船跟在你后面过了浅滩。萨法在图上圈出阿尔金的大致水域","小船举着灯往岸边漂。你驶向深水"}},
    {"帆垂下来了","风停了，帆布贴在桅杆上。西边落着雨，等一天可用一件织锦接水。有安娜或测过两处水域，也能沿云边找风。",
     {"铺布接雨","沿云找风","漂一天"},{"雨水汇进桶里。雨歇后，帆也重新鼓了起来","船沿云边驶进一片起风的水面","船在原地漂了一天，傍晚终于起了风"}}
};
static_assert(std::size(Encounters) == EncounterCount);
static_assert(EncounterCount<=16);

struct SeaRecord {
    int kind, choice, day;
};
inline constexpr std::uint32_t record_sea(int kind, int choice, int day) {
    return (std::uint32_t(day) << 6) | (std::uint32_t(choice) << 4) | std::uint32_t(kind);
}
inline constexpr SeaRecord read_sea(std::uint32_t record) {
    return {int(record & 15), int((record >> 4) & 3), int(record >> 6)};
}

} // namespace tide
