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
     "我父亲只点一盏灯。昨夜外海却亮着两盏。带上旧潮表，沿丰沙尔到大加那利岛查一查，再告诉莱昂。",
     "真正的灯只在落潮后点亮。渔会愿意公开潮表。船行出二百二十金买走独家灯讯。你决定留给谁。",
     "答应查灯", "交给渔会", "卖给船行",
     "莱昂把潮表钉在鱼市。往后遇到可疑灯标时，可用这份潮表免费辨认暗礁。",
     "船行付了二百二十金。第二天，鱼市木板上的旧潮表又被撕了。"},
    {tangier, tunis, 1, 2, "欠名的海图", "纳迪娅", "优素福",
     "这张海峡图是我母亲画的。商号把她的名字刮掉了。你若去突尼斯，请让优素福看看背面的针孔。",
     "针孔拼出了画图人的名字。给我两件木料刻印版，我就把名字印回去。也有商号愿出二百五十金收图。",
     "带上海图", "捐木料×2", "售图换金",
     "新印的海图署了母亲的名字。地中海船坞认得你的印记，维修便宜四分之一。",
     "商号付了二百五十金。优素福只留下一张拓印，仍能摸到纸背的针孔。"},
    {mozambique, malindi, 4, 4, "借来的季风", "阿明", "萨菲亚",
     "去年季风提前来，我的船没赶上。替我把改期的潮信送到马林迪吧。她一直把空泊位留到日落。",
     "原来他仍想回来。给泊地留八份水粮，来船就能等风。商人也愿出二百七十金买我们的船期。",
     "带上潮信", "留下水粮八份", "转售船期",
     "萨菲亚把水留给下一艘船。印度洋港口给你补给折扣，海上的友船也愿低价供水。",
     "你收下二百七十金。萨菲亚把潮信叠好，等候的泊位从此只留到正午。"}
};
static_assert(std::size(Tales) == TaleCount);

enum Encounter : std::uint8_t {
    Calm, Storm, Privateer, Castaways, FalseBeacon, Rigging, ReliefShip, Fog,
    Boarding, DockDuel, ReversedFlag, StrandedPost, EncounterCount
};
enum EscortStage : std::uint8_t {
    EscortOpen, EscortSailing, EscortWonGuns, EscortWonDuel, EscortPublished, EscortSold
};
enum NavalOrder { CloseRange, FireGuns, BreakAway, BoardShip };
enum NavalIntent { EnemyClose, EnemyFire, EnemyEvade };
enum DuelOrder { Thrust, Parry, Feint, Yield };
inline constexpr Text SailOrders[]={"机动装填","火炮齐射","借风脱离","接舷决斗"};
inline constexpr Text EnemyOrders[]={"敌船逼近","敌船要开炮","敌船正在拉远"};
inline constexpr Text DuelOrders[]={"突刺","格挡","佯攻","认输"};
inline constexpr Text DuelIntent[]={"敌手蓄势突刺","敌手摆出守势","敌手准备佯攻"};
struct EncounterScene {
    Text title, message;
    std::array<Text,3> choices, results;
};
inline constexpr EncounterScene Encounters[] {
    {},
    {"风暴封住了航路", "浪头越过船舷。等风要多耗四份口粮。抢浪会伤船，转舵会退回一段航程。",
     {"缩帆待过","顶浪前进","转舵避浪"}, {"缩帆待过，延误一日","顶浪前进，船体受损","转舵避浪，退回一段航程"}},
    {"私掠船追上来了", "",
     {"贴近瞄准","一轮齐射","借风脱离"}, {"击退私掠船","借风摆脱私掠船","被商船救回港"}},
    {"折桅的小船", "小船正挂旗求救。带人上船要分出四份口粮。附近也漂着一只淡水桶。",
     {"救援小船","捞取淡水","继续航行"}, {"救下遇难者，受到答谢","从漂桶取得淡水","继续航行，未停靠"}},
    {"岬角上的双灯", "两盏灯一明一暗，船头测得水深正在变浅。核对潮表要多用一日和三份口粮，也能停船等天亮。",
     {"核对潮表","循灯近岸","待到天亮"}, {"查明灯位，绘下暗礁","循假灯近岸，触及浅滩","等到天亮，从外海绕过"}},
    {"断索横过甲板", "主帆的一根索绷断了。木料可做撑杆。换用备用帆会慢些，强行拉索会伤船。",
     {"木料修缮","换备用帆","强行拉索"}, {"用木料支住桅杆","换帆慢行，未伤船体","拉索脱手，船体受损"}},
    {"无旗补给船", "一艘小商船升起盛水罐。一件海盐可换十八份淡水。付金币可以买十四份。",
     {"海盐换水","付金买水","谢绝离开"}, {"用海盐换得淡水","付金补足淡水","谢过友船，继续航行"}},
    {"雾中钟声", "船员能听到岸上的钟，却看不见岸。等晨雾散去会耗一天。测过两处水域就能用自己的海图定方位。",
     {"等雾散去","据图辨位","摸黑前行"}, {"等了一日，晨雾散去","按自己的海图绕过浅滩","摸黑前行，擦伤船壳"}},
    {"接舷决斗","",{},{"在甲板上赢得决斗","割缆脱身，船员受损","落败，带伤退回本船"}},
    {"加的斯的旧誓","",{},{"裁判见证胜负，费罗交出信物","交出押金，改日再来","在演练中落败，船员未受伤"}},
    {"倒挂的旗号","商船把旗倒挂着。追它的武装小船却挂着救助旗。你能上前护航，也能拿淡水换它递出的证词。",
     {"上前护航","分水问讯","绕礁离开"},{"借旗号接近私掠船","分水换得证词，渔会记下姓名","等追船远去，用去一日"}},
    {"搁浅的邮船","礁边邮船在涨潮前无法脱身。邮袋里有几张可能被抹掉地名的海图，仍来得及保存。",
     {"按图登船","用木料拖带","记下坐标"},{"救出邮袋，保住了海图","邮船拖离浅滩，船主给了谢礼","在海图上留下这处危险浅滩"}}
};
static_assert(std::size(Encounters) == EncounterCount);

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
